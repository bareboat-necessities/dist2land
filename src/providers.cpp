#include "providers.h"
#include "app_paths.h"
#include "util.h"

#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <limits.h>
#endif

// ------------------------- small helpers -------------------------

static std::string trim_copy(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

static bool starts_with(const std::string& s, const char* pfx) {
  return s.rfind(pfx, 0) == 0;
}

static std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream iss(s);
  while (std::getline(iss, cur, ',')) {
    cur = trim_copy(cur);
    if (!cur.empty()) out.push_back(cur);
  }
  return out;
}

static std::filesystem::path exe_dir() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::filesystem::path p(buf, buf + n);
  return p.parent_path();
#else
  // Linux: /proc/self/exe
  char buf[PATH_MAX];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    return std::filesystem::path(buf).parent_path();
  }
  return std::filesystem::current_path();
#endif
}

static std::vector<std::filesystem::path> provider_config_candidates() {
  std::vector<std::filesystem::path> cands;

  if (const char* env = std::getenv("DIST2LAND_PROVIDERS"); env && *env) {
    cands.emplace_back(env);
  }

  const auto dir = exe_dir();
  cands.push_back(dir / "providers.ini");
  cands.push_back(dir / "share" / "dist2land" / "providers.ini");

#ifndef _WIN32
  cands.emplace_back("/usr/local/share/dist2land/providers.ini");
  cands.emplace_back("/usr/share/dist2land/providers.ini");
#endif

  return cands;
}

static std::filesystem::path find_provider_config_or_throw() {
  for (const auto& p : provider_config_candidates()) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec) && !ec) return p;
  }

  std::ostringstream oss;
  oss << "Providers config not found.\n"
      << "Expected one of:\n";
  for (const auto& p : provider_config_candidates()) {
    oss << "  " << p.string() << "\n";
  }
  oss << "You should ship share/dist2land/providers.ini with the release, or set DIST2LAND_PROVIDERS.\n";
  throw std::runtime_error(oss.str());
}

static std::vector<Provider> parse_providers_ini(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f.is_open()) throw std::runtime_error("Failed to open providers config: " + path.string());

  std::vector<Provider> out;
  Provider cur{};
  bool in_section = false;

  auto push_cur = [&]() {
    if (!in_section) return;

    cur.id = to_lower(cur.id);

    if (cur.id.empty()) throw std::runtime_error("providers.ini: missing section id");
    if (cur.display_name.empty()) throw std::runtime_error("providers.ini: provider '" + cur.id + "' missing display_name");
    if (cur.url_zip.empty()) throw std::runtime_error("providers.ini: provider '" + cur.id + "' missing url_zip");
    if (cur.license_hint.empty()) throw std::runtime_error("providers.ini: provider '" + cur.id + "' missing license_hint");
    if (cur.shp_name_contains.empty() && cur.explicit_shp.empty()) {
      throw std::runtime_error("providers.ini: provider '" + cur.id + "' missing shp_name_contains (or explicit_shp)");
    }

    out.push_back(cur);
    cur = Provider{};
    in_section = false;
  };

  std::string line;
  int lineno = 0;
  while (std::getline(f, line)) {
    ++lineno;
    line = trim_copy(line);
    if (line.empty()) continue;
    if (starts_with(line, "#") || starts_with(line, ";")) continue;

    // [section]
    if (line.size() >= 3 && line.front() == '[' && line.back() == ']') {
      push_cur();
      cur = Provider{};
      cur.id = trim_copy(line.substr(1, line.size() - 2));
      in_section = true;
      continue;
    }

    // key=value
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      // ignore malformed lines (but keep it strict-ish with a useful error)
      throw std::runtime_error("providers.ini parse error at " + path.string() + ":" + std::to_string(lineno) +
                               " (expected key=value)");
    }

    if (!in_section) {
      throw std::runtime_error("providers.ini parse error at " + path.string() + ":" + std::to_string(lineno) +
                               " (key=value outside any [section])");
    }

    std::string key = to_lower(trim_copy(line.substr(0, eq)));
    std::string val = trim_copy(line.substr(eq + 1));

    if (key == "display_name") cur.display_name = val;
    else if (key == "url_zip") cur.url_zip = val;
    else if (key == "license_hint") cur.license_hint = val;
    else if (key == "explicit_shp") cur.explicit_shp = val;
    else if (key == "shp_name_contains") cur.shp_name_contains = split_csv(val);
    else {
      // ignore unknown keys (forward compatible)
    }
  }
  push_cur();

  if (out.empty()) {
    throw std::runtime_error("providers.ini: no providers found in " + path.string());
  }
  return out;
}

static const std::vector<Provider>& providers_cached() {
  static std::vector<Provider> cached = [] {
    auto cfg = find_provider_config_or_throw();
    return parse_providers_ini(cfg);
  }();
  return cached;
}

// ------------------------- existing API -------------------------

std::vector<Provider> all_providers() {
  // Return a copy (keeps your original signature).
  return providers_cached();
}

Provider provider_by_id(const std::string& id) {
  auto want = to_lower(id);
  if (want == "auto") return Provider{.id="auto"};
  for (auto& p : providers_cached()) {
    if (p.id == want) return p;
  }
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

    // If explicit_shp is set, match by exact filename.
    if (!p.explicit_shp.empty()) {
      if (to_lower(e.path().filename().string()) == to_lower(p.explicit_shp)) {
        out = e.path();
        return true;
      }
      continue;
    }

    // Otherwise match by substring patterns.
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
    throw std::runtime_error("Provider not installed or shapefile not found: " + p.id +
                             "\nRun: dist2land setup --provider " + p.id);
  }
  return shp;
}

std::string best_available_provider_id() {
  // Preference order is the order in the config file.
  for (const auto& p : providers_cached()) {
    if (provider_installed(p)) return p.id;
  }
  return "";
}
