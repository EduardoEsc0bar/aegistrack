#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "common/angles.h"
#include "common/rng.h"
#include "messages/types.h"
#include "sensors/radar/radar_sensor.h"

namespace sensor_fusion {
namespace {

TruthState make_truth(double x, double y, double z = 0.0) {
  return TruthState{
      .t = Timestamp::from_seconds(1.0),
      .target_id = 1,
      .position_xyz = {x, y, z},
      .velocity_xyz = {0.0, 0.0, 0.0},
  };
}

RadarConfig base_config() {
  return RadarConfig{
      .sensor_id = SensorId(10),
      .hz = 20.0,
      .max_range_m = 1000.0,
      .fov_deg = 120.0,
      .sigma_range_m = 0.0,
      .sigma_bearing_rad = 0.0,
      .p_detect = 1.0,
      .p_false_alarm = 0.0,
      .drop_prob = 0.0,
      .latency_mean_s = 0.0,
      .latency_jitter_s = 0.0,
  };
}

}  // namespace

TEST_CASE("radar: basic measurement shape") {
  auto cfg = base_config();
  RadarSensor radar(cfg, Rng(7));

  const auto measurements = radar.sense({make_truth(100.0, 0.0)});

  CHECK(measurements.size() == 1);
  CHECK(measurements[0].measurement_type == MeasurementType::RangeBearing2D);
  CHECK(measurements[0].z_dim == 2);
  CHECK(measurements[0].z.size() == 2);
  CHECK(measurements[0].z[0] == 100.0);
  CHECK(std::abs(measurements[0].z[1]) < 1e-12);
  CHECK(measurements[0].R_rowmajor.size() == 4);
  CHECK(measurements[0].R_rowmajor[0] == 0.0);
  CHECK(measurements[0].R_rowmajor[1] == 0.0);
  CHECK(measurements[0].R_rowmajor[2] == 0.0);
  CHECK(measurements[0].R_rowmajor[3] == 0.0);
}

TEST_CASE("radar: fov gating") {
  auto cfg = base_config();
  cfg.fov_deg = 60.0;
  RadarSensor radar(cfg, Rng(42));

  const auto measurements = radar.sense({make_truth(0.0, 100.0)});

  CHECK(measurements.empty());
}

TEST_CASE("radar: deterministic rng") {
  auto cfg = base_config();
  cfg.sigma_range_m = 2.0;
  cfg.sigma_bearing_rad = deg_to_rad(1.5);
  cfg.latency_mean_s = 0.1;
  cfg.latency_jitter_s = 0.02;

  RadarSensor radar_a(cfg, Rng(999));
  RadarSensor radar_b(cfg, Rng(999));

  const std::vector<TruthState> truth = {
      make_truth(100.0, 5.0),
      make_truth(150.0, -10.0),
  };

  const auto measurements_a = radar_a.sense(truth);
  const auto measurements_b = radar_b.sense(truth);

  CHECK(measurements_a.size() == measurements_b.size());
  for (size_t i = 0; i < measurements_a.size(); ++i) {
    CHECK(measurements_a[i].z_dim == measurements_b[i].z_dim);
    CHECK(measurements_a[i].z.size() == measurements_b[i].z.size());
    CHECK(std::abs(measurements_a[i].t_meas.to_seconds() - measurements_b[i].t_meas.to_seconds()) <
          1e-12);
    CHECK(std::abs(measurements_a[i].t_sent.to_seconds() - measurements_b[i].t_sent.to_seconds()) <
          1e-12);
    for (size_t j = 0; j < measurements_a[i].z.size(); ++j) {
      CHECK(std::abs(measurements_a[i].z[j] - measurements_b[i].z[j]) < 1e-12);
    }
  }
}

TEST_CASE("radar: drop probability extreme") {
  auto cfg = base_config();
  cfg.drop_prob = 1.0;

  RadarSensor radar(cfg, Rng(1));
  const auto measurements = radar.sense({make_truth(100.0, 0.0)});

  CHECK(measurements.empty());
}

}  // namespace sensor_fusion
