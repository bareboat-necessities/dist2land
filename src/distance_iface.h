#pragma once
#include <string>
#include <filesystem>

struct DistanceQueryResult {
  std::string provider_id;
  std::filesystem::path shp_path;

  // "Geodesic" distance in meters (your primary query metric; other metrics are computed in main)
  double geodesic_m = 0.0;

  // Closest point on land polygon boundary (or query point if already in-land)
  double land_lat_deg = 0.0;
  double land_lon_deg = 0.0;

  // True if the query point is inside the land polygon dataset
  bool in_land = false;
};

// Cross-platform API: implemented by distance_call_posix.cpp (direct OGR)
// and distance_call_win.cpp (loads dist2land_gdal.dll plugin).
DistanceQueryResult distance_query_geodesic(double lat_deg, double lon_deg,
                                           const std::string& provider_id,
                                           const std::filesystem::path& shp_path);
