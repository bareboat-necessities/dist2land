#include "distance_iface.h"
#include "win_runtime.h"

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
#endif

#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef _WIN32

namespace {

using FnQuery = DistanceQueryResult (*)(
  double lat_deg,
  double lon_deg,
  const char* provider_id,
  const char* shp_path_utf8
);

std::once_flag g_once;
HMODULE g_mod = nullptr;
FnQuery g_fn = nullptr;
std::filesystem::path g_loaded_path;

static std::wstring w(const std::filesystem::path& p) { return p.wstring(); }

static void load_plugin_once() {
  const auto base = win_exe_dir();

  std::filesystem::path dll = base / "dist2land_gdal.dll";
  if (!std::filesystem::exists(dll)) dll = base / "libdist2land_gdal.dll";

  if (!std::filesystem::exists(dll)) {
    throw std::runtime_error(
      "dist2land_gdal plugin DLL not found next to executable. Expected:\n  " +
      (base / "dist2land_gdal.dll").string() + "\n(or libdist2land_gdal.dll)"
    );
  }

  // Avoid OS “This app can’t run” UI boxes on failure.
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

  // Critical: make Windows search dependencies relative to the DLL being loaded.
  g_mod = LoadLibraryExW(
    w(dll).c_str(),
    nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
  );

  if (!g_mod) {
    const DWORD e = GetLastError();
    throw std::runtime_error(
      "Failed to load dist2land_gdal.dll from: " + dll.string() + "\n" + win_last_error_utf8(e) +
      "\nMost common cause: a missing dependent DLL (transitive GDAL/PROJ/GEOS/etc)."
    );
  }

  // Adjust to your actual exported symbol name.
  FARPROC fp = GetProcAddress(g_mod, "dist2land_distance_query_geodesic_v1");
  if (!fp) {
    const DWORD e = GetLastError();
    FreeLibrary(g_mod);
    g_mod = nullptr;
    throw std::runtime_error(
      "Loaded plugin but missing symbol dist2land_distance_query_geodesic_v1\n" +
      win_last_error_utf8(e)
    );
  }

  g_fn = reinterpret_cast<FnQuery>(fp);
  g_loaded_path = dll;
}

} // namespace

DistanceQueryResult distance_query_geodesic(double lat_deg,
                                           double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  std::call_once(g_once, load_plugin_once);
  if (!g_fn) throw std::runtime_error("GDAL plugin not loaded (unexpected)");

  // Keep shp_path as UTF-8-ish narrow; if you need full Unicode, pass wide and convert in plugin.
  return g_fn(lat_deg, lon_deg, provider_id.c_str(), shp_path.string().c_str());
}

#else

// non-windows should not compile this file
DistanceQueryResult distance_query_geodesic(double, double, const std::string&, const std::filesystem::path&) {
  throw std::runtime_error("distance_call_win compiled on non-Windows");
}

#endif
