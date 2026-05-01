#include <string>

#include <doctest/doctest.h>

#include "decision/mission_behavior_tree.h"
#include "observability/jsonl_logger.h"

namespace sensor_fusion::decision {
namespace {

TrackFact make_fact(sensor_fusion::TrackId id,
                    sensor_fusion::fusion_core::TrackStatus status,
                    double confidence,
                    double score,
                    double priority) {
  return TrackFact{
      .track_id = id,
      .status = status,
      .position = {100.0, 0.0, 0.0},
      .confidence = confidence,
      .score = score,
      .priority = priority,
  };
}

InterceptorFact available_interceptor() {
  return InterceptorFact{
      .interceptor_id = 1,
      .available = true,
      .engaged = false,
      .target_id = sensor_fusion::TrackId(0),
      .position = {0.0, 0.0, 0.0},
  };
}

}  // namespace

TEST_CASE("mission behavior tree: no tracks enters search") {
  MissionBlackboard blackboard;
  blackboard.set_tick(1, 0.05);
  blackboard.set_interceptors({available_interceptor()});

  MissionBehaviorTree tree(DecisionConfig{});
  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.mode == MissionMode::Search);
  CHECK(result.event.decision_type == "search");
  CHECK(result.engagement_commands.empty());
  CHECK(!result.node_trace.empty());
}

TEST_CASE("mission behavior tree: target appears then tracks then engages") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
  });

  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(10),
                                   sensor_fusion::fusion_core::TrackStatus::Tentative, 0.8, 1.0,
                                   0.4)});
  blackboard.set_interceptors({available_interceptor()});
  BtTickResult result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Track);
  CHECK(result.event.decision_type == "acquire");
  CHECK(result.engagement_commands.empty());

  blackboard.set_tick(2, 0.10);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(10),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({available_interceptor()});
  result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Engage);
  CHECK(result.event.decision_type == "engage");
  REQUIRE(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].interceptor_id == 1);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(10));
  CHECK(blackboard.snapshot().engagement.active);
}

TEST_CASE("mission behavior tree: active engagement is maintained without duplicate assignment") {
  MissionBlackboard blackboard;
  blackboard.set_tick(3, 0.15);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(10),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({InterceptorFact{
      .interceptor_id = 1,
      .available = false,
      .engaged = true,
      .target_id = sensor_fusion::TrackId(10),
      .position = {0.0, 0.0, 0.0},
  }});
  blackboard.set_engagement(1, sensor_fusion::TrackId(10));

  MissionBehaviorTree tree(DecisionConfig{});
  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.mode == MissionMode::Engage);
  CHECK(result.event.decision_type == "engage");
  CHECK(result.event.reason == "engagement_active");
  CHECK(result.engagement_commands.empty());
}

TEST_CASE("mission behavior tree: degraded sensor denies engagement") {
  MissionBlackboard blackboard;
  blackboard.set_tick(3, 0.15);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(11),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({available_interceptor()});
  blackboard.set_sensor_health({SensorHealthFact{
      .sensor_id = sensor_fusion::SensorId(1),
      .active = true,
      .stale = true,
      .last_seen_age_s = 3.0,
  }});

  MissionBehaviorTree tree(DecisionConfig{});
  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.mode == MissionMode::Track);
  CHECK(result.event.decision_type == "engage_denied");
  CHECK(result.event.reason == "sensor_health_degraded");
  CHECK(result.engagement_commands.empty());
}

TEST_CASE("mission behavior tree: JSONL logging includes node outcomes") {
  MissionBlackboard blackboard;
  blackboard.set_tick(4, 0.20);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(12),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({available_interceptor()});

  MissionBehaviorTree tree(DecisionConfig{});
  const BtTickResult result = tree.tick(blackboard);
  const std::string line = sensor_fusion::observability::serialize_bt_decision_json(
      sensor_fusion::Timestamp::from_seconds(0.20), result, 99, 0);

  CHECK(line.find("\"type\":\"bt_decision\"") != std::string::npos);
  CHECK(line.find("\"mode\":\"engage\"") != std::string::npos);
  CHECK(line.find("\"node\":\"engage.target_ready\"") != std::string::npos);
}

}  // namespace sensor_fusion::decision
