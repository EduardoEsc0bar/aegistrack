#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "faults/fault_injector.h"
#include "observability/metrics.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "services/telemetry_ingest/telemetry_ingest_service.h"
#include "transport/message_bus.h"

namespace {

struct CliOptions {
  std::string bind_addr = "0.0.0.0:50051";
  uint64_t log_every_batches = 50;
  size_t queue_max = 10000;
  double stale_timeout_s = 2.0;
  std::string forward_addr;
  uint64_t fault_seed = 2026;
  double global_drop_prob = 0.0;
  double global_delay_prob = 0.0;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--bind_addr" && i + 1 < argc) {
      options.bind_addr = argv[++i];
    } else if (arg == "--log_every_batches" && i + 1 < argc) {
      options.log_every_batches = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--queue_max" && i + 1 < argc) {
      options.queue_max = static_cast<size_t>(std::stoull(argv[++i]));
    } else if (arg == "--stale_timeout_s" && i + 1 < argc) {
      options.stale_timeout_s = std::stod(argv[++i]);
    } else if (arg == "--forward_addr" && i + 1 < argc) {
      options.forward_addr = argv[++i];
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
          .enable_stale_monitor_thread = true,
          .log_every_batches = options.log_every_batches,
          .stale_timeout_s = options.stale_timeout_s,
          .fault_injector = &fault_injector,
          .queue_fault_drop_prob = 0.0,
          .queue_fault_delay_prob = 0.0,
          .queue_fault_jitter_s = 0.2,
      });
  sensor_fusion::services::telemetry_ingest::TelemetryIngestServiceImpl telemetry_service(
      &pipeline, options.forward_addr);
  sensor_fusion::services::telemetry_ingest::HealthServiceImpl health_service(&pipeline);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(options.bind_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&telemetry_service);
  builder.RegisterService(&health_service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  if (server == nullptr) {
    std::cerr << "failed to start telemetry_ingest on " << options.bind_addr << std::endl;
    return 1;
  }

  std::cout << "telemetry_ingest listening on " << options.bind_addr << std::endl;
  if (!options.forward_addr.empty()) {
    std::cout << "forwarding batches to " << options.forward_addr << std::endl;
  }

  server->Wait();
  return 0;
}
