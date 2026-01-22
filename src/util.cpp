#include "util.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return (char)std::tolower(c); });
  return s;
}

bool starts_with(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

ArgvView::ArgvView(int argc, char** argv) {
  args.reserve((size_t)argc);
  for (int i=0;i<argc;i++) args.emplace_back(argv[i]);
}

bool ArgvView::has(const std::string& key) const {
  for (size_t i=0;i<args.size();i++) if (args[i] == key) return true;
  return false;
}

std::string ArgvView::get(const std::string& key, const std::string& def) const {
  for (size_t i=0;i+1<args.size();i++) {
    if (args[i] == key) return args[i+1];
  }
  return def;
}

double ArgvView::get_double(const std::string& key, double def) const {
  auto s = get(key, "");
  if (s.empty()) return def;
  char* end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  if (!end || end == s.c_str() || *end != '\0') throw std::runtime_error("Bad number for " + key);
  return v;
}
