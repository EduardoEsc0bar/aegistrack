#pragma once

#include <Eigen/Dense>

namespace sensor_fusion::covariance_checks {

bool covariance_sane(const Eigen::MatrixXd& P,
                     double symmetry_tolerance = 1e-6,
                     double jitter = 1e-9);

bool stabilize_covariance(Eigen::MatrixXd& P, double epsilon = 1e-6);

}  // namespace sensor_fusion::covariance_checks
