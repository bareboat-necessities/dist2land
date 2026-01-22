#include "distance_iface.h"
#include "ogr_distance.h"

#ifndef _WIN32
DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  return distance_query_geodesic_ogr(lat_deg, lon_deg, provider_id, shp_path);
}
#endif
