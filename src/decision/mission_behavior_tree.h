#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "decision/blackboard.h"
#include "decision/decision_engine.h"
#include "decision/node.h"

namespace sensor_fusion::observability {
class Metrics;
}

namespace sensor_fusion::decision {

struct BtNodeTrace {
  std::string node;
  Status status = Status::Failure;
  std::string detail;
};

struct BtEventTrace {
  std::string event;
  sensor_fusion::TrackId track_id{0};
  uint64_t interceptor_id = 0;
  std::string reason;
};

struct EngagementCommand {
  uint64_t interceptor_id = 0;
  sensor_fusion::TrackId track_id{0};
  std::array<double, 3> target_position{0.0, 0.0, 0.0};
  std::string reason;
};

struct BtTickResult {
  uint64_t tick = 0;
  double time_s = 0.0;
  MissionMode mode = MissionMode::Search;
  DecisionEvent event;
  uint32_t active_engagement_count = 0;
  uint32_t idle_interceptor_count = 0;
  uint32_t reconciliation_removals = 0;
  uint32_t active_engagement_count_after_reconcile = 0;
  sensor_fusion::TrackId selected_track_id{0};
  uint64_t selected_interceptor_id = 0;
  double selected_engagement_score = 0.0;
  double selected_estimated_intercept_time_s = 0.0;
  std::string assignment_reason;
  std::vector<BtNodeTrace> node_trace;
  std::vector<BtEventTrace> events;
  std::vector<EngagementCommand> engagement_commands;
};

class MissionBehaviorTree {
 public:
  explicit MissionBehaviorTree(DecisionConfig config,
                               sensor_fusion::observability::Metrics* metrics = nullptr);

  BtTickResult tick(MissionBlackboard& blackboard);

 private:
  struct MissionMemory {
    struct ActiveEngagement {
      sensor_fusion::TrackId track_id{0};
      uint64_t interceptor_id = 0;
      double engagement_start_time_s = 0.0;
      double last_command_time_s = 0.0;
      uint32_t missing_ticks = 0;
      uint32_t low_confidence_ticks = 0;
    };

    struct TrackStability {
      sensor_fusion::TrackId track_id{0};
      uint32_t ticks = 0;
    };

    MissionMode last_mode = MissionMode::Search;
    sensor_fusion::TrackId last_denied_track_id{0};
    std::string last_denial_reason;
    double denial_cooldown_until_s = 0.0;
    sensor_fusion::TrackId stable_track_id{0};
    uint32_t consecutive_track_ticks = 0;
    uint32_t consecutive_engage_ticks = 0;
    std::vector<ActiveEngagement> active_engagements;
    std::vector<TrackStability> stable_tracks;
  };

  DecisionConfig config_;
  sensor_fusion::observability::Metrics* metrics_ = nullptr;
  MissionMemory memory_;
};

const char* bt_status_to_string(Status status);

}  // namespace sensor_fusion::decision
