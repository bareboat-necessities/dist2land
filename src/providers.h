#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct Provider {
  std::string id;            // "osm", "gshhg", "ne"
  std::string display_name;
  std::string url_zip;       // download ZIP
  std::string license_hint;  // user-facing note
  // shapefile selection rules:
  // - if explicit_shp is non-empty, open that path (relative to extracted root).
  // - else scan extracted tree for first *.shp whose filename matches any pattern.
  std::string explicit_shp;
  std::vector<std::string> shp_name_contains; // e.g. {"GSHHS_f_L1"} or {"land_polygons"}
};

std::vector<Provider> all_providers();
Provider provider_by_id(const std::string& id);
std::string best_available_provider_id(); // auto selection based on installed data

bool provider_installed(const Provider& p);
std::filesystem::path provider_extract_root(const Provider& p);
std::filesystem::path provider_shapefile_path(const Provider& p);
