#include "common/tick_profiler.h"

#include <algorithm>
#include <chrono>

namespace sensor_fusion {
namespace {

long long now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

}  // namespace

TickProfiler::TickProfiler(double deadline_ms) : deadline_ms_(deadline_ms) {}

void TickProfiler::begin_tick() {
  tick_open_ = true;
  begin_tick_ns_ = now_ns();
}

void TickProfiler::end_tick() {
  if (!tick_open_) {
    return;
  }

  const long long end_ns = now_ns();
  tick_open_ = false;

  last_tick_ms_ = static_cast<double>(end_ns - begin_tick_ns_) / 1e6;

  if (stats_.ticks == 0) {
    stats_.min_ms = last_tick_ms_;
    stats_.max_ms = last_tick_ms_;
    stats_.mean_ms = last_tick_ms_;
  } else {
    stats_.min_ms = std::min(stats_.min_ms, last_tick_ms_);
    stats_.max_ms = std::max(stats_.max_ms, last_tick_ms_);
    stats_.mean_ms =
        ((stats_.mean_ms * static_cast<double>(stats_.ticks)) + last_tick_ms_) /
        static_cast<double>(stats_.ticks + 1);
  }

  ++stats_.ticks;
  if (last_tick_ms_ > deadline_ms_) {
    ++stats_.deadline_misses;
  }
}

const TickStats& TickProfiler::stats() const {
  return stats_;
}

double TickProfiler::last_tick_ms() const {
  return last_tick_ms_;
}

}  // namespace sensor_fusion
