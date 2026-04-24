#pragma once

#include <Eigen/Dense>

namespace sensor_fusion::observability {
class Metrics;
}

namespace sensor_fusion {

class EkfCv {
 public:
  EkfCv();

  void initialize(const Eigen::VectorXd& x0, const Eigen::MatrixXd& P0);
  void set_metrics(sensor_fusion::observability::Metrics* metrics);

  void predict(double dt);

  bool update_range_bearing(double range, double bearing, const Eigen::Matrix2d& R);
  bool update_bearing_elevation(double bearing,
                                double elevation,
                                const Eigen::Matrix2d& R);

  Eigen::VectorXd state() const;
  Eigen::MatrixXd covariance() const;

  bool is_initialized() const;

 private:
  bool apply_measurement_update(const Eigen::Vector2d& innovation,
                                const Eigen::Matrix<double, 2, 6>& H,
                                const Eigen::Matrix2d& R);
  void record_skipped_update(bool nonfinite);
  void record_covariance_stabilized();
  void record_nonfinite_detected();

  Eigen::VectorXd x_;
  Eigen::MatrixXd P_;
  bool initialized_;
  sensor_fusion::observability::Metrics* metrics_;
};

}  // namespace sensor_fusion
