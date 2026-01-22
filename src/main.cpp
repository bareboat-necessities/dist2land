#include "util.h"
#include "providers.h"
#include "app_paths.h"
#include "http_download.h"
#include "archive_extract.h"
#include "distance_iface.h"
#include "win_runtime.h"

#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <limits>
#include <array>
#include <algorithm>

static void print_usage() {
  std::cout <<
R"(dist2land

Commands:
  dist2land help
  dist2land providers
  dist2land setup --provider (osm|gshhg|ne|all)
  dist2land distance --lat <deg> --lon <deg>
                    [--provider (auto|osm|gshhg|ne)]
                    [--units (m|km|nm)]
                    [--metric (geodesic|chord|rhumb)]

Examples:
  dist2land setup --provider osm
  dist2land distance --lat 36.84 --lon -122.42 --provider auto
  dist2land distance --lat 0 --lon -30 --metric rhumb --units nm

Output:
  <distance> <units> <land_lat_deg> <land_lon_deg>

Notes:
  - First run: you must download a dataset:
      dist2land setup --provider osm
  - If your point is on land (inside polygon), distance is 0 and the reported land point
    is the query point itself.

Performance (optional spatial index for faster queries):
  dist2land uses OGR spatial filters; performance improves a lot if your shapefile has a .qix index.
  Create it once per provider shapefile using GDAL's ogrinfo tool (produces a *.qix next to the *.shp).

  1) Find the layer name (usually the shapefile base name):
     ogrinfo -ro -so "<path-to-shapefile>.shp"

  2) Create the spatial index:
     ogrinfo -ro "<path-to-shapefile>.shp" -sql "CREATE SPATIAL INDEX ON <layer_name>"

  Example (layer name matches file base name):
     ogrinfo -ro -so "land_polygons.shp"
     ogrinfo -ro "land_polygons.shp" -sql "CREATE SPATIAL INDEX ON land_polygons"

  You can locate the provider shapefile path by running:
     dist2land distance --lat 0 --lon 0 --provider osm
  (it prints shp=... on stderr).
)";
}

static double convert_units(double meters, const std::string& units) {
  auto u = to_lower(units);
  if (u == "m")  return meters;
  if (u == "km") return meters / 1000.0;
  if (u == "nm") return meters / 1852.0;
  throw std::runtime_error("Unknown units: " + units);
}

static void cmd_providers() {
  std::cout << "Providers:\n";
  for (auto& p : all_providers()) {
    bool ok = provider_installed(p);
    std::cout << "  " << p.id << "  [" << (ok ? "installed" : "not installed") << "]  "
              << p.display_name << "\n";
  }
}

static void setup_one(const Provider& p) {
  auto pdir = provider_dir(p.id);
  auto ddir = downloads_dir();
  std::filesystem::create_directories(pdir);
  std::filesystem::create_directories(ddir);

  auto zip_path = ddir / (p.id + ".zip");
  std::cout << "Downloading " << p.id << "...\n";
  http_download_to(p.url_zip, zip_path);

  auto out_root = provider_extract_root(p);
  std::cout << "Extracting to " << out_root.string() << "...\n";
  std::filesystem::remove_all(out_root);
  extract_zip(zip_path, out_root);

  // quick validation: locate the shapefile
  auto shp = provider_shapefile_path(p);
  std::cout << "OK: found shapefile: " << shp.string() << "\n";
  std::cout << "License note: " << p.license_hint << "\n";
}

static void cmd_setup(const ArgvView& av) {
  auto prov = to_lower(av.get("--provider", ""));
  if (prov.empty()) throw std::runtime_error("setup requires --provider");

  if (prov == "all") {
    for (auto& p : all_providers()) setup_one(p);
    return;
  }
  setup_one(provider_by_id(prov));
}

static constexpr double kPi = 3.141592653589793238462643383279502884;
static double deg2rad(double d) { return d * (kPi / 180.0); }

static double wrap_pi(double x) {
  while (x >  kPi) x -= 2.0 * kPi;
  while (x < -kPi) x += 2.0 * kPi;
  return x;
}

static double chord_distance_wgs84_m(double lat1_deg, double lon1_deg,
                                    double lat2_deg, double lon2_deg) {
  // WGS84 ellipsoid
  constexpr double a = 6378137.0;
  constexpr double f = 1.0 / 298.257223563;
  constexpr double e2 = f * (2.0 - f);

  auto ecef = [&](double lat_deg, double lon_deg) {
    const double lat = deg2rad(lat_deg);
    const double lon = deg2rad(lon_deg);
    const double sl = std::sin(lat), cl = std::cos(lat);
    const double so = std::sin(lon), co = std::cos(lon);
    const double N = a / std::sqrt(1.0 - e2 * sl * sl);
    const double x = (N) * cl * co;
    const double y = (N) * cl * so;
    const double z = (N * (1.0 - e2)) * sl;
    return std::array<double, 3>{x, y, z};
  };

  auto p1 = ecef(lat1_deg, lon1_deg);
  auto p2 = ecef(lat2_deg, lon2_deg);
  const double dx = p2[0] - p1[0];
  const double dy = p2[1] - p1[1];
  const double dz = p2[2] - p1[2];
  return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static double rhumb_distance_sphere_m(double lat1_deg, double lon1_deg,
                                     double lat2_deg, double lon2_deg) {
  // Spherical rhumb-line approximation
  constexpr double R = 6371008.8; // mean Earth radius

  const double phi1 = deg2rad(lat1_deg);
  const double phi2 = deg2rad(lat2_deg);
  const double dphi = phi2 - phi1;

  const double lam1 = deg2rad(lon1_deg);
  const double lam2 = deg2rad(lon2_deg);
  const double dlam = wrap_pi(lam2 - lam1);

  const auto merc = [](double phi) {
    const double eps = 1e-12;
    const double p = std::max(std::min(phi,  kPi/2 - eps), -kPi/2 + eps);
    return std::log(std::tan(kPi/4 + p/2));
  };

  const double dpsi = merc(phi2) - merc(phi1);
  const double q = (std::abs(dpsi) > 1e-12) ? (dphi / dpsi) : std::cos(phi1);

  return std::sqrt(dphi*dphi + (q*dlam)*(q*dlam)) * R;
}

static void cmd_distance(const ArgvView& av) {
  const double lat = av.get_double("--lat", std::numeric_limits<double>::quiet_NaN());
  const double lon = av.get_double("--lon", std::numeric_limits<double>::quiet_NaN());
  if (!std::isfinite(lat) || !std::isfinite(lon)) {
    throw std::runtime_error("distance requires --lat and --lon");
  }

  const std::string prov   = to_lower(av.get("--provider", "auto"));
  const std::string units  = av.get("--units", "m");
  const std::string metric = to_lower(av.get("--metric", "geodesic"));

  Provider p;
  if (prov == "auto") {
    auto best = best_available_provider_id();
    if (best.empty()) {
      throw std::runtime_error("No providers installed. Run: dist2land setup --provider osm (or gshhg/ne)");
    }
    p = provider_by_id(best);
  } else {
    p = provider_by_id(prov);
  }

  const auto shp = provider_shapefile_path(p);

  // Find nearest land point by geodesic (AEQD) and return its coordinates.
  auto r = distance_query_geodesic(lat, lon, p.id, shp);

  double d_m = 0.0;
  if (metric == "geodesic") {
    d_m = r.geodesic_m;
  } else if (metric == "chord") {
    d_m = chord_distance_wgs84_m(lat, lon, r.land_lat_deg, r.land_lon_deg);
  } else if (metric == "rhumb") {
    d_m = rhumb_distance_sphere_m(lat, lon, r.land_lat_deg, r.land_lon_deg);
  } else {
    throw std::runtime_error("Unknown --metric: " + metric + " (use geodesic|chord|rhumb)");
  }

  const double out = convert_units(d_m, units);

  // Output: <distance> <units> <land_lat_deg> <land_lon_deg>
  std::cout.setf(std::ios::fixed);
  std::cout << std::setprecision(3) << out << " " << to_lower(units) << " "
            << std::setprecision(8) << r.land_lat_deg << " "
            << std::setprecision(8) << r.land_lon_deg
            << "\n";

  // Debug/trace to stderr
  std::cerr << "provider=" << r.provider_id
            << " metric=" << metric
            << " shp=" << r.shp_path.string()
            << " geodesic_m=" << r.geodesic_m
            << "\n";
}

int main(int argc, char** argv) {
  win_prepare_runtime();
  try {
    if (!provider_installed(p)) {
      throw std::runtime_error("Provider '" + p.id + "' not installed. Run: dist2land setup --provider " + p.id);
    }
    ArgvView av(argc, argv);
    if (argc < 2) { print_usage(); return 2; }

    const std::string cmd = to_lower(av.args[1]);
    if (cmd == "help" || cmd == "-h" || cmd == "--help") { print_usage(); return 0; }
    if (cmd == "providers") { cmd_providers(); return 0; }
    if (cmd == "setup")     { cmd_setup(av);   return 0; }
    if (cmd == "distance")  { cmd_distance(av); return 0; }

    print_usage();
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
