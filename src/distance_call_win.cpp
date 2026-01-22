#ifdef _WIN32

#include "distance_iface.h"
#include "win_runtime.h"

#include <windows.h>

#include <filesystem>
#include <stdexcept>
#include <string>

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L".";
  return path.substr(0, pos);
}

static std::string utf8_from_wstring(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  if (n <= 0) return {};
  std::string out;
  out.resize((size_t)n);
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), n, nullptr, nullptr);
  return out;
}

static std::string win_errmsg(DWORD err) {
  wchar_t* msg = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD n = FormatMessageW(flags, nullptr, err, 0, (LPWSTR)&msg, 0, nullptr);
  std::wstring w = (n && msg) ? std::wstring(msg, msg + n) : L"";
  if (msg) LocalFree(msg);
  // trim
  while (!w.empty() && (w.back() == L'\r' || w.back() == L'\n' || w.back() == L' ')) w.pop_back();
  auto s = utf8_from_wstring(w);
  if (s.empty()) s = "Windows error " + std::to_string((unsigned long)err);
  return s;
}

using DistFn = int (*)(
  const char* shp_path,
  const char* provider_id,
  double lat_deg,
  double lon_deg,
  double* geodesic_m,
  double* land_lat_deg,
  double* land_lon_deg,
  char* errbuf,
  int errbuf_cap
);

static HMODULE g_mod = nullptr;
static DistFn   g_fn  = nullptr;

static void load_backend_or_throw() {
  if (g_fn) return;

  // Critical: set PROJ_DATA/PROJ_LIB/GDAL_DATA to native paths BEFORE GDAL/PROJ loads.
  win_prepare_runtime();

  const std::wstring dir = exe_dir_w();
  const std::wstring plugin_path = dir + L"\\dist2land_gdal.dll";

  // Load plugin; its dependent DLLs should be resolved from the same folder.
  HMODULE mod = LoadLibraryExW(
      plugin_path.c_str(),
      nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

  if (!mod) {
    DWORD e = GetLastError();
    throw std::runtime_error(
        "Failed to load dist2land_gdal.dll from: " + utf8_from_wstring(plugin_path) +
        " (" + win_errmsg(e) + ")");
  }

  auto sym = (DistFn)GetProcAddress(mod, "dist2land_gdal_distance");
  if (!sym) {
    DWORD e = GetLastError();
    FreeLibrary(mod);
    throw std::runtime_error("dist2land_gdal.dll is missing symbol dist2land_gdal_distance (" + win_errmsg(e) + ")");
  }

  g_mod = mod;
  g_fn = sym;
}

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  load_backend_or_throw();

  // Pass UTF-8 to the plugin.
  const std::string shp_u8 = utf8_from_wstring(shp_path.wstring());

  double geod = 0.0, land_lat = 0.0, land_lon = 0.0;
  char errbuf[2048] = {0};

  int rc = g_fn(
      shp_u8.c_str(),
      provider_id.c_str(),
      lat_deg, lon_deg,
      &geod, &land_lat, &land_lon,
      errbuf, (int)sizeof(errbuf));

  if (rc != 0) {
    std::string msg = errbuf[0] ? std::string(errbuf) : "GDAL backend call failed";
    throw std::runtime_error(msg);
  }

  DistanceQueryResult out;
  out.provider_id = provider_id;
  out.shp_path = shp_path;
  out.geodesic_m = geod;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.in_land = (geod == 0.0);
  return out;
}

#endif
