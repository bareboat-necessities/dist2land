#ifdef _WIN32

#include "distance_iface.h"

#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using FnDistance = int(*)(const char* shp_path,
                          const char* provider_id,
                          double lat_deg, double lon_deg,
                          double* geodesic_m,
                          double* land_lat_deg,
                          double* land_lon_deg,
                          char* errbuf,
                          int errbuf_cap);

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L".";
  return path.substr(0, pos);
}

static std::string narrow_utf8(const std::wstring& w) {
  if (w.empty()) return {};
  int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string out(sz, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), sz, nullptr, nullptr);
  return out;
}

DistanceResult distance_query_geodesic(double lat_deg, double lon_deg,
                                      const std::string& provider_id,
                                      const std::filesystem::path& shp_path) {
  const std::filesystem::path dllPath = std::filesystem::path(exe_dir_w()) / L"dist2land_gdal.dll";

  HMODULE h = LoadLibraryW(dllPath.wstring().c_str());
  if (!h) {
    throw std::runtime_error("Failed to load dist2land_gdal.dll from: " + dllPath.string());
  }

  auto fn = (FnDistance)GetProcAddress(h, "dist2land_gdal_distance");
  if (!fn) {
    FreeLibrary(h);
    throw std::runtime_error("dist2land_gdal.dll missing export dist2land_gdal_distance()");
  }

  double geodesic_m = 0.0, land_lat = 0.0, land_lon = 0.0;
  char err[1024]; err[0] = '\0';

  const int rc = fn(shp_path.string().c_str(),
                    provider_id.c_str(),
                    lat_deg, lon_deg,
                    &geodesic_m, &land_lat, &land_lon,
                    err, (int)sizeof(err));

  FreeLibrary(h);

  if (rc != 0) {
    std::string msg = err[0] ? std::string(err) : std::string("Unknown GDAL backend error");
    throw std::runtime_error(msg);
  }

  DistanceResult out;
  out.geodesic_m = geodesic_m;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.provider_id = provider_id;
  out.shp_path = shp_path;
  return out;
}

#endif
