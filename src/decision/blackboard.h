#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "agents/interceptor/interceptor.h"
#include "fusion_core/track.h"

namespace sensor_fusion::decision {

enum class MissionMode { Search, Track, Engage };

struct TrackFact {
  sensor_fusion::TrackId track_id{0};
  sensor_fusion::fusion_core::TrackStatus status =
      sensor_fusion::fusion_core::TrackStatus::Tentative;
  std::array<double, 3> position{0.0, 0.0, 0.0};
  double confidence = 0.0;
  double score = 0.0;
  double priority = 0.0;
};

struct InterceptorFact {
  uint64_t interceptor_id = 0;
  bool available = false;
  bool engaged = false;
  sensor_fusion::TrackId target_id{0};
  std::array<double, 3> position{0.0, 0.0, 0.0};
};

struct SensorHealthFact {
  sensor_fusion::SensorId sensor_id{0};
  bool active = true;
  bool stale = false;
  double last_seen_age_s = 0.0;
};

struct EngagementState {
  bool active = false;
  uint64_t interceptor_id = 0;
  sensor_fusion::TrackId track_id{0};
};

struct BlackboardSnapshot {
  uint64_t tick = 0;
  double time_s = 0.0;
  std::vector<TrackFact> tracks;
  std::vector<InterceptorFact> interceptors;
  std::vector<SensorHealthFact> sensor_health;
  EngagementState engagement;
  MissionMode mode = MissionMode::Search;
};

class MissionBlackboard {
 public:
  void set_tick(uint64_t tick, double time_s);
  void set_tracks(std::vector<TrackFact> tracks);
  void set_interceptors(std::vector<InterceptorFact> interceptors);
  void set_interceptors_from_states(
      const std::vector<sensor_fusion::agents::InterceptorState>& states);
  void set_sensor_health(std::vector<SensorHealthFact> sensor_health);
  void set_mode(MissionMode mode);
  void set_engagement(uint64_t interceptor_id, sensor_fusion::TrackId track_id);
  void clear_engagement();

  BlackboardSnapshot snapshot() const;

 private:
  mutable std::mutex mutex_;
  BlackboardSnapshot state_;
};

std::optional<TrackFact> select_highest_priority_track(
    const BlackboardSnapshot& snapshot,
    bool confirmed_only);

std::optional<InterceptorFact> select_available_interceptor(
    const BlackboardSnapshot& snapshot);

bool has_stale_required_sensor(const BlackboardSnapshot& snapshot);

const char* mission_mode_to_string(MissionMode mode);

}  // namespace sensor_fusion::decision
