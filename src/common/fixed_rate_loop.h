#pragma once

#include <cstdint>

namespace sensor_fusion {

class FixedRateLoop {
 public:
  explicit FixedRateLoop(double hz);

  double tick_dt() const;
  uint64_t next_tick();

 private:
  double dt_seconds_;
  uint64_t tick_index_{0};
};

}  // namespace sensor_fusion
