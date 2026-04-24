#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/fixed_rate_loop.h"
#include "common/rng.h"
#include "detections.pb.h"
#include "faults/fault_injector.h"
#include "observability/metrics.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "transport/message_bus.h"

namespace {

struct CliOptions {
  int virtual_sensors = 20;
  double hz = 200.0;
  int duration_s = 10;
  uint64_t seed = 1;
  size_t queue_max = 10000;
  int burst_mode = 0;
  double drop_prob = 0.0;
  double reorder_prob = 0.0;
  uint64_t fault_seed = 1;
  double global_drop_prob = 0.0;
  double global_delay_prob = 0.0;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--virtual_sensors" && i + 1 < argc) {
      options.virtual_sensors = std::stoi(argv[++i]);
    } else if (arg == "--hz" && i + 1 < argc) {
      options.hz = std::stod(argv[++i]);
    } else if (arg == "--duration_s" && i + 1 < argc) {
      options.duration_s = std::stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      options.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--queue_max" && i + 1 < argc) {
      options.queue_max = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (arg == "--burst_mode" && i + 1 < argc) {
      options.burst_mode = std::stoi(argv[++i]);
    } else if (arg == "--drop_prob" && i + 1 < argc) {
      options.drop_prob = std::stod(argv[++i]);
    } else if (arg == "--reorder_prob" && i + 1 < argc) {
      options.reorder_prob = std::stod(argv[++i]);
    } else if (arg == "--fault_seed" && i + 1 < argc) {
      options.fault_seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--global_drop_prob" && i + 1 < argc) {
      options.global_drop_prob = std::stod(argv[++i]);
    } else if (arg == "--global_delay_prob" && i + 1 < argc) {
      options.global_delay_prob = std::stod(argv[++i]);
    }
  }
  return options;
}

aegistrack::telemetry::v1::DetectionBatch make_batch(uint64_t sensor_id,
                                                      uint64_t seq,
                                                      double t_s,
                                                      double target_offset_x,
                                                      sensor_fusion::Rng& rng) {
  const double x = target_offset_x + 20.0 + 10.0 * t_s;
  const double y = 5.0 + 1.0 * t_s;
  const double range = std::sqrt(x * x + y * y) + rng.normal(0.0, 1.5);
  const double bearing = std::atan2(y, x) + rng.normal(0.0, 0.01);

  aegistrack::telemetry::v1::DetectionBatch batch;
  auto* header = batch.mutable_header();
  header->set_seq(seq);
  header->set_t_meas_s(t_s);
  header->set_t_sent_s(t_s);
  header->set_source_id("virtual_sensor_" + std::to_string(sensor_id));
  header->set_frame_id("world");

  auto* det = batch.add_detections();
  *det->mutable_header() = *header;
  det->set_sensor_id(sensor_id);
  det->set_sensor_type("Radar");
  det->set_measurement_type("RangeBearing2D");
  det->set_z_dim(2);
  det->set_confidence(0.9);
  det->add_z(range);
  det->add_z(bearing);
  det->add_r(4.0);
  det->add_r(0.0);
  det->add_r(0.0);
  det->add_r(0.01);
  return batch;
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  sensor_fusion::MessageBus bus;
  sensor_fusion::observability::Metrics metrics;
  sensor_fusion::faults::FaultInjector fault_injector(
      options.fault_seed,
      sensor_fusion::faults::FaultInjector::Config{
          .enabled = true,
          .global_drop_prob = options.global_drop_prob,
          .global_delay_prob = options.global_delay_prob,
      },
      &metrics);
  sensor_fusion::services::telemetry_ingest::IngestPipeline pipeline(
      &bus, &metrics,
      sensor_fusion::services::telemetry_ingest::IngestPipeline::Options{
          .max_queue = options.queue_max,
          .drop_oldest = true,
          .enable_worker_thread = true,
          .enable_stale_monitor_thread = false,
          .log_every_batches = 0,
          .stale_timeout_s = 2.0,
          .fault_injector = &fault_injector,
          .queue_fault_drop_prob = 0.0,
          .queue_fault_delay_prob = 0.0,
          .queue_fault_jitter_s = 0.05,
      });

  uint64_t consumed = 0;
  bus.topic<sensor_fusion::Measurement>("measurements")
      .subscribe([&consumed](const sensor_fusion::Measurement&) { ++consumed; });

  std::vector<sensor_fusion::Rng> sensor_rngs;
  sensor_rngs.reserve(static_cast<size_t>(options.virtual_sensors));
  std::vector<uint64_t> seqs(static_cast<size_t>(options.virtual_sensors), 1);
  std::vector<std::unique_ptr<aegistrack::telemetry::v1::DetectionBatch>> delayed(
      static_cast<size_t>(options.virtual_sensors));
  for (int i = 0; i < options.virtual_sensors; ++i) {
    sensor_rngs.emplace_back(options.seed + static_cast<uint64_t>(i) * 1001ULL);
  }

  const auto start = std::chrono::steady_clock::now();
  const int total_ticks = static_cast<int>(options.duration_s * options.hz);
  sensor_fusion::FixedRateLoop loop(options.hz);
  uint64_t sent_detections = 0;

  for (int tick = 0; tick < total_ticks; ++tick) {
    loop.next_tick();
    const double sim_t = static_cast<double>(tick + 1) * loop.tick_dt();

    const bool pause_for_burst = (options.burst_mode != 0 && (tick % 20) >= 5);
    const int batches_per_tick =
        (options.burst_mode != 0 && (tick % 20) < 5) ? 4 : 1;
    if (pause_for_burst) {
      continue;
    }

    for (int sensor = 0; sensor < options.virtual_sensors; ++sensor) {
      for (int b = 0; b < batches_per_tick; ++b) {
        if (fault_injector.should_drop(options.drop_prob)) {
          continue;
        }

        auto batch = std::make_unique<aegistrack::telemetry::v1::DetectionBatch>(
            make_batch(static_cast<uint64_t>(sensor + 1), seqs[static_cast<size_t>(sensor)]++,
                       sim_t, 50.0 * static_cast<double>(sensor),
                       sensor_rngs[static_cast<size_t>(sensor)]));

        if (delayed[static_cast<size_t>(sensor)] != nullptr) {
          if (fault_injector.should_delay(options.reorder_prob)) {
            pipeline.ingest_batch(*batch);
            pipeline.ingest_batch(*delayed[static_cast<size_t>(sensor)]);
          } else {
            pipeline.ingest_batch(*delayed[static_cast<size_t>(sensor)]);
            pipeline.ingest_batch(*batch);
          }
          sent_detections += static_cast<uint64_t>(batch->detections_size());
          sent_detections += static_cast<uint64_t>(
              delayed[static_cast<size_t>(sensor)]->detections_size());
          delayed[static_cast<size_t>(sensor)].reset();
        } else if (fault_injector.should_delay(options.reorder_prob)) {
          delayed[static_cast<size_t>(sensor)] = std::move(batch);
        } else {
          pipeline.ingest_batch(*batch);
          sent_detections += static_cast<uint64_t>(batch->detections_size());
        }
      }
    }
  }

  for (auto& pending : delayed) {
    if (pending != nullptr) {
      pipeline.ingest_batch(*pending);
      sent_detections += static_cast<uint64_t>(pending->detections_size());
    }
  }

  while (pipeline.queue_depth() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  const auto elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const double rate = elapsed > 0.0 ? static_cast<double>(pipeline.published_total()) / elapsed
                                    : 0.0;
  const auto snap = metrics.snapshot();
  const uint64_t drops =
      snap.counters.contains("ingest_queue_drops_total")
          ? snap.counters.at("ingest_queue_drops_total")
          : 0;

  std::cout << "load_test sent_detections=" << sent_detections
            << " published=" << pipeline.published_total() << " consumed=" << consumed
            << " ingest_queue_drops_total=" << drops << " rate_dps=" << rate << std::endl;
  return 0;
}
