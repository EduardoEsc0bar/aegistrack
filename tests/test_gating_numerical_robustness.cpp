#include <cmath>

#include <doctest/doctest.h>

#include <Eigen/Dense>

#include "fusion_core/ekf_cv.h"
#include "fusion_core/gating.h"

namespace sensor_fusion::fusion_core {

TEST_CASE("gate range-bearing fails deterministically near origin") {
  sensor_fusion::EkfCv ekf;

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 1e-12;
  x0(1) = -1e-12;
  const Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6);
  ekf.initialize(x0, P0);

  const Eigen::Matrix2d R = Eigen::Matrix2d::Identity();
  const GateResult gate = gate_range_bearing(ekf, 100.0, 0.0, R, 9.21);

  CHECK(!gate.pass);
  CHECK(!std::isnan(gate.mahalanobis2));
  CHECK(gate.mahalanobis2 >= 0.0);
  CHECK(gate.numerical_issue);
}

}  // namespace sensor_fusion::fusion_core
