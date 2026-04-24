#pragma once

#include <cstdint>

#include "common/tick_profiler.h"
#include "observability/metrics.h"
#include "tools/sim_config.h"

namespace sensor_fusion::tools {

struct BenchOptions {
  double deadline_ms = 10.0;
  int profile_every = 0;
};

struct BenchResult {
  sensor_fusion::observability::MetricsSnapshot metrics;
  sensor_fusion::TickStats tick_stats;
  double elapsed_s = 0.0;
};

sensor_fusion::observability::MetricsSnapshot run_bench_case(const SimConfig& config,
                                                             uint64_t seed);
BenchResult run_bench_case_detailed(const SimConfig& config,
                                    uint64_t seed,
                                    const BenchOptions& options);

}  // namespace sensor_fusion::tools
