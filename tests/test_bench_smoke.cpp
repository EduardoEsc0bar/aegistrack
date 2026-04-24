#include <doctest/doctest.h>

#include "tools/bench_harness.h"
#include "tools/sim_config.h"

namespace sensor_fusion::tools {

TEST_CASE("bench harness smoke") {
  SimConfig cfg;
  cfg.ticks = 30;
  cfg.hz = 20.0;
  cfg.targets = 3;
  cfg.enable_eoir = 1;
  cfg.enable_eoir_updates = 1;
  cfg.use_hungarian = 1;

  const auto snap = run_bench_case(cfg, 1);

  CHECK(snap.counters.find("meas_radar_total") != snap.counters.end());
  CHECK(snap.counters.at("meas_radar_total") > 0);
  CHECK(snap.counters.find("assoc_attempts") != snap.counters.end());
  CHECK(snap.counters.at("assoc_attempts") > 0);
}

}  // namespace sensor_fusion::tools
