#ifdef _WIN32

#include "ogr_distance.h"
#include <cstring>
#include <exception>
#include <cstdio>

extern "C" __declspec(dllexport)
int dist2land_gdal_distance_v2(const char* shp_path,
                              const char* provider_id,
                              double lat_deg, double lon_deg,
                              double* geodesic_m,
                              double* land_lat_deg,
                              double* land_lon_deg,
                              int* in_land,
                              char* errbuf,
                              int errbuf_cap) {
  try {
    if (!shp_path || !geodesic_m || !land_lat_deg || !land_lon_deg) {
      throw std::runtime_error("dist2land_gdal_distance_v2: invalid arguments");
    }

    const std::string prov = provider_id ? provider_id : "";
    auto r = distance_to_land_geodesic(lat_deg, lon_deg, prov, std::filesystem::path(shp_path));

    *geodesic_m   = r.geodesic_m;
    *land_lat_deg = r.land_lat_deg;
    *land_lon_deg = r.land_lon_deg;
    if (in_land) *in_land = r.in_land ? 1 : 0;
    return 0;
  } catch (const std::exception& e) {
    if (errbuf && errbuf_cap > 0) {
      std::snprintf(errbuf, (size_t)errbuf_cap, "%s", e.what());
      errbuf[errbuf_cap - 1] = '\0';
    }
    return 1;
  } catch (...) {
    if (errbuf && errbuf_cap > 0) {
      std::snprintf(errbuf, (size_t)errbuf_cap, "Unknown exception in GDAL backend");
      errbuf[errbuf_cap - 1] = '\0';
    }
    return 2;
  }
}

// Back-compat symbol (no in_land). Calls v2 with a dummy.
extern "C" __declspec(dllexport)
int dist2land_gdal_distance(const char* shp_path,
                            const char* provider_id,
                            double lat_deg, double lon_deg,
                            double* geodesic_m,
                            double* land_lat_deg,
                            double* land_lon_deg,
                            char* errbuf,
                            int errbuf_cap) {
  int dummy_in_land = 0;
  return dist2land_gdal_distance_v2(shp_path, provider_id, lat_deg, lon_deg,
                                   geodesic_m, land_lat_deg, land_lon_deg,
                                   &dummy_in_land, errbuf, errbuf_cap);
}

#endif
