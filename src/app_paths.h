#pragma once
#include <filesystem>

std::filesystem::path cache_root_dir();
std::filesystem::path provider_dir(const std::string& provider_id);
std::filesystem::path downloads_dir();
