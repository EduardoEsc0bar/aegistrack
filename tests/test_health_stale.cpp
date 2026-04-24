#include <doctest/doctest.h>

#include "health.pb.h"
#include "observability/metrics.h"
#include "services/telemetry_ingest/ingest_pipeline.h"
#include "transport/message_bus.h"

TEST_CASE("health stale detection increments stale counter") {
  sensor_fusion::MessageBus bus;
  sensor_fusion::observability::Metrics metrics;
  double now_s = 0.0;

  sensor_fusion::services::telemetry_ingest::IngestPipeline pipeline(
      &bus, &metrics,
      sensor_fusion::services::telemetry_ingest::IngestPipeline::Options{
          .max_queue = 100,
          .drop_oldest = true,
          .enable_worker_thread = false,
          .enable_stale_monitor_thread = false,
          .log_every_batches = 0,
          .stale_timeout_s = 2.0,
      },
      [&]() { return sensor_fusion::Timestamp::from_seconds(now_s); });

  aegistrack::telemetry::v1::HealthPing ping;
  ping.mutable_header()->set_seq(1);
  ping.mutable_header()->set_t_meas_s(0.0);
  ping.mutable_header()->set_t_sent_s(0.0);
  ping.mutable_header()->set_source_id("sensor_7");
  ping.mutable_header()->set_frame_id("world");
  ping.set_sensor_id(7);
  ping.set_cpu_pct(20.0);
  ping.set_mem_mb(128.0);

  pipeline.ingest_health_ping(ping);
  now_s = 3.5;
  pipeline.check_stale_sensors();

  const auto snap = metrics.snapshot();
  CHECK(snap.counters.contains("sensor_stale_total"));
  CHECK(snap.counters.at("sensor_stale_total") == 1);
  CHECK(snap.gauges.contains("sensor_last_seen_age_s_7"));
  CHECK(snap.gauges.at("sensor_last_seen_age_s_7") > 2.0);
}
