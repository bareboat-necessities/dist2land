#include "distance_iface.h"
#include "ogr_distance.h"

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  return distance_query_geodesic_ogr(lat_deg, lon_deg, provider_id, shp_path);
}

bool distance_backend_selftest(std::string* out_error) {
  (void)out_error;
  return true; // POSIX links GDAL directly; no plugin load step.
}
