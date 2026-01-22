#pragma once
#include "distance_iface.h"
#include <filesystem>
#include <string>

// Internal OGR-backed implementation (compiled into plugin on Windows,
// compiled directly into the exe on POSIX via distance_call_posix.cpp).
DistanceQueryResult distance_query_geodesic_ogr(double lat_deg, double lon_deg,
                                               const std::string& provider_id,
                                               const std::filesystem::path& shp_path);
