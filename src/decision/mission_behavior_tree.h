#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "decision/blackboard.h"
#include "decision/decision_engine.h"
#include "decision/node.h"

namespace sensor_fusion::decision {

struct BtNodeTrace {
  std::string node;
  Status status = Status::Failure;
  std::string detail;
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
  std::vector<BtNodeTrace> node_trace;
  std::vector<EngagementCommand> engagement_commands;
};

class MissionBehaviorTree {
 public:
  explicit MissionBehaviorTree(DecisionConfig config);

  BtTickResult tick(MissionBlackboard& blackboard) const;

 private:
  DecisionConfig config_;
};

const char* bt_status_to_string(Status status);

}  // namespace sensor_fusion::decision
