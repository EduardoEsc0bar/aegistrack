#pragma once

#include <cstdint>

namespace sensor_fusion {

struct TickStats {
  uint64_t ticks = 0;
  double mean_ms = 0.0;
  double max_ms = 0.0;
  double min_ms = 0.0;
  uint64_t deadline_misses = 0;
};

class TickProfiler {
 public:
  explicit TickProfiler(double deadline_ms);

  void begin_tick();
  void end_tick();

  const TickStats& stats() const;
  double last_tick_ms() const;

 private:
  double deadline_ms_ = 0.0;
  TickStats stats_;
  double last_tick_ms_ = 0.0;
  bool tick_open_ = false;
  long long begin_tick_ns_ = 0;
};

}  // namespace sensor_fusion
