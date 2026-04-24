#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "agents/interceptor/interceptor.h"
#include "common/allocation_tracker.h"
#include "common/fixed_rate_loop.h"
#include "common/tick_profiler.h"
#include "decision/decision_engine.h"
#include "fusion_core/measurement_buffer.h"
#include "fusion_core/track_manager.h"
#include "messages/types.h"
#include "observability/metrics.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "services/telemetry_ingest/telemetry_ingest_service.h"
#include "transport/message_bus.h"
#include "viz/visualizer.h"

namespace {

struct CliOptions {
  std::string bind_addr = "0.0.0.0:50052";
  int ticks = 400;
  double hz = 20.0;
  int use_hungarian = 1;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;
  double gate_mahal = 9.21;
  int enable_interceptor = 1;
  double interceptor_speed = 150.0;
  double engage_score_threshold = 3.0;
  size_t queue_max = 10000;
  double deadline_ms = 10.0;
  int profile_every = 50;
  int enable_viz = 0;
  double viz_meters_per_pixel = 2.0;
  double viz_protected_radius = 200.0;
};

struct SharedVizState {
  std::mutex mutex;
  sensor_fusion::viz::VizSnapshot latest;
  bool has_snapshot = false;
  bool done = false;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--bind_addr" && i + 1 < argc) {
      options.bind_addr = argv[++i];
    } else if (arg == "--ticks" && i + 1 < argc) {
      options.ticks = std::stoi(argv[++i]);
    } else if (arg == "--hz" && i + 1 < argc) {
      options.hz = std::stod(argv[++i]);
    } else if (arg == "--use_hungarian" && i + 1 < argc) {
      options.use_hungarian = std::stoi(argv[++i]);
    } else if (arg == "--confirm_hits" && i + 1 < argc) {
      options.confirm_hits = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--delete_misses" && i + 1 < argc) {
      options.delete_misses = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--gate_mahal" && i + 1 < argc) {
      options.gate_mahal = std::stod(argv[++i]);
    } else if (arg == "--enable_interceptor" && i + 1 < argc) {
      options.enable_interceptor = std::stoi(argv[++i]);
    } else if (arg == "--interceptor_speed" && i + 1 < argc) {
      options.interceptor_speed = std::stod(argv[++i]);
    } else if (arg == "--engage_score_threshold" && i + 1 < argc) {
      options.engage_score_threshold = std::stod(argv[++i]);
    } else if (arg == "--queue_max" && i + 1 < argc) {
      options.queue_max = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (arg == "--deadline_ms" && i + 1 < argc) {
      options.deadline_ms = std::stod(argv[++i]);
    } else if (arg == "--profile_every" && i + 1 < argc) {
      options.profile_every = std::stoi(argv[++i]);
    } else if (arg == "--enable_viz" && i + 1 < argc) {
      options.enable_viz = std::stoi(argv[++i]);
    } else if (arg == "--viz_meters_per_pixel" && i + 1 < argc) {
      options.viz_meters_per_pixel = std::stod(argv[++i]);
    } else if (arg == "--viz_protected_radius" && i + 1 < argc) {
      options.viz_protected_radius = std::stod(argv[++i]);
    }
  }
  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  sensor_fusion::MessageBus bus;
  sensor_fusion::observability::Metrics metrics;
  sensor_fusion::services::telemetry_ingest::IngestPipeline pipeline(
      &bus, &metrics,
      sensor_fusion::services::telemetry_ingest::IngestPipeline::Options{
          .max_queue = options.queue_max,
          .drop_oldest = true,
          .enable_worker_thread = true,
          .enable_stale_monitor_thread = true,
          .log_every_batches = 100,
          .stale_timeout_s = 2.0,
      });
  sensor_fusion::services::telemetry_ingest::TelemetryIngestServiceImpl ingest_service(
      &pipeline);
  sensor_fusion::services::telemetry_ingest::HealthServiceImpl health_service(&pipeline);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(options.bind_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&ingest_service);
  builder.RegisterService(&health_service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  if (server == nullptr) {
    std::cerr << "failed to start run_cluster_demo ingest on " << options.bind_addr
              << std::endl;
    return 1;
  }
  std::cout << "run_cluster_demo ingest listening on " << options.bind_addr << std::endl;

  std::mutex incoming_mutex;
  std::vector<sensor_fusion::Measurement> incoming_measurements;
  bus.topic<sensor_fusion::Measurement>("measurements")
      .subscribe([&incoming_mutex, &incoming_measurements](
                     const sensor_fusion::Measurement& measurement) {
        std::lock_guard<std::mutex> lock(incoming_mutex);
        incoming_measurements.push_back(measurement);
      });

  sensor_fusion::fusion_core::MeasurementBuffer measurement_buffer;
  sensor_fusion::fusion_core::TrackManager manager(
      sensor_fusion::fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = options.gate_mahal,
          .gate_mahal_eoir = options.gate_mahal,
          .confirm_hits = options.confirm_hits,
          .delete_misses = options.delete_misses,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = options.use_hungarian != 0,
          .enable_eoir_updates = true,
          .unassigned_cost = 1e9,
      },
      &metrics);

  sensor_fusion::agents::Interceptor interceptor(options.interceptor_speed);
  sensor_fusion::decision::DecisionEngine decision_engine(options.engage_score_threshold);
  sensor_fusion::FixedRateLoop loop(options.hz);
  sensor_fusion::TickProfiler tick_profiler(options.deadline_ms);
  sensor_fusion::AllocationTracker allocation_tracker(&metrics);
  metrics.inc("vector_growth_events_total", 0);

  uint64_t engagements = 0;
  uint64_t intercepts = 0;
  double fusion_time_s = 0.0;

  std::vector<sensor_fusion::Measurement> ready;
  ready.reserve(256);
  size_t ready_capacity = ready.capacity();

  std::shared_ptr<SharedVizState> viz_state;
  std::thread viz_thread;
  if (options.enable_viz != 0) {
#if defined(AEGISTRACK_VIZ)
    viz_state = std::make_shared<SharedVizState>();
    sensor_fusion::viz::VizConfig viz_cfg;
    viz_cfg.meters_per_pixel = options.viz_meters_per_pixel;
    viz_cfg.protected_zone_radius_m = options.viz_protected_radius;
    viz_thread = std::thread([viz_cfg, viz_state]() mutable {
      sensor_fusion::viz::Visualizer visualizer(viz_cfg);
      visualizer.run([viz_state](sensor_fusion::viz::VizSnapshot& out) {
        std::lock_guard<std::mutex> lock(viz_state->mutex);
        if (viz_state->has_snapshot) {
          out = viz_state->latest;
        }
        return !viz_state->done;
      });
    });
#else
    std::cout << "visualization requested but not available in this build" << std::endl;
#endif
  }

  for (int tick = 0; tick < options.ticks; ++tick) {
    const uint64_t misses_before = tick_profiler.stats().deadline_misses;
    tick_profiler.begin_tick();

    loop.next_tick();
    const double dt = loop.tick_dt();
    fusion_time_s += dt;

    std::vector<sensor_fusion::Measurement> drained;
    {
      std::lock_guard<std::mutex> lock(incoming_mutex);
      drained.swap(incoming_measurements);
    }
    for (const auto& measurement : drained) {
      measurement_buffer.push(measurement);
    }

    const auto fusion_time = sensor_fusion::Timestamp::from_seconds(fusion_time_s);
    measurement_buffer.pop_ready(fusion_time, &ready);
    allocation_tracker.note_vector_capacity(ready, &ready_capacity);

    manager.predict_all(dt);
    const std::vector<sensor_fusion::fusion_core::Track> snapshot =
        manager.update_with_measurements(ready);

    std::unordered_map<uint64_t, std::array<double, 3>> confirmed_positions;
    size_t confirmed_count = 0;
    for (const auto& track : snapshot) {
      if (track.status() != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
        continue;
      }
      ++confirmed_count;
      const auto x = track.filter().state();
      confirmed_positions.emplace(track.id().value, std::array<double, 3>{x(0), x(1), x(2)});

      if (options.enable_interceptor != 0) {
        const bool was_engaged = interceptor.state().engaged;
        decision_engine.decide(
            track, !interceptor.state().engaged, {x(0), x(1), x(2)},
            [&](sensor_fusion::TrackId id, const std::array<double, 3>& target_pos) {
              interceptor.assign_target(id, target_pos);
            });

        if (!was_engaged && interceptor.state().engaged) {
          ++engagements;
          metrics.inc("engagements_total");
        }
      }
    }

    if (options.enable_interceptor != 0 && interceptor.state().engaged) {
      const uint64_t target_id = interceptor.state().target_id.value;
      const auto pos_it = confirmed_positions.find(target_id);
      if (pos_it != confirmed_positions.end()) {
        interceptor.assign_target(interceptor.state().target_id, pos_it->second);
        interceptor.step(dt);
        if (interceptor.has_intercepted(pos_it->second)) {
          ++intercepts;
          metrics.inc("successful_intercepts_total");
          interceptor.clear_engagement();
        }
      } else {
        interceptor.clear_engagement();
      }
    }

    if (viz_state != nullptr) {
      sensor_fusion::viz::VizSnapshot viz_snapshot;
      viz_snapshot.t_s = fusion_time_s;
      viz_snapshot.tracks.reserve(snapshot.size());
      for (const auto& track : snapshot) {
        const auto x = track.filter().state();
        sensor_fusion::viz::VizSnapshot::TrackViz track_viz;
        track_viz.id = track.id().value;
        track_viz.confirmed = track.status() == sensor_fusion::fusion_core::TrackStatus::Confirmed;
        track_viz.pos = {x(0), x(1), x(2)};
        const auto P = track.filter().covariance();
        if (P.rows() == 6 && P.cols() == 6) {
          track_viz.P = P;
        }
        viz_snapshot.tracks.push_back(track_viz);
      }

      if (options.enable_interceptor != 0) {
        const auto state = interceptor.state();
        sensor_fusion::viz::VizSnapshot::InterceptorViz interceptor_viz;
        interceptor_viz.id = state.interceptor_id;
        interceptor_viz.engaged = state.engaged;
        interceptor_viz.pos = state.position;
        if (state.engaged && state.target_id.value != 0) {
          interceptor_viz.target_track_id = state.target_id.value;
          const auto target_it = confirmed_positions.find(state.target_id.value);
          if (target_it != confirmed_positions.end()) {
            viz_snapshot.engagement_lines.emplace_back(state.position, target_it->second);
          }
        }
        viz_snapshot.interceptors.push_back(interceptor_viz);
      }

      std::lock_guard<std::mutex> lock(viz_state->mutex);
      viz_state->latest = std::move(viz_snapshot);
      viz_state->has_snapshot = true;
    }

    if ((tick + 1) % 20 == 0 || tick == options.ticks - 1) {
      std::cout << "[cluster_demo] tick=" << (tick + 1) << " ready=" << ready.size()
                << " confirmed_tracks=" << confirmed_count
                << " engagements=" << engagements << " intercepts=" << intercepts
                << std::endl;
    }

    tick_profiler.end_tick();
    metrics.observe("tick_ms", tick_profiler.last_tick_ms());
    if (tick_profiler.stats().deadline_misses > misses_before) {
      metrics.inc("deadline_misses_total");
    }
    if (options.profile_every > 0 && ((tick + 1) % options.profile_every == 0)) {
      std::cout << "[cluster_profile] tick=" << (tick + 1)
                << " mean_ms=" << tick_profiler.stats().mean_ms
                << " max_ms=" << tick_profiler.stats().max_ms
                << " deadline_misses=" << tick_profiler.stats().deadline_misses << std::endl;
    }
  }

  if (viz_state != nullptr) {
    {
      std::lock_guard<std::mutex> lock(viz_state->mutex);
      viz_state->done = true;
    }
    if (viz_thread.joinable()) {
      viz_thread.join();
    }
  }

  server->Shutdown();
  return 0;
}
