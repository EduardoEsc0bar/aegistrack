#include <doctest/doctest.h>

#include <Eigen/Dense>

#include "common/covariance_checks.h"

namespace sensor_fusion::covariance_checks {

TEST_CASE("covariance sanity rejects negative diagonal") {
  Eigen::MatrixXd P = Eigen::MatrixXd::Identity(6, 6);
  P(0, 0) = -0.5;

  CHECK(!covariance_sane(P));
}

TEST_CASE("covariance stabilization repairs basic invalid covariance") {
  Eigen::MatrixXd P = Eigen::MatrixXd::Identity(6, 6);
  P(0, 0) = -1.0;
  P(1, 0) = 1e-8;

  CHECK(!covariance_sane(P));
  CHECK(stabilize_covariance(P));
  CHECK(covariance_sane(P));
}

TEST_CASE("covariance sanity uses optional jitter for semidefinite covariance") {
  Eigen::MatrixXd P = Eigen::MatrixXd::Identity(6, 6);
  P(5, 5) = 0.0;

  CHECK(!covariance_sane(P, 1e-6, 0.0));
  CHECK(covariance_sane(P, 1e-6, 1e-6));
}

}  // namespace sensor_fusion::covariance_checks
