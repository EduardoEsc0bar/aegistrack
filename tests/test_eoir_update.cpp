#include <cmath>

#include <doctest/doctest.h>
#include <Eigen/Dense>

#include "common/angles.h"
#include "fusion_core/ekf_cv.h"
#include "fusion_core/track_manager.h"

namespace {

sensor_fusion::Measurement make_eoir_meas(double bearing,
                                          double elevation,
                                          double confidence = 0.9,
                                          double bearing_var = 1e-3,
                                          double elevation_var = 1e-3,
                                          double t = 0.1) {
  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(t),
      .t_sent = sensor_fusion::Timestamp::from_seconds(t),
      .sensor_id = sensor_fusion::SensorId(2),
      .sensor_type = sensor_fusion::SensorType::EOIR,
      .measurement_type = sensor_fusion::MeasurementType::BearingElevation,
      .z = {bearing, elevation},
      .R_rowmajor = {bearing_var, 0.0, 0.0, elevation_var},
      .z_dim = 2,
      .confidence = confidence,
      .snr = 0.0,
  };
}

}  // namespace

namespace sensor_fusion {

TEST_CASE("eoir update: reduces angular error") {
  EkfCv ekf;
  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 10.0;
  x0(1) = 2.0;
  x0(2) = 1.0;
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 50.0;
  ekf.initialize(x0, P0);

  const double true_bearing = 0.0;
  const double true_elevation = 0.0;

  const auto x_before = ekf.state();
  const double err_before =
      std::abs(wrap_angle_pi(std::atan2(x_before(1), x_before(0)) - true_bearing));

  Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
  R(0, 0) = 1e-4;
  R(1, 1) = 1e-4;
  ekf.update_bearing_elevation(true_bearing, true_elevation, R);

  const auto x_after = ekf.state();
  const double err_after =
      std::abs(wrap_angle_pi(std::atan2(x_after(1), x_after(0)) - true_bearing));

  CHECK(err_after < err_before);
}

TEST_CASE("track manager: no EOIR-only track creation") {
  fusion_core::TrackManager manager(
      fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = 9.21,
          .gate_mahal_eoir = 9.21,
          .confirm_hits = 3,
          .delete_misses = 5,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = true,
          .enable_eoir_updates = true,
          .unassigned_cost = 1e9,
      });

  manager.predict_all(0.1);
  manager.update_with_measurements({make_eoir_meas(0.1, 0.05, 1.0)});

  CHECK(manager.tracks().empty());
}

TEST_CASE("eoir update: radar plus eoir reduces px/py covariance") {
  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 100.0;
  x0(1) = 10.0;
  x0(2) = 20.0;

  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 100.0;

  EkfCv ekf_radar;
  ekf_radar.initialize(x0, P0);

  EkfCv ekf_both;
  ekf_both.initialize(x0, P0);

  Eigen::Matrix2d radar_R = Eigen::Matrix2d::Zero();
  radar_R(0, 0) = 4.0;
  radar_R(1, 1) = 0.01;

  ekf_radar.update_range_bearing(100.0, 0.0, radar_R);
  ekf_both.update_range_bearing(100.0, 0.0, radar_R);

  Eigen::Matrix2d eoir_R = Eigen::Matrix2d::Zero();
  eoir_R(0, 0) = 0.01;
  eoir_R(1, 1) = 0.01;
  ekf_both.update_bearing_elevation(0.0, 0.0, eoir_R);

  const auto p_radar = ekf_radar.covariance();
  const auto p_both = ekf_both.covariance();

  CHECK(p_both(0, 0) <= p_radar(0, 0));
  CHECK(p_both(1, 1) <= p_radar(1, 1));
}

}  // namespace sensor_fusion
