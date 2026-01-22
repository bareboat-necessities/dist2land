#include "win_runtime.h"

#include <filesystem>
#include <string>

#ifdef _WIN32
  #include <windows.h>

static std::wstring exe_dir_w() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, buf + n);
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L".";
  return path.substr(0, pos);
}

static bool file_exists_w(const std::wstring& p) {
  DWORD attrs = GetFileAttributesW(p.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void set_env_if_missing(const wchar_t* key, const std::wstring& value) {
  wchar_t tmp[2];
  DWORD n = GetEnvironmentVariableW(key, tmp, 2);
  if (n == 0) SetEnvironmentVariableW(key, value.c_str());
}

static std::string utf8_from_wide(const std::wstring& ws) {
  if (ws.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), n, nullptr, nullptr);
  return out;
}

std::filesystem::path win_exe_dir() {
  return std::filesystem::path(exe_dir_w());
}

std::string win_last_error_utf8(unsigned long err) {
  wchar_t* msg = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageW(flags, nullptr, (DWORD)err, 0, (LPWSTR)&msg, 0, nullptr);
  std::wstring ws = (len && msg) ? std::wstring(msg, msg + len) : L"(no message)";
  if (msg) LocalFree(msg);

  // trim trailing newlines
  while (!ws.empty() && (ws.back() == L'\r' || ws.back() == L'\n')) ws.pop_back();
  return "Win32Error " + std::to_string(err) + ": " + utf8_from_wide(ws);
}

void win_prepare_runtime() {
  const std::wstring dir = exe_dir_w();

  // Robust DLL search: make “next to exe” reliable for LoadLibraryEx dependency resolution.
  // Prefer modern safe search APIs when available.
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  auto pSetDefaultDllDirectories =
      (BOOL(WINAPI*)(DWORD))GetProcAddress(k32, "SetDefaultDllDirectories");
  auto pAddDllDirectory =
      (DLL_DIRECTORY_COOKIE(WINAPI*)(PCWSTR))GetProcAddress(k32, "AddDllDirectory");

  if (pSetDefaultDllDirectories && pAddDllDirectory) {
    // Search: application dir + system + user-added dirs.
    pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    pAddDllDirectory(dir.c_str());
  } else {
    // Fallback (older Windows): affects dependency resolution order.
    SetDllDirectoryW(dir.c_str());
  }

  // PROJ/GDAL data defaults (bundle these directories in the zip)
  set_env_if_missing(L"PROJ_LIB",  dir + L"\\share\\proj");
  set_env_if_missing(L"GDAL_DATA", dir + L"\\share\\gdal");

  // Curl CA bundle default (bundle this file in the zip)
  const std::wstring ca = dir + L"\\share\\certs\\ca-bundle.crt";
  if (file_exists_w(ca)) {
    set_env_if_missing(L"CURL_CA_BUNDLE", ca);
    set_env_if_missing(L"SSL_CERT_FILE", ca); // helps some OpenSSL builds too
  }
}

#else

std::filesystem::path win_exe_dir() { return std::filesystem::current_path(); }
std::string win_last_error_utf8(unsigned long) { return {}; }
void win_prepare_runtime() {}

#endif
