#include "http_download.h"
#include <curl/curl.h>
#include <cstdio>
#include <stdexcept>
#include <filesystem>

static size_t write_file(void* ptr, size_t size, size_t nmemb, void* stream) {
  return std::fwrite(ptr, size, nmemb, (FILE*)stream);
}

DownloadResult http_download_to(const std::string& url, const std::filesystem::path& out_file) {
  std::filesystem::create_directories(out_file.parent_path());

  auto tmp = out_file;
  tmp += ".part";

  FILE* fp = std::fopen(tmp.string().c_str(), "wb");
  if (!fp) throw std::runtime_error("Failed to open for write: " + tmp.string());

  CURL* curl = curl_easy_init();
  if (!curl) { std::fclose(fp); throw std::runtime_error("curl_easy_init failed"); }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "dist2land/1.0");

  CURLcode res = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

  curl_easy_cleanup(curl);
  std::fclose(fp);

  if (res != CURLE_OK) {
    std::filesystem::remove(tmp);
    throw std::runtime_error(std::string("Download failed: ") + curl_easy_strerror(res));
  }
  if (code >= 400) {
    std::filesystem::remove(tmp);
    throw std::runtime_error("HTTP error code: " + std::to_string(code));
  }

  std::filesystem::rename(tmp, out_file);
  return DownloadResult{out_file, code};
}
