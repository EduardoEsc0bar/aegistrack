#include "agents/interceptor/interceptor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sensor_fusion::agents {

Interceptor::Interceptor(double max_speed_mps,
                         uint64_t interceptor_id,
                         const std::array<double, 3>& start_position)
    : max_speed_mps_(max_speed_mps) {
  if (max_speed_mps_ <= 0.0) {
    throw std::invalid_argument("interceptor max_speed_mps must be > 0");
  }
  state_.interceptor_id = interceptor_id;
  state_.position = start_position;
}

void Interceptor::assign_target(sensor_fusion::TrackId id,
                                const std::array<double, 3>& target_pos) {
  if (state_.target_id != id) {
    state_.engagement_time_s = 0.0;
  }
  state_.engaged = true;
  state_.target_id = id;
  state_.assigned_target_position = target_pos;
  target_pos_ = target_pos;
}

void Interceptor::step(double dt) {
  if (!state_.engaged || dt <= 0.0) {
    return;
  }
  state_.engagement_time_s += dt;

  const std::array<double, 3> los = {
      target_pos_[0] - state_.position[0],
      target_pos_[1] - state_.position[1],
      target_pos_[2] - state_.position[2],
  };

  const double dist = std::sqrt(los[0] * los[0] + los[1] * los[1] + los[2] * los[2]);
  if (dist < 1e-9) {
    return;
  }

  const std::array<double, 3> desired_velocity = {
      (los[0] / dist) * max_speed_mps_,
      (los[1] / dist) * max_speed_mps_,
      (los[2] / dist) * max_speed_mps_,
  };

  constexpr double kNavGain = 2.0;
  for (size_t i = 0; i < 3; ++i) {
    const double accel = kNavGain * (desired_velocity[i] - state_.velocity[i]);
    state_.velocity[i] += accel * dt;
  }

  const double speed = std::sqrt(state_.velocity[0] * state_.velocity[0] +
                                 state_.velocity[1] * state_.velocity[1] +
                                 state_.velocity[2] * state_.velocity[2]);
  if (speed > max_speed_mps_) {
    const double scale = max_speed_mps_ / speed;
    for (size_t i = 0; i < 3; ++i) {
      state_.velocity[i] *= scale;
    }
  }

  for (size_t i = 0; i < 3; ++i) {
    state_.position[i] += state_.velocity[i] * dt;
  }
}

bool Interceptor::has_intercepted(const std::array<double, 3>& target_pos) const {
  const double dx = state_.position[0] - target_pos[0];
  const double dy = state_.position[1] - target_pos[1];
  const double dz = state_.position[2] - target_pos[2];
  const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  return dist < 5.0;
}

const InterceptorState& Interceptor::state() const {
  return state_;
}

void Interceptor::clear_engagement() {
  state_.engaged = false;
  state_.target_id = sensor_fusion::TrackId(0);
  state_.engagement_time_s = 0.0;
}

}  // namespace sensor_fusion::agents
