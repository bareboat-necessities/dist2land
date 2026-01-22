#include "ogr_distance.h"
#include <gdal.h>
#include <ogrsf_frmts.h>

#include <cmath>
#include <limits>
#include <stdexcept>

static constexpr double kPi = 3.141592653589793238462643383279502884;
static double deg2rad(double d) { return d * (kPi / 180.0); }

static void metersToDegWindow(double lat_deg, double radius_m, double& dlat_deg, double& dlon_deg) {
  const double meters_per_deg_lat = 111320.0;
  dlat_deg = radius_m / meters_per_deg_lat;
  double coslat = std::cos(deg2rad(lat_deg));
  if (coslat < 1e-6) coslat = 1e-6;
  dlon_deg = radius_m / (meters_per_deg_lat * coslat);
}

static void update_best_on_lines(const OGRGeometry* geom,
                                 const OGRPoint& p_xy,
                                 double& best_dist_m,
                                 OGRPoint& best_pt_xy) {
  if (!geom) return;

  const auto gt = wkbFlatten(geom->getGeometryType());

  if (gt == wkbLineString || gt == wkbLinearRing) {
    const auto* ls = dynamic_cast<const OGRLineString*>(geom);
    if (!ls || ls->getNumPoints() < 2) return;

    const double s = ls->Project(&p_xy);
    OGRPoint cp;
    ls->Value(s, &cp);

    const double d = cp.Distance(&p_xy);
    if (d < best_dist_m) {
      best_dist_m = d;
      best_pt_xy = cp;
    }
    return;
  }

  // MultiLineString inherits OGRGeometryCollection
  if (gt == wkbMultiLineString || gt == wkbGeometryCollection) {
    const auto* coll = dynamic_cast<const OGRGeometryCollection*>(geom);
    if (!coll) return;
    const int n = coll->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      update_best_on_lines(coll->getGeometryRef(i), p_xy, best_dist_m, best_pt_xy);
    }
    return;
  }

  // Unexpected type: ignore (boundary() from polygons should yield lines)
}

DistanceResult distance_to_land_geodesic(double lat_deg, double lon_deg,
                                         const std::string& provider_id,
                                         const std::filesystem::path& shp_path) {
  GDALAllRegister();

  GDALDataset* ds = (GDALDataset*)GDALOpenEx(
      shp_path.string().c_str(),
      GDAL_OF_VECTOR | GDAL_OF_READONLY,
      nullptr, nullptr, nullptr);

  if (!ds) throw std::runtime_error("Failed to open shapefile: " + shp_path.string());

  OGRLayer* layer = ds->GetLayer(0);
  if (!layer) { GDALClose(ds); throw std::runtime_error("No layer in shapefile"); }

  // Build CRS + coordinate transforms
  OGRSpatialReference wgs84;
  wgs84.importFromEPSG(4326);

  OGRSpatialReference aeqd;
  {
    std::string proj4 = "+proj=aeqd +lat_0=" + std::to_string(lat_deg) +
                        " +lon_0=" + std::to_string(lon_deg) +
                        " +datum=WGS84 +units=m +no_defs";
    aeqd.importFromProj4(proj4.c_str());
  }

  OGRCoordinateTransformation* toAEQD = OGRCreateCoordinateTransformation(&wgs84, &aeqd);
  OGRCoordinateTransformation* toWGS  = OGRCreateCoordinateTransformation(&aeqd, &wgs84);
  if (!toAEQD || !toWGS) {
    if (toAEQD) OCTDestroyCoordinateTransformation(toAEQD);
    if (toWGS)  OCTDestroyCoordinateTransformation(toWGS);
    GDALClose(ds);
    throw std::runtime_error("Failed to create coordinate transformations (WGS84 <-> AEQD)");
  }

  // Query point
  OGRPoint p_wgs(lon_deg, lat_deg);
  OGRPoint p_xy = p_wgs;
  if (p_xy.transform(toAEQD) != OGRERR_NONE) {
    OCTDestroyCoordinateTransformation(toAEQD);
    OCTDestroyCoordinateTransformation(toWGS);
    GDALClose(ds);
    throw std::runtime_error("Failed to transform query point to AEQD");
  }

  double best = std::numeric_limits<double>::infinity();
  OGRPoint best_pt_xy; // AEQD coords for nearest land point

  double radius_m = 10'000.0;
  const double max_radius_m = 20'000'000.0;

  auto scanWindow = [&](double xmin, double ymin, double xmax, double ymax) {
    layer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
    layer->ResetReading();

    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      OGRGeometry* g = feat->GetGeometryRef();
      if (!g) { OGRFeature::DestroyFeature(feat); continue; }

      // Work in AEQD meters
      OGRGeometry* g_xy = g->clone();
      if (g_xy->transform(toAEQD) != OGRERR_NONE) {
        OGRGeometryFactory::destroyGeometry(g_xy);
        OGRFeature::DestroyFeature(feat);
        continue;
      }

      // If point is on/inside land polygon, distance is 0 and nearest land point is the query point
      // (Distance(point, polygon) == 0 covers inside and boundary contact)
      const double d_poly = p_xy.Distance(g_xy);
      if (d_poly == 0.0) {
        best = 0.0;
        best_pt_xy = p_xy;
        OGRGeometryFactory::destroyGeometry(g_xy);
        OGRFeature::DestroyFeature(feat);
        return; // early exit from scanWindow
      }

      // Nearest point to a polygon (when outside) lies on its boundary
      OGRGeometry* bnd = g_xy->Boundary(); // new geometry
      if (bnd) {
        update_best_on_lines(bnd, p_xy, best, best_pt_xy);
        OGRGeometryFactory::destroyGeometry(bnd);
      }

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

    // Handle antimeridian by splitting
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

    if (best == 0.0) break;
    if (std::isfinite(best) && best <= radius_m * 1.2) break;

    radius_m *= 2.0;
  }

  layer->SetSpatialFilter(nullptr);

  if (!std::isfinite(best)) {
    OCTDestroyCoordinateTransformation(toAEQD);
    OCTDestroyCoordinateTransformation(toWGS);
    GDALClose(ds);
    throw std::runtime_error("No distance computed (bad dataset?)");
  }

  // Convert best point back to WGS84 lat/lon
  OGRPoint land_wgs = best_pt_xy;
  if (land_wgs.transform(toWGS) != OGRERR_NONE) {
    OCTDestroyCoordinateTransformation(toAEQD);
    OCTDestroyCoordinateTransformation(toWGS);
    GDALClose(ds);
    throw std::runtime_error("Failed to transform nearest land point back to WGS84");
  }

  const double land_lon = land_wgs.getX();
  const double land_lat = land_wgs.getY();

  OCTDestroyCoordinateTransformation(toAEQD);
  OCTDestroyCoordinateTransformation(toWGS);
  GDALClose(ds);

  DistanceResult out;
  out.geodesic_m = best;
  out.land_lat_deg = land_lat;
  out.land_lon_deg = land_lon;
  out.provider_id = provider_id;
  out.shp_path = shp_path;
  return out;
}
