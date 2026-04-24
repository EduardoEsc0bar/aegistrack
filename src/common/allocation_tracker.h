#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "observability/metrics.h"

namespace sensor_fusion {

class AllocationTracker {
 public:
  explicit AllocationTracker(sensor_fusion::observability::Metrics* metrics = nullptr)
      : metrics_(metrics) {}

  template <typename T>
  void note_vector_capacity(const std::vector<T>& vec, size_t* previous_capacity) {
    if (previous_capacity == nullptr) {
      return;
    }
    if (vec.capacity() > *previous_capacity) {
      ++vector_growth_events_;
      if (metrics_ != nullptr) {
        metrics_->inc("vector_growth_events_total");
      }
      *previous_capacity = vec.capacity();
    }
  }

  uint64_t vector_growth_events() const {
    return vector_growth_events_;
  }

 private:
  sensor_fusion::observability::Metrics* metrics_ = nullptr;
  uint64_t vector_growth_events_ = 0;
};

}  // namespace sensor_fusion
