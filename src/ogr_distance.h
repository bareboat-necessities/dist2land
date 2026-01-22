#pragma once
#include "distance_iface.h"

DistanceResult distance_to_land_geodesic(double lat_deg, double lon_deg,
                                        const std::string& provider_id,
                                        const std::filesystem::path& shp_path);
