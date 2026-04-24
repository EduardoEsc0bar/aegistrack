#pragma once

#include <array>
#include <cstdint>

#include "common/ids.h"

namespace sensor_fusion::agents {

struct InterceptorState {
  uint64_t interceptor_id = 0;
  std::array<double, 3> position{0.0, 0.0, 0.0};
  std::array<double, 3> velocity{0.0, 0.0, 0.0};
  bool engaged = false;
  sensor_fusion::TrackId target_id{0};
  std::array<double, 3> assigned_target_position{0.0, 0.0, 0.0};
  double engagement_time_s = 0.0;
};

class Interceptor {
 public:
  explicit Interceptor(double max_speed_mps,
                       uint64_t interceptor_id = 0,
                       const std::array<double, 3>& start_position = {0.0, 0.0, 0.0});

  void assign_target(sensor_fusion::TrackId id, const std::array<double, 3>& target_pos);
  void step(double dt);

  bool has_intercepted(const std::array<double, 3>& target_pos) const;

  const InterceptorState& state() const;

  void clear_engagement();

 private:
  double max_speed_mps_;
  InterceptorState state_;
  std::array<double, 3> target_pos_{0.0, 0.0, 0.0};
};

}  // namespace sensor_fusion::agents
