#pragma once
#include <string>
#include <filesystem>

struct DownloadResult {
  std::filesystem::path file_path;
  long http_code = 0;
};

DownloadResult http_download_to(const std::string& url, const std::filesystem::path& out_file);
