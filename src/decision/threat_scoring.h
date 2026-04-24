#pragma once

#include <array>

#include "fusion_core/track.h"

namespace sensor_fusion::decision {

struct ThreatScoringConfig {
  std::array<double, 3> protected_zone{0.0, 0.0, 0.0};
  double max_distance_m = 5000.0;
  double max_speed_mps = 400.0;
  double w_distance = 0.4;
  double w_speed = 0.25;
  double w_confidence = 0.25;
  double w_heading = 0.1;
};

double compute_threat_score(const sensor_fusion::fusion_core::Track& track,
                            const ThreatScoringConfig& cfg = {});

}  // namespace sensor_fusion::decision
