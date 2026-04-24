#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

#include <doctest/doctest.h>

#include "fusion_core/track_manager.h"
#include "observability/jsonl_logger.h"

namespace {

std::vector<sensor_fusion::fusion_core::Track> run_manager_with_measurements(
    std::vector<sensor_fusion::Measurement> measurements) {
  std::stable_sort(measurements.begin(), measurements.end(),
                   [](const sensor_fusion::Measurement& a, const sensor_fusion::Measurement& b) {
                     if (a.t_sent < b.t_sent) {
                       return true;
                     }
                     if (b.t_sent < a.t_sent) {
                       return false;
                     }
                     return a.t_meas < b.t_meas;
                   });

  sensor_fusion::fusion_core::TrackManager manager(
      sensor_fusion::fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = 9.21,
          .confirm_hits = 2,
          .delete_misses = 5,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = true,
          .unassigned_cost = 1e9,
      });

  if (!measurements.empty()) {
    double prev_fusion_s = measurements.front().t_sent.to_seconds();
    size_t i = 0;
    while (i < measurements.size()) {
      const double current_t_sent_s = measurements[i].t_sent.to_seconds();
      const double dt = std::max(0.0, current_t_sent_s - prev_fusion_s);
      manager.predict_all(dt);

      std::vector<sensor_fusion::Measurement> batch;
      while (i < measurements.size() &&
             std::abs(measurements[i].t_sent.to_seconds() - current_t_sent_s) < 1e-12) {
        batch.push_back(measurements[i]);
        ++i;
      }

      manager.update_with_measurements(batch);
      prev_fusion_s = current_t_sent_s;
    }
  }

  return manager.tracks();
}

sensor_fusion::Measurement make_measurement(double t_sent,
                                            double range,
                                            double bearing,
                                            double confidence = 0.9) {
  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(t_sent - 0.05),
      .t_sent = sensor_fusion::Timestamp::from_seconds(t_sent),
      .sensor_id = sensor_fusion::SensorId(1),
      .sensor_type = sensor_fusion::SensorType::Radar,
      .measurement_type = sensor_fusion::MeasurementType::RangeBearing2D,
      .z = {range, bearing},
      .R_rowmajor = {4.0, 0.0, 0.0, 0.01},
      .z_dim = 2,
      .confidence = confidence,
      .snr = 0.0,
  };
}

}  // namespace

namespace sensor_fusion {

TEST_CASE("replay determinism: parsed JSONL matches live processing") {
  const std::vector<Measurement> input = {
      make_measurement(0.10, 100.0, 0.00),
      make_measurement(0.20, 101.0, 0.01),
      make_measurement(0.30, 102.0, 0.01),
      make_measurement(0.30, 199.0, -0.02),
      make_measurement(0.40, 200.0, -0.01),
      make_measurement(0.50, 201.0, -0.01),
  };

  const std::filesystem::path tmp_path =
      std::filesystem::temp_directory_path() / "sensor_fusion_replay_test.jsonl";

  {
    observability::JsonlLogger logger(tmp_path.string());
    for (const auto& m : input) {
      logger.log_measurement(m);
    }
  }

  const std::vector<Measurement> replay_loaded =
      observability::load_measurements_from_jsonl(tmp_path.string());

  const auto live_tracks = run_manager_with_measurements(input);
  const auto replay_tracks = run_manager_with_measurements(replay_loaded);

  CHECK(live_tracks.size() == replay_tracks.size());

  std::map<uint64_t, fusion_core::Track> live_by_id;
  std::map<uint64_t, fusion_core::Track> replay_by_id;
  for (const auto& track : live_tracks) {
    live_by_id.emplace(track.id().value, track);
  }
  for (const auto& track : replay_tracks) {
    replay_by_id.emplace(track.id().value, track);
  }

  CHECK(live_by_id.size() == replay_by_id.size());

  for (const auto& [id, live_track] : live_by_id) {
    const auto it = replay_by_id.find(id);
    CHECK(it != replay_by_id.end());
    if (it == replay_by_id.end()) {
      continue;
    }
    const auto& replay_track = it->second;

    CHECK(static_cast<int>(live_track.status()) == static_cast<int>(replay_track.status()));
    CHECK(live_track.quality().hits == replay_track.quality().hits);
    CHECK(live_track.quality().misses == replay_track.quality().misses);

    const auto x_live = live_track.filter().state();
    const auto x_replay = replay_track.filter().state();
    CHECK(x_live.size() == x_replay.size());
    for (int i = 0; i < x_live.size(); ++i) {
      CHECK(std::abs(x_live(i) - x_replay(i)) < 1e-9);
    }

    const auto p_live = live_track.filter().covariance();
    const auto p_replay = replay_track.filter().covariance();
    CHECK(p_live.rows() == p_replay.rows());
    CHECK(p_live.cols() == p_replay.cols());
    for (int r = 0; r < p_live.rows(); ++r) {
      for (int c = 0; c < p_live.cols(); ++c) {
        CHECK(std::abs(p_live(r, c) - p_replay(r, c)) < 1e-9);
      }
    }
  }

  std::error_code ec;
  std::filesystem::remove(tmp_path, ec);
}

}  // namespace sensor_fusion
