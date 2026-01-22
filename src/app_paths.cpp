#include "app_paths.h"
#include <cstdlib>

static std::filesystem::path getenv_path(const char* k) {
  const char* v = std::getenv(k);
  return (v && *v) ? std::filesystem::path(v) : std::filesystem::path();
}

std::filesystem::path cache_root_dir() {
#ifdef _WIN32
  auto base = getenv_path("LOCALAPPDATA");
  if (base.empty()) base = std::filesystem::temp_directory_path();
  return base / "dist2land";
#elif __APPLE__
  auto home = getenv_path("HOME");
  if (home.empty()) home = std::filesystem::temp_directory_path();
  return home / "Library" / "Caches" / "dist2land";
#else
  auto xdg = getenv_path("XDG_CACHE_HOME");
  if (!xdg.empty()) return xdg / "dist2land";
  auto home = getenv_path("HOME");
  if (home.empty()) home = std::filesystem::temp_directory_path();
  return home / ".cache" / "dist2land";
#endif
}

std::filesystem::path provider_dir(const std::string& provider_id) {
  return cache_root_dir() / "providers" / provider_id;
}

std::filesystem::path downloads_dir() {
  return cache_root_dir() / "downloads";
}
