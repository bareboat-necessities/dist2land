#ifndef _WIN32

#include "distance_iface.h"
#include "ogr_distance.h"

DistanceResult distance_query_geodesic(double lat_deg, double lon_deg,
                                      const std::string& provider_id,
                                      const std::filesystem::path& shp_path) {
  return distance_to_land_geodesic(lat_deg, lon_deg, provider_id, shp_path);
}

#endif
