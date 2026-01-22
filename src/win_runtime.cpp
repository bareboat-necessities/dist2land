#include "win_runtime.h"

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #define WIN32_LEAN_AND_MEAN
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

  static void set_env(const wchar_t* key, const std::wstring& value) {
    SetEnvironmentVariableW(key, value.c_str());
  }

  static void set_path_minimal(const std::wstring& exeDir) {
    wchar_t sysdir[MAX_PATH];
    wchar_t windir[MAX_PATH];
    sysdir[0] = 0; windir[0] = 0;
    GetSystemDirectoryW(sysdir, MAX_PATH);
    GetWindowsDirectoryW(windir, MAX_PATH);

    // Hard override PATH for *this process* to avoid mixing MSYS/user PATH DLL stacks.
    std::wstring p = exeDir;
    p += L";";
    p += sysdir;
    p += L";";
    p += windir;
    set_env(L"PATH", p);
  }

  void win_prepare_runtime() {
    const std::wstring dir = exe_dir_w();

    // 1) Strong DLL search hygiene (Win8+).
    // Use GetProcAddress so it still runs on older systems.
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto pSetDefaultDllDirectories =
      reinterpret_cast<decltype(&SetDefaultDllDirectories)>(
        GetProcAddress(k32, "SetDefaultDllDirectories"));
    auto pAddDllDirectory =
      reinterpret_cast<decltype(&AddDllDirectory)>(
        GetProcAddress(k32, "AddDllDirectory"));

    if (pSetDefaultDllDirectories && pAddDllDirectory) {
      // Default dirs: app dir + system32 + user dirs we add.
      pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
      pAddDllDirectory(dir.c_str());
    }

    // 2) Compatibility: also set the DLL directory for dependency resolution.
    SetDllDirectoryW(dir.c_str());

    // 3) Force process PATH to our packaged stack first.
    set_path_minimal(dir);

    // 4) Force runtime data paths to packaged copies (don’t “if missing”).
    set_env(L"PROJ_LIB",       dir + L"\\share\\proj");
    set_env(L"GDAL_DATA",      dir + L"\\share\\gdal");
    set_env(L"GDAL_DRIVER_PATH", L""); // don't auto-load external plugin drivers

    // Used by libcurl/OpenSSL
    set_env(L"CURL_CA_BUNDLE", dir + L"\\share\\certs\\ca-bundle.crt");
    set_env(L"SSL_CERT_FILE",  dir + L"\\share\\certs\\ca-bundle.crt");
  }

#else
  void win_prepare_runtime() {}
#endif
