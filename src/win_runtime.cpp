#include "win_runtime.h"

#ifdef _WIN32
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

  static std::wstring get_env_w(const wchar_t* key) {
    DWORD n = GetEnvironmentVariableW(key, nullptr, 0);
    if (n == 0) return L"";
    std::wstring out;
    out.resize(n);
    DWORD m = GetEnvironmentVariableW(key, out.data(), n);
    if (m == 0) return L"";
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
  }

  static bool looks_posix_path(const std::wstring& v) {
    if (v.empty()) return false;
    // MSYS/Cygwin paths (/..., /c/..., /cygdrive/c/...)
    if (v[0] == L'/') return true;
    if (v.find(L"/cygdrive/") != std::wstring::npos) return true;
    return false;
  }

  static bool file_exists_w(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return (a != INVALID_FILE_ATTRIBUTES) && ((a & FILE_ATTRIBUTE_DIRECTORY) == 0);
  }

  static void set_env_if_missing_or_posix(const wchar_t* key, const std::wstring& value) {
    const std::wstring cur = get_env_w(key);
    if (cur.empty() || looks_posix_path(cur)) {
      SetEnvironmentVariableW(key, value.c_str());
    }
  }

  void win_prepare_runtime() {
    const std::wstring dir = exe_dir_w();

    // Ensure dependent DLLs next to the exe can be found.
    SetDllDirectoryW(dir.c_str());

    // Bundle paths (must be native Windows paths).
    const std::wstring proj_dir = dir + L"\\share\\proj";
    const std::wstring gdal_dir = dir + L"\\share\\gdal";
    const std::wstring ca_file  = dir + L"\\share\\certs\\ca-bundle.crt";

    // PROJ prefers PROJ_DATA. Keep PROJ_LIB too for compatibility.
    set_env_if_missing_or_posix(L"PROJ_DATA", proj_dir);
    set_env_if_missing_or_posix(L"PROJ_LIB",  proj_dir);
    set_env_if_missing_or_posix(L"GDAL_DATA", gdal_dir);

    // Curl/OpenSSL CA bundle (optional but helps portable builds).
    if (file_exists_w(ca_file)) {
      set_env_if_missing_or_posix(L"CURL_CA_BUNDLE", ca_file);
      set_env_if_missing_or_posix(L"SSL_CERT_FILE", ca_file);
    }
  }

#else
  void win_prepare_runtime() {}
#endif
