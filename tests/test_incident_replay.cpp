#include <filesystem>
#include <fstream>

#include <doctest/doctest.h>

#include "common/time.h"
#include "messages/types.h"
#include "observability/jsonl_logger.h"
#include "tools/incident_replay.h"

namespace sensor_fusion {

TEST_CASE("incident replay matches recorded snapshot") {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "sensor_fusion_incident_replay_test.json";

  Measurement m0{
      .t_meas = Timestamp::from_seconds(0.05),
      .t_sent = Timestamp::from_seconds(0.10),
      .sensor_id = SensorId(1),
      .sensor_type = SensorType::Radar,
      .measurement_type = MeasurementType::RangeBearing2D,
      .z = {100.0, 0.01},
      .R_rowmajor = {4.0, 0.0, 0.0, 0.01},
      .z_dim = 2,
      .confidence = 0.9,
      .snr = 0.0,
      .trace_id = 101,
      .causal_parent_id = 0,
  };
  Measurement m1 = m0;
  m1.t_meas = Timestamp::from_seconds(0.15);
  m1.t_sent = Timestamp::from_seconds(0.20);
  m1.z = {101.0, 0.01};
  m1.trace_id = 102;

  Measurement m2 = m0;
  m2.t_meas = Timestamp::from_seconds(0.25);
  m2.t_sent = Timestamp::from_seconds(0.30);
  m2.z = {102.0, 0.01};
  m2.trace_id = 103;

  {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    CHECK(out.is_open());
    out << observability::serialize_measurement_json(m0) << "\n";
    out << observability::serialize_measurement_json(m1) << "\n";
    out << observability::serialize_measurement_json(m2) << "\n";
  }

  const auto baseline = tools::replay_incident_file(path.string(), tools::IncidentReplayOptions{});

  {
    std::ofstream out(path, std::ios::out | std::ios::app);
    CHECK(out.is_open());
    for (const auto& track : baseline.tracks) {
      out << tools::serialize_replay_snapshot_track(track) << "\n";
    }
    out << tools::serialize_replay_snapshot_interceptor(baseline.interceptor_state) << "\n";
  }

  const auto replayed = tools::replay_incident_file(path.string(), tools::IncidentReplayOptions{});
  CHECK(replayed.had_snapshot);
  CHECK(replayed.snapshot_match);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace sensor_fusion
