#ifdef _WIN32

#define NOMINMAX
#include "distance_iface.h"
#include <windows.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

static std::wstring format_win_error(DWORD code) {
  wchar_t* msg = nullptr;
  DWORD n = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPWSTR)&msg, 0, nullptr);
  std::wstring out = (n && msg) ? std::wstring(msg, msg + n) : L"(no message)";
  if (msg) LocalFree(msg);
  return out;
}

static std::filesystem::path exe_dir() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::filesystem::path p(std::wstring(buf, buf + n));
  return p.parent_path();
}

static std::string path_to_utf8(const std::filesystem::path& p) {
  std::wstring ws = p.wstring();
  if (ws.empty()) return std::string();

  int needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                                   nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return std::string();

  std::string out((size_t)needed, '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                      out.data(), needed, nullptr, nullptr);
  return out;
}

using FnPing = int (*)(char* errbuf, int errcap);

using FnDist = int (*)(const char* shp_path,
                       const char* provider_id,
                       double lat_deg, double lon_deg,
                       double* geodesic_m,
                       double* land_lat_deg,
                       double* land_lon_deg,
                       char* errbuf,
                       int errbuf_cap);

struct Backend {
  HMODULE mod = nullptr;
  FnPing ping = nullptr;
  FnDist dist = nullptr;
  std::filesystem::path dll_path;
};

static Backend& backend() {
  static Backend b;
  return b;
}

static void load_backend_or_throw() {
  Backend& b = backend();
  if (b.mod) return;

  b.dll_path = exe_dir() / "dist2land_gdal.dll";
  const std::wstring dll_w = b.dll_path.wstring();

  HMODULE mod = LoadLibraryExW(plugin_path.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
      LOAD_LIBRARY_SEARCH_USER_DIRS);
  if (!mod) {
    DWORD err = GetLastError();
    std::wstring msg = format_win_error(err);
    throw std::runtime_error(
        "Failed to load dist2land_gdal.dll from: " + b.dll_path.string() +
        " (LoadLibraryExW error " + std::to_string((int)err) + ": " +
        std::string(msg.begin(), msg.end()) + ")");
  }

  auto ping = (FnPing)GetProcAddress(mod, "dist2land_gdal_ping");
  auto dist = (FnDist)GetProcAddress(mod, "dist2land_gdal_distance");
  if (!dist) {
    FreeLibrary(mod);
    throw std::runtime_error("dist2land_gdal.dll missing export: dist2land_gdal_distance");
  }
  if (!ping) {
    // Optional, but useful for selftest; allow missing and just skip.
  }

  b.mod = mod;
  b.ping = ping;
  b.dist = dist;
}

bool distance_backend_selftest(std::string* out_error) {
  try {
    load_backend_or_throw();
    Backend& b = backend();
    if (b.ping) {
      char err[1024] = {0};
      int rc = b.ping(err, (int)sizeof(err));
      if (rc != 0) {
        if (out_error) *out_error = err[0] ? err : "backend ping failed";
        return false;
      }
    }
    return true;
  } catch (const std::exception& e) {
    if (out_error) *out_error = e.what();
    return false;
  }
}

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  load_backend_or_throw();
  Backend& b = backend();

  const std::string shp_u8 = path_to_utf8(shp_path);

  double geod = 0.0, land_lat = 0.0, land_lon = 0.0;
  char err[2048] = {0};

  int rc = b.dist(shp_u8.c_str(),
                  provider_id.c_str(),
                  lat_deg, lon_deg,
                  &geod, &land_lat, &land_lon,
                  err, (int)sizeof(err));

  if (rc != 0) {
    std::string msg = err[0] ? err : "Unknown error from GDAL backend";
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
