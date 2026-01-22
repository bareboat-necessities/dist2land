#include "providers.h"
#include "app_paths.h"
#include "util.h"
#include <stdexcept>
#include <filesystem>

std::vector<Provider> all_providers() {
  // OSM land polygons: page provides WGS84 split variant; ODbL. :contentReference[oaicite:6]{index=6}
  // GSHHG shapefiles: NOAA index provides gshhg-shp-2.3.7.zip; L1 is land/ocean boundary. :contentReference[oaicite:7]{index=7}
  // Natural Earth land: public domain; common download endpoint used widely. :contentReference[oaicite:8]{index=8}
  return {
    Provider{
      .id="osm",
      .display_name="OSM Land Polygons (WGS84 split, high detail)",
      .url_zip="https://osmdata.openstreetmap.de/download/land-polygons-split-4326.zip",
      .license_hint="ODbL — attribute “© OpenStreetMap contributors”.",
      .explicit_shp="",
      .shp_name_contains={"land_polygons"}
    },
    Provider{
      .id="gshhg",
      .display_name="NOAA GSHHG Shapefiles (full resolution, L1 land/ocean)",
      .url_zip="https://www.ngdc.noaa.gov/mgg/shorelines/data/gshhg/latest/gshhg-shp-2.3.7.zip",
      .license_hint="LGPL (GSHHG).",
      .explicit_shp="",
      .shp_name_contains={"GSHHS_f_L1"} // inside extracted tree
    },
    Provider{
      .id="ne",
      .display_name="Natural Earth 10m Land (coarse, tiny)",
      .url_zip="http://www.naturalearthdata.com/download/10m/physical/ne_10m_land.zip",
      .license_hint="Public domain (Natural Earth).",
      .explicit_shp="",
      .shp_name_contains={"ne_10m_land"}
    }
  };
}

Provider provider_by_id(const std::string& id) {
  auto want = to_lower(id);
  for (auto& p : all_providers()) if (p.id == want) return p;
  if (want == "auto") return Provider{.id="auto"};
  throw std::runtime_error("Unknown provider: " + id);
}

std::filesystem::path provider_extract_root(const Provider& p) {
  return provider_dir(p.id) / "extracted";
}

static bool any_shp_matches(const std::filesystem::path& root, const Provider& p, std::filesystem::path& out) {
  if (!std::filesystem::exists(root)) return false;
  for (auto const& e : std::filesystem::recursive_directory_iterator(root)) {
    if (!e.is_regular_file()) continue;
    if (to_lower(e.path().extension().string()) != ".shp") continue;
    auto fname = to_lower(e.path().filename().string());
    bool ok = false;
    for (auto& pat : p.shp_name_contains) {
      if (fname.find(to_lower(pat)) != std::string::npos) { ok = true; break; }
    }
    if (ok) { out = e.path(); return true; }
  }
  return false;
}

bool provider_installed(const Provider& p) {
  if (p.id == "auto") return false;
  std::filesystem::path shp;
  return any_shp_matches(provider_extract_root(p), p, shp);
}

std::filesystem::path provider_shapefile_path(const Provider& p) {
  if (p.id == "auto") throw std::runtime_error("auto has no direct shapefile");
  auto root = provider_extract_root(p);
  std::filesystem::path shp;
  if (!any_shp_matches(root, p, shp)) {
    throw std::runtime_error("Provider not installed or shapefile not found: " + p.id);
  }
  return shp;
}

std::string best_available_provider_id() {
  // Prefer most detailed if installed:
  for (auto id : {"osm","gshhg","ne"}) {
    auto p = provider_by_id(id);
    if (provider_installed(p)) return p.id;
  }
  return "";
}
