#include <array>
#include <cmath>
#include <string>

#include <doctest/doctest.h>

#include "decision/engagement_scoring.h"
#include "decision/mission_behavior_tree.h"
#include "observability/jsonl_logger.h"
#include "observability/metrics.h"

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

TrackFact make_fact_at(sensor_fusion::TrackId id,
                       sensor_fusion::fusion_core::TrackStatus status,
                       double confidence,
                       double score,
                       double priority,
                       std::array<double, 3> position) {
  return TrackFact{
      .track_id = id,
      .status = status,
      .position = position,
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

InterceptorFact interceptor_fact(uint64_t interceptor_id,
                                 bool available,
                                 bool engaged,
                                 sensor_fusion::TrackId target_id) {
  return InterceptorFact{
      .interceptor_id = interceptor_id,
      .available = available,
      .engaged = engaged,
      .target_id = target_id,
      .position = {0.0, 0.0, 0.0},
  };
}

InterceptorFact interceptor_fact_at(uint64_t interceptor_id,
                                    bool available,
                                    bool engaged,
                                    sensor_fusion::TrackId target_id,
                                    std::array<double, 3> position) {
  return InterceptorFact{
      .interceptor_id = interceptor_id,
      .available = available,
      .engaged = engaged,
      .target_id = target_id,
      .position = position,
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
      .denial_cooldown_s = 0.35,
      .stable_track_ticks_to_engage = 1,
      .retask_priority_margin = 0.15,
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
  CHECK(result.engagement_commands.size() == 1);
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

  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });
  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.mode == MissionMode::Engage);
  CHECK(result.event.decision_type == "engage_maintain");
  CHECK(result.event.reason == "engagement_active");
  CHECK(result.engagement_commands.empty());
}

TEST_CASE("mission behavior tree: idle interceptor engages second stable target") {
  MissionBlackboard blackboard;
  sensor_fusion::observability::Metrics metrics;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
      .denial_cooldown_s = 0.35,
      .stable_track_ticks_to_engage = 1,
      .retask_priority_margin = 0.15,
  },
      &metrics);

  blackboard.set_engagement(1, sensor_fusion::TrackId(10));
  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact(sensor_fusion::TrackId(10),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.8),
      make_fact(sensor_fusion::TrackId(11),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.7),
  });
  blackboard.set_interceptors({
      interceptor_fact(1, false, true, sensor_fusion::TrackId(10)),
      interceptor_fact(2, true, false, sensor_fusion::TrackId(0)),
  });

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.mode == MissionMode::Engage);
  CHECK(result.engagement_commands.size() == 1);
  if (!result.engagement_commands.empty()) {
    CHECK(result.engagement_commands[0].interceptor_id == 2);
    CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(11));
    CHECK(result.engagement_commands[0].reason == "intercept_aware_idle_assignment");
  }
  CHECK(result.active_engagement_count == 2);
  CHECK(metrics.snapshot().counters.at("bt_idle_interceptor_engage_total") == 1);
}

TEST_CASE("mission behavior tree: retask is avoided when idle interceptor can engage") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
      .denial_cooldown_s = 0.35,
      .stable_track_ticks_to_engage = 1,
      .retask_priority_margin = 0.15,
  });

  blackboard.set_engagement(1, sensor_fusion::TrackId(20));
  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact(sensor_fusion::TrackId(20),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.6),
      make_fact(sensor_fusion::TrackId(21),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.9),
  });
  blackboard.set_interceptors({
      interceptor_fact(1, false, true, sensor_fusion::TrackId(20)),
      interceptor_fact(2, true, false, sensor_fusion::TrackId(0)),
  });

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.engagement_commands.size() == 1);
  if (!result.engagement_commands.empty()) {
    CHECK(result.engagement_commands[0].interceptor_id == 2);
    CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(21));
  }
  bool retask_approved = false;
  for (const auto& event : result.events) {
    if (event.event == "retask_approved") {
      retask_approved = true;
    }
  }
  CHECK(!retask_approved);
}

TEST_CASE("mission behavior tree: duplicate assignment suppression is counted") {
  MissionBlackboard blackboard;
  sensor_fusion::observability::Metrics metrics;
  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  },
      &metrics);

  blackboard.set_engagement(1, sensor_fusion::TrackId(30));
  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(30),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({interceptor_fact(1, false, true, sensor_fusion::TrackId(30))});

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.engagement_commands.empty());
  CHECK(metrics.snapshot().counters.at("bt_duplicate_assignment_suppressed_total") == 1);
}

TEST_CASE("mission behavior tree: closer target wins when priorities are similar") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });

  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact_at(sensor_fusion::TrackId(60),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   0.70, {150.0, 0.0, 0.0}),
      make_fact_at(sensor_fusion::TrackId(61),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   0.72, {4000.0, 0.0, 0.0}),
  });
  blackboard.set_interceptors({available_interceptor()});

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(60));
  CHECK(result.assignment_reason == "intercept_aware_idle_assignment");
}

TEST_CASE("mission behavior tree: high priority can beat closer target") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });

  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact_at(sensor_fusion::TrackId(62),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   0.50, {150.0, 0.0, 0.0}),
      make_fact_at(sensor_fusion::TrackId(63),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   2.0, {4000.0, 0.0, 0.0}),
  });
  blackboard.set_interceptors({available_interceptor()});

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(63));
}

TEST_CASE("mission behavior tree: already engaged target is skipped for idle assignment") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });

  blackboard.set_engagement(1, sensor_fusion::TrackId(64));
  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact_at(sensor_fusion::TrackId(64),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   2.0, {100.0, 0.0, 0.0}),
      make_fact_at(sensor_fusion::TrackId(65),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                   0.8, {250.0, 0.0, 0.0}),
  });
  blackboard.set_interceptors({
      interceptor_fact(1, false, true, sensor_fusion::TrackId(64)),
      interceptor_fact(2, true, false, sensor_fusion::TrackId(0)),
  });

  const BtTickResult result = tree.tick(blackboard);

  CHECK(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].interceptor_id == 2);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(65));
}

TEST_CASE("mission behavior tree: engagement scoring is deterministic") {
  const TrackFact track =
      make_fact_at(sensor_fusion::TrackId(66),
                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.91, 4.0,
                   0.83, {700.0, 100.0, 0.0});
  const InterceptorFact interceptor =
      interceptor_fact_at(3, true, false, sensor_fusion::TrackId(0),
                          {10.0, 25.0, 0.0});
  const EngagementScoringConfig config;

  const EngagementScore first =
      score_engagement_pair(track, interceptor, config, false);
  const EngagementScore second =
      score_engagement_pair(track, interceptor, config, false);

  CHECK(std::abs(first.score - second.score) < 1e-12);
  CHECK(std::abs(first.estimated_intercept_time_s -
                 second.estimated_intercept_time_s) < 1e-12);
}

TEST_CASE("mission behavior tree: denial cooldown prevents repeated denial spam") {
  MissionBlackboard blackboard;
  sensor_fusion::observability::Metrics metrics;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
      .denial_cooldown_s = 0.5,
      .stable_track_ticks_to_engage = 1,
      .retask_priority_margin = 0.15,
  },
      &metrics);

  blackboard.set_tick(1, 0.10);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(20),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.2, 4.0,
                                   0.9)});
  blackboard.set_interceptors({available_interceptor()});
  BtTickResult result = tree.tick(blackboard);
  CHECK(result.event.decision_type == "engage_denied");
  CHECK(result.event.reason == "low_confidence");

  blackboard.set_tick(2, 0.20);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(20),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.2, 4.0,
                                   0.9)});
  result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Track);
  CHECK(result.event.decision_type == "track");
  CHECK(result.event.reason == "engage_cooldown");
  CHECK(result.engagement_commands.empty());
  CHECK(metrics.snapshot().counters.at("bt_engage_cooldown_total") == 1);
}

TEST_CASE("mission behavior tree: engagement waits for required stable ticks") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
      .denial_cooldown_s = 0.35,
      .stable_track_ticks_to_engage = 2,
      .retask_priority_margin = 0.15,
  });

  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(30),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  blackboard.set_interceptors({available_interceptor()});
  BtTickResult result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Track);
  CHECK(result.engagement_commands.empty());

  blackboard.set_tick(2, 0.10);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(30),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Engage);
  CHECK(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(30));
}

TEST_CASE("mission behavior tree: retask only occurs when priority margin is exceeded") {
  MissionBlackboard blackboard;
  MissionBehaviorTree tree(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 5000.0,
      .min_confidence_to_engage = 0.6,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
      .denial_cooldown_s = 0.35,
      .stable_track_ticks_to_engage = 1,
      .retask_priority_margin = 0.15,
      .retask_engagement_score_margin = 0.15,
  });

  blackboard.set_engagement(1, sensor_fusion::TrackId(40));
  blackboard.set_interceptors({InterceptorFact{
      .interceptor_id = 1,
      .available = false,
      .engaged = true,
      .target_id = sensor_fusion::TrackId(40),
      .position = {0.0, 0.0, 0.0},
  }});

  blackboard.set_tick(1, 0.05);
  blackboard.set_tracks({
      make_fact(sensor_fusion::TrackId(40),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.70),
      make_fact(sensor_fusion::TrackId(41),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.80),
  });
  BtTickResult result = tree.tick(blackboard);
  CHECK(result.engagement_commands.empty());
  CHECK(result.event.reason == "retask_denied");

  blackboard.set_tick(2, 0.10);
  blackboard.set_tracks({
      make_fact(sensor_fusion::TrackId(40),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.70),
      make_fact(sensor_fusion::TrackId(41),
                sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0, 0.90),
  });
  result = tree.tick(blackboard);
  CHECK(result.engagement_commands.size() == 1);
  CHECK(result.engagement_commands[0].interceptor_id == 1);
  CHECK(result.engagement_commands[0].track_id == sensor_fusion::TrackId(41));
  CHECK(result.event.reason == "retask_approved");
  CHECK(result.assignment_reason == "intercept_aware_retask");
}

TEST_CASE("mission behavior tree: mode transitions are recorded") {
  MissionBlackboard blackboard;
  sensor_fusion::observability::Metrics metrics;
  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  },
      &metrics);

  blackboard.set_tick(1, 0.05);
  blackboard.set_interceptors({available_interceptor()});
  BtTickResult result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Search);

  blackboard.set_tick(2, 0.10);
  blackboard.set_tracks({make_fact(sensor_fusion::TrackId(50),
                                   sensor_fusion::fusion_core::TrackStatus::Confirmed, 0.95, 4.0,
                                   0.9)});
  result = tree.tick(blackboard);
  CHECK(result.mode == MissionMode::Engage);
  CHECK(metrics.snapshot().counters.at("bt_mode_transition_total") == 1);
  CHECK(!result.events.empty());
  CHECK(result.events.back().event == "mode_transition");
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

  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });
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

  MissionBehaviorTree tree(DecisionConfig{
      .stable_track_ticks_to_engage = 1,
  });
  const BtTickResult result = tree.tick(blackboard);
  const std::string line = sensor_fusion::observability::serialize_bt_decision_json(
      sensor_fusion::Timestamp::from_seconds(0.20), result, 99, 0);

  CHECK(line.find("\"type\":\"bt_decision\"") != std::string::npos);
  CHECK(line.find("\"mode\":\"engage\"") != std::string::npos);
  CHECK(line.find("\"active_engagements\":1") != std::string::npos);
  CHECK(line.find("\"idle_interceptors\":0") != std::string::npos);
  CHECK(line.find("\"selected_track_id\":12") != std::string::npos);
  CHECK(line.find("\"selected_interceptor_id\":1") != std::string::npos);
  CHECK(line.find("\"assignment_reason\":\"intercept_aware_idle_assignment\"") !=
        std::string::npos);
  CHECK(line.find("\"events\"") != std::string::npos);
  CHECK(line.find("\"event\":\"engage_start\"") != std::string::npos);
  CHECK(line.find("\"node\":\"engage.target_ready\"") != std::string::npos);
}

}  // namespace sensor_fusion::decision
