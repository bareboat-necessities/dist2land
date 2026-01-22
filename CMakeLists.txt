cmake_minimum_required(VERSION 3.20)
project(dist2land LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(PkgConfig REQUIRED)

add_executable(dist2land
  src/main.cpp
  src/app_paths.cpp
  src/providers.cpp
  src/http_download.cpp
  src/archive_extract.cpp
  src/util.cpp
  src/win_runtime.cpp
)

target_include_directories(dist2land PRIVATE
  src
  ${LibArchive_INCLUDE_DIRS}
)

target_link_libraries(dist2land PRIVATE
  CURL::libcurl
  ${LibArchive_LIBRARIES}
)

if (MSVC)
  target_compile_definitions(dist2land PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- providers.ini in build tree for CI/dev runs ---
add_custom_command(TARGET dist2land POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:dist2land>/share/dist2land"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/share/dist2land/providers.ini"
          "$<TARGET_FILE_DIR:dist2land>/share/dist2land/providers.ini"
)

# --- GDAL linkage / backend selection ---
if (WIN32)
  # Windows: executable does NOT link GDAL; it loads a plugin DLL.
  target_sources(dist2land PRIVATE
    src/distance_call_win.cpp
  )

  pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)

  add_library(dist2land_gdal SHARED
    src/dist2land_gdal_plugin.cpp
    src/ogr_distance.cpp
  )
  target_include_directories(dist2land_gdal PRIVATE src)
  target_link_libraries(dist2land_gdal PRIVATE PkgConfig::GDAL)

  # Ensure the DLL name is exactly dist2land_gdal.dll (no "lib" prefix)
  set_target_properties(dist2land_gdal PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dist2land_gdal"
  )

else()
  # Non-Windows: link GDAL directly into the executable.
  target_sources(dist2land PRIVATE
    src/distance_call_posix.cpp
    src/ogr_distance.cpp
  )
  pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)
  target_link_libraries(dist2land PRIVATE PkgConfig::GDAL)
endif()
