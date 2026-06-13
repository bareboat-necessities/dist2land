#include "distance_iface.h"

#include <stdexcept>

DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path) {
  (void)lat_deg;
  (void)lon_deg;
  (void)provider_id;
  (void)shp_path;
  throw std::runtime_error("GDAL support is not available in this dist2land build");
}

bool distance_backend_selftest(std::string* out_error) {
  if (out_error) {
    *out_error = "GDAL support is not available in this dist2land build";
  }
  return false;
}
