#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "common/angles.h"
#include "common/rng.h"
#include "sensors/eoir/eoir_sensor.h"

namespace sensor_fusion {
namespace {

TruthState make_truth(double x, double y, double z) {
  return TruthState{
      .t = Timestamp::from_seconds(1.0),
      .target_id = 1,
      .position_xyz = {x, y, z},
      .velocity_xyz = {0.0, 0.0, 0.0},
  };
}

EoirConfig base_config() {
  return EoirConfig{
      .sensor_id = SensorId(20),
      .sigma_bearing_rad = 0.0,
      .sigma_elevation_rad = 0.0,
      .p_detect = 1.0,
      .drop_prob = 0.0,
      .latency_mean_s = 0.0,
      .latency_jitter_s = 0.0,
  };
}

}  // namespace

TEST_CASE("eoir: basic measurement shape") {
  EoirSensor sensor(base_config(), Rng(10));
  const auto measurements = sensor.sense({make_truth(10.0, 0.0, 10.0)});

  CHECK(measurements.size() == 1);
  CHECK(measurements[0].measurement_type == MeasurementType::BearingElevation);
  CHECK(measurements[0].z_dim == 2);
  CHECK(measurements[0].z.size() == 2);
  CHECK(std::abs(measurements[0].z[0]) < 1e-12);
  CHECK(std::abs(measurements[0].z[1] - (kPi / 4.0)) < 1e-12);
  CHECK(measurements[0].R_rowmajor.size() == 4);
  CHECK(measurements[0].R_rowmajor[0] == 0.0);
  CHECK(measurements[0].R_rowmajor[3] == 0.0);
}

TEST_CASE("eoir: deterministic seed behavior") {
  auto cfg = base_config();
  cfg.sigma_bearing_rad = deg_to_rad(1.0);
  cfg.sigma_elevation_rad = deg_to_rad(1.0);
  cfg.latency_mean_s = 0.1;
  cfg.latency_jitter_s = 0.05;

  EoirSensor a(cfg, Rng(123));
  EoirSensor b(cfg, Rng(123));

  const std::vector<TruthState> truth = {
      make_truth(10.0, 2.0, 3.0),
      make_truth(20.0, -1.0, 5.0),
  };

  const auto out_a = a.sense(truth);
  const auto out_b = b.sense(truth);

  CHECK(out_a.size() == out_b.size());
  for (size_t i = 0; i < out_a.size(); ++i) {
    CHECK(std::abs(out_a[i].z[0] - out_b[i].z[0]) < 1e-12);
    CHECK(std::abs(out_a[i].z[1] - out_b[i].z[1]) < 1e-12);
    CHECK(std::abs(out_a[i].t_sent.to_seconds() - out_b[i].t_sent.to_seconds()) < 1e-12);
  }
}

TEST_CASE("eoir: drop probability one yields zero") {
  auto cfg = base_config();
  cfg.drop_prob = 1.0;

  EoirSensor sensor(cfg, Rng(55));
  const auto measurements = sensor.sense({make_truth(10.0, 0.0, 5.0)});

  CHECK(measurements.empty());
}

}  // namespace sensor_fusion
