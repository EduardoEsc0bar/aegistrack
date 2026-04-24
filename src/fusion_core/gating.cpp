#include "fusion_core/gating.h"

#include <cmath>

#include "common/math_checks.h"

namespace sensor_fusion::fusion_core {
namespace {

constexpr double kEpsilon = 1e-9;
constexpr double kGateFailMahalanobis2 = 1e12;

GateResult fail_gate(bool numerical_issue = false) {
  return {.pass = false,
          .mahalanobis2 = kGateFailMahalanobis2,
          .numerical_issue = numerical_issue};
}

}  // namespace

GateResult gate_range_bearing(const sensor_fusion::EkfCv& ekf,
                              double meas_range,
                              double meas_bearing,
                              const Eigen::Matrix2d& R,
                              double gate_threshold_mahalanobis2) {
  if (!ekf.is_initialized()) {
    return fail_gate();
  }
  if (!math_checks::is_finite(meas_range) || !math_checks::is_finite(meas_bearing) ||
      !R.array().isFinite().all() || !math_checks::is_finite(gate_threshold_mahalanobis2) ||
      gate_threshold_mahalanobis2 < 0.0) {
    return fail_gate(true);
  }

  const Eigen::VectorXd x = ekf.state();
  const Eigen::MatrixXd P = ekf.covariance();

  if (x.size() != 6 || P.rows() != 6 || P.cols() != 6 || !math_checks::vector_finite(x) ||
      !math_checks::matrix_finite(P)) {
    return fail_gate(true);
  }

  const double px = x(0);
  const double py = x(1);
  const double r2 = px * px + py * py;
  const double r = std::sqrt(r2);
  if (!math_checks::is_finite(r2) || !math_checks::is_finite(r) || r <= kEpsilon ||
      r2 <= kEpsilon) {
    return fail_gate(true);
  }

  Eigen::Vector2d z;
  z << meas_range, meas_bearing;

  Eigen::Vector2d h;
  h << r, std::atan2(py, px);

  Eigen::Vector2d innovation = z - h;
  innovation(1) = math_checks::wrap_angle_pi(innovation(1));

  Eigen::Matrix<double, 2, 6> H = Eigen::Matrix<double, 2, 6>::Zero();
  H(0, 0) = px / r;
  H(0, 1) = py / r;
  H(1, 0) = -py / r2;
  H(1, 1) = px / r2;

  const Eigen::Matrix2d S = H * P * H.transpose() + R;
  if (!S.array().isFinite().all() || !innovation.array().isFinite().all()) {
    return fail_gate(true);
  }
  const Eigen::Matrix2d S_sym = 0.5 * (S + S.transpose());

  Eigen::LDLT<Eigen::Matrix2d> ldlt(S_sym);
  if (ldlt.info() != Eigen::Success || !ldlt.isPositive()) {
    return fail_gate(true);
  }

  const Eigen::Vector2d solved = ldlt.solve(innovation);
  if (!solved.array().isFinite().all()) {
    return fail_gate(true);
  }

  const double mahal2 = innovation.dot(solved);
  if (!math_checks::is_finite(mahal2)) {
    return fail_gate(true);
  }

  return {.pass = mahal2 <= gate_threshold_mahalanobis2,
          .mahalanobis2 = mahal2,
          .numerical_issue = false};
}

GateResult gate_bearing_elevation(const sensor_fusion::EkfCv& ekf,
                                  double meas_bearing,
                                  double meas_elevation,
                                  const Eigen::Matrix2d& R,
                                  double gate_threshold_mahalanobis2) {
  if (!ekf.is_initialized()) {
    return fail_gate();
  }
  if (!math_checks::is_finite(meas_bearing) || !math_checks::is_finite(meas_elevation) ||
      !R.array().isFinite().all() || !math_checks::is_finite(gate_threshold_mahalanobis2) ||
      gate_threshold_mahalanobis2 < 0.0) {
    return fail_gate(true);
  }

  const Eigen::VectorXd x = ekf.state();
  const Eigen::MatrixXd P = ekf.covariance();
  if (x.size() != 6 || P.rows() != 6 || P.cols() != 6 || !math_checks::vector_finite(x) ||
      !math_checks::matrix_finite(P)) {
    return fail_gate(true);
  }

  const double px = x(0);
  const double py = x(1);
  const double pz = x(2);

  const double r_xy2 = px * px + py * py;
  const double r_xy = std::sqrt(r_xy2);
  const double r_xyz2 = r_xy2 + pz * pz;
  const double r_xyz = std::sqrt(r_xyz2);

  if (!math_checks::is_finite(r_xy2) || !math_checks::is_finite(r_xy) ||
      !math_checks::is_finite(r_xyz2) || !math_checks::is_finite(r_xyz) || r_xy <= kEpsilon ||
      r_xy2 <= kEpsilon || r_xyz <= kEpsilon || r_xyz2 <= kEpsilon) {
    return fail_gate(true);
  }

  Eigen::Vector2d z;
  z << meas_bearing, meas_elevation;

  Eigen::Vector2d h;
  h << std::atan2(py, px), std::atan2(pz, r_xy);

  Eigen::Vector2d innovation = z - h;
  innovation(0) = math_checks::wrap_angle_pi(innovation(0));

  Eigen::Matrix<double, 2, 6> H = Eigen::Matrix<double, 2, 6>::Zero();
  H(0, 0) = -py / r_xy2;
  H(0, 1) = px / r_xy2;
  H(1, 0) = -(px * pz) / (r_xy2 * r_xyz);
  H(1, 1) = -(py * pz) / (r_xy2 * r_xyz);
  H(1, 2) = r_xy / r_xyz2;

  const Eigen::Matrix2d S = H * P * H.transpose() + R;
  if (!S.array().isFinite().all() || !innovation.array().isFinite().all()) {
    return fail_gate(true);
  }
  const Eigen::Matrix2d S_sym = 0.5 * (S + S.transpose());

  Eigen::LDLT<Eigen::Matrix2d> ldlt(S_sym);
  if (ldlt.info() != Eigen::Success || !ldlt.isPositive()) {
    return fail_gate(true);
  }

  const Eigen::Vector2d solved = ldlt.solve(innovation);
  if (!solved.array().isFinite().all()) {
    return fail_gate(true);
  }

  const double mahal2 = innovation.dot(solved);
  if (!math_checks::is_finite(mahal2)) {
    return fail_gate(true);
  }

  return {.pass = mahal2 <= gate_threshold_mahalanobis2,
          .mahalanobis2 = mahal2,
          .numerical_issue = false};
}

}  // namespace sensor_fusion::fusion_core
