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
./dist2land setup --provider osm
```

## Query

```bash
./dist2land distance --lat 36.84 --lon -62.42
```

## Creating spacial index (Windows)

Example for OSM:

```bash
ogrinfo -ro -so ../../../AppData/Local/dist2land/providers/osm/extracted/land-polygons-split-4326/land_polygons.shp
```

```bash
ogrinfo -ro ../../../AppData/Local/dist2land/providers/osm/extracted/land-polygons-split-4326/land_polygons.shp -sql "CREATE SPATIAL INDEX ON land_polygons"
```

## Example output

```
$ time ./dist2land distance --lat 36.84 --lon -62.42
404187.590 m 44.65687940 -62.86739060
provider=osm metric=geodesic shp=C:\Users\17326\AppData\Local\dist2land\providers\osm\extracted\land-polygons-split-4326\land_polygons.shp geodesic_m=404188

real    0m6.634s

```

Json:

```
$ ./dist2land distance --lat 36.84 --lon -62.42 --json 2>/dev/null
{"query":{"lat_deg":36.84000000,"lon_deg":-62.42000000},"result":{"distance":404187.590,"units":"m","metric":"geodesic","provider":"osm","land_lat_deg":44.65687940,"land_lon_deg":-62.86739060,"distance_m":404187.590,"geodesic_m":404187.590,"in_land":false,"shp":"C:\\Users\\17326\\AppData\\Local\\dist2land\\providers\\osm\\extracted\\land-polygons-split-4326\\land_polygons.shp"}}
```

## Debian packages install via apt

Add the repo.

Create /etc/apt/sources.list.d/dist2land.list:

```bash
sudo tee /etc/apt/sources.list.d/dist2land.list >/dev/null <<'EOF'
deb [trusted=yes] https://github.com/bareboat-necessities/dist2land/releases/download/apt/ ./
EOF
```

Update + install

```bash
sudo apt-get update
sudo apt-get install dist2land
```