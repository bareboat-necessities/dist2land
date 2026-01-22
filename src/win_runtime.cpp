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

    // Ensure dependent DLLs next to the exe can be found reliably.
    // (Applies to later LoadLibrary calls and their dependency resolution.)
    SetDllDirectoryW(dir.c_str());

    // Provide sane defaults for PROJ/GDAL data if user hasn't set them.
    set_env_if_missing(L"PROJ_LIB",  dir + L"\\share\\proj");
    set_env_if_missing(L"GDAL_DATA", dir + L"\\share\\gdal");
  }

#else
  void win_prepare_runtime() {}
#endif
