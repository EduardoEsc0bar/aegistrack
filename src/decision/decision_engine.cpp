#include "decision/decision_engine.h"

#include <cmath>

namespace sensor_fusion::decision {

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
  if (track.status() == sensor_fusion::fusion_core::TrackStatus::Tentative) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "acquire",
        .reason = "tentative_wait",
    };
  }

  if (track.status() != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "idle",
        .reason = "track_not_confirmed",
    };
  }

  if (track.quality().score <= config_.engage_score_threshold) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "idle",
        .reason = "score_below_threshold",
    };
  }

  if (track.quality().confidence < config_.min_confidence_to_engage) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "engage_denied",
        .reason = "low_confidence",
    };
  }

  const double range = std::sqrt(target_pos[0] * target_pos[0] + target_pos[1] * target_pos[1] +
                                 target_pos[2] * target_pos[2]);
  if (range < config_.no_engage_zone_radius_m) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "engage_denied",
        .reason = "inside_no_engage_zone",
    };
  }

  if (range > config_.max_engagement_range_m) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "engage_denied",
        .reason = "outside_max_engagement_range",
    };
  }

  if (!interceptor_available) {
    return DecisionEvent{
        .track_id = track.id(),
        .decision_type = "engage_denied",
        .reason = "interceptor_unavailable",
    };
  }

  assign_action(track.id(), target_pos);
  return DecisionEvent{
      .track_id = track.id(),
      .decision_type = "engage",
      .reason = "safety_checks_passed",
  };
}

const DecisionConfig& DecisionEngine::config() const {
  return config_;
}

}  // namespace sensor_fusion::decision
