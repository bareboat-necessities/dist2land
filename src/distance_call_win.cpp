#include "distance_iface.h"

#ifdef _WIN32

#include "dist2land_gdal_plugin_api.h"

#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L".";
  return path.substr(0, pos);
}

static std::string winerr_last_error(DWORD e) {
  LPSTR msg = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD n = FormatMessageA(flags, nullptr, e, 0, (LPSTR)&msg, 0, nullptr);
  std::string s = (n && msg) ? std::string(msg, msg + n) : std::string("unknown error");
  if (msg) LocalFree(msg);
  return s;
}

using query_fn_t = int (*)(double lat_deg,
                           double lon_deg,
                           const char* provider_id_utf8,
                           const char* shp_path_utf8,
                           Dist2LandQueryOut* out,
                           char* err_buf,
                           size_t err_cap);

static HMODULE g_mod = nullptr;
static query_fn_t g_query = nullptr;

static void ensure_loaded() {
  if (g_query) return;

  const std::wstring dll_path = exe_dir_w() + L"\\dist2land_gdal.dll";

  g_mod = LoadLibraryW(dll_path.c_str());
  if (!g_mod) {
    DWORD e = GetLastError();
    throw std::runtime_error(
      "Failed to load dist2land_gdal.dll from: " + std::filesystem::path(dll_path).string() +
      " (Win32 error " + std::to_string(e) + ": " + winerr_last_error(e) + "). "
      "This almost always means a dependent DLL is missing. Run: ldd dist2land_gdal.dll"
    );
  }

  FARPROC p = GetProcAddress(g_mod, kDist2LandGdalProcName);
  if (!p) {
    throw std::runtime_error(std::string("dist2land_gdal.dll missing export: ") + kDist2LandGdalProcName);
  }

  g_query = reinterpret_cast<query_fn_t>(p);
}

} // namespace

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  ensure_loaded();

  Dist2LandQueryOut out{};
  char err[1024];
  err[0] = 0;

  const std::string shp_u8 = shp_path.u8string();

  const int rc = g_query(lat_deg, lon_deg,
                        provider_id.c_str(),
                        shp_u8.c_str(),
                        &out,
                        err, sizeof(err));

  if (rc != 0) {
    std::string msg = err[0] ? std::string(err) : std::string("GDAL plugin call failed");
    throw std::runtime_error(msg);
  }

  DistanceQueryResult r;
  r.provider_id = provider_id;
  r.shp_path = shp_path;
  r.geodesic_m = out.geodesic_m;
  r.land_lat_deg = out.land_lat_deg;
  r.land_lon_deg = out.land_lon_deg;
  r.in_land = (out.in_land != 0);
  return r;
}

#endif // _WIN32
