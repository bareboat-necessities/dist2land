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
  src/distance_call_win.cpp
  src/distance_call_posix.cpp
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

# --- GDAL linkage ---
if (WIN32)
  # Build a plugin DLL that links to GDAL, so dist2land.exe itself does NOT load GDAL at startup.
  add_library(dist2land_gdal SHARED
    src/dist2land_gdal_plugin.cpp
    src/ogr_distance.cpp
  )
  target_include_directories(dist2land_gdal PRIVATE src)

  # Ensure the DLL name is exactly: dist2land_gdal.dll (no "lib" prefix on MinGW)
  set_target_properties(dist2land_gdal PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dist2land_gdal"
  )

  # Use pkg-config to locate GDAL. (MSYS2 provides gdal.pc)
  pkg_check_modules(GDAL REQUIRED gdal)

  # Convert MSYS paths (/mingw64/...) to native (C:/msys64/mingw64/...) for CMake/MinGW
  function(msys_to_native out inpath)
    if ("${inpath}" MATCHES "^/")
      execute_process(
        COMMAND cygpath -m "${inpath}"
        OUTPUT_VARIABLE _p
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      set(${out} "${_p}" PARENT_SCOPE)
    else()
      set(${out} "${inpath}" PARENT_SCOPE)
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

  target_include_directories(dist2land_gdal PRIVATE ${GDAL_INCLUDE_DIRS_NATIVE})
  if (GDAL_LIBRARY_DIRS_NATIVE)
    target_link_directories(dist2land_gdal PRIVATE ${GDAL_LIBRARY_DIRS_NATIVE})
  endif()

  # pkg-config may provide extra compile/link flags
  if (GDAL_CFLAGS_OTHER)
    target_compile_options(dist2land_gdal PRIVATE ${GDAL_CFLAGS_OTHER})
  endif()
  if (GDAL_LDFLAGS_OTHER)
    target_link_options(dist2land_gdal PRIVATE ${GDAL_LDFLAGS_OTHER})
  endif()

  # Link GDAL libraries listed by pkg-config
  target_link_libraries(dist2land_gdal PRIVATE ${GDAL_LIBRARIES})

else()
  # Non-Windows: link GDAL directly into the executable
  pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)
  target_sources(dist2land PRIVATE src/ogr_distance.cpp)
  target_link_libraries(dist2land PRIVATE PkgConfig::GDAL)
endif()
