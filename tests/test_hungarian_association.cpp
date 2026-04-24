#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "fusion_core/association/hungarian.h"
#include "fusion_core/track_manager.h"

namespace {

std::vector<int> greedy_row_order(const std::vector<std::vector<double>>& cost) {
  std::vector<int> assignment(cost.size(), -1);
  std::vector<bool> used(cost.empty() ? 0 : cost[0].size(), false);

  for (size_t i = 0; i < cost.size(); ++i) {
    double best = 1e18;
    int best_j = -1;
    for (size_t j = 0; j < cost[i].size(); ++j) {
      if (used[j]) {
        continue;
      }
      if (cost[i][j] < best) {
        best = cost[i][j];
        best_j = static_cast<int>(j);
      }
    }
    assignment[i] = best_j;
    if (best_j >= 0) {
      used[static_cast<size_t>(best_j)] = true;
    }
  }

  return assignment;
}

sensor_fusion::Measurement make_meas(double range,
                                     double bearing,
                                     double confidence = 0.9,
                                     double range_var = 1.0,
                                     double bearing_var = 0.01,
                                     double t = 0.0) {
  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(t),
      .t_sent = sensor_fusion::Timestamp::from_seconds(t),
      .sensor_id = sensor_fusion::SensorId(1),
      .sensor_type = sensor_fusion::SensorType::Radar,
      .measurement_type = sensor_fusion::MeasurementType::RangeBearing2D,
      .z = {range, bearing},
      .R_rowmajor = {range_var, 0.0, 0.0, bearing_var},
      .z_dim = 2,
      .confidence = confidence,
      .snr = 0.0,
  };
}

}  // namespace

namespace sensor_fusion::fusion_core {

TEST_CASE("hungarian association: global optimum beats row-order greedy") {
  const std::vector<std::vector<double>> cost = {
      {1.0, 2.0},
      {1.1, 100.0},
  };

  const std::vector<int> greedy = greedy_row_order(cost);
  const std::vector<int> global = association::hungarian_solve(cost);

  CHECK(greedy[0] == 0);
  CHECK(greedy[1] == 1);

  CHECK(global[0] == 1);
  CHECK(global[1] == 0);
}

TEST_CASE("track manager: hungarian mode keeps one-to-one unique assignments") {
  TrackManagerConfig cfg;
  cfg.use_hungarian = true;
  cfg.confirm_hits = 10;
  TrackManager manager(cfg);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0), make_meas(200.0, 0.0)});

  manager.predict_all(0.1);
  manager.update_with_measurements(
      {make_meas(101.0, 0.01, 0.9, 2.0, 0.01), make_meas(199.0, -0.01, 0.9, 2.0, 0.01)});

  CHECK(manager.tracks().size() == 2);

  const auto& t0 = manager.tracks()[0];
  const auto& t1 = manager.tracks()[1];
  CHECK(t0.quality().hits == 2);
  CHECK(t1.quality().hits == 2);
  CHECK(t0.quality().misses == 0);
  CHECK(t1.quality().misses == 0);
  CHECK(t0.filter().state()(0) < t1.filter().state()(0));
}

}  // namespace sensor_fusion::fusion_core
