# dist2land

Command line utility: given a GPS position (lat, lon), returns distance to nearest land.

## Providers (free datasets)

- **OSM Land Polygons (WGS84, split)**: highest detail; large download; ODbL.
  Source: https://osmdata.openstreetmap.de/data/land-polygons.html

- **NOAA GSHHG Shapefiles**: includes multiple resolutions; we use full-resolution L1 (land/ocean).
  Source index: https://www.ngdc.noaa.gov/mgg/shorelines/data/gshhg/latest/

- **Natural Earth Land**: small & coarse; public domain.
  Common download endpoint: http://www.naturalearthdata.com/download/10m/physical/ne_10m_land.zip

## Build

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## One-time setup (downloads to cache)

```bash
./build/dist2land setup --provider osm
```

## Query

```bash
./build/dist2land distance --lat 36.84 --lon -122.42
```

