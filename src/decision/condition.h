#pragma once

#include <functional>
#include <utility>

#include "decision/node.h"

namespace sensor_fusion::decision {

class Condition : public Node {
 public:
  explicit Condition(std::function<bool()> predicate)
      : predicate_(std::move(predicate)) {}

  Status tick() override {
    return predicate_() ? Status::Success : Status::Failure;
  }

 private:
  std::function<bool()> predicate_;
};

}  // namespace sensor_fusion::decision
