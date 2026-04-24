#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "decision/node.h"

namespace sensor_fusion::decision {

class Selector : public Node {
 public:
  void add_child(std::unique_ptr<Node> node) {
    children_.push_back(std::move(node));
  }

  Status tick() override {
    for (auto& child : children_) {
      const Status s = child->tick();
      if (s == Status::Success || s == Status::Running) {
        return s;
      }
    }
    return Status::Failure;
  }

 private:
  std::vector<std::unique_ptr<Node>> children_;
};

}  // namespace sensor_fusion::decision
