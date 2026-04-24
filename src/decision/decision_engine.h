#pragma once

#include <array>
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
