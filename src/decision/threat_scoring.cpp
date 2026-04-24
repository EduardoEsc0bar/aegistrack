#include "decision/threat_scoring.h"

#include <algorithm>
#include <cmath>

namespace sensor_fusion::decision {
namespace {

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

}  // namespace

double compute_threat_score(const sensor_fusion::fusion_core::Track& track,
                            const ThreatScoringConfig& cfg) {
  const auto x = track.filter().state();

  const double dx = x(0) - cfg.protected_zone[0];
  const double dy = x(1) - cfg.protected_zone[1];
  const double dz = x(2) - cfg.protected_zone[2];
  const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  const double proximity = 1.0 - clamp01(distance / std::max(1.0, cfg.max_distance_m));

  const double speed = std::sqrt(x(3) * x(3) + x(4) * x(4) + x(5) * x(5));
  const double speed_term = clamp01(speed / std::max(1.0, cfg.max_speed_mps));

  const double confidence_term = clamp01(track.quality().confidence > 0.0
                                             ? track.quality().confidence
                                             : (track.quality().hits > 0
                                                    ? track.quality().score /
                                                          static_cast<double>(
                                                              std::max<uint32_t>(1,
                                                                                 track.quality().hits))
                                                    : 0.0));

  const double to_zone_norm = std::sqrt(dx * dx + dy * dy + dz * dz);
  const double vel_norm = std::sqrt(x(3) * x(3) + x(4) * x(4) + x(5) * x(5));
  double heading = 0.0;
  if (to_zone_norm > 1e-9 && vel_norm > 1e-9) {
    const double ux = -dx / to_zone_norm;
    const double uy = -dy / to_zone_norm;
    const double uz = -dz / to_zone_norm;
    const double vx = x(3) / vel_norm;
    const double vy = x(4) / vel_norm;
    const double vz = x(5) / vel_norm;
    heading = clamp01((ux * vx + uy * vy + uz * vz + 1.0) * 0.5);
  }

  const double score = cfg.w_distance * proximity + cfg.w_speed * speed_term +
                       cfg.w_confidence * confidence_term + cfg.w_heading * heading;
  return clamp01(score);
}

}  // namespace sensor_fusion::decision
