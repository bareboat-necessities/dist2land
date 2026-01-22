#pragma once
#include <filesystem>
#include <string>

void win_prepare_runtime();
std::filesystem::path win_exe_dir();
std::string win_last_error_utf8(unsigned long err);
