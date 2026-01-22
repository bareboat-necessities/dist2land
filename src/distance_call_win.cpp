#ifdef _WIN32

#include "distance_iface.h"
#include "win_runtime.h"

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

using dist_fn_t = int(__declspec(dllimport) *)(const char* shp_path,
                                              const char* provider_id,
                                              double lat_deg, double lon_deg,
                                              double* geodesic_m,
                                              double* land_lat_deg,
                                              double* land_lon_deg,
                                              char* errbuf,
                                              int errbuf_cap);

static std::once_flag g_once;
static HMODULE g_mod = nullptr;
static dist_fn_t g_fn = nullptr;

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L".";
  return path.substr(0, pos);
}

static std::string win32_error_message(DWORD code) {
  char* msg = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD n = FormatMessageA(flags, nullptr, code, 0, (LPSTR)&msg, 0, nullptr);
  std::string out = (n && msg) ? std::string(msg, msg + n) : std::string("Unknown Win32 error");
  if (msg) LocalFree(msg);
  // Trim trailing newlines/spaces
  while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
  return out;
}

static HMODULE load_from_exe_dir_or_throw(const std::wstring& dllName) {
  const std::wstring dir = exe_dir_w();
  const std::wstring full = dir + L"\\" + dllName;

  DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

  HMODULE h = LoadLibraryExW(full.c_str(), nullptr, flags);
  if (!h) {
    DWORD err = GetLastError();
    throw std::runtime_error(
      "Failed to load dependency '" + std::string(dllName.begin(), dllName.end()) +
      "' from: " + std::string(full.begin(), full.end()) +
      " (LoadLibraryExW error " + std::to_string(err) + ": " + win32_error_message(err) + ")"
    );
  }
  return h;
}

static void load_backend_or_throw() {
  // Ensure env + DLL search are locked before any loads.
  win_prepare_runtime();

  // Preload the most failure-prone libs so 1114 points to the real culprit.
  // (If libgdal or libproj DllMain fails, you’ll see *which one* here.)
  // Names can change across MSYS2 updates; try common ones.
  const std::vector<std::wstring> gdalCandidates = {
    L"libgdal-38.dll", L"libgdal-37.dll", L"libgdal-36.dll"
  };
  const std::vector<std::wstring> projCandidates = {
    L"libproj-25.dll", L"libproj-24.dll", L"libproj-23.dll"
  };

  bool loadedGdal = false;
  for (const auto& n : gdalCandidates) {
    std::wstring full = exe_dir_w() + L"\\" + n;
    if (std::filesystem::exists(full)) { load_from_exe_dir_or_throw(n); loadedGdal = true; break; }
  }
  if (!loadedGdal) {
    throw std::runtime_error("libgdal-*.dll not found next to dist2land.exe (expected e.g. libgdal-38.dll).");
  }

  bool loadedProj = false;
  for (const auto& n : projCandidates) {
    std::wstring full = exe_dir_w() + L"\\" + n;
    if (std::filesystem::exists(full)) { load_from_exe_dir_or_throw(n); loadedProj = true; break; }
  }
  // PROJ might be linked statically into GDAL in some builds, so don’t hard-fail if missing.

  const std::wstring plugin_path = exe_dir_w() + L"\\dist2land_gdal.dll";
  if (!std::filesystem::exists(plugin_path)) {
    throw std::runtime_error("dist2land_gdal.dll not found next to dist2land.exe: " +
                             std::string(plugin_path.begin(), plugin_path.end()));
  }

  DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
  g_mod = LoadLibraryExW(plugin_path.c_str(), nullptr, flags);
  if (!g_mod) {
    DWORD err = GetLastError();
    throw std::runtime_error(
      "Failed to load dist2land_gdal.dll from: " + std::string(plugin_path.begin(), plugin_path.end()) +
      " (LoadLibraryExW error " + std::to_string(err) + ": " + win32_error_message(err) + "). "
      "Error 1114 usually means a dependency DLL's initialization failed. "
      "This build now preloads libgdal/libproj first; see earlier error if one failed."
    );
  }

  g_fn = reinterpret_cast<dist_fn_t>(GetProcAddress(g_mod, "dist2land_gdal_distance"));
  if (!g_fn) {
    FreeLibrary(g_mod);
    g_mod = nullptr;
    throw std::runtime_error("dist2land_gdal.dll is missing export: dist2land_gdal_distance");
  }
}

static std::string path_to_utf8(const std::filesystem::path& p) {
#if defined(__cpp_lib_char8_t) && __cpp_lib_char8_t >= 201811L
  auto u8 = p.u8string(); // std::u8string
  return std::string(u8.begin(), u8.end());
#else
  return p.u8string();
#endif
}

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  std::call_once(g_once, load_backend_or_throw);

  DistanceQueryResult out;
  out.provider_id = provider_id;
  out.shp_path = shp_path;

  const std::string shp_u8 = path_to_utf8(shp_path);

  double geodesic_m = 0.0;
  double land_lat = 0.0;
  double land_lon = 0.0;

  char errbuf[1024];
  errbuf[0] = 0;

  int rc = g_fn(shp_u8.c_str(),
                provider_id.c_str(),
                lat_deg, lon_deg,
                &geodesic_m, &land_lat, &land_lon,
                errbuf, (int)sizeof(errbuf));

  if (rc != 0) {
    std::string msg = errbuf[0] ? std::string(errbuf) : std::string("GDAL backend returned error");
    throw std::runtime_error(msg);
  }

  out.geodesic_m = geodesic_m;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.in_land = (geodesic_m == 0.0);
  return out;
}

#endif // _WIN32
