#ifdef _WIN32

#include "distance_iface.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <stdexcept>
#include <sstream>

static std::string win_last_error_string(DWORD err) {
  if (err == 0) return "OK";
  LPSTR msg = nullptr;
  DWORD n = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&msg, 0, nullptr);
  std::string out = (n && msg) ? std::string(msg, msg + n) : ("Win32 error " + std::to_string(err));
  if (msg) LocalFree(msg);
  while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) out.pop_back();
  return out;
}

static std::filesystem::path exe_dir() {
  std::wstring buf;
  buf.resize(32768);
  DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
  if (n == 0 || n >= buf.size()) return std::filesystem::path(L".");
  buf.resize(n);
  return std::filesystem::path(buf).parent_path();
}

// C++20: path::u8string() is std::u8string (char8_t). Convert to bytes for C ABI.
static std::string path_to_utf8_bytes(const std::filesystem::path& p) {
  std::u8string u8 = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

using DistFnV2 = int (*)(const char* shp_path,
                         const char* provider_id,
                         double lat_deg, double lon_deg,
                         double* geodesic_m,
                         double* land_lat_deg,
                         double* land_lon_deg,
                         int* in_land,
                         char* errbuf,
                         int errbuf_cap);

struct GdalPlugin {
  HMODULE h = nullptr;
  DistFnV2 fn = nullptr;
  std::filesystem::path dll_path;

  GdalPlugin() {
    dll_path = exe_dir() / "dist2land_gdal.dll";

    // Critical: ensure the loader resolves *dependencies of the DLL* from the DLL's directory first.
    // This avoids accidentally pulling a different GDAL stack from PATH (QGIS/OSGeo4W/MSYS2, etc).
    h = LoadLibraryExW(dll_path.wstring().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) {
      DWORD e = GetLastError();
      std::ostringstream oss;
      oss << "Failed to load dist2land_gdal.dll from: " << dll_path.string()
          << " (LoadLibraryExW error " << e << ": " << win_last_error_string(e) << ")";
      if (e == 126) {
        oss << " (error 126 usually means a dependency DLL is missing)";
      } else if (e == 1114) {
        oss << " (error 1114 means a dependency DLL's initialization failed; "
               "this is commonly caused by loading the wrong GDAL/PROJ/CURL stack from PATH, "
               "or a mismatched dependency set in the folder)";
      }
      throw std::runtime_error(oss.str());
    }

    fn = reinterpret_cast<DistFnV2>(GetProcAddress(h, "dist2land_gdal_distance_v2"));
    if (!fn) {
      DWORD e = GetLastError();
      std::ostringstream oss;
      oss << "dist2land_gdal.dll missing required symbol: dist2land_gdal_distance_v2"
          << " (GetProcAddress error " << e << ": " << win_last_error_string(e) << ")";
      FreeLibrary(h);
      h = nullptr;
      throw std::runtime_error(oss.str());
    }
  }

  ~GdalPlugin() {
    if (h) FreeLibrary(h);
  }
};

static GdalPlugin& plugin() {
  static GdalPlugin p;
  return p;
}

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                            const std::string& provider_id,
                                            const std::filesystem::path& shp_path) {
  auto& p = plugin();

  const std::string shp_u8 = path_to_utf8_bytes(shp_path);

  double geodesic_m = 0.0;
  double land_lat = 0.0;
  double land_lon = 0.0;
  int in_land_i = 0;
  char err[2048] = {0};

  const char* prov_c = provider_id.empty() ? nullptr : provider_id.c_str();

  int rc = p.fn(shp_u8.c_str(), prov_c, lat_deg, lon_deg,
                &geodesic_m, &land_lat, &land_lon, &in_land_i,
                err, (int)sizeof(err));

  if (rc != 0) {
    std::string msg = (err[0] != '\0') ? std::string(err) : "Unknown error from GDAL backend";
    throw std::runtime_error("GDAL backend failed: " + msg);
  }

  DistanceQueryResult out;
  out.provider_id = provider_id;
  out.shp_path = shp_path;
  out.geodesic_m = geodesic_m;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.in_land = (in_land_i != 0);
  return out;
}

#endif // _WIN32
