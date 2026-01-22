cmake_minimum_required(VERSION 3.20)
project(dist2land LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)

add_executable(dist2land
  src/main.cpp
  src/app_paths.cpp
  src/providers.cpp
  src/http_download.cpp
  src/archive_extract.cpp
  src/ogr_distance.cpp
  src/util.cpp
)

target_include_directories(dist2land PRIVATE
  src
  ${LibArchive_INCLUDE_DIRS}
)

target_link_libraries(dist2land PRIVATE
  PkgConfig::GDAL
  CURL::libcurl
  ${LibArchive_LIBRARIES}
)

if (MSVC)
  target_compile_definitions(dist2land PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
