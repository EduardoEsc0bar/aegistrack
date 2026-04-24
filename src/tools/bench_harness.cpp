#include "tools/bench_harness.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "common/allocation_tracker.h"
#include "common/angles.h"
#include "common/fixed_rate_loop.h"
#include "common/rng.h"
#include "fusion_core/measurement_buffer.h"
#include "fusion_core/track_manager.h"
#include "scenario/truth_generator.h"
#include "sensors/eoir/eoir_sensor.h"
#include "sensors/radar/radar_sensor.h"

namespace sensor_fusion::tools {
namespace {

std::vector<sensor_fusion::TruthGenerator::TargetInit> build_targets(int count) {
  std::vector<sensor_fusion::TruthGenerator::TargetInit> targets;
  targets.reserve(static_cast<size_t>(count));

  for (int i = 0; i < count; ++i) {
    double vy = 0.0;
    if (i % 3 == 1) {
      vy = 2.0;
    } else if (i % 3 == 2) {
      vy = -2.0;
    }

    targets.push_back(sensor_fusion::TruthGenerator::TargetInit{
        .target_id = static_cast<uint64_t>(i + 1),
        .position_xyz = {100.0 * static_cast<double>(i), 0.0, 50.0},
        .velocity_xyz = {10.0, vy, 0.5},
    });
  }

  return targets;
}

}  // namespace

sensor_fusion::observability::MetricsSnapshot run_bench_case(const SimConfig& config,
                                                             uint64_t seed) {
  return run_bench_case_detailed(config, seed, BenchOptions{}).metrics;
}

BenchResult run_bench_case_detailed(const SimConfig& config,
                                    uint64_t seed,
                                    const BenchOptions& options) {
  sensor_fusion::observability::Metrics metrics;
  metrics.inc("vector_growth_events_total", 0);

  sensor_fusion::TruthGenerator generator(build_targets(config.targets));
  sensor_fusion::FixedRateLoop loop(config.hz);
  sensor_fusion::TickProfiler tick_profiler(options.deadline_ms);
  sensor_fusion::AllocationTracker allocation_tracker(&metrics);

  const sensor_fusion::RadarConfig radar_cfg{
      .sensor_id = sensor_fusion::SensorId(1),
      .hz = config.hz,
      .max_range_m = 10000.0,
      .fov_deg = 120.0,
      .sigma_range_m = config.radar_sigma_range_m,
      .sigma_bearing_rad = sensor_fusion::deg_to_rad(config.radar_sigma_bearing_deg),
      .p_detect = config.radar_p_detect,
      .p_false_alarm = config.radar_p_false_alarm,
      .drop_prob = config.radar_drop_prob,
      .latency_mean_s = config.radar_latency_ms / 1000.0,
      .latency_jitter_s = config.radar_jitter_ms / 1000.0,
  };
  sensor_fusion::RadarSensor radar(radar_cfg, sensor_fusion::Rng(seed * 2 + 1));

  const sensor_fusion::EoirConfig eoir_cfg{
      .sensor_id = sensor_fusion::SensorId(2),
      .sigma_bearing_rad = sensor_fusion::deg_to_rad(config.eoir_sigma_bearing_deg),
      .sigma_elevation_rad = sensor_fusion::deg_to_rad(config.eoir_sigma_elevation_deg),
      .p_detect = config.eoir_p_detect,
      .drop_prob = config.eoir_drop_prob,
      .latency_mean_s = config.eoir_latency_ms / 1000.0,
      .latency_jitter_s = config.eoir_jitter_ms / 1000.0,
  };
  sensor_fusion::EoirSensor eoir(eoir_cfg, sensor_fusion::Rng(seed * 2 + 2));

  sensor_fusion::fusion_core::MeasurementBuffer radar_buffer;
  sensor_fusion::fusion_core::MeasurementBuffer eoir_buffer;

  sensor_fusion::fusion_core::TrackManager manager(
      sensor_fusion::fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = config.gate_mahal,
          .gate_mahal_eoir = config.gate_mahal_eoir,
          .confirm_hits = config.confirm_hits,
          .delete_misses = config.delete_misses,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = config.use_hungarian != 0,
          .enable_eoir_updates = config.enable_eoir_updates != 0,
          .unassigned_cost = 1e9,
      },
      &metrics);

  const size_t reserve_hint =
      std::max<size_t>(64, static_cast<size_t>(std::max(1, config.targets)) * 8);
  std::vector<sensor_fusion::Measurement> ready_radar;
  std::vector<sensor_fusion::Measurement> ready_eoir;
  std::vector<sensor_fusion::Measurement> combined_ready;
  ready_radar.reserve(reserve_hint);
  ready_eoir.reserve(reserve_hint);
  combined_ready.reserve(reserve_hint * 2);

  size_t ready_radar_capacity = ready_radar.capacity();
  size_t ready_eoir_capacity = ready_eoir.capacity();
  size_t combined_capacity = combined_ready.capacity();

  const auto wall_start = std::chrono::steady_clock::now();

  for (int tick = 0; tick < config.ticks; ++tick) {
    const uint64_t misses_before = tick_profiler.stats().deadline_misses;
    tick_profiler.begin_tick();

    loop.next_tick();
    const double dt = loop.tick_dt();

    const auto truths = generator.step(dt);

    const auto radar_meas = radar.sense(truths);
    for (const auto& measurement : radar_meas) {
      radar_buffer.push(measurement);
    }

    if (config.enable_eoir != 0) {
      const auto eoir_meas = eoir.sense(truths);
      for (const auto& measurement : eoir_meas) {
        eoir_buffer.push(measurement);
      }
    }

    const sensor_fusion::Timestamp fusion_time =
        sensor_fusion::Timestamp::from_seconds(generator.t_seconds());

    radar_buffer.pop_ready(fusion_time, &ready_radar);
    eoir_buffer.pop_ready(fusion_time, &ready_eoir);

    combined_ready.clear();
    combined_ready.insert(combined_ready.end(), ready_radar.begin(), ready_radar.end());
    combined_ready.insert(combined_ready.end(), ready_eoir.begin(), ready_eoir.end());

    std::sort(combined_ready.begin(), combined_ready.end(),
              [](const sensor_fusion::Measurement& a, const sensor_fusion::Measurement& b) {
                if (a.t_sent < b.t_sent) {
                  return true;
                }
                if (b.t_sent < a.t_sent) {
                  return false;
                }
                return a.t_meas < b.t_meas;
              });

    allocation_tracker.note_vector_capacity(ready_radar, &ready_radar_capacity);
    allocation_tracker.note_vector_capacity(ready_eoir, &ready_eoir_capacity);
    allocation_tracker.note_vector_capacity(combined_ready, &combined_capacity);

    manager.predict_all(dt);
    const auto snapshot = manager.update_with_measurements(combined_ready);

    size_t confirmed_count = 0;
    for (const auto& track : snapshot) {
      if (track.status() == sensor_fusion::fusion_core::TrackStatus::Confirmed) {
        ++confirmed_count;
      }
    }
    metrics.set_gauge("tracks_confirmed", static_cast<double>(confirmed_count));

    for (const auto& track : snapshot) {
      if (track.status() != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
        continue;
      }

      const auto x = track.filter().state();
      double best_error = std::numeric_limits<double>::infinity();
      for (const auto& truth : truths) {
        const double dx = x(0) - truth.position_xyz[0];
        const double dy = x(1) - truth.position_xyz[1];
        const double dz = x(2) - truth.position_xyz[2];
        const double error = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (error < best_error) {
          best_error = error;
        }
      }

      if (std::isfinite(best_error)) {
        metrics.observe("pos_error", best_error);
      }
    }

    tick_profiler.end_tick();
    metrics.observe("tick_ms", tick_profiler.last_tick_ms());
    if (tick_profiler.stats().deadline_misses > misses_before) {
      metrics.inc("deadline_misses_total");
    }

    if (options.profile_every > 0 && ((tick + 1) % options.profile_every == 0)) {
      std::cout << "[bench_profile] tick=" << (tick + 1)
                << " mean_ms=" << tick_profiler.stats().mean_ms
                << " max_ms=" << tick_profiler.stats().max_ms
                << " deadline_misses=" << tick_profiler.stats().deadline_misses << std::endl;
    }
  }

  const auto elapsed_s =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start).count();

  return BenchResult{
      .metrics = metrics.snapshot(),
      .tick_stats = tick_profiler.stats(),
      .elapsed_s = elapsed_s,
  };
}

}  // namespace sensor_fusion::tools
