#pragma once

namespace sensor_fusion::decision {

enum class Status { Success, Failure, Running };

class Node {
 public:
  virtual ~Node() = default;
  virtual Status tick() = 0;
};

}  // namespace sensor_fusion::decision
