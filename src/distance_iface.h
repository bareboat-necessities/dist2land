#pragma once
#include <string>
#include <filesystem>

struct DistanceQueryResult {
  std::string provider_id;
  std::filesystem::path shp_path;

  // primary metric (meters)
  double geodesic_m = 0.0;

  // closest land point (or query point if in land)
  double land_lat_deg = 0.0;
  double land_lon_deg = 0.0;

  bool in_land = false;
};

// Cross-platform API.
DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path);

// “Does the backend load?” (Windows: loads plugin + calls ping; POSIX: always true)
bool distance_backend_selftest(std::string* out_error = nullptr);
