#pragma once

#include <string>

#include <grpcpp/grpcpp.h>

#include "health.grpc.pb.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "telemetry.grpc.pb.h"

namespace sensor_fusion::services::telemetry_ingest {

class TelemetryIngestServiceImpl final
    : public aegistrack::telemetry::v1::TelemetryIngestService::Service {
 public:
  explicit TelemetryIngestServiceImpl(IngestPipeline* pipeline, std::string forward_addr = "");

  grpc::Status StreamDetections(
      grpc::ServerContext* context,
      grpc::ServerReader<aegistrack::telemetry::v1::DetectionBatch>* reader,
      aegistrack::telemetry::v1::Ack* response) override;

 private:
  IngestPipeline* pipeline_;
  std::string forward_addr_;
};

class HealthServiceImpl final : public aegistrack::telemetry::v1::HealthService::Service {
 public:
  explicit HealthServiceImpl(IngestPipeline* pipeline);

  grpc::Status Heartbeat(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<aegistrack::telemetry::v1::HealthPong,
                               aegistrack::telemetry::v1::HealthPing>* stream) override;

 private:
  IngestPipeline* pipeline_;
};

}  // namespace sensor_fusion::services::telemetry_ingest
