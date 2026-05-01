#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "fusion_core/track.h"

namespace sensor_fusion::decision {

struct DecisionConfig {
  double engage_score_threshold = 3.0;
  double max_engagement_range_m = 5000.0;
  double min_confidence_to_engage = 0.6;
  double no_engage_zone_radius_m = 0.0;
  double engagement_timeout_s = 10.0;
  double denial_cooldown_s = 0.35;
  uint32_t stable_track_ticks_to_engage = 2;
  double retask_priority_margin = 0.15;
  double engagement_priority_weight = 1.0;
  double engagement_confidence_weight = 0.5;
  double engagement_intercept_time_weight = 0.05;
  double engagement_distance_weight = 0.05;
  double engagement_distance_normalizer_m = 5000.0;
  double default_interceptor_speed_mps = 150.0;
  double retask_engagement_score_margin = 0.25;
};

struct DecisionEvent {
  sensor_fusion::TrackId track_id{0};
  std::string decision_type;
  std::string reason;
};

class DecisionEngine {
 public:
  explicit DecisionEngine(double engage_score_threshold);
  explicit DecisionEngine(DecisionConfig config);

  DecisionEvent decide(
      const sensor_fusion::fusion_core::Track& track,
      bool interceptor_available,
      const std::array<double, 3>& target_pos,
      const std::function<void(sensor_fusion::TrackId, const std::array<double, 3>&)>&
          assign_action) const;

  const DecisionConfig& config() const;

 private:
  DecisionConfig config_;
};

}  // namespace sensor_fusion::decision
