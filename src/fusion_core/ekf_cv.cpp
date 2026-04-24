#include "fusion_core/ekf_cv.h"

#include <cmath>
#include <stdexcept>

#include "common/covariance_checks.h"
#include "common/math_checks.h"
#include "observability/metrics.h"

namespace sensor_fusion {
namespace {

constexpr double kEpsilon = 1e-9;
constexpr double kCovarianceEpsilon = 1e-6;

}  // namespace

EkfCv::EkfCv()
    : x_(Eigen::VectorXd::Zero(6)),
      P_(Eigen::MatrixXd::Identity(6, 6)),
      initialized_(false),
      metrics_(nullptr) {}

void EkfCv::initialize(const Eigen::VectorXd& x0, const Eigen::MatrixXd& P0) {
  if (x0.size() != 6 || P0.rows() != 6 || P0.cols() != 6) {
    throw std::invalid_argument("EkfCv initialize expects 6x1 state and 6x6 covariance");
  }

  x_ = x0;
  P_ = P0;
  initialized_ = true;

  if (!math_checks::vector_finite(x_) || !math_checks::matrix_finite(P_)) {
    record_nonfinite_detected();
  }
  if (!covariance_checks::covariance_sane(P_)) {
    if (covariance_checks::stabilize_covariance(P_, kCovarianceEpsilon)) {
      record_covariance_stabilized();
    }
  }
}

void EkfCv::set_metrics(sensor_fusion::observability::Metrics* metrics) {
  metrics_ = metrics;
}

void EkfCv::predict(double dt) {
  if (!initialized_) {
    return;
  }
  if (!math_checks::is_finite(dt) || dt < 0.0) {
    record_skipped_update(true);
    return;
  }

  if (!math_checks::vector_finite(x_) || !math_checks::matrix_finite(P_)) {
    record_skipped_update(true);
    return;
  }

  Eigen::MatrixXd P_prior = P_;
  if (!covariance_checks::covariance_sane(P_prior)) {
    if (!covariance_checks::stabilize_covariance(P_prior, kCovarianceEpsilon)) {
      record_skipped_update(true);
      return;
    }
    record_covariance_stabilized();
  }
  P_prior = 0.5 * (P_prior + P_prior.transpose());

  Eigen::Matrix<double, 6, 6> F = Eigen::Matrix<double, 6, 6>::Identity();
  F(0, 3) = dt;
  F(1, 4) = dt;
  F(2, 5) = dt;

  constexpr double q_scale = 1.0;
  const double q = q_scale * dt;
  Eigen::Matrix<double, 6, 6> Q = Eigen::Matrix<double, 6, 6>::Identity() * q;

  const Eigen::VectorXd x_pred = F * x_;
  Eigen::MatrixXd P_pred = F * P_prior * F.transpose() + Q;

  if (!math_checks::vector_finite(x_pred) || !math_checks::matrix_finite(P_pred)) {
    record_skipped_update(true);
    return;
  }

  if (!covariance_checks::covariance_sane(P_pred)) {
    if (covariance_checks::stabilize_covariance(P_pred, kCovarianceEpsilon)) {
      record_covariance_stabilized();
    } else {
      record_skipped_update(true);
      return;
    }
  }

  x_ = x_pred;
  P_ = P_pred;
}

bool EkfCv::update_range_bearing(double range, double bearing, const Eigen::Matrix2d& R) {
  if (!initialized_) {
    return false;
  }

  if (!math_checks::is_finite(range) || !math_checks::is_finite(bearing) ||
      !R.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }
  if (!math_checks::vector_finite(x_) || !math_checks::matrix_finite(P_)) {
    record_skipped_update(true);
    return false;
  }

  const double px = x_(0);
  const double py = x_(1);
  const double r2 = px * px + py * py;
  const double r = std::sqrt(r2);

  if (!math_checks::is_finite(r2) || !math_checks::is_finite(r) || r <= kEpsilon ||
      r2 <= kEpsilon) {
    record_skipped_update(true);
    return false;
  }

  Eigen::Vector2d h;
  h << r, std::atan2(py, px);

  Eigen::Vector2d z;
  z << range, bearing;

  Eigen::Vector2d innovation = z - h;
  innovation(1) = math_checks::wrap_angle_pi(innovation(1));

  Eigen::Matrix<double, 2, 6> H = Eigen::Matrix<double, 2, 6>::Zero();
  H(0, 0) = px / r;
  H(0, 1) = py / r;
  H(1, 0) = -py / r2;
  H(1, 1) = px / r2;

  return apply_measurement_update(innovation, H, R);
}

bool EkfCv::update_bearing_elevation(double bearing,
                                     double elevation,
                                     const Eigen::Matrix2d& R) {
  if (!initialized_) {
    return false;
  }

  if (!math_checks::is_finite(bearing) || !math_checks::is_finite(elevation) ||
      !R.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }
  if (!math_checks::vector_finite(x_) || !math_checks::matrix_finite(P_)) {
    record_skipped_update(true);
    return false;
  }

  const double px = x_(0);
  const double py = x_(1);
  const double pz = x_(2);

  const double r_xy2 = px * px + py * py;
  const double r_xy = std::sqrt(r_xy2);
  const double r_xyz2 = r_xy2 + pz * pz;
  const double r_xyz = std::sqrt(r_xyz2);

  if (!math_checks::is_finite(r_xy2) || !math_checks::is_finite(r_xy) ||
      !math_checks::is_finite(r_xyz2) || !math_checks::is_finite(r_xyz) || r_xy <= kEpsilon ||
      r_xy2 <= kEpsilon || r_xyz <= kEpsilon || r_xyz2 <= kEpsilon) {
    record_skipped_update(true);
    return false;
  }

  Eigen::Vector2d h;
  h << std::atan2(py, px), std::atan2(pz, r_xy);

  Eigen::Vector2d z;
  z << bearing, elevation;

  Eigen::Vector2d innovation = z - h;
  innovation(0) = math_checks::wrap_angle_pi(innovation(0));

  Eigen::Matrix<double, 2, 6> H = Eigen::Matrix<double, 2, 6>::Zero();
  H(0, 0) = -py / r_xy2;
  H(0, 1) = px / r_xy2;
  H(1, 0) = -(px * pz) / (r_xy2 * r_xyz);
  H(1, 1) = -(py * pz) / (r_xy2 * r_xyz);
  H(1, 2) = r_xy / r_xyz2;

  return apply_measurement_update(innovation, H, R);
}

bool EkfCv::apply_measurement_update(const Eigen::Vector2d& innovation,
                                     const Eigen::Matrix<double, 2, 6>& H,
                                     const Eigen::Matrix2d& R) {
  if (!math_checks::vector_finite(innovation) || !H.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }
  if (!math_checks::matrix_finite(P_) || !R.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }

  Eigen::MatrixXd P_prior = P_;
  if (!covariance_checks::covariance_sane(P_prior)) {
    if (!covariance_checks::stabilize_covariance(P_prior, kCovarianceEpsilon)) {
      record_skipped_update(true);
      return false;
    }
    record_covariance_stabilized();
  }
  P_prior = 0.5 * (P_prior + P_prior.transpose());

  const Eigen::Matrix2d S = H * P_prior * H.transpose() + R;
  if (!S.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }
  const Eigen::Matrix2d S_sym = 0.5 * (S + S.transpose());

  Eigen::LDLT<Eigen::Matrix2d> ldlt(S_sym);
  if (ldlt.info() != Eigen::Success || !ldlt.isPositive()) {
    record_skipped_update(true);
    return false;
  }

  const Eigen::Matrix<double, 6, 2> PHt = P_prior * H.transpose();
  const Eigen::Matrix<double, 6, 2> K = ldlt.solve(PHt.transpose()).transpose();
  if (!K.array().isFinite().all()) {
    record_skipped_update(true);
    return false;
  }

  const Eigen::VectorXd x_next = x_ + K * innovation;
  if (!math_checks::vector_finite(x_next)) {
    record_skipped_update(true);
    return false;
  }

  const Eigen::Matrix<double, 6, 6> I = Eigen::Matrix<double, 6, 6>::Identity();
  const Eigen::Matrix<double, 6, 6> IKH = I - K * H;
  Eigen::MatrixXd P_next = IKH * P_prior * IKH.transpose() + K * R * K.transpose();

  if (!math_checks::matrix_finite(P_next)) {
    record_skipped_update(true);
    return false;
  }

  if (!covariance_checks::covariance_sane(P_next)) {
    if (covariance_checks::stabilize_covariance(P_next, kCovarianceEpsilon)) {
      record_covariance_stabilized();
    } else {
      record_skipped_update(true);
      return false;
    }
  }

  x_ = x_next;
  P_ = P_next;
  return true;
}

void EkfCv::record_skipped_update(bool nonfinite) {
  if (metrics_ != nullptr) {
    metrics_->inc("ekf_update_skipped_total");
    if (nonfinite) {
      metrics_->inc("nonfinite_detected_total");
    }
  }
}

void EkfCv::record_covariance_stabilized() {
  if (metrics_ != nullptr) {
    metrics_->inc("covariance_stabilized_total");
  }
}

void EkfCv::record_nonfinite_detected() {
  if (metrics_ != nullptr) {
    metrics_->inc("nonfinite_detected_total");
  }
}

Eigen::VectorXd EkfCv::state() const {
  return x_;
}

Eigen::MatrixXd EkfCv::covariance() const {
  return P_;
}

bool EkfCv::is_initialized() const {
  return initialized_;
}

}  // namespace sensor_fusion
