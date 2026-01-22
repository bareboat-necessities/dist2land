#include "util.h"
#include "providers.h"
#include "app_paths.h"
#include "http_download.h"
#include "archive_extract.h"
#include "ogr_distance.h"

#include <iostream>
#include <filesystem>
#include <stdexcept>

#include <cmath>
#ifndef isfinite
  using std::isfinite;
#endif

static void print_usage() {
  std::cout <<
R"(dist2land

Commands:
  dist2land providers
  dist2land setup --provider (osm|gshhg|ne|all)
  dist2land distance --lat <deg> --lon <deg> [--provider (auto|osm|gshhg|ne)] [--units (m|km|nm)]

Examples:
  dist2land setup --provider osm
  dist2land distance --lat 36.84 --lon -122.42 --provider auto
)";
}

static double convert_units(double meters, const std::string& units) {
  auto u = to_lower(units);
  if (u == "m") return meters;
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

static void cmd_distance(const ArgvView& av) {
  double lat = av.get_double("--lat", std::numeric_limits<double>::quiet_NaN());
  double lon = av.get_double("--lon", std::numeric_limits<double>::quiet_NaN());
  if (!std::isfinite(lat) || !std::isfinite(lon)) throw std::runtime_error("distance requires --lat and --lon");

  std::string prov = to_lower(av.get("--provider", "auto"));
  std::string units = av.get("--units", "m");

  Provider p;
  std::filesystem::path shp;

  if (prov == "auto") {
    auto best = best_available_provider_id();
    if (best.empty()) throw std::runtime_error("No providers installed. Run: dist2land setup --provider osm (or gshhg/ne)");
    p = provider_by_id(best);
  } else {
    p = provider_by_id(prov);
  }

  shp = provider_shapefile_path(p);
  auto r = distance_to_land_m(lat, lon, p.id, shp);

  double out = convert_units(r.meters, units);
  std::cout << out << " " << units << "\n";
  std::cerr << "provider=" << r.provider_id << " shp=" << r.shp_path.string() << "\n";
}

int main(int argc, char** argv) {
  try {
    ArgvView av(argc, argv);
    if (argc < 2) { print_usage(); return 2; }

    std::string cmd = to_lower(av.args[1]);
    if (cmd == "providers") { cmd_providers(); return 0; }
    if (cmd == "setup") { cmd_setup(av); return 0; }
    if (cmd == "distance") { cmd_distance(av); return 0; }

    print_usage();
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
