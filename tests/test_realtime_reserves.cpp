#include <doctest/doctest.h>

#include "tools/bench_harness.h"
#include "tools/sim_config.h"

namespace sensor_fusion::tools {

TEST_CASE("realtime reserves avoid vector growth after warm-up") {
  SimConfig cfg;
  cfg.ticks = 80;
  cfg.hz = 20.0;
  cfg.targets = 3;
  cfg.enable_eoir = 1;
  cfg.enable_eoir_updates = 1;
  cfg.use_hungarian = 1;
  cfg.radar_p_false_alarm = 0.0;
  cfg.radar_drop_prob = 0.0;
  cfg.eoir_drop_prob = 0.0;

  const auto result = run_bench_case_detailed(
      cfg, 123,
      BenchOptions{
          .deadline_ms = 1000.0,
          .profile_every = 0,
      });

  CHECK(result.metrics.counters.find("vector_growth_events_total") !=
        result.metrics.counters.end());
  CHECK(result.metrics.counters.at("vector_growth_events_total") == 0);
}

}  // namespace sensor_fusion::tools
