#pragma once

#include <Eigen/Dense>

namespace sensor_fusion::math_checks {

bool is_finite(double value);
bool vector_finite(const Eigen::VectorXd& v);
bool matrix_finite(const Eigen::MatrixXd& m);

double wrap_angle_pi(double angle_rad);

bool covariance_sane(const Eigen::MatrixXd& P);
bool stabilize_covariance(Eigen::MatrixXd& P, double epsilon = 1e-6);

}  // namespace sensor_fusion::math_checks
