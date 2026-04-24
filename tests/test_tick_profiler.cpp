#include <chrono>
#include <thread>

#include <doctest/doctest.h>

#include "common/tick_profiler.h"

namespace sensor_fusion {

TEST_CASE("tick profiler counts deadline misses") {
  TickProfiler profiler(1.0);

  profiler.begin_tick();
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  profiler.end_tick();

  CHECK(profiler.stats().ticks == 1);
  CHECK(profiler.stats().deadline_misses == 1);
  CHECK(profiler.last_tick_ms() >= 1.0);
}

TEST_CASE("tick profiler aggregates stats") {
  TickProfiler profiler(1000.0);

  profiler.begin_tick();
  profiler.end_tick();

  profiler.begin_tick();
  profiler.end_tick();

  const TickStats& stats = profiler.stats();
  CHECK(stats.ticks == 2);
  CHECK(stats.deadline_misses == 0);
  CHECK(stats.max_ms >= stats.min_ms);
  CHECK(stats.mean_ms >= stats.min_ms);
  CHECK(stats.mean_ms <= stats.max_ms);
}

}  // namespace sensor_fusion
