#include "scenario/truth_generator.h"

#include <stdexcept>

namespace sensor_fusion {

TruthGenerator::TruthGenerator(std::vector<TargetInit> targets) {
  targets_.reserve(targets.size());
  for (const auto& target : targets) {
    targets_.push_back(TargetState{
        .target_id = target.target_id,
        .position_xyz = target.position_xyz,
        .velocity_xyz = target.velocity_xyz,
    });
  }
}

std::vector<TruthState> TruthGenerator::step(double dt_seconds) {
  if (dt_seconds < 0.0) {
    throw std::invalid_argument("dt_seconds must be >= 0");
  }

  sim_time_seconds_ += dt_seconds;

  std::vector<TruthState> states;
  states.reserve(targets_.size());

  for (auto& target : targets_) {
    for (size_t i = 0; i < target.position_xyz.size(); ++i) {
      target.position_xyz[i] += target.velocity_xyz[i] * dt_seconds;
    }

    states.push_back(TruthState{
        .t = Timestamp::from_seconds(sim_time_seconds_),
        .target_id = target.target_id,
        .position_xyz = target.position_xyz,
        .velocity_xyz = target.velocity_xyz,
    });
  }

  return states;
}

double TruthGenerator::t_seconds() const {
  return sim_time_seconds_;
}

}  // namespace sensor_fusion
