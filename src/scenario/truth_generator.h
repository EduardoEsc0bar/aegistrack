#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "messages/types.h"

namespace sensor_fusion {

class TruthGenerator {
 public:
  struct TargetInit {
    uint64_t target_id;
    std::array<double, 3> position_xyz;
    std::array<double, 3> velocity_xyz;
  };

  explicit TruthGenerator(std::vector<TargetInit> targets);

  std::vector<TruthState> step(double dt_seconds);

  double t_seconds() const;

 private:
  struct TargetState {
    uint64_t target_id;
    std::array<double, 3> position_xyz;
    std::array<double, 3> velocity_xyz;
  };

  double sim_time_seconds_{0.0};
  std::vector<TargetState> targets_;
};

}  // namespace sensor_fusion
