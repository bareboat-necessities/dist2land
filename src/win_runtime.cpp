#include "win_runtime.h"

#ifdef _WIN32
  #include <windows.h>
  #include <string>

  static std::wstring exe_dir_w() {
    std::wstring buf;
    buf.resize(32768);
    DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
    if (n == 0 || n >= buf.size()) return L".";
    buf.resize(n);
    auto pos = buf.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return buf.substr(0, pos);
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

    // Ensure dependent DLLs next to the exe (and next to the plugin) are found.
    SetDllDirectoryW(dir.c_str());

    // Provide defaults for PROJ/GDAL data if user hasn't set them.
    set_env_if_missing(L"PROJ_LIB",  dir + L"\\share\\proj");
    set_env_if_missing(L"GDAL_DATA", dir + L"\\share\\gdal");

    // libcurl certificate bundle (for `setup` downloads)
    // Ship: dist/share/certs/ca-bundle.crt
    const std::wstring cabundle = dir + L"\\share\\certs\\ca-bundle.crt";
    set_env_if_missing(L"CURL_CA_BUNDLE", cabundle);
    set_env_if_missing(L"SSL_CERT_FILE", cabundle);
  }

#else
  void win_prepare_runtime() {}
#endif
