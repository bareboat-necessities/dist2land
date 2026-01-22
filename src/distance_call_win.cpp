#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <sstream>

#include "distance_iface.h"

using dist_fn_t = int (*)(const char* shp_path,
                          const char* provider_id,
                          double lat_deg, double lon_deg,
                          double* geodesic_m,
                          double* land_lat_deg,
                          double* land_lon_deg,
                          char* errbuf,
                          int errbuf_cap);

static HMODULE   g_mod = nullptr;
static dist_fn_t g_fn  = nullptr;

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  return (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
}

static std::string win_err(DWORD e) {
  LPWSTR msg = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageW(flags, nullptr, e, 0, (LPWSTR)&msg, 0, nullptr);
  std::wstring w = (len && msg) ? std::wstring(msg, msg + len) : L"";
  if (msg) LocalFree(msg);

  // naive utf16->utf8 for error strings (BMP only, fine for Windows messages)
  std::string out;
  out.reserve(w.size());
  for (wchar_t c : w) out.push_back((c < 128) ? char(c) : '?');
  return out;
}

static void load_backend_or_throw() {
  if (g_fn) return;

  const std::filesystem::path plugin_path = std::filesystem::path(exe_dir_w()) / L"dist2land_gdal.dll";

  if (!std::filesystem::exists(plugin_path)) {
    throw std::runtime_error("dist2land_gdal.dll not found next to dist2land.exe: " + plugin_path.string());
  }

  // Load plugin with strict search semantics:
  // - DLL load dir (plugin folder) is searched for its dependencies
  // - default/system dirs
  // - user-added dirs (AddDllDirectory) if configured in win_prepare_runtime()
  HMODULE mod = LoadLibraryExW(plugin_path.c_str(), nullptr,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                               LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                               LOAD_LIBRARY_SEARCH_USER_DIRS);

  if (!mod) {
    DWORD e = GetLastError();
    std::ostringstream oss;
    oss << "Failed to load dist2land_gdal.dll from: " << plugin_path.string()
        << " (LoadLibraryExW error " << e << ": " << win_err(e) << ")";
    oss << " (error 1114 usually means a dependency DLL init failed; ensure all DLLs are shipped next to the EXE)";
    throw std::runtime_error(oss.str());
  }

  FARPROC sym = GetProcAddress(mod, "dist2land_gdal_distance");
  if (!sym) {
    DWORD e = GetLastError();
    FreeLibrary(mod);
    std::ostringstream oss;
    oss << "dist2land_gdal.dll missing export dist2land_gdal_distance"
        << " (GetProcAddress error " << e << ": " << win_err(e) << ")";
    throw std::runtime_error(oss.str());
  }

  g_mod = mod;
  g_fn  = reinterpret_cast<dist_fn_t>(sym);
}

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  load_backend_or_throw();

  // Convert path to UTF-8 bytes for the plugin ABI.
  const auto u8 = shp_path.u8string(); // std::u8string
  std::string shp_u8(u8.begin(), u8.end());

  double geodesic_m = 0.0;
  double land_lat = 0.0;
  double land_lon = 0.0;
  char errbuf[1024] = {0};

  int rc = g_fn(shp_u8.c_str(),
                provider_id.c_str(),
                lat_deg, lon_deg,
                &geodesic_m,
                &land_lat,
                &land_lon,
                errbuf,
                (int)sizeof(errbuf));

  if (rc != 0) {
    std::string msg = errbuf[0] ? std::string(errbuf) : "GDAL backend call failed";
    throw std::runtime_error(msg);
  }

  DistanceQueryResult out;
  out.provider_id = provider_id;
  out.shp_path = shp_path;
  out.geodesic_m = geodesic_m;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.in_land = (geodesic_m == 0.0);
  return out;
}

#endif // _WIN32
