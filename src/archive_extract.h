#pragma once
#include <filesystem>

void extract_zip(const std::filesystem::path& zip_file, const std::filesystem::path& out_dir);
