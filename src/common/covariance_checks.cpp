#include "common/covariance_checks.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sensor_fusion::covariance_checks {
namespace {

bool finite_matrix(const Eigen::MatrixXd& m) {
  return m.array().isFinite().all();
}

}  // namespace

bool covariance_sane(const Eigen::MatrixXd& P, double symmetry_tolerance, double jitter) {
  if (P.rows() == 0 || P.cols() == 0 || P.rows() != P.cols()) {
    return false;
  }
  if (!finite_matrix(P)) {
    return false;
  }

  const Eigen::MatrixXd asym = P - P.transpose();
  const double scale = std::max(1.0, P.norm());
  if (asym.norm() > symmetry_tolerance * scale) {
    return false;
  }

  for (int i = 0; i < P.rows(); ++i) {
    if (!std::isfinite(P(i, i)) || P(i, i) < -symmetry_tolerance) {
      return false;
    }
  }

  const Eigen::MatrixXd sym = 0.5 * (P + P.transpose());
  Eigen::LLT<Eigen::MatrixXd> llt(sym);
  if (llt.info() == Eigen::Success) {
    return true;
  }

  if (!std::isfinite(jitter) || jitter <= 0.0) {
    return false;
  }

  const Eigen::MatrixXd jittered =
      sym + jitter * Eigen::MatrixXd::Identity(P.rows(), P.cols());
  Eigen::LLT<Eigen::MatrixXd> jittered_llt(jittered);
  return jittered_llt.info() == Eigen::Success;
}

bool stabilize_covariance(Eigen::MatrixXd& P, double epsilon) {
  if (P.rows() == 0 || P.cols() == 0 || P.rows() != P.cols()) {
    return false;
  }
  if (!std::isfinite(epsilon) || epsilon <= 0.0) {
    return false;
  }

  for (int r = 0; r < P.rows(); ++r) {
    for (int c = 0; c < P.cols(); ++c) {
      if (!std::isfinite(P(r, c))) {
        P(r, c) = 0.0;
      }
    }
  }

  P = 0.5 * (P + P.transpose());
  for (int i = 0; i < P.rows(); ++i) {
    if (P(i, i) < 0.0) {
      P(i, i) = 0.0;
    }
  }

  const Eigen::MatrixXd base = P;
  double jitter = epsilon;
  for (int attempt = 0; attempt < 8; ++attempt) {
    Eigen::MatrixXd candidate = base;
    candidate += jitter * Eigen::MatrixXd::Identity(P.rows(), P.cols());
    if (covariance_sane(candidate, 1e-6, jitter)) {
      P = std::move(candidate);
      return true;
    }
    jitter *= 10.0;
  }

  return false;
}

}  // namespace sensor_fusion::covariance_checks
