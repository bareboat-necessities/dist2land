#ifdef _WIN32

#include "ogr_distance.h"
#include "dist2land_gdal_plugin_api.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

extern "C" __declspec(dllexport)
int dist2land_gdal_query_geodesic(double lat_deg,
                                  double lon_deg,
                                  const char* provider_id_utf8,
                                  const char* shp_path_utf8,
                                  Dist2LandQueryOut* out,
                                  char* errbuf,
                                  size_t errbuf_cap) {
  try {
    if (!out || !shp_path_utf8 || !*shp_path_utf8) {
      throw std::runtime_error("dist2land_gdal_query_geodesic: invalid arguments");
    }

    const std::string prov = provider_id_utf8 ? std::string(provider_id_utf8) : std::string();
    const std::filesystem::path shp(shp_path_utf8);

    const auto r = distance_query_geodesic_ogr(lat_deg, lon_deg, prov, shp);

    out->geodesic_m   = r.geodesic_m;
    out->land_lat_deg = r.land_lat_deg;
    out->land_lon_deg = r.land_lon_deg;
    out->in_land      = r.in_land ? 1 : 0;

    return 0; // success
  } catch (const std::exception& e) {
    if (errbuf && errbuf_cap > 0) {
      std::snprintf(errbuf, errbuf_cap, "%s", e.what());
      errbuf[errbuf_cap - 1] = '\0';
    }
    return 1;
  } catch (...) {
    if (errbuf && errbuf_cap > 0) {
      std::snprintf(errbuf, errbuf_cap, "Unknown exception in GDAL backend");
      errbuf[errbuf_cap - 1] = '\0';
    }
    return 2;
  }
}

#endif
