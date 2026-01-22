#pragma once
#include <string>
#include <vector>

std::string to_lower(std::string s);
bool starts_with(const std::string& s, const std::string& p);

struct ArgvView {
  std::vector<std::string> args;
  explicit ArgvView(int argc, char** argv);
  bool has(const std::string& key) const;
  std::string get(const std::string& key, const std::string& def = "") const;
  double get_double(const std::string& key, double def = 0.0) const;
};
