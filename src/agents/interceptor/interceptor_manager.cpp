#include "agents/interceptor/interceptor_manager.h"

#include <algorithm>

namespace sensor_fusion::agents {

InterceptorManager::InterceptorManager(std::vector<InterceptorConfig> configs) {
  std::sort(configs.begin(), configs.end(),
            [](const InterceptorConfig& a, const InterceptorConfig& b) {
              return a.interceptor_id < b.interceptor_id;
            });

  entries_.reserve(configs.size());
  for (const auto& config : configs) {
    index_by_id_.emplace(config.interceptor_id, entries_.size());
    entries_.emplace_back(
        config.interceptor_id,
        Interceptor(config.max_speed_mps, config.interceptor_id, config.start_position));
  }
}

void InterceptorManager::set_engagement_timeout_s(double timeout_s) {
  if (timeout_s > 0.0) {
    engagement_timeout_s_ = timeout_s;
  }
}

void InterceptorManager::update_track_positions(
    const std::vector<std::pair<sensor_fusion::TrackId, std::array<double, 3>>>& track_positions) {
  track_positions_.clear();
  for (const auto& [track_id, pos] : track_positions) {
    track_positions_[track_id.value] = pos;
  }
}

void InterceptorManager::step(double dt) {
  for (auto& entry : entries_) {
    auto& interceptor = entry.interceptor;
    const auto state = interceptor.state();

    if (state.engaged) {
      const auto it = track_positions_.find(state.target_id.value);
      if (it == track_positions_.end()) {
        interceptor.clear_engagement();
      } else {
        interceptor.assign_target(state.target_id, it->second);
        interceptor.step(dt);
        if (interceptor.state().engagement_time_s > engagement_timeout_s_) {
          interceptor.clear_engagement();
        }
      }
    }
  }
}

void InterceptorManager::assign(
    const std::vector<std::pair<uint64_t, sensor_fusion::TrackId>>& assignments) {
  for (const auto& [interceptor_id, track_id] : assignments) {
    const auto id_it = index_by_id_.find(interceptor_id);
    if (id_it == index_by_id_.end()) {
      continue;
    }
    const auto track_it = track_positions_.find(track_id.value);
    if (track_it == track_positions_.end()) {
      continue;
    }

    entries_[id_it->second].interceptor.assign_target(track_id, track_it->second);
  }
}

const std::vector<InterceptorState>& InterceptorManager::states() const {
  cached_states_.clear();
  cached_states_.reserve(entries_.size());
  for (const auto& entry : entries_) {
    cached_states_.push_back(entry.interceptor.state());
  }
  return cached_states_;
}

void InterceptorManager::handle_intercepts(
    const std::vector<std::pair<uint64_t, sensor_fusion::TrackId>>& hits) {
  for (const auto& [interceptor_id, track_id] : hits) {
    const auto id_it = index_by_id_.find(interceptor_id);
    if (id_it == index_by_id_.end()) {
      continue;
    }

    auto& interceptor = entries_[id_it->second].interceptor;
    if (interceptor.state().engaged && interceptor.state().target_id == track_id) {
      interceptor.clear_engagement();
    }
  }
}

}  // namespace sensor_fusion::agents
