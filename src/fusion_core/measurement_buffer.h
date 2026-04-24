#pragma once

#include <deque>
#include <vector>

#include "messages/types.h"

namespace sensor_fusion::fusion_core {

class MeasurementBuffer {
 public:
  void push(const sensor_fusion::Measurement& m);

  std::vector<sensor_fusion::Measurement> pop_ready(const sensor_fusion::Timestamp& fusion_time);
  void pop_ready(const sensor_fusion::Timestamp& fusion_time,
                 std::vector<sensor_fusion::Measurement>* out_ready);

 private:
  std::deque<sensor_fusion::Measurement> buffer_;
};

}  // namespace sensor_fusion::fusion_core
