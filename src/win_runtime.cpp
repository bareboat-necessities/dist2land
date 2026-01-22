#include "win_runtime.h"

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <string>
  #include <filesystem>

  static std::filesystem::path exe_dir_fs() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf, buf + n);
    return p.parent_path();
  }

  static void set_env_if_missing(const wchar_t* key, const std::wstring& value) {
    wchar_t tmp[2];
    DWORD n = GetEnvironmentVariableW(key, tmp, 2);
    if (n == 0) {
      SetEnvironmentVariableW(key, value.c_str());
    }
  }

  void win_prepare_runtime() {
    const auto dir = exe_dir_fs();

    // Help dependency resolution (esp. for LoadLibrary of the GDAL plugin + its deps)
    SetDllDirectoryW(dir.wstring().c_str());

    // Provide sane defaults for PROJ/GDAL data if user hasn't set them.
    set_env_if_missing(L"PROJ_LIB",  (dir / L"share" / L"proj").wstring());
    set_env_if_missing(L"GDAL_DATA", (dir / L"share" / L"gdal").wstring());

    // CA bundle for libcurl/OpenSSL (avoid "Problem with the SSL CA cert")
    const auto ca = dir / L"share" / L"certs" / L"ca-bundle.crt";
    if (std::filesystem::exists(ca)) {
      set_env_if_missing(L"CURL_CA_BUNDLE", ca.wstring());
      set_env_if_missing(L"SSL_CERT_FILE",  ca.wstring());
    }
  }

#else
  void win_prepare_runtime() {}
#endif
