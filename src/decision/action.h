#pragma once

#include <functional>
#include <utility>

#include "decision/node.h"

namespace sensor_fusion::decision {

class Action : public Node {
 public:
  explicit Action(std::function<Status()> action)
      : action_(std::move(action)) {}

  Status tick() override {
    return action_();
  }

 private:
  std::function<Status()> action_;
};

}  // namespace sensor_fusion::decision
