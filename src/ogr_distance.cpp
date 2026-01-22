#include "ogr_distance.h"
#include <gdal.h>
#include <ogrsf_frmts.h>
#include <cmath>
#include <limits>
#include <stdexcept>

static double deg2rad(double d) { return d * M_PI / 180.0; }

static void metersToDegWindow(double lat_deg, double radius_m, double& dlat_deg, double& dlon_deg) {
  const double meters_per_deg_lat = 111320.0;
  dlat_deg = radius_m / meters_per_deg_lat;
  double coslat = std::cos(deg2rad(lat_deg));
  if (coslat < 1e-6) coslat = 1e-6;
  dlon_deg = radius_m / (meters_per_deg_lat * coslat);
}

static OGRCoordinateTransformation* make_to_aeqd(double lat, double lon) {
  OGRSpatialReference wgs84;
  wgs84.importFromEPSG(4326);

  OGRSpatialReference aeqd;
  std::string proj4 = "+proj=aeqd +lat_0=" + std::to_string(lat) +
                      " +lon_0=" + std::to_string(lon) +
                      " +datum=WGS84 +units=m +no_defs";
  aeqd.importFromProj4(proj4.c_str());

  return OGRCreateCoordinateTransformation(&wgs84, &aeqd);
}

DistanceResult distance_to_land_m(double lat_deg, double lon_deg,
                                  const std::string& provider_id,
                                  const std::filesystem::path& shp_path) {
  GDALAllRegister();

  GDALDataset* ds = (GDALDataset*)GDALOpenEx(shp_path.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr);
  if (!ds) throw std::runtime_error("Failed to open shapefile: " + shp_path.string());

  OGRLayer* layer = ds->GetLayer(0);
  if (!layer) { GDALClose(ds); throw std::runtime_error("No layer in shapefile"); }

  // NOTE: spatial index creation is driver/version dependent.
  // For best performance with shapefiles, create a .qix once:
  //   ogrinfo land.shp -sql "CREATE SPATIAL INDEX ON <layername>"

  OGRPoint p_wgs(lon_deg, lat_deg);

  OGRCoordinateTransformation* toAEQD = make_to_aeqd(lat_deg, lon_deg);
  if (!toAEQD) { GDALClose(ds); throw std::runtime_error("Failed to create AEQD transform"); }

  double best = std::numeric_limits<double>::infinity();
  double radius_m = 10'000.0;
  const double max_radius_m = 20'000'000.0;

  auto scanWindow = [&](double xmin, double ymin, double xmax, double ymax) {
    layer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
    layer->ResetReading();

    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      OGRGeometry* g = feat->GetGeometryRef();
      if (!g) { OGRFeature::DestroyFeature(feat); continue; }

      OGRGeometry* g_xy = g->clone();
      OGRPoint p_xy = p_wgs;

      if (g_xy->transform(toAEQD) != OGRERR_NONE || p_xy.transform(toAEQD) != OGRERR_NONE) {
        OGRGeometryFactory::destroyGeometry(g_xy);
        OGRFeature::DestroyFeature(feat);
        continue;
      }

      if (p_xy.Within(g_xy)) { // requires GEOS for best robustness
        best = 0.0;
        OGRGeometryFactory::destroyGeometry(g_xy);
        OGRFeature::DestroyFeature(feat);
        return;
      }

      double d = p_xy.Distance(g_xy); // meters in AEQD
      if (d < best) best = d;

      OGRGeometryFactory::destroyGeometry(g_xy);
      OGRFeature::DestroyFeature(feat);
    }
  };

  while (radius_m <= max_radius_m) {
    double dlat, dlon;
    metersToDegWindow(lat_deg, radius_m, dlat, dlon);

    double ymin = lat_deg - dlat;
    double ymax = lat_deg + dlat;
    double xmin = lon_deg - dlon;
    double xmax = lon_deg + dlon;

    layer->SetSpatialFilter(nullptr);

    // Handle antimeridian crudely by splitting window
    if (xmin < -180.0) {
      scanWindow(xmin + 360.0, ymin, 180.0, ymax);
      if (best == 0.0) break;
      scanWindow(-180.0, ymin, xmax, ymax);
    } else if (xmax > 180.0) {
      scanWindow(xmin, ymin, 180.0, ymax);
      if (best == 0.0) break;
      scanWindow(-180.0, ymin, xmax - 360.0, ymax);
    } else {
      scanWindow(xmin, ymin, xmax, ymax);
    }

    layer->SetSpatialFilter(nullptr);

    if (std::isfinite(best) && best <= radius_m * 1.2) break;
    radius_m *= 2.0;
  }

  OCTDestroyCoordinateTransformation(toAEQD);
  GDALClose(ds);

  if (!std::isfinite(best)) throw std::runtime_error("No distance computed (bad dataset?)");
  return DistanceResult{best, provider_id, shp_path};
}
