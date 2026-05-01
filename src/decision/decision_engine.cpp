#include "decision/decision_engine.h"

#include "decision/blackboard.h"
#include "decision/mission_behavior_tree.h"

namespace sensor_fusion::decision {
namespace {

TrackFact make_track_fact(const sensor_fusion::fusion_core::Track& track,
                          const std::array<double, 3>& target_pos) {
  return TrackFact{
      .track_id = track.id(),
      .status = track.status(),
      .position = target_pos,
      .confidence = track.quality().confidence,
      .score = track.quality().score,
      .priority = track.quality().score,
  };
}

}  // namespace

DecisionEngine::DecisionEngine(double engage_score_threshold)
    : config_(DecisionConfig{
          .engage_score_threshold = engage_score_threshold,
          .max_engagement_range_m = 5000.0,
          .min_confidence_to_engage = 0.6,
          .no_engage_zone_radius_m = 0.0,
          .engagement_timeout_s = 10.0,
      }) {}

DecisionEngine::DecisionEngine(DecisionConfig config) : config_(config) {}

DecisionEvent DecisionEngine::decide(
    const sensor_fusion::fusion_core::Track& track,
    bool interceptor_available,
    const std::array<double, 3>& target_pos,
    const std::function<void(sensor_fusion::TrackId, const std::array<double, 3>&)>&
        assign_action) const {
  MissionBlackboard blackboard;
  blackboard.set_tracks({make_track_fact(track, target_pos)});
  blackboard.set_interceptors({InterceptorFact{
      .interceptor_id = 1,
      .available = interceptor_available,
      .engaged = !interceptor_available,
      .target_id = sensor_fusion::TrackId(0),
      .position = {0.0, 0.0, 0.0},
  }});

  DecisionConfig single_tick_config = config_;
  single_tick_config.stable_track_ticks_to_engage = 1;
  MissionBehaviorTree tree(single_tick_config);
  const BtTickResult result = tree.tick(blackboard);
  for (const auto& command : result.engagement_commands) {
    assign_action(command.track_id, command.target_position);
  }
  return result.event;
}

const DecisionConfig& DecisionEngine::config() const {
  return config_;
}

}  // namespace sensor_fusion::decision
