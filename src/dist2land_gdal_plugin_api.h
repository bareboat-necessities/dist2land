#pragma once
#include <cstddef>

struct Dist2LandQueryOut {
  double geodesic_m;
  double land_lat_deg;
  double land_lon_deg;
  int    in_land; // 0/1
};

static constexpr const char* kDist2LandGdalProcName = "dist2land_gdal_query_geodesic";
