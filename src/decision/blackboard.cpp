#include "decision/blackboard.h"

#include <algorithm>
#include <cmath>

namespace sensor_fusion::decision {

void MissionBlackboard::set_tick(uint64_t tick, double time_s) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.tick = tick;
  state_.time_s = time_s;
}

void MissionBlackboard::set_tracks(std::vector<TrackFact> tracks) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::sort(tracks.begin(), tracks.end(), [](const TrackFact& a, const TrackFact& b) {
    if (a.priority != b.priority) {
      return a.priority > b.priority;
    }
    return a.track_id.value < b.track_id.value;
  });
  state_.tracks = std::move(tracks);
}

void MissionBlackboard::set_interceptors(std::vector<InterceptorFact> interceptors) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::sort(interceptors.begin(), interceptors.end(),
            [](const InterceptorFact& a, const InterceptorFact& b) {
              return a.interceptor_id < b.interceptor_id;
            });
  state_.interceptors = std::move(interceptors);
}

void MissionBlackboard::set_interceptors_from_states(
    const std::vector<sensor_fusion::agents::InterceptorState>& states) {
  std::vector<InterceptorFact> facts;
  facts.reserve(states.size());
  for (const auto& state : states) {
    facts.push_back(InterceptorFact{
        .interceptor_id = state.interceptor_id,
        .available = !state.engaged,
        .engaged = state.engaged,
        .target_id = state.target_id,
        .position = state.position,
        .speed_mps = std::sqrt(state.velocity[0] * state.velocity[0] +
                               state.velocity[1] * state.velocity[1] +
                               state.velocity[2] * state.velocity[2]),
    });
  }
  set_interceptors(std::move(facts));
}

void MissionBlackboard::set_sensor_health(std::vector<SensorHealthFact> sensor_health) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::sort(sensor_health.begin(), sensor_health.end(),
            [](const SensorHealthFact& a, const SensorHealthFact& b) {
              return a.sensor_id.value < b.sensor_id.value;
            });
  state_.sensor_health = std::move(sensor_health);
}

void MissionBlackboard::set_mode(MissionMode mode) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.mode = mode;
}

void MissionBlackboard::set_engagement(uint64_t interceptor_id,
                                       sensor_fusion::TrackId track_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.engagement = EngagementState{
      .active = true,
      .interceptor_id = interceptor_id,
      .track_id = track_id,
  };
  state_.mode = MissionMode::Engage;
}

void MissionBlackboard::clear_engagement() {
  std::lock_guard<std::mutex> lock(mutex_);
  state_.engagement = EngagementState{};
}

BlackboardSnapshot MissionBlackboard::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

std::optional<TrackFact> select_highest_priority_track(
    const BlackboardSnapshot& snapshot,
    bool confirmed_only) {
  std::optional<TrackFact> best;
  for (const auto& track : snapshot.tracks) {
    if (track.status == sensor_fusion::fusion_core::TrackStatus::Deleted) {
      continue;
    }
    if (confirmed_only &&
        track.status != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
      continue;
    }
    if (!best.has_value() || track.priority > best->priority ||
        (track.priority == best->priority && track.track_id.value < best->track_id.value)) {
      best = track;
    }
  }
  return best;
}

std::optional<InterceptorFact> select_available_interceptor(
    const BlackboardSnapshot& snapshot) {
  for (const auto& interceptor : snapshot.interceptors) {
    if (interceptor.available && !interceptor.engaged) {
      return interceptor;
    }
  }
  return std::nullopt;
}

bool has_stale_required_sensor(const BlackboardSnapshot& snapshot) {
  for (const auto& sensor : snapshot.sensor_health) {
    if (!sensor.active || sensor.stale) {
      return true;
    }
  }
  return false;
}

const char* mission_mode_to_string(MissionMode mode) {
  switch (mode) {
    case MissionMode::Search:
      return "search";
    case MissionMode::Track:
      return "track";
    case MissionMode::Engage:
      return "engage";
  }
  return "search";
}

}  // namespace sensor_fusion::decision
