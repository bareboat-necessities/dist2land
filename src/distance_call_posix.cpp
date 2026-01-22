#ifndef _WIN32

#include "distance_iface.h"
#include "ogr_distance.h"

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                            const std::string& provider_id,
                                            const std::filesystem::path& shp_path) {
  // Direct OGR implementation on non-Windows.
  DistanceResult r = distance_to_land_geodesic(lat_deg, lon_deg, provider_id, shp_path);

  DistanceQueryResult out;
  out.provider_id   = r.provider_id;
  out.shp_path      = r.shp_path;
  out.geodesic_m    = r.geodesic_m;
  out.land_lat_deg  = r.land_lat_deg;
  out.land_lon_deg  = r.land_lon_deg;

  // In-land is represented by distance==0 and land point == query point (per ogr_distance.cpp logic).
  out.in_land = (r.geodesic_m == 0.0);

  return out;
}

#endif
