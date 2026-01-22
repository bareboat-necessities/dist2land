cmake_minimum_required(VERSION 3.20)
project(dist2land LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(GDAL REQUIRED)

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
  ${GDAL_INCLUDE_DIR}
  ${LibArchive_INCLUDE_DIRS}
  src
)

target_link_libraries(dist2land PRIVATE
  ${GDAL_LIBRARY}
  CURL::libcurl
  ${LibArchive_LIBRARIES}
)

if (MSVC)
  target_compile_definitions(dist2land PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
