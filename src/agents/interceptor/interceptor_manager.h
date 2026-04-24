#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "agents/interceptor/interceptor.h"

namespace sensor_fusion::agents {

struct InterceptorConfig {
  uint64_t interceptor_id;
  double max_speed_mps;
  std::array<double, 3> start_position;
};

class InterceptorManager {
 public:
  explicit InterceptorManager(std::vector<InterceptorConfig> configs);

  void set_engagement_timeout_s(double timeout_s);

  void update_track_positions(
      const std::vector<std::pair<sensor_fusion::TrackId, std::array<double, 3>>>&
          track_positions);

  void step(double dt);

  void assign(const std::vector<std::pair<uint64_t, sensor_fusion::TrackId>>& assignments);

  const std::vector<InterceptorState>& states() const;

  void handle_intercepts(const std::vector<std::pair<uint64_t, sensor_fusion::TrackId>>& hits);

 private:
  struct Entry {
    uint64_t interceptor_id = 0;
    Interceptor interceptor;

    Entry(uint64_t id, Interceptor effector)
        : interceptor_id(id), interceptor(std::move(effector)) {}
  };

  std::vector<Entry> entries_;
  std::unordered_map<uint64_t, size_t> index_by_id_;
  std::unordered_map<uint64_t, std::array<double, 3>> track_positions_;

  mutable std::vector<InterceptorState> cached_states_;
  double engagement_timeout_s_ = 10.0;
};

}  // namespace sensor_fusion::agents
