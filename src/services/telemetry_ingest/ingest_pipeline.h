#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "detections.pb.h"
#include "faults/fault_injector.h"
#include "health.pb.h"
#include "messages/types.h"
#include "observability/metrics.h"
#include "transport/message_bus.h"

namespace sensor_fusion::services::telemetry_ingest {

class IngestPipeline {
 public:
  struct Options {
    size_t max_queue = 10000;
    bool drop_oldest = true;
    bool enable_worker_thread = true;
    bool enable_stale_monitor_thread = true;
    uint64_t log_every_batches = 50;
    double stale_timeout_s = 2.0;
    sensor_fusion::faults::FaultInjector* fault_injector = nullptr;
    double queue_fault_drop_prob = 0.0;
    double queue_fault_delay_prob = 0.0;
    double queue_fault_jitter_s = 0.0;
  };

  IngestPipeline(sensor_fusion::MessageBus* bus,
                 sensor_fusion::observability::Metrics* metrics,
                 Options options,
                 std::function<sensor_fusion::Timestamp()> now_fn =
                     []() { return sensor_fusion::Timestamp::now(); });
  ~IngestPipeline();

  IngestPipeline(const IngestPipeline&) = delete;
  IngestPipeline& operator=(const IngestPipeline&) = delete;

  void ingest_batch(const aegistrack::telemetry::v1::DetectionBatch& batch);
  void ingest_health_ping(const aegistrack::telemetry::v1::HealthPing& ping);
  void check_stale_sensors();

  size_t queue_depth() const;
  uint64_t queue_drops() const;
  uint64_t published_total() const;

 private:
  struct SensorHealthState {
    sensor_fusion::Timestamp last_seen = sensor_fusion::Timestamp::from_seconds(0.0);
    bool stale_reported = false;
  };

  bool enqueue_measurement(sensor_fusion::Measurement measurement);
  void worker_loop();
  void stale_monitor_loop();
  void set_queue_depth_gauge_locked(size_t depth);

  sensor_fusion::MessageBus* bus_;
  sensor_fusion::observability::Metrics* metrics_;
  Options options_;
  std::function<sensor_fusion::Timestamp()> now_fn_;

  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<sensor_fusion::Measurement> queue_;

  mutable std::mutex state_mutex_;
  mutable std::mutex metrics_mutex_;
  std::unordered_map<uint64_t, uint64_t> last_seq_by_sensor_;
  std::unordered_map<uint64_t, SensorHealthState> health_by_sensor_;

  std::atomic<bool> stop_{false};
  std::thread worker_thread_;
  std::thread stale_thread_;

  std::atomic<uint64_t> total_batches_{0};
  std::atomic<uint64_t> total_detections_{0};
  std::atomic<uint64_t> total_drops_{0};
  std::atomic<uint64_t> total_published_{0};
};

}  // namespace sensor_fusion::services::telemetry_ingest
