#include "common/fixed_rate_loop.h"

#include <stdexcept>

namespace sensor_fusion {

FixedRateLoop::FixedRateLoop(double hz) {
  if (hz <= 0.0) {
    throw std::invalid_argument("hz must be > 0");
  }
  dt_seconds_ = 1.0 / hz;
}

double FixedRateLoop::tick_dt() const {
  return dt_seconds_;
}

uint64_t FixedRateLoop::next_tick() {
  return tick_index_++;
}

}  // namespace sensor_fusion
