#include <doctest/doctest.h>

#include "detections.pb.h"
#include "observability/metrics.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "transport/message_bus.h"

namespace {

aegistrack::telemetry::v1::DetectionBatch make_batch(uint64_t seq, uint64_t sensor_id) {
  aegistrack::telemetry::v1::DetectionBatch batch;
  auto* header = batch.mutable_header();
  header->set_seq(seq);
  header->set_t_meas_s(static_cast<double>(seq) * 0.1);
  header->set_t_sent_s(static_cast<double>(seq) * 0.1);
  header->set_source_id("test_sensor");
  header->set_frame_id("world");

  auto* detection = batch.add_detections();
  *detection->mutable_header() = *header;
  detection->set_sensor_id(sensor_id);
  detection->set_sensor_type("Radar");
  detection->set_measurement_type("RangeBearing2D");
  detection->set_z_dim(2);
  detection->set_confidence(0.9);
  detection->add_z(100.0);
  detection->add_z(0.0);
  detection->add_r(4.0);
  detection->add_r(0.0);
  detection->add_r(0.0);
  detection->add_r(0.01);
  return batch;
}

}  // namespace

TEST_CASE("ingest queue bounded behavior increments drop counter") {
  sensor_fusion::MessageBus bus;
  sensor_fusion::observability::Metrics metrics;
  sensor_fusion::services::telemetry_ingest::IngestPipeline pipeline(
      &bus, &metrics,
      sensor_fusion::services::telemetry_ingest::IngestPipeline::Options{
          .max_queue = 3,
          .drop_oldest = true,
          .enable_worker_thread = false,
          .enable_stale_monitor_thread = false,
          .log_every_batches = 0,
          .stale_timeout_s = 2.0,
      });

  for (uint64_t i = 1; i <= 8; ++i) {
    pipeline.ingest_batch(make_batch(i, 1));
  }

  CHECK(pipeline.queue_depth() == 3);
  CHECK(pipeline.queue_drops() == 5);

  const auto snap = metrics.snapshot();
  CHECK(snap.counters.contains("ingest_queue_drops_total"));
  CHECK(snap.counters.at("ingest_queue_drops_total") == 5);
}
