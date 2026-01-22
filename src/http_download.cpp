#include "http_download.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <filesystem>
#include <string>

#ifdef _WIN32
  #include <windows.h>
#endif

static size_t write_file(void* ptr, size_t size, size_t nmemb, void* stream) {
  return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
}

static void curl_global_init_once() {
  static bool inited = false;
  if (!inited) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    inited = true;
  }
}

static std::string u8path(const std::filesystem::path& p) {
#if defined(_WIN32)
  // C++20: u8string() returns std::u8string
  auto u8 = p.u8string();
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
  return p.string();
#endif
}

#ifdef _WIN32
static std::filesystem::path exe_dir() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::filesystem::path p(buf, buf + n);
  return p.parent_path();
}
#endif

static std::string find_ca_bundle_path() {
  // 1) Prefer explicit env vars if present
  if (const char* p = std::getenv("CURL_CA_BUNDLE")) {
    if (*p && std::filesystem::exists(p)) return std::string(p);
  }
  if (const char* p = std::getenv("SSL_CERT_FILE")) {
    if (*p && std::filesystem::exists(p)) return std::string(p);
  }

#ifdef _WIN32
  // 2) Fallback: packaged bundle next to exe
  const auto ca = exe_dir() / "share" / "certs" / "ca-bundle.crt";
  if (std::filesystem::exists(ca)) return u8path(ca);
#endif

  return {};
}

DownloadResult http_download_to(const std::string& url, const std::filesystem::path& out_file) {
  curl_global_init_once();

  std::filesystem::create_directories(out_file.parent_path());

  auto tmp = out_file;
  tmp += ".part";

  FILE* fp = std::fopen(tmp.string().c_str(), "wb");
  if (!fp) throw std::runtime_error("Failed to open for write: " + tmp.string());

  CURL* curl = curl_easy_init();
  if (!curl) {
    std::fclose(fp);
    throw std::runtime_error("curl_easy_init failed");
  }

  // Basic request
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "dist2land/1.0");

  // Timeouts (avoid CI hangs)
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 20000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 600000L); // 10 min

  // Prefer HTTP/2 when available; fallback ok
#ifdef CURL_HTTP_VERSION_2TLS
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
#endif

  // Ensure TLS verification stays ON; provide CA bundle where needed (Windows/OpenSSL)
  const std::string ca = find_ca_bundle_path();
  if (!ca.empty()) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
  }

  CURLcode res = curl_easy_perform(curl);

  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

  curl_easy_cleanup(curl);
  std::fclose(fp);

  if (res != CURLE_OK) {
    std::filesystem::remove(tmp);
    std::string msg = std::string("Download failed: ") + curl_easy_strerror(res);

#ifdef _WIN32
    // Common Windows runtime issue: missing CA bundle
    if (res == CURLE_PEER_FAILED_VERIFICATION || res == CURLE_SSL_CACERT || res == CURLE_SSL_CACERT_BADFILE) {
      msg += "\nTLS verification failed. On Windows you must ship a CA bundle and/or set CURL_CA_BUNDLE.\n"
             "Expected bundled path: <exe_dir>\\share\\certs\\ca-bundle.crt";
      if (!ca.empty()) msg += std::string("\nUsing CA bundle: ") + ca;
      else msg += "\nNo CA bundle found via CURL_CA_BUNDLE/SSL_CERT_FILE or packaged share/certs.";
    }
#endif

    throw std::runtime_error(msg);
  }

  if (code >= 400) {
    std::filesystem::remove(tmp);
    throw std::runtime_error("HTTP error code: " + std::to_string(code));
  }

  std::filesystem::rename(tmp, out_file);
  return DownloadResult{out_file, code};
}
