find_path(GDAL_INCLUDE_DIR
  NAMES gdal.h gdal/gdal.h
  PATHS
    /usr/include
    /usr/include/gdal
    /usr/local/include
    /usr/local/include/gdal
  PATH_SUFFIXES gdal
)

find_library(GDAL_LIBRARY
  NAMES gdal
  PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDAL DEFAULT_MSG GDAL_LIBRARY GDAL_INCLUDE_DIR)
