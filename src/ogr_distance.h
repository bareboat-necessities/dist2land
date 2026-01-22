#pragma once
#include <string>
#include <filesystem>

struct DistanceResult {
  // Always computed as surface/geodesic distance to the nearest land point found
  // (via local AEQD projection centered at query point)
  double geodesic_m = 0.0;

  // Coordinates of the nearest land point (WGS84 degrees)
  double land_lat_deg = 0.0;
  double land_lon_deg = 0.0;

  std::string provider_id;
  std::filesystem::path shp_path;
};

// Finds nearest land point (by geodesic) and returns geodesic distance + land point lat/lon.
DistanceResult distance_to_land_geodesic(double lat_deg, double lon_deg,
                                         const std::string& provider_id,
                                         const std::filesystem::path& shp_path);
