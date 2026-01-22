#pragma once
#include <string>
#include <filesystem>

struct DistanceResult {
  double meters = 0.0;
  std::string provider_id;
  std::filesystem::path shp_path;
};

DistanceResult distance_to_land_m(double lat_deg, double lon_deg,
                                  const std::string& provider_id,
                                  const std::filesystem::path& shp_path);
