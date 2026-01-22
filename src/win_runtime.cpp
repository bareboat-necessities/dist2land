#include "win_runtime.h"

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <string>

  static std::wstring exe_dir_w() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf, buf + n);
    auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
  }

  static void set_env_if_missing(const wchar_t* key, const std::wstring& value) {
    wchar_t tmp[2];
    DWORD n = GetEnvironmentVariableW(key, tmp, 2);
    if (n == 0) {
      SetEnvironmentVariableW(key, value.c_str());
    }
  }

  void win_prepare_runtime() {
    const std::wstring dir = exe_dir_w();

    // Make "next to exe" the primary DLL resolution location.
    SetDllDirectoryW(dir.c_str());

    // Data directories (if user hasn't set them).
    set_env_if_missing(L"PROJ_LIB",  dir + L"\\share\\proj");
    set_env_if_missing(L"GDAL_DATA", dir + L"\\share\\gdal");

    // Curl/OpenSSL CA (if present in bundle)
    set_env_if_missing(L"SSL_CERT_FILE", dir + L"\\share\\certs\\ca-bundle.crt");
  }

#else
  void win_prepare_runtime() {}
#endif
