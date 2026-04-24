#include "services/telemetry_ingest/telemetry_ingest_service.h"

#include <memory>
#include <utility>

namespace sensor_fusion::services::telemetry_ingest {

TelemetryIngestServiceImpl::TelemetryIngestServiceImpl(IngestPipeline* pipeline,
                                                       std::string forward_addr)
    : pipeline_(pipeline), forward_addr_(std::move(forward_addr)) {}

grpc::Status TelemetryIngestServiceImpl::StreamDetections(
    grpc::ServerContext* /*context*/,
    grpc::ServerReader<aegistrack::telemetry::v1::DetectionBatch>* reader,
    aegistrack::telemetry::v1::Ack* response) {
  std::unique_ptr<aegistrack::telemetry::v1::TelemetryIngestService::Stub> forward_stub;
  grpc::ClientContext forward_context;
  aegistrack::telemetry::v1::Ack forward_ack;
  std::unique_ptr<grpc::ClientWriter<aegistrack::telemetry::v1::DetectionBatch>> forward_writer;

  if (!forward_addr_.empty()) {
    auto channel = grpc::CreateChannel(forward_addr_, grpc::InsecureChannelCredentials());
    forward_stub = aegistrack::telemetry::v1::TelemetryIngestService::NewStub(channel);
    forward_writer = forward_stub->StreamDetections(&forward_context, &forward_ack);
  }

  aegistrack::telemetry::v1::DetectionBatch batch;
  uint64_t stream_batches = 0;
  while (reader->Read(&batch)) {
    ++stream_batches;
    pipeline_->ingest_batch(batch);

    if (forward_writer != nullptr) {
      if (!forward_writer->Write(batch)) {
        break;
      }
    }
  }

  if (forward_writer != nullptr) {
    forward_writer->WritesDone();
    (void)forward_writer->Finish();
  }

  response->set_batches_received(stream_batches);
  return grpc::Status::OK;
}

HealthServiceImpl::HealthServiceImpl(IngestPipeline* pipeline) : pipeline_(pipeline) {}

grpc::Status HealthServiceImpl::Heartbeat(
    grpc::ServerContext* /*context*/,
    grpc::ServerReaderWriter<aegistrack::telemetry::v1::HealthPong,
                             aegistrack::telemetry::v1::HealthPing>* stream) {
  aegistrack::telemetry::v1::HealthPing ping;
  while (stream->Read(&ping)) {
    pipeline_->ingest_health_ping(ping);

    aegistrack::telemetry::v1::HealthPong pong;
    *pong.mutable_header() = ping.header();
    pong.set_status("ok");
    if (!stream->Write(pong)) {
      break;
    }
  }

  return grpc::Status::OK;
}

}  // namespace sensor_fusion::services::telemetry_ingest
