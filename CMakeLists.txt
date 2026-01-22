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
  src/ogr_distance.cpp
  src/util.cpp
)

target_include_directories(dist2land PRIVATE
  src
  ${LibArchive_INCLUDE_DIRS}
)

# --- GDAL via pkg-config ---
# On Windows + MSYS2, IMPORTED_TARGET often contains POSIX paths like /mingw64/include
# which CMake treats as non-existent. So:
#  - Non-Windows: use IMPORTED_TARGET (cleanest)
#  - Windows: use raw pkg-config vars + convert MSYS paths via cygpath
if (WIN32)
  pkg_check_modules(GDAL REQUIRED gdal)

  function(msys_to_native out in)
    if ("${in}" MATCHES "^/")
      execute_process(
        COMMAND cygpath -m "${in}"
        OUTPUT_VARIABLE _p
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      set(${out} "${_p}" PARENT_SCOPE)
    else()
      set(${out} "${in}" PARENT_SCOPE)
    endif()
  endfunction()

  set(GDAL_INCLUDE_DIRS_NATIVE "")
  foreach(d IN LISTS GDAL_INCLUDE_DIRS)
    msys_to_native(nd "${d}")
    list(APPEND GDAL_INCLUDE_DIRS_NATIVE "${nd}")
  endforeach()

  set(GDAL_LIBRARY_DIRS_NATIVE "")
  foreach(d IN LISTS GDAL_LIBRARY_DIRS)
    msys_to_native(nd "${d}")
    list(APPEND GDAL_LIBRARY_DIRS_NATIVE "${nd}")
  endforeach()

  target_include_directories(dist2land PRIVATE ${GDAL_INCLUDE_DIRS_NATIVE})
  target_link_directories(dist2land PRIVATE ${GDAL_LIBRARY_DIRS_NATIVE})
  target_compile_options(dist2land PRIVATE ${GDAL_CFLAGS_OTHER})
  target_link_options(dist2land PRIVATE ${GDAL_LDFLAGS_OTHER})
  target_link_libraries(dist2land PRIVATE ${GDAL_LIBRARIES})
else()
  pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)
  target_link_libraries(dist2land PRIVATE PkgConfig::GDAL)
endif()

target_link_libraries(dist2land PRIVATE
  CURL::libcurl
  ${LibArchive_LIBRARIES}
)

if (MSVC)
  target_compile_definitions(dist2land PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
