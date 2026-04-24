#pragma once

#include <Eigen/Dense>

#include "fusion_core/ekf_cv.h"

namespace sensor_fusion::fusion_core {

struct GateResult {
  bool pass;
  double mahalanobis2;
  bool numerical_issue = false;
};

GateResult gate_range_bearing(const sensor_fusion::EkfCv& ekf,
                              double meas_range,
                              double meas_bearing,
                              const Eigen::Matrix2d& R,
                              double gate_threshold_mahalanobis2);

GateResult gate_bearing_elevation(const sensor_fusion::EkfCv& ekf,
                                  double meas_bearing,
                                  double meas_elevation,
                                  const Eigen::Matrix2d& R,
                                  double gate_threshold_mahalanobis2);

}  // namespace sensor_fusion::fusion_core
