#pragma once
#include <filesystem>
#include <string>

struct DistanceResult {
  double geodesic_m = 0.0;
  double land_lat_deg = 0.0;
  double land_lon_deg = 0.0;

  std::string provider_id;
  std::filesystem::path shp_path;
};

// Cross-platform entry point used by main.cpp.
// - On Windows: loads dist2land_gdal.dll and calls into it.
// - Elsewhere: calls GDAL/OGR directly.
DistanceResult distance_query_geodesic(double lat_deg, double lon_deg,
                                      const std::string& provider_id,
                                      const std::filesystem::path& shp_path);
