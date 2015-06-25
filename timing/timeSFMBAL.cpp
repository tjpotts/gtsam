/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    timeSFMBAL.cpp
 * @brief   time structure from motion with BAL file
 * @author  Frank Dellaert
 * @date    June 6, 2015
 */

#include <gtsam/slam/dataset.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/inference/FactorGraph.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/base/timing.h>

#include <boost/foreach.hpp>
#include <stddef.h>
#include <stdexcept>
#include <string>

using namespace std;
using namespace gtsam;

#define USE_GTSAM_FACTOR
#ifdef USE_GTSAM_FACTOR
#include <gtsam/slam/GeneralSFMFactor.h>
#include <gtsam/geometry/Cal3Bundler.h>
#include <gtsam/geometry/PinholeCamera.h>
typedef PinholeCamera<Cal3Bundler> Camera;
typedef GeneralSFMFactor<Camera, Point3> SfmFactor;
#else
#include <gtsam/3rdparty/ceres/example.h>
#include <gtsam/nonlinear/ExpressionFactor.h>
#include <gtsam/nonlinear/AdaptAutoDiff.h>
// Special version of Cal3Bundler so that default constructor = 0,0,0
struct CeresCalibration: public Cal3Bundler {
  CeresCalibration(double f = 0, double k1 = 0, double k2 = 0, double u0 = 0,
      double v0 = 0) :
      Cal3Bundler(f, k1, k2, u0, v0) {
  }
  CeresCalibration(const Cal3Bundler& cal) :
      Cal3Bundler(cal) {
  }
  CeresCalibration retract(const Vector& d) const {
    return CeresCalibration(fx() + d(0), k1() + d(1), k2() + d(2), u0(), v0());
  }
  Vector3 localCoordinates(const CeresCalibration& T2) const {
    return T2.vector() - vector();
  }
};

namespace gtsam {
template<>
struct traits<CeresCalibration> : public internal::Manifold<CeresCalibration> {
};
}

// With that, camera below behaves like Snavely's 9-dim vector
typedef PinholeCamera<CeresCalibration> Camera;
#endif

int main(int argc, char* argv[]) {
  using symbol_shorthand::P;

  // Load BAL file (default is tiny)
  string defaultFilename = findExampleDataFile("dubrovnik-3-7-pre");
  SfM_data db;
  bool success = readBAL(argc > 1 ? argv[1] : defaultFilename, db);
  if (!success)
    throw runtime_error("Could not access file!");

#ifndef USE_GTSAM_FACTOR
  AdaptAutoDiff<SnavelyProjection, Point2, Camera, Point3> snavely;
#endif

  // Build graph
  SharedNoiseModel unit2 = noiseModel::Unit::Create(2);
  NonlinearFactorGraph graph;
  for (size_t j = 0; j < db.number_tracks(); j++) {
    BOOST_FOREACH (const SfM_Measurement& m, db.tracks[j].measurements) {
      size_t i = m.first;
      Point2 measurement = m.second;
#ifdef USE_GTSAM_FACTOR
      graph.push_back(SfmFactor(measurement, unit2, i, P(j)));
#else
      Expression<Camera> camera_(i);
      Expression<Point3> point_(P(j));
      graph.addExpressionFactor(unit2, measurement,
          Expression<Point2>(snavely, camera_, point_));
#endif
    }
  }

  Values initial;
  size_t i = 0, j = 0;
  BOOST_FOREACH(const SfM_Camera& camera, db.cameras) {
#ifdef USE_GTSAM_FACTOR
    initial.insert((i++), camera);
#else
    Camera ceresCamera(camera.pose(), camera.calibration());
    initial.insert((i++), ceresCamera);
#endif
  }
  BOOST_FOREACH(const SfM_Track& track, db.tracks)
    initial.insert(P(j++), track.p);

  // Check projection
  Point2 expected = db.tracks.front().measurements.front().second;
  Camera camera = initial.at<Camera>(0);
  Point3 point = initial.at<Point3>(P(0));
#ifdef USE_GTSAM_FACTOR
  Point2 actual = camera.project(point);
#else
  Point2 actual = snavely(camera, point);
#endif
  assert_equal(expected,actual,10);

  // Create Schur-complement ordering
#ifdef CCOLAMD
  vector<Key> pointKeys;
  for (size_t j = 0; j < db.number_tracks(); j++) pointKeys.push_back(P(j));
  Ordering ordering = Ordering::colamdConstrainedFirst(graph, pointKeys, true);
#else
  Ordering ordering;
  for (size_t j = 0; j < db.number_tracks(); j++)
    ordering.push_back(P(j));
  for (size_t i = 0; i < db.number_cameras(); i++)
    ordering.push_back(i);
#endif

  // Optimize
  // Set parameters to be similar to ceres
  LevenbergMarquardtParams params;
  LevenbergMarquardtParams::SetCeresDefaults(&params);
  params.setOrdering(ordering);
  params.setVerbosity("ERROR");
  params.setVerbosityLM("TRYLAMBDA");
  LevenbergMarquardtOptimizer lm(graph, initial, params);
  Values result = lm.optimize();

  tictoc_finishedIteration_();
  tictoc_print_();

  return 0;
}