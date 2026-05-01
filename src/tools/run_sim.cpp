#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "agents/interceptor/interceptor_manager.h"
#include "common/allocation_tracker.h"
#include "common/angles.h"
#include "common/fixed_rate_loop.h"
#include "common/rng.h"
#include "common/tick_profiler.h"
#include "decision/blackboard.h"
#include "decision/mission_behavior_tree.h"
#include "decision/threat_scoring.h"
#include "faults/fault_injector.h"
#include "fusion_core/measurement_buffer.h"
#include "fusion_core/track_manager.h"
#include "messages/types.h"
#include "observability/incident_capture.h"
#include "observability/jsonl_logger.h"
#include "observability/metrics.h"
#include "scenario/truth_generator.h"
#include "sensors/eoir/eoir_sensor.h"
#include "sensors/radar/radar_sensor.h"
#include "transport/message_bus.h"
#include "viz/visualizer.h"

namespace {

struct CliOptions {
  int ticks = 100;
  int targets = 5;
  double hz = 20.0;
  double radar_sigma_range = 3.0;
  double radar_sigma_bearing_deg = 1.0;
  double radar_p_detect = 0.9;
  double radar_p_false_alarm = 0.1;
  double radar_drop_prob = 0.1;
  double radar_latency_ms = 100.0;
  double radar_jitter_ms = 50.0;
  int enable_eoir = 1;
  double eoir_sigma_bearing_deg = 1.0;
  double eoir_sigma_elevation_deg = 1.0;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;
  double gate_mahal = 9.21;
  int use_hungarian = 1;
  int enable_eoir_updates = 1;
  int enable_interceptor = 1;
  int interceptors = 3;
  int allow_multi_engage = 0;
  int retask_enable = 1;
  double commit_distance_m = 50.0;
  double interceptor_speed = 150.0;
  double engage_score_threshold = 3.0;
  double max_engagement_range_m = 5000.0;
  double min_confidence_to_engage = 0.6;
  double no_engage_zone_radius_m = 0.0;
  double engagement_timeout_s = 10.0;
  int metrics_dump_every = 20;
  int enable_logging = 1;
  std::string log_path = "logs/run.jsonl";
  uint64_t fault_seed = 2026;
  double global_drop_prob = 0.0;
  double global_delay_prob = 0.0;
  std::string incident_capture_path;
  double incident_window_s = 5.0;
  double deadline_ms = 10.0;
  int profile_every = 50;
  int enable_viz = 0;
  double viz_meters_per_pixel = 2.0;
  double viz_protected_radius = 200.0;
};

struct EngagementInfo {
  sensor_fusion::TrackId track_id{0};
  double start_time_s = 0.0;
};

struct SharedVizState {
  std::mutex mutex;
  sensor_fusion::viz::VizSnapshot latest;
  bool has_snapshot = false;
  bool done = false;
};

uint64_t get_counter(const sensor_fusion::observability::MetricsSnapshot& snapshot,
                     const std::string& key) {
  const auto it = snapshot.counters.find(key);
  if (it == snapshot.counters.end()) {
    return 0;
  }
  return it->second;
}

double distance3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  const double dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--ticks" && i + 1 < argc) {
      options.ticks = std::stoi(argv[++i]);
    } else if (arg == "--targets" && i + 1 < argc) {
      options.targets = std::stoi(argv[++i]);
    } else if (arg == "--hz" && i + 1 < argc) {
      options.hz = std::stod(argv[++i]);
    } else if (arg == "--radar_sigma_range" && i + 1 < argc) {
      options.radar_sigma_range = std::stod(argv[++i]);
    } else if (arg == "--radar_sigma_bearing_deg" && i + 1 < argc) {
      options.radar_sigma_bearing_deg = std::stod(argv[++i]);
    } else if (arg == "--radar_p_detect" && i + 1 < argc) {
      options.radar_p_detect = std::stod(argv[++i]);
    } else if (arg == "--radar_p_false_alarm" && i + 1 < argc) {
      options.radar_p_false_alarm = std::stod(argv[++i]);
    } else if (arg == "--radar_drop_prob" && i + 1 < argc) {
      options.radar_drop_prob = std::stod(argv[++i]);
    } else if (arg == "--radar_latency_ms" && i + 1 < argc) {
      options.radar_latency_ms = std::stod(argv[++i]);
    } else if (arg == "--radar_jitter_ms" && i + 1 < argc) {
      options.radar_jitter_ms = std::stod(argv[++i]);
    } else if (arg == "--enable_eoir" && i + 1 < argc) {
      options.enable_eoir = std::stoi(argv[++i]);
    } else if (arg == "--eoir_sigma_bearing_deg" && i + 1 < argc) {
      options.eoir_sigma_bearing_deg = std::stod(argv[++i]);
    } else if (arg == "--eoir_sigma_elevation_deg" && i + 1 < argc) {
      options.eoir_sigma_elevation_deg = std::stod(argv[++i]);
    } else if (arg == "--confirm_hits" && i + 1 < argc) {
      options.confirm_hits = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--delete_misses" && i + 1 < argc) {
      options.delete_misses = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--gate_mahal" && i + 1 < argc) {
      options.gate_mahal = std::stod(argv[++i]);
    } else if (arg == "--use_hungarian" && i + 1 < argc) {
      options.use_hungarian = std::stoi(argv[++i]);
    } else if (arg == "--enable_eoir_updates" && i + 1 < argc) {
      options.enable_eoir_updates = std::stoi(argv[++i]);
    } else if (arg == "--enable_interceptor" && i + 1 < argc) {
      options.enable_interceptor = std::stoi(argv[++i]);
    } else if (arg == "--interceptors" && i + 1 < argc) {
      options.interceptors = std::stoi(argv[++i]);
    } else if (arg == "--allow_multi_engage" && i + 1 < argc) {
      options.allow_multi_engage = std::stoi(argv[++i]);
    } else if (arg == "--retask_enable" && i + 1 < argc) {
      options.retask_enable = std::stoi(argv[++i]);
    } else if (arg == "--commit_distance_m" && i + 1 < argc) {
      options.commit_distance_m = std::stod(argv[++i]);
    } else if (arg == "--interceptor_speed" && i + 1 < argc) {
      options.interceptor_speed = std::stod(argv[++i]);
    } else if (arg == "--engage_score_threshold" && i + 1 < argc) {
      options.engage_score_threshold = std::stod(argv[++i]);
    } else if (arg == "--max_engagement_range_m" && i + 1 < argc) {
      options.max_engagement_range_m = std::stod(argv[++i]);
    } else if (arg == "--min_confidence_to_engage" && i + 1 < argc) {
      options.min_confidence_to_engage = std::stod(argv[++i]);
    } else if (arg == "--no_engage_zone_radius_m" && i + 1 < argc) {
      options.no_engage_zone_radius_m = std::stod(argv[++i]);
    } else if (arg == "--engagement_timeout_s" && i + 1 < argc) {
      options.engagement_timeout_s = std::stod(argv[++i]);
    } else if (arg == "--metrics_dump_every" && i + 1 < argc) {
      options.metrics_dump_every = std::stoi(argv[++i]);
    } else if (arg == "--enable_logging" && i + 1 < argc) {
      options.enable_logging = std::stoi(argv[++i]);
    } else if (arg == "--log_path" && i + 1 < argc) {
      options.log_path = argv[++i];
    } else if (arg == "--fault_seed" && i + 1 < argc) {
      options.fault_seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--global_drop_prob" && i + 1 < argc) {
      options.global_drop_prob = std::stod(argv[++i]);
    } else if (arg == "--global_delay_prob" && i + 1 < argc) {
      options.global_delay_prob = std::stod(argv[++i]);
    } else if (arg == "--incident_capture_path" && i + 1 < argc) {
      options.incident_capture_path = argv[++i];
    } else if (arg == "--incident_window_s" && i + 1 < argc) {
      options.incident_window_s = std::stod(argv[++i]);
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

std::vector<sensor_fusion::agents::InterceptorConfig> build_interceptors(int count,
                                                                          double speed) {
  std::vector<sensor_fusion::agents::InterceptorConfig> configs;
  configs.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    configs.push_back(sensor_fusion::agents::InterceptorConfig{
        .interceptor_id = static_cast<uint64_t>(i + 1),
        .max_speed_mps = speed,
        .start_position = {0.0, static_cast<double>(i) * 20.0, 0.0},
    });
  }
  return configs;
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  sensor_fusion::MessageBus bus;
  auto& truth_topic = bus.topic<sensor_fusion::TruthState>("truth");
  auto& measurements_topic = bus.topic<sensor_fusion::Measurement>("measurements");

  measurements_topic.subscribe([](const sensor_fusion::Measurement& measurement) {
    if (measurement.measurement_type == sensor_fusion::MeasurementType::RangeBearing2D &&
        measurement.z.size() >= 2) {
      std::cout << "radar t_meas=" << measurement.t_meas.to_seconds() << " t_sent="
                << measurement.t_sent.to_seconds() << " sensor=" << measurement.sensor_id
                << " range=" << measurement.z[0] << " bearing=" << measurement.z[1]
                << std::endl;
    }
  });

  sensor_fusion::TruthGenerator generator(build_targets(options.targets));
  sensor_fusion::FixedRateLoop loop(options.hz);

  sensor_fusion::observability::Metrics metrics;
  sensor_fusion::TickProfiler tick_profiler(options.deadline_ms);
  sensor_fusion::AllocationTracker allocation_tracker(&metrics);
  metrics.inc("vector_growth_events_total", 0);
  sensor_fusion::faults::FaultInjector fault_injector(
      options.fault_seed,
      sensor_fusion::faults::FaultInjector::Config{
          .enabled = true,
          .global_drop_prob = options.global_drop_prob,
          .global_delay_prob = options.global_delay_prob,
      },
      &metrics);

  const sensor_fusion::RadarConfig radar_cfg{
      .sensor_id = sensor_fusion::SensorId(1),
      .hz = options.hz,
      .max_range_m = 10000.0,
      .fov_deg = 120.0,
      .sigma_range_m = options.radar_sigma_range,
      .sigma_bearing_rad = sensor_fusion::deg_to_rad(options.radar_sigma_bearing_deg),
      .p_detect = options.radar_p_detect,
      .p_false_alarm = options.radar_p_false_alarm,
      .drop_prob = options.radar_drop_prob,
      .latency_mean_s = options.radar_latency_ms / 1000.0,
      .latency_jitter_s = options.radar_jitter_ms / 1000.0,
  };
  sensor_fusion::RadarSensor radar(radar_cfg, sensor_fusion::Rng(1234), &fault_injector);

  const sensor_fusion::EoirConfig eoir_cfg{
      .sensor_id = sensor_fusion::SensorId(2),
      .sigma_bearing_rad = sensor_fusion::deg_to_rad(options.eoir_sigma_bearing_deg),
      .sigma_elevation_rad = sensor_fusion::deg_to_rad(options.eoir_sigma_elevation_deg),
      .p_detect = 0.85,
      .drop_prob = 0.05,
      .latency_mean_s = options.radar_latency_ms / 1000.0,
      .latency_jitter_s = options.radar_jitter_ms / 1000.0,
  };
  sensor_fusion::EoirSensor eoir(eoir_cfg, sensor_fusion::Rng(5678), &fault_injector);

  sensor_fusion::fusion_core::MeasurementBuffer radar_buffer;
  sensor_fusion::fusion_core::MeasurementBuffer eoir_buffer;

  std::unique_ptr<sensor_fusion::observability::JsonlLogger> logger;
  if (options.enable_logging != 0) {
    logger = std::make_unique<sensor_fusion::observability::JsonlLogger>(options.log_path);
  }

  std::unique_ptr<sensor_fusion::observability::IncidentCapture> incident_capture;
  if (!options.incident_capture_path.empty()) {
    incident_capture = std::make_unique<sensor_fusion::observability::IncidentCapture>(
        options.incident_window_s, 50000);
  }

  uint64_t next_trace_id = 1;
  auto alloc_trace_id = [&next_trace_id]() { return next_trace_id++; };

  double current_sim_time_s = 0.0;
  fault_injector.set_observer([&](const std::string& fault_type) {
    if (incident_capture == nullptr) {
      return;
    }
    std::string line = "{\"type\":\"fault_event\",\"t_s\":" +
                       std::to_string(current_sim_time_s) + ",\"fault\":\"" + fault_type +
                       "\",\"trace_id\":" + std::to_string(alloc_trace_id()) +
                       ",\"causal_parent_id\":0}";
    incident_capture->add_event(current_sim_time_s, line);
  });

  auto emit_line = [&](const sensor_fusion::Timestamp& event_time, const std::string& line) {
    if (logger != nullptr) {
      logger->write_raw_line(line);
    }
    if (incident_capture != nullptr) {
      incident_capture->add_event(event_time.to_seconds(), line);
    }
  };

  sensor_fusion::fusion_core::TrackManager track_manager(
      sensor_fusion::fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = options.gate_mahal,
          .gate_mahal_eoir = options.gate_mahal,
          .confirm_hits = options.confirm_hits,
          .delete_misses = options.delete_misses,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = options.use_hungarian != 0,
          .enable_eoir_updates = options.enable_eoir_updates != 0,
          .unassigned_cost = 1e9,
      },
      &metrics);

  sensor_fusion::decision::MissionBehaviorTree mission_tree(sensor_fusion::decision::DecisionConfig{
      .engage_score_threshold = options.engage_score_threshold,
      .max_engagement_range_m = options.max_engagement_range_m,
      .min_confidence_to_engage = options.min_confidence_to_engage,
      .no_engage_zone_radius_m = options.no_engage_zone_radius_m,
      .engagement_timeout_s = options.engagement_timeout_s,
  });
  sensor_fusion::decision::MissionBlackboard mission_blackboard;
  std::vector<sensor_fusion::decision::SensorHealthFact> sensor_health = {
      sensor_fusion::decision::SensorHealthFact{
          .sensor_id = sensor_fusion::SensorId(1),
          .active = true,
          .stale = false,
          .last_seen_age_s = 0.0,
      },
  };
  if (options.enable_eoir != 0) {
    sensor_health.push_back(sensor_fusion::decision::SensorHealthFact{
        .sensor_id = sensor_fusion::SensorId(2),
        .active = true,
        .stale = false,
        .last_seen_age_s = 0.0,
    });
  }
  mission_blackboard.set_sensor_health(std::move(sensor_health));

  sensor_fusion::agents::InterceptorManager interceptor_manager(
      build_interceptors(std::max(0, options.interceptors), options.interceptor_speed));
  interceptor_manager.set_engagement_timeout_s(options.engagement_timeout_s);

  std::unordered_map<uint64_t, uint64_t> last_track_event_trace_by_track;
  std::unordered_map<uint64_t, EngagementInfo> engagement_by_interceptor;

  sensor_fusion::observability::MetricsSnapshot previous_snapshot;
  bool incident_captured = false;
  const size_t reserve_hint =
      std::max<size_t>(64, static_cast<size_t>(std::max(1, options.targets)) * 8);
  std::vector<sensor_fusion::Measurement> ready_radar;
  std::vector<sensor_fusion::Measurement> ready_eoir;
  std::vector<sensor_fusion::Measurement> combined_ready;
  ready_radar.reserve(reserve_hint);
  ready_eoir.reserve(reserve_hint);
  combined_ready.reserve(reserve_hint * 2);
  size_t ready_radar_capacity = ready_radar.capacity();
  size_t ready_eoir_capacity = ready_eoir.capacity();
  size_t combined_capacity = combined_ready.capacity();

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

  for (int i = 0; i < options.ticks; ++i) {
    const uint64_t misses_before = tick_profiler.stats().deadline_misses;
    tick_profiler.begin_tick();

    loop.next_tick();
    const double dt = loop.tick_dt();

    const auto truths = generator.step(dt);
    current_sim_time_s = generator.t_seconds();

    for (const auto& truth : truths) {
      truth_topic.publish(truth);
    }

    const auto radar_meas = radar.sense(truths);
    for (auto measurement : radar_meas) {
      if (measurement.trace_id == 0) {
        measurement.trace_id = alloc_trace_id();
      }
      radar_buffer.push(measurement);
    }

    if (options.enable_eoir != 0) {
      const auto eoir_meas = eoir.sense(truths);
      for (auto measurement : eoir_meas) {
        if (measurement.trace_id == 0) {
          measurement.trace_id = alloc_trace_id();
        }
        eoir_buffer.push(measurement);
      }
    }

    const sensor_fusion::Timestamp fusion_time =
        sensor_fusion::Timestamp::from_seconds(generator.t_seconds());

    radar_buffer.pop_ready(fusion_time, &ready_radar);
    eoir_buffer.pop_ready(fusion_time, &ready_eoir);
    combined_ready.clear();
    combined_ready.reserve(ready_radar.size() + ready_eoir.size());
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

    for (auto& measurement : combined_ready) {
      if (measurement.trace_id == 0) {
        measurement.trace_id = alloc_trace_id();
      }
      measurements_topic.publish(measurement);
      emit_line(measurement.t_sent,
                sensor_fusion::observability::serialize_measurement_json(
                    measurement, measurement.trace_id, measurement.causal_parent_id));
    }

    track_manager.predict_all(dt);
    const auto snapshot = track_manager.update_with_measurements(combined_ready);
    const auto& deltas = track_manager.last_deltas();
    const auto event_time = sensor_fusion::Timestamp::from_seconds(generator.t_seconds());

    for (const auto& track : deltas.created) {
      const uint64_t trace_id = alloc_trace_id();
      const uint64_t parent = track.quality().last_trace_id;
      last_track_event_trace_by_track[track.id().value] = trace_id;
      emit_line(event_time, sensor_fusion::observability::serialize_track_event_json(
                                "track_created", event_time, track, trace_id, parent));
      emit_line(event_time,
                sensor_fusion::observability::serialize_track_stability_event_json(
                    event_time, "track_created", track, "new_unassigned_measurement",
                    sensor_fusion::TrackId(0), -1.0, alloc_trace_id(), parent));
    }
    for (const auto& track : deltas.confirmed) {
      const uint64_t trace_id = alloc_trace_id();
      const uint64_t parent = track.quality().last_trace_id;
      last_track_event_trace_by_track[track.id().value] = trace_id;
      emit_line(event_time,
                sensor_fusion::observability::serialize_track_stability_event_json(
                    event_time, "track_confirmed", track, "confirm_hits_reached",
                    sensor_fusion::TrackId(0), -1.0, trace_id, parent));
    }
    for (const auto& track : deltas.updated) {
      const uint64_t trace_id = alloc_trace_id();
      const uint64_t parent = track.quality().last_trace_id;
      last_track_event_trace_by_track[track.id().value] = trace_id;
      emit_line(event_time, sensor_fusion::observability::serialize_track_event_json(
                                "track_updated", event_time, track, trace_id, parent));
    }
    for (const auto& track : deltas.coasted) {
      const uint64_t parent = last_track_event_trace_by_track.contains(track.id().value)
                                  ? last_track_event_trace_by_track[track.id().value]
                                  : track.quality().last_trace_id;
      emit_line(event_time,
                sensor_fusion::observability::serialize_track_stability_event_json(
                    event_time, "track_coasted", track, "missed_radar_update",
                    sensor_fusion::TrackId(0), -1.0, alloc_trace_id(), parent));
    }
    for (const auto& track : deltas.deleted) {
      const uint64_t trace_id = alloc_trace_id();
      const uint64_t parent = track.quality().last_trace_id;
      last_track_event_trace_by_track.erase(track.id().value);
      emit_line(event_time, sensor_fusion::observability::serialize_track_event_json(
                                "track_deleted", event_time, track, trace_id, parent));
      metrics.observe("track_lifetime_s",
                      track.quality().age_ticks * std::max(0.0, loop.tick_dt()));
      emit_line(event_time,
                sensor_fusion::observability::serialize_track_stability_event_json(
                    event_time, "track_deleted", track, "delete_misses_reached",
                    sensor_fusion::TrackId(0), -1.0, alloc_trace_id(), parent));
    }
    for (const auto& warning : deltas.fragmentation_warnings) {
      const auto created_it = std::find_if(
          deltas.created.begin(), deltas.created.end(),
          [&](const sensor_fusion::fusion_core::Track& track) {
            return track.id() == warning.new_track_id;
          });
      if (created_it == deltas.created.end()) {
        continue;
      }
      emit_line(event_time,
                sensor_fusion::observability::serialize_track_stability_event_json(
                    event_time, "track_fragmentation_warning", *created_it,
                    "new_track_near_recent_deleted_confirmed_track", warning.original_track_id,
                    warning.distance_m, alloc_trace_id(), 0));
    }

    size_t tentative_count = 0;
    size_t confirmed_count = 0;
    uint64_t confirmed_age_ticks_sum = 0;
    std::vector<sensor_fusion::decision::TrackFact> blackboard_tracks;
    std::vector<std::pair<sensor_fusion::TrackId, std::array<double, 3>>> confirmed_positions;

    for (const auto& track : snapshot) {
      if (track.status() == sensor_fusion::fusion_core::TrackStatus::Tentative) {
        ++tentative_count;
      }

      const auto x = track.filter().state();
      const std::array<double, 3> pos = {x(0), x(1), x(2)};
      const double threat = sensor_fusion::decision::compute_threat_score(track);
      blackboard_tracks.push_back(sensor_fusion::decision::TrackFact{
          .track_id = track.id(),
          .status = track.status(),
          .position = pos,
          .confidence = track.quality().confidence,
          .score = track.quality().score,
          .priority = threat,
      });

      if (track.status() == sensor_fusion::fusion_core::TrackStatus::Confirmed) {
        ++confirmed_count;
        confirmed_age_ticks_sum += track.quality().age_ticks;
        confirmed_positions.emplace_back(track.id(), pos);
        metrics.observe("threat_score", threat);
        metrics.inc("threats_confirmed_total");
      }
    }

    std::unordered_map<uint64_t, std::array<double, 3>> confirmed_pos_by_track;
    confirmed_pos_by_track.reserve(confirmed_positions.size());
    for (const auto& [track_id, pos] : confirmed_positions) {
      confirmed_pos_by_track.emplace(track_id.value, pos);
    }

    metrics.set_gauge("tracks_total", static_cast<double>(snapshot.size()));
    metrics.set_gauge("tracks_confirmed", static_cast<double>(confirmed_count));
    metrics.set_gauge(
        "tracks_confirmed_avg_age_s",
        confirmed_count == 0 ? 0.0
                             : (static_cast<double>(confirmed_age_ticks_sum) /
                                static_cast<double>(confirmed_count)) *
                                   std::max(0.0, loop.tick_dt()));

    std::cout << "t=" << generator.t_seconds() << " meas=" << combined_ready.size()
              << " tracks=" << snapshot.size() << " tentative=" << tentative_count
              << " confirmed=" << confirmed_count << std::endl;

    interceptor_manager.update_track_positions(confirmed_positions);

    if (options.enable_interceptor != 0) {
      mission_blackboard.set_tick(static_cast<uint64_t>(i + 1), generator.t_seconds());
      mission_blackboard.set_tracks(std::move(blackboard_tracks));
      mission_blackboard.set_interceptors_from_states(interceptor_manager.states());
      const sensor_fusion::decision::BtTickResult bt_result =
          mission_tree.tick(mission_blackboard);
      emit_line(event_time, sensor_fusion::observability::serialize_bt_decision_json(
                                event_time, bt_result, alloc_trace_id(), 0));
      metrics.inc("bt_ticks_total");
      metrics.inc(std::string("bt_mode_") +
                  sensor_fusion::decision::mission_mode_to_string(bt_result.mode));
      if (bt_result.event.decision_type == "engage_denied") {
        metrics.inc("engage_denied_total");
        const uint64_t parent =
            last_track_event_trace_by_track.contains(bt_result.event.track_id.value)
                ? last_track_event_trace_by_track[bt_result.event.track_id.value]
                : 0;
        emit_line(event_time,
                  sensor_fusion::observability::serialize_assignment_event_json(
                      event_time, 0, bt_result.event.track_id, "engage_denied",
                      bt_result.event.reason, alloc_trace_id(), parent));
      }

      std::vector<std::pair<uint64_t, sensor_fusion::TrackId>> assignments;
      assignments.reserve(bt_result.engagement_commands.size());
      for (const auto& command : bt_result.engagement_commands) {
        assignments.emplace_back(command.interceptor_id, command.track_id);
        metrics.inc("assignments_total");

        const uint64_t parent = last_track_event_trace_by_track.contains(command.track_id.value)
                                    ? last_track_event_trace_by_track[command.track_id.value]
                                    : 0;
        const uint64_t trace = alloc_trace_id();
        emit_line(event_time,
                  sensor_fusion::observability::serialize_assignment_event_json(
                      event_time, command.interceptor_id, command.track_id, "engage_assigned",
                      command.reason, trace, parent));

        engagement_by_interceptor[command.interceptor_id] =
            EngagementInfo{.track_id = command.track_id, .start_time_s = generator.t_seconds()};
      }

      interceptor_manager.assign(assignments);
      interceptor_manager.step(dt);

      std::vector<std::pair<uint64_t, sensor_fusion::TrackId>> hits;
      size_t engaged_count = 0;
      for (const auto& state : interceptor_manager.states()) {
        if (state.engaged) {
          ++engaged_count;
          const auto pos_it = confirmed_pos_by_track.find(state.target_id.value);
          if (pos_it != confirmed_pos_by_track.end()) {
            if (distance3(state.position, pos_it->second) < 5.0) {
              hits.emplace_back(state.interceptor_id, state.target_id);
            }
          }
        }

        emit_line(event_time,
                  sensor_fusion::observability::serialize_interceptor_state_json(
                      event_time, state, alloc_trace_id(), 0));
      }

      interceptor_manager.handle_intercepts(hits);

      for (const auto& [interceptor_id, track_id] : hits) {
        mission_blackboard.clear_engagement();
        metrics.inc("successful_intercepts_total");
        metrics.inc("kills_total");

        const auto engagement_it = engagement_by_interceptor.find(interceptor_id);
        if (engagement_it != engagement_by_interceptor.end()) {
          const double dt_ms = (generator.t_seconds() - engagement_it->second.start_time_s) * 1000.0;
          metrics.observe("time_to_intercept_ms", dt_ms);
          metrics.observe("time_to_intercept_ms_interceptor_" + std::to_string(interceptor_id),
                          dt_ms);
          engagement_by_interceptor.erase(engagement_it);
        }

        emit_line(event_time,
                  sensor_fusion::observability::serialize_intercept_event_json(
                      event_time, track_id, "intercept_success", "distance_threshold_met",
                      alloc_trace_id(), 0));
      }

      for (auto it = engagement_by_interceptor.begin(); it != engagement_by_interceptor.end();) {
        const uint64_t interceptor_id = it->first;
        const bool still_engaged = std::any_of(
            interceptor_manager.states().begin(), interceptor_manager.states().end(),
            [&](const sensor_fusion::agents::InterceptorState& state) {
              return state.interceptor_id == interceptor_id && state.engaged;
            });

        if (!still_engaged) {
          mission_blackboard.clear_engagement();
          if ((generator.t_seconds() - it->second.start_time_s) > options.engagement_timeout_s) {
            metrics.inc("intercept_fail_total");
          }
          it = engagement_by_interceptor.erase(it);
        } else {
          ++it;
        }
      }

      const double idle_count =
          static_cast<double>(interceptor_manager.states().size() - engaged_count);
      metrics.set_gauge("interceptors_idle", idle_count);
      metrics.set_gauge("interceptors_engaged", static_cast<double>(engaged_count));
    }

    if (viz_state != nullptr) {
      sensor_fusion::viz::VizSnapshot viz_snapshot;
      viz_snapshot.t_s = generator.t_seconds();
      viz_snapshot.truth_positions.reserve(truths.size());
      for (const auto& truth : truths) {
        viz_snapshot.truth_positions.push_back(truth.position_xyz);
      }

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

      const auto interceptor_states = interceptor_manager.states();
      viz_snapshot.interceptors.reserve(interceptor_states.size());
      for (const auto& state : interceptor_states) {
        sensor_fusion::viz::VizSnapshot::InterceptorViz interceptor_viz;
        interceptor_viz.id = state.interceptor_id;
        interceptor_viz.engaged = state.engaged;
        interceptor_viz.pos = state.position;
        if (state.engaged && state.target_id.value != 0) {
          interceptor_viz.target_track_id = state.target_id.value;
          const auto target_it = confirmed_pos_by_track.find(state.target_id.value);
          if (target_it != confirmed_pos_by_track.end()) {
            viz_snapshot.engagement_lines.emplace_back(state.position, target_it->second);
          }
        }
        viz_snapshot.interceptors.push_back(interceptor_viz);
      }

      std::lock_guard<std::mutex> lock(viz_state->mutex);
      viz_state->latest = std::move(viz_snapshot);
      viz_state->has_snapshot = true;
    }

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

    const auto snapshot_metrics = metrics.snapshot();
    const uint64_t assoc_attempts_delta =
        get_counter(snapshot_metrics, "assoc_attempts") -
        get_counter(previous_snapshot, "assoc_attempts");
    const uint64_t assoc_success_delta =
        get_counter(snapshot_metrics, "assoc_success") -
        get_counter(previous_snapshot, "assoc_success");

    std::string incident_reason;
    if (assoc_attempts_delta >= 5) {
      const double failure_rate =
          1.0 - static_cast<double>(assoc_success_delta) / static_cast<double>(assoc_attempts_delta);
      if (failure_rate >= 0.6) {
        incident_reason = "association_failure_spike";
      }
    }

    if (!incident_reason.empty() && incident_capture != nullptr && !incident_captured) {
      metrics.inc("incident_captured_total");
      const uint64_t trace = alloc_trace_id();
      emit_line(event_time,
                "{\"type\":\"health_event\",\"t_s\":" +
                    std::to_string(event_time.to_seconds()) +
                    ",\"event_type\":\"incident_trigger\",\"reason\":\"" +
                    incident_reason + "\",\"trace_id\":" + std::to_string(trace) +
                    ",\"causal_parent_id\":0}");

      incident_capture->dump_to_file(options.incident_capture_path, event_time.to_seconds());
      incident_captured = true;
    }

    previous_snapshot = snapshot_metrics;

    tick_profiler.end_tick();
    metrics.observe("tick_ms", tick_profiler.last_tick_ms());
    if (tick_profiler.stats().deadline_misses > misses_before) {
      metrics.inc("deadline_misses_total");
    }

    if (options.profile_every > 0 && ((i + 1) % options.profile_every == 0)) {
      std::cout << "[sim_profile] tick=" << (i + 1)
                << " mean_ms=" << tick_profiler.stats().mean_ms
                << " max_ms=" << tick_profiler.stats().max_ms
                << " deadline_misses=" << tick_profiler.stats().deadline_misses << std::endl;
    }

    if (options.metrics_dump_every > 0 && ((i + 1) % options.metrics_dump_every == 0)) {
      metrics.dump();
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

  std::cout << "Sensor Fusion Engine initialized." << std::endl;
  return 0;
}
