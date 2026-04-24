#include <cmath>

#include <doctest/doctest.h>
#include <Eigen/Dense>

#include "common/angles.h"
#include "common/fixed_rate_loop.h"
#include "common/rng.h"
#include "fusion_core/ekf_cv.h"
#include "scenario/truth_generator.h"
#include "sensors/radar/radar_sensor.h"

namespace sensor_fusion {

TEST_CASE("ekf: predict only motion") {
  EkfCv ekf;

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(3) = 1.0;
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6);

  ekf.initialize(x0, P0);
  ekf.predict(1.0);

  CHECK(std::abs(ekf.state()(0) - 1.0) < 1e-12);
}

TEST_CASE("ekf: update improves estimate") {
  EkfCv ekf;

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 90.0;
  x0(1) = 20.0;
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 100.0;

  ekf.initialize(x0, P0);

  const double truth_x = 100.0;
  const double truth_y = 0.0;
  const double before = std::sqrt(std::pow(ekf.state()(0) - truth_x, 2) +
                                  std::pow(ekf.state()(1) - truth_y, 2));

  Eigen::Matrix2d R = Eigen::Matrix2d::Identity() * 0.01;
  ekf.update_range_bearing(100.0, 0.0, R);

  const double after = std::sqrt(std::pow(ekf.state()(0) - truth_x, 2) +
                                 std::pow(ekf.state()(1) - truth_y, 2));
  CHECK(after < before);
}

TEST_CASE("ekf: covariance remains symmetric") {
  EkfCv ekf;

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 50.0;
  x0(1) = 5.0;
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 10.0;
  ekf.initialize(x0, P0);

  Eigen::Matrix2d R = Eigen::Matrix2d::Identity() * 0.5;

  for (int i = 0; i < 8; ++i) {
    ekf.predict(0.1);
    ekf.update_range_bearing(50.0 + 0.2 * i, 0.1, R);
  }

  const Eigen::MatrixXd asymmetry = ekf.covariance() - ekf.covariance().transpose();
  CHECK(asymmetry.norm() < 1e-8);
}

namespace {

Eigen::VectorXd run_deterministic_once(uint64_t seed) {
  TruthGenerator generator({TruthGenerator::TargetInit{
      .target_id = 1,
      .position_xyz = {100.0, 0.0, 0.0},
      .velocity_xyz = {1.0, 0.5, 0.0},
  }});

  RadarConfig cfg{
      .sensor_id = SensorId(1),
      .hz = 10.0,
      .max_range_m = 10000.0,
      .fov_deg = 120.0,
      .sigma_range_m = 0.5,
      .sigma_bearing_rad = deg_to_rad(0.5),
      .p_detect = 1.0,
      .p_false_alarm = 0.0,
      .drop_prob = 0.0,
      .latency_mean_s = 0.0,
      .latency_jitter_s = 0.0,
  };

  RadarSensor radar(cfg, Rng(seed));
  FixedRateLoop loop(10.0);
  EkfCv ekf;

  Eigen::Matrix2d R;
  R << cfg.sigma_range_m * cfg.sigma_range_m, 0.0, 0.0,
      cfg.sigma_bearing_rad * cfg.sigma_bearing_rad;

  for (int i = 0; i < 20; ++i) {
    loop.next_tick();
    const auto truths = generator.step(loop.tick_dt());
    ekf.predict(loop.tick_dt());

    const auto measurements = radar.sense(truths);
    for (const auto& measurement : measurements) {
      if (!ekf.is_initialized()) {
        Eigen::VectorXd init_state = Eigen::VectorXd::Zero(6);
        const double range = measurement.z[0];
        const double bearing = measurement.z[1];
        init_state(0) = range * std::cos(bearing);
        init_state(1) = range * std::sin(bearing);

        Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6) * 1000.0;
        ekf.initialize(init_state, P0);
      } else {
        ekf.update_range_bearing(measurement.z[0], measurement.z[1], R);
      }
    }
  }

  return ekf.state();
}

}  // namespace

TEST_CASE("ekf: deterministic with same seed") {
  const Eigen::VectorXd s1 = run_deterministic_once(77);
  const Eigen::VectorXd s2 = run_deterministic_once(77);

  CHECK(s1.size() == s2.size());
  for (int i = 0; i < s1.size(); ++i) {
    CHECK(std::abs(s1(i) - s2(i)) < 1e-12);
  }
}

}  // namespace sensor_fusion
