#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "common/angles.h"
#include "common/fixed_rate_loop.h"
#include "common/rng.h"
#include "detections.pb.h"
#include "faults/fault_injector.h"
#include "health.grpc.pb.h"
#include "messages/types.h"
#include "sensors/radar/radar_sensor.h"
#include "telemetry.grpc.pb.h"

namespace {

struct CliOptions {
  std::string ingest_addr = "localhost:50051";
  uint64_t sensor_id = 1;
  double hz = 20.0;
  uint64_t seed = 1;
  double target_offset_x = 0.0;
  int ticks = 200;
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
    if (arg == "--ingest_addr" && i + 1 < argc) {
      options.ingest_addr = argv[++i];
    } else if (arg == "--sensor_id" && i + 1 < argc) {
      options.sensor_id = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--hz" && i + 1 < argc) {
      options.hz = std::stod(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      options.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--target_offset_x" && i + 1 < argc) {
      options.target_offset_x = std::stod(argv[++i]);
    } else if (arg == "--ticks" && i + 1 < argc) {
      options.ticks = std::stoi(argv[++i]);
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

std::string sensor_type_to_string(sensor_fusion::SensorType type) {
  switch (type) {
    case sensor_fusion::SensorType::Radar:
      return "Radar";
    case sensor_fusion::SensorType::EOIR:
      return "EOIR";
    case sensor_fusion::SensorType::ADSB:
      return "ADSB";
  }
  return "Radar";
}

std::string measurement_type_to_string(sensor_fusion::MeasurementType type) {
  switch (type) {
    case sensor_fusion::MeasurementType::RangeBearing2D:
      return "RangeBearing2D";
    case sensor_fusion::MeasurementType::BearingElevation:
      return "BearingElevation";
    case sensor_fusion::MeasurementType::Position3D:
      return "Position3D";
  }
  return "RangeBearing2D";
}

void fill_detection(const sensor_fusion::Measurement& measurement,
                    uint64_t seq,
                    const std::string& source_id,
                    aegistrack::telemetry::v1::Detection* out) {
  auto* header = out->mutable_header();
  header->set_seq(seq);
  header->set_t_meas_s(measurement.t_meas.to_seconds());
  header->set_t_sent_s(measurement.t_sent.to_seconds());
  header->set_source_id(source_id);
  header->set_frame_id("world");

  out->set_sensor_id(measurement.sensor_id.value);
  out->set_sensor_type(sensor_type_to_string(measurement.sensor_type));
  out->set_measurement_type(measurement_type_to_string(measurement.measurement_type));
  out->set_z_dim(measurement.z_dim);
  out->set_confidence(measurement.confidence);
  for (double value : measurement.z) {
    out->add_z(value);
  }
  for (double value : measurement.R_rowmajor) {
    out->add_r(value);
  }
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  auto channel =
      grpc::CreateChannel(options.ingest_addr, grpc::InsecureChannelCredentials());
  auto telemetry_stub = aegistrack::telemetry::v1::TelemetryIngestService::NewStub(channel);
  auto health_stub = aegistrack::telemetry::v1::HealthService::NewStub(channel);

  grpc::ClientContext context;
  aegistrack::telemetry::v1::Ack ack;
  std::unique_ptr<grpc::ClientWriter<aegistrack::telemetry::v1::DetectionBatch>> writer =
      telemetry_stub->StreamDetections(&context, &ack);

  grpc::ClientContext health_context;
  std::shared_ptr<grpc::ClientReaderWriter<aegistrack::telemetry::v1::HealthPing,
                                           aegistrack::telemetry::v1::HealthPong>>
      health_stream = health_stub->Heartbeat(&health_context);

  sensor_fusion::RadarConfig radar_cfg{
      .sensor_id = sensor_fusion::SensorId(options.sensor_id),
      .hz = options.hz,
      .max_range_m = 20000.0,
      .fov_deg = 120.0,
      .sigma_range_m = 2.0,
      .sigma_bearing_rad = sensor_fusion::deg_to_rad(0.8),
      .p_detect = 0.95,
      .p_false_alarm = 0.05,
      .drop_prob = 0.05,
      .latency_mean_s = 0.05,
      .latency_jitter_s = 0.01,
  };
  sensor_fusion::faults::FaultInjector fault_injector(
      options.fault_seed,
      sensor_fusion::faults::FaultInjector::Config{
          .enabled = true,
          .global_drop_prob = options.global_drop_prob,
          .global_delay_prob = options.global_delay_prob,
      });
  sensor_fusion::RadarSensor radar(
      radar_cfg, sensor_fusion::Rng(options.seed), &fault_injector);

  sensor_fusion::FixedRateLoop loop(options.hz);
  uint64_t seq = 1;
  double elapsed_since_heartbeat = 0.0;
  std::unique_ptr<aegistrack::telemetry::v1::DetectionBatch> delayed_batch;
  bool stream_failed = false;

  std::array<double, 3> position = {options.target_offset_x, 0.0, 0.0};
  const std::array<double, 3> velocity = {10.0, 1.0, 0.0};

  auto write_batch = [&](const aegistrack::telemetry::v1::DetectionBatch& batch) -> bool {
    return writer->Write(batch);
  };

  for (int tick = 0; tick < options.ticks; ++tick) {
    loop.next_tick();
    const double dt = loop.tick_dt();
    elapsed_since_heartbeat += dt;
    for (size_t i = 0; i < 3; ++i) {
      position[i] += velocity[i] * dt;
    }
    const double sim_time_s = static_cast<double>(tick + 1) * dt;

    const sensor_fusion::TruthState truth{
        .t = sensor_fusion::Timestamp::from_seconds(sim_time_s),
        .target_id = 1,
        .position_xyz = position,
        .velocity_xyz = velocity,
    };

    const std::vector<sensor_fusion::Measurement> measurements = radar.sense({truth});
    const int batches_this_tick =
        (options.burst_mode != 0 && (tick % 20) < 5) ? 4 : 1;
    const bool pause_for_burst = (options.burst_mode != 0 && (tick % 20) >= 5);

    if (!pause_for_burst) {
      for (int burst_i = 0; burst_i < batches_this_tick; ++burst_i) {
        aegistrack::telemetry::v1::DetectionBatch batch;
        auto* header = batch.mutable_header();
        header->set_seq(seq);
        header->set_t_meas_s(sim_time_s);
        header->set_t_sent_s(sim_time_s);
        header->set_source_id("sensor_node_" + std::to_string(options.sensor_id));
        header->set_frame_id("world");

        for (const auto& measurement : measurements) {
          fill_detection(measurement, seq, header->source_id(), batch.add_detections());
        }

        ++seq;

        if (fault_injector.should_drop(options.drop_prob)) {
          continue;
        }

        if (delayed_batch != nullptr) {
          if (fault_injector.should_delay(options.reorder_prob)) {
            if (!write_batch(batch) || !write_batch(*delayed_batch)) {
              std::cerr << "stream write failed to " << options.ingest_addr << std::endl;
              stream_failed = true;
              break;
            }
          } else {
            if (!write_batch(*delayed_batch) || !write_batch(batch)) {
              std::cerr << "stream write failed to " << options.ingest_addr << std::endl;
              stream_failed = true;
              break;
            }
          }
          delayed_batch.reset();
        } else if (fault_injector.should_delay(options.reorder_prob)) {
          delayed_batch = std::make_unique<aegistrack::telemetry::v1::DetectionBatch>(batch);
        } else if (!write_batch(batch)) {
          std::cerr << "stream write failed to " << options.ingest_addr << std::endl;
          stream_failed = true;
          break;
        }
      }
    }

    if (stream_failed) {
      break;
    }

    if (elapsed_since_heartbeat >= 1.0) {
      aegistrack::telemetry::v1::HealthPing ping;
      auto* header = ping.mutable_header();
      header->set_seq(seq);
      header->set_t_meas_s(sim_time_s);
      header->set_t_sent_s(sim_time_s);
      header->set_source_id("sensor_node_" + std::to_string(options.sensor_id));
      header->set_frame_id("world");
      ping.set_sensor_id(options.sensor_id);
      ping.set_cpu_pct(15.0 + static_cast<double>(options.sensor_id % 10));
      ping.set_mem_mb(128.0 + static_cast<double>(options.sensor_id));
      (void)health_stream->Write(ping);

      aegistrack::telemetry::v1::HealthPong pong;
      (void)health_stream->Read(&pong);
      elapsed_since_heartbeat = 0.0;
    }
  }

  if (!stream_failed && delayed_batch != nullptr) {
    (void)write_batch(*delayed_batch);
  }

  writer->WritesDone();
  health_stream->WritesDone();
  aegistrack::telemetry::v1::HealthPong final_pong;
  while (health_stream->Read(&final_pong)) {
  }

  const grpc::Status status = writer->Finish();
  const grpc::Status health_status = health_stream->Finish();
  if (!status.ok()) {
    std::cerr << "sensor_node stream failed: " << status.error_message() << std::endl;
    return 1;
  }
  if (!health_status.ok()) {
    std::cerr << "sensor_node health stream failed: " << health_status.error_message()
              << std::endl;
    return 1;
  }

  std::cout << "sensor_node sent batches, ack batches_received="
            << ack.batches_received() << std::endl;
  return 0;
}
