#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "agents/interceptor/interceptor.h"

namespace sensor_fusion::decision {

struct AssignmentTrack {
  sensor_fusion::TrackId track_id{0};
  std::array<double, 3> position{0.0, 0.0, 0.0};
  double threat_score = 0.0;
};

enum class AssignmentAction { Assigned, Retasked };

struct AssignmentDecision {
  uint64_t interceptor_id = 0;
  sensor_fusion::TrackId track_id{0};
  AssignmentAction action = AssignmentAction::Assigned;
  double cost = 0.0;
};

struct AssignmentConfig {
  bool allow_multi_engage = false;
  bool retask_enable = true;
  double commit_distance_m = 50.0;
};

std::vector<AssignmentDecision> compute_assignments(
    const std::vector<sensor_fusion::agents::InterceptorState>& interceptors,
    const std::vector<AssignmentTrack>& tracks,
    const AssignmentConfig& cfg);

}  // namespace sensor_fusion::decision
