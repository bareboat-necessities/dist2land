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
    return (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
  }

  static void set_env_if_missing(const wchar_t* key, const std::wstring& value) {
    wchar_t tmp[2];
    DWORD n = GetEnvironmentVariableW(key, tmp, 2);
    if (n == 0) SetEnvironmentVariableW(key, value.c_str());
  }

  // Dynamically resolve these so we still run on older systems gracefully.
  using SetDefaultDllDirectoriesFn = BOOL (WINAPI*)(DWORD);
  using AddDllDirectoryFn         = DLL_DIRECTORY_COOKIE (WINAPI*)(PCWSTR);

  static void configure_strict_dll_search(const std::wstring& appdir) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto pSetDefault = (SetDefaultDllDirectoriesFn)GetProcAddress(k32, "SetDefaultDllDirectories");
    auto pAddDir     = (AddDllDirectoryFn)GetProcAddress(k32, "AddDllDirectory");

    if (pSetDefault && pAddDir) {
      // Removes PATH/current-dir from the default DLL search path for *subsequent* loads.
      pSetDefault(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);

      // Add our app directory as an explicit search directory.
      pAddDir(appdir.c_str());
    } else {
      // Fallback: older behavior. Still helps, but PATH can still matter in edge cases.
      SetDllDirectoryW(appdir.c_str());
    }
  }

  void win_prepare_runtime() {
    const std::wstring dir = exe_dir_w();

    configure_strict_dll_search(dir);

    // Data files
    set_env_if_missing(L"PROJ_LIB",  dir + L"\\share\\proj");
    set_env_if_missing(L"GDAL_DATA", dir + L"\\share\\gdal");

    // Curl CA bundle (ship it)
    set_env_if_missing(L"SSL_CERT_FILE", dir + L"\\share\\certs\\ca-bundle.crt");
  }

#else
  void win_prepare_runtime() {}
#endif
