#include "common/math_checks.h"

#include <cmath>
#include <limits>
#include <numbers>

#include "common/covariance_checks.h"

namespace sensor_fusion::math_checks {

bool is_finite(double value) {
  return std::isfinite(value);
}

bool vector_finite(const Eigen::VectorXd& v) {
  return v.array().isFinite().all();
}

bool matrix_finite(const Eigen::MatrixXd& m) {
  return m.array().isFinite().all();
}

double wrap_angle_pi(double angle_rad) {
  if (!is_finite(angle_rad)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  double wrapped = std::remainder(angle_rad, 2.0 * std::numbers::pi_v<double>);
  if (wrapped <= -std::numbers::pi_v<double>) {
    wrapped += 2.0 * std::numbers::pi_v<double>;
  } else if (wrapped > std::numbers::pi_v<double>) {
    wrapped -= 2.0 * std::numbers::pi_v<double>;
  }
  return wrapped;
}

bool covariance_sane(const Eigen::MatrixXd& P) {
  return sensor_fusion::covariance_checks::covariance_sane(P);
}

bool stabilize_covariance(Eigen::MatrixXd& P, double epsilon) {
  return sensor_fusion::covariance_checks::stabilize_covariance(P, epsilon);
}

}  // namespace sensor_fusion::math_checks
