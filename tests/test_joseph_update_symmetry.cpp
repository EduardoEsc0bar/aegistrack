#include <cmath>

#include <doctest/doctest.h>

#include <Eigen/Dense>

#include "common/covariance_checks.h"
#include "fusion_core/ekf_cv.h"

namespace sensor_fusion {

TEST_CASE("joseph covariance update preserves symmetry") {
  EkfCv ekf;

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 120.0;
  x0(1) = 10.0;
  x0(2) = 20.0;
  x0(3) = 5.0;
  x0(4) = -1.0;
  x0(5) = 0.25;

  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 50.0;
  ekf.initialize(x0, P0);

  const Eigen::Matrix2d radar_R = Eigen::Matrix2d::Identity() * 0.5;
  const Eigen::Matrix2d eoir_R = Eigen::Matrix2d::Identity() * 0.01;

  for (int i = 0; i < 25; ++i) {
    ekf.predict(0.1);

    const Eigen::VectorXd truth_like = ekf.state();
    const double px = truth_like(0);
    const double py = truth_like(1);
    const double pz = truth_like(2);
    const double range = std::sqrt(px * px + py * py);
    const double bearing = std::atan2(py, px);
    const double elevation = std::atan2(pz, range);

    CHECK(ekf.update_range_bearing(range, bearing, radar_R));
    CHECK(ekf.update_bearing_elevation(bearing, elevation, eoir_R));

    const Eigen::MatrixXd P = ekf.covariance();
    const double asym = (P - P.transpose()).norm();
    CHECK(asym < 1e-8);
    CHECK(covariance_checks::covariance_sane(P));
  }
}

}  // namespace sensor_fusion
