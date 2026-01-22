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

# --- GDAL backend strategy ---
# Windows: build plugin DLL that links GDAL. EXE loads it at runtime.
# Non-Windows: link GDAL directly into the EXE.

if (WIN32)
  target_sources(dist2land PRIVATE
    src/distance_call_win.cpp
  )

  add_library(dist2land_gdal SHARED
    src/dist2land_gdal_plugin.cpp
    src/ogr_distance.cpp
  )
  target_include_directories(dist2land_gdal PRIVATE src)

  # Force predictable DLL name (no "lib" prefix) and place next to build/ outputs
  set_target_properties(dist2land_gdal PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dist2land_gdal"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )

  # NOTE: On GitHub Actions + MSYS2, cmake is often a Windows-native cmake.exe.
  # That means POSIX paths like /mingw64/include can look "non-existent".
  # So: DO NOT use IMPORTED_TARGET on Windows. Translate paths using cygpath.
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

  target_include_directories(dist2land_gdal PRIVATE ${GDAL_INCLUDE_DIRS_NATIVE})
  target_link_directories(dist2land_gdal PRIVATE ${GDAL_LIBRARY_DIRS_NATIVE})
  target_compile_options(dist2land_gdal PRIVATE ${GDAL_CFLAGS_OTHER})
  target_link_options(dist2land_gdal PRIVATE ${GDAL_LDFLAGS_OTHER})
  target_link_libraries(dist2land_gdal PRIVATE ${GDAL_LIBRARIES})

else()
  target_sources(dist2land PRIVATE
    src/distance_call_posix.cpp
    src/ogr_distance.cpp
  )

  pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)
  target_link_libraries(dist2land PRIVATE PkgConfig::GDAL)
endif()
