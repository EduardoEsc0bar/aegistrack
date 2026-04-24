#include <doctest/doctest.h>

#include <Eigen/Dense>

#include "fusion_core/ekf_cv.h"
#include "observability/metrics.h"

namespace sensor_fusion {

TEST_CASE("ekf skips unstable update near origin and increments metric") {
  observability::Metrics metrics;

  EkfCv ekf;
  ekf.set_metrics(&metrics);

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6);
  ekf.initialize(x0, P0);

  const Eigen::VectorXd before_x = ekf.state();
  const Eigen::MatrixXd before_P = ekf.covariance();

  Eigen::Matrix2d R = Eigen::Matrix2d::Identity();
  const bool updated = ekf.update_range_bearing(100.0, 0.0, R);

  CHECK(!updated);
  CHECK((ekf.state() - before_x).norm() < 1e-12);
  CHECK((ekf.covariance() - before_P).norm() < 1e-12);

  const auto snap = metrics.snapshot();
  CHECK(snap.counters.find("ekf_update_skipped_total") != snap.counters.end());
  CHECK(snap.counters.at("ekf_update_skipped_total") >= 1);
  CHECK(snap.counters.find("nonfinite_detected_total") != snap.counters.end());
  CHECK(snap.counters.at("nonfinite_detected_total") >= 1);
}

}  // namespace sensor_fusion
