# Minimal FindGDAL.cmake for common setups
find_path(GDAL_INCLUDE_DIR gdal.h
  PATHS /usr/include /usr/local/include
)

find_library(GDAL_LIBRARY NAMES gdal
  PATHS /usr/lib /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDAL DEFAULT_MSG GDAL_LIBRARY GDAL_INCLUDE_DIR)
