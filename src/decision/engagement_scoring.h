#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "decision/blackboard.h"

namespace sensor_fusion::decision {

struct EngagementScoringConfig {
  double priority_weight = 1.0;
  double confidence_weight = 0.5;
  double intercept_time_weight = 0.05;
  double distance_weight = 0.05;
  double distance_normalizer_m = 5000.0;
  double default_interceptor_speed_mps = 150.0;
  double already_engaged_penalty = 1000.0;
};

struct EngagementScore {
  double score = 0.0;
  double distance_m = 0.0;
  double estimated_intercept_time_s = 0.0;
};

inline double distance_m(const std::array<double, 3>& lhs,
                         const std::array<double, 3>& rhs) {
  const double dx = lhs[0] - rhs[0];
  const double dy = lhs[1] - rhs[1];
  const double dz = lhs[2] - rhs[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline EngagementScore score_engagement_pair(const TrackFact& track,
                                             const InterceptorFact& interceptor,
                                             const EngagementScoringConfig& config,
                                             bool already_engaged) {
  const double distance = distance_m(track.position, interceptor.position);
  const double configured_speed = interceptor.speed_mps > 0.0
                                      ? interceptor.speed_mps
                                      : config.default_interceptor_speed_mps;
  const double speed = std::max(configured_speed, 1.0);
  const double estimated_intercept_time_s = distance / speed;
  const double normalized_distance =
      distance / std::max(config.distance_normalizer_m, 1.0);
  double score = config.priority_weight * track.priority +
                 config.confidence_weight * track.confidence -
                 config.intercept_time_weight * estimated_intercept_time_s -
                 config.distance_weight * normalized_distance;
  if (already_engaged) {
    score -= config.already_engaged_penalty;
  }
  return EngagementScore{
      .score = score,
      .distance_m = distance,
      .estimated_intercept_time_s = estimated_intercept_time_s,
  };
}

}  // namespace sensor_fusion::decision
