#include <vector>

#include <doctest/doctest.h>

#include "decision/blackboard.h"

namespace sensor_fusion::decision {

TEST_CASE("blackboard: stores deterministic track and interceptor snapshots") {
  MissionBlackboard blackboard;
  blackboard.set_tick(42, 2.1);
  blackboard.set_tracks({
      TrackFact{
          .track_id = sensor_fusion::TrackId(2),
          .status = sensor_fusion::fusion_core::TrackStatus::Tentative,
          .position = {20.0, 0.0, 0.0},
          .confidence = 0.4,
          .score = 1.0,
          .priority = 0.2,
      },
      TrackFact{
          .track_id = sensor_fusion::TrackId(1),
          .status = sensor_fusion::fusion_core::TrackStatus::Confirmed,
          .position = {10.0, 0.0, 0.0},
          .confidence = 0.9,
          .score = 4.0,
          .priority = 0.8,
      },
  });
  blackboard.set_interceptors({
      InterceptorFact{
          .interceptor_id = 7,
          .available = true,
          .engaged = false,
          .target_id = sensor_fusion::TrackId(0),
          .position = {0.0, 0.0, 0.0},
      },
  });

  const auto snapshot = blackboard.snapshot();
  CHECK(snapshot.tick == 42);
  CHECK(snapshot.time_s == doctest::Approx(2.1));
  CHECK(snapshot.tracks.size() == 2);
  CHECK(snapshot.tracks[0].track_id == sensor_fusion::TrackId(1));
  CHECK(snapshot.interceptors.size() == 1);
  CHECK(snapshot.interceptors[0].interceptor_id == 7);

  const auto selected = select_highest_priority_track(snapshot, true);
  REQUIRE(selected.has_value());
  CHECK(selected->track_id == sensor_fusion::TrackId(1));
}

TEST_CASE("blackboard: sensor stale health is visible to behavior tree") {
  MissionBlackboard blackboard;
  blackboard.set_sensor_health({
      SensorHealthFact{
          .sensor_id = sensor_fusion::SensorId(1),
          .active = true,
          .stale = false,
          .last_seen_age_s = 0.1,
      },
      SensorHealthFact{
          .sensor_id = sensor_fusion::SensorId(2),
          .active = true,
          .stale = true,
          .last_seen_age_s = 3.0,
      },
  });

  CHECK(has_stale_required_sensor(blackboard.snapshot()));
}

}  // namespace sensor_fusion::decision
