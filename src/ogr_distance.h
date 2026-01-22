#pragma once
#include "distance_iface.h"
#include <filesystem>
#include <string>

// Direct GDAL/OGR implementation used on POSIX and inside the Windows plugin.
DistanceQueryResult distance_query_geodesic_ogr(double lat_deg, double lon_deg,
                                               const std::string& provider_id,
                                               const std::filesystem::path& shp_path);
