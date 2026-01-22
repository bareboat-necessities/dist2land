Run set -euo pipefail
  set -euo pipefail
  rm -rf dist dist2land-sha-b26f48f-windows-x64.zip
  mkdir -p dist/share/dist2land dist/share/certs
  
  cp -f build/dist2land.exe dist/dist2land.exe
  
  # With CMake PROPERTIES PREFIX "" OUTPUT_NAME "dist2land_gdal"
  cp -f build/dist2land_gdal.dll dist/dist2land_gdal.dll
  
  cp -f share/dist2land/providers.ini dist/share/dist2land/providers.ini
  
  [ -f README.md ] && cp -f README.md dist/ || true
  [ -f LICENSE ] && cp -f LICENSE dist/ || true
  
  copy_deps () {
    local bin="$1"
    [ -f "$bin" ] || return 0
    ldd "$bin" | awk '{print $3}' | grep -E "^/mingw64/bin/.*\.dll$" | sort -u | while read -r d; do
      [ -f "$d" ] && cp -f "$d" dist/
    done
  }
  
  # deps for exe + plugin
  copy_deps dist/dist2land.exe
  copy_deps dist/dist2land_gdal.dll
  
  # PROJ data
  mkdir -p dist/share/proj
  cp -r /mingw64/share/proj/* dist/share/proj/ || true
  
  # GDAL data
  mkdir -p dist/share/gdal
  cp -r /mingw64/share/gdal/* dist/share/gdal/ || true
  
  # CA bundle used by libcurl(OpenSSL) (http_download.cpp sets CURLOPT_CAINFO to this path)
  cp -f /mingw64/etc/ssl/certs/ca-bundle.crt dist/share/certs/ca-bundle.crt
  
  # Zip without 7z (avoid p7zip weird crashes)
  ( cd dist && cmake -E tar cfv ../dist2land-sha-b26f48f-windows-x64.zip --format=zip . )
  shell: D:\a\_temp\setup-msys2\msys2.CMD {0}
  env:
    MSYSTEM: MINGW64
    ARTIFACT_TAG: sha-b26f48f
cp: cannot stat 'build/dist2land_gdal.dll': No such file or directory
Error: Process completed with exit code 1.