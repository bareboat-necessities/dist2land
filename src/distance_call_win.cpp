#ifdef _WIN32

#include "distance_iface.h"

#define NOMINMAX
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
      nullptr,
      err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&msg,
      0,
      nullptr);
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
  std::filesystem::path p(buf);
  return p.parent_path();
}

// C++20: u8string() is std::u8string (char8_t). Convert to byte string for C ABI.
static std::string path_to_utf8_bytes(const std::filesystem::path& p) {
  std::u8string u8 = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

using DistFnV1 = int (*)(const char* shp_path,
                         const char* provider_id,
                         double lat_deg, double lon_deg,
                         double* geodesic_m,
                         double* land_lat_deg,
                         double* land_lon_deg,
                         char* errbuf,
                         int errbuf_cap);

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
  DistFnV2 fn2 = nullptr;
  DistFnV1 fn1 = nullptr;
  std::filesystem::path dll_path;

  GdalPlugin() {
    dll_path = exe_dir() / "dist2land_gdal.dll";
    h = LoadLibraryW(dll_path.wstring().c_str());
    if (!h) {
      DWORD e = GetLastError();
      std::ostringstream oss;
      oss << "Failed to load dist2land_gdal.dll from: " << dll_path.string()
          << " (LoadLibraryW error " << e << ": " << win_last_error_string(e) << ")";
      throw std::runtime_error(oss.str());
    }

    fn2 = reinterpret_cast<DistFnV2>(GetProcAddress(h, "dist2land_gdal_distance_v2"));
    fn1 = reinterpret_cast<DistFnV1>(GetProcAddress(h, "dist2land_gdal_distance"));

    if (!fn2 && !fn1) {
      DWORD e = GetLastError();
      std::ostringstream oss;
      oss << "dist2land_gdal.dll missing symbol dist2land_gdal_distance[_v2]"
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

  int rc = 0;
  if (p.fn2) {
    rc = p.fn2(shp_u8.c_str(), prov_c, lat_deg, lon_deg,
               &geodesic_m, &land_lat, &land_lon, &in_land_i,
               err, (int)sizeof(err));
  } else {
    rc = p.fn1(shp_u8.c_str(), prov_c, lat_deg, lon_deg,
               &geodesic_m, &land_lat, &land_lon,
               err, (int)sizeof(err));
    // Best-effort fallback: treat distance==0 as in_land.
    in_land_i = (geodesic_m == 0.0) ? 1 : 0;
  }

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
