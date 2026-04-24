#include "services/telemetry_ingest/ingest_pipeline.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

#include "services/telemetry_ingest/conversion.h"

namespace sensor_fusion::services::telemetry_ingest {

IngestPipeline::IngestPipeline(sensor_fusion::MessageBus* bus,
                               sensor_fusion::observability::Metrics* metrics,
                               Options options,
                               std::function<sensor_fusion::Timestamp()> now_fn)
    : bus_(bus),
      metrics_(metrics),
      options_(options),
      now_fn_(std::move(now_fn)) {
  if (options_.enable_worker_thread) {
    worker_thread_ = std::thread(&IngestPipeline::worker_loop, this);
  }
  if (options_.enable_stale_monitor_thread) {
    stale_thread_ = std::thread(&IngestPipeline::stale_monitor_loop, this);
  }
}

IngestPipeline::~IngestPipeline() {
  stop_.store(true);
  queue_cv_.notify_all();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  if (stale_thread_.joinable()) {
    stale_thread_.join();
  }
}

void IngestPipeline::ingest_batch(const aegistrack::telemetry::v1::DetectionBatch& batch) {
  const uint64_t batches_total = total_batches_.fetch_add(1) + 1;
  const uint64_t detections = static_cast<uint64_t>(batch.detections_size());
  total_detections_.fetch_add(detections);

  if (metrics_ != nullptr) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_->inc("grpc_batches_total");
    metrics_->inc("grpc_detections_total", detections);
  }

  for (const auto& detection : batch.detections()) {
    const uint64_t sensor_id = detection.sensor_id();
    const uint64_t seq = detection.header().seq();

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      const auto it = last_seq_by_sensor_.find(sensor_id);
      if (it != last_seq_by_sensor_.end() && seq <= it->second) {
        if (metrics_ != nullptr) {
          std::lock_guard<std::mutex> lock(metrics_mutex_);
          metrics_->inc("ingest_out_of_order_total");
        }
      }
      last_seq_by_sensor_[sensor_id] = seq;
    }

    enqueue_measurement(detection_to_measurement(detection));
  }

  if (options_.log_every_batches > 0 &&
      (batches_total % options_.log_every_batches == 0)) {
    std::cout << "[telemetry_ingest] batches=" << batches_total
              << " detections=" << total_detections_.load()
              << " queue_depth=" << queue_depth() << std::endl;
  }
}

void IngestPipeline::ingest_health_ping(const aegistrack::telemetry::v1::HealthPing& ping) {
  const auto now = now_fn_();
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto& state = health_by_sensor_[ping.sensor_id()];
    state.last_seen = now;
    state.stale_reported = false;
  }

  if (metrics_ != nullptr) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_->set_gauge("sensor_last_seen_age_s_" + std::to_string(ping.sensor_id()), 0.0);
  }
}

void IngestPipeline::check_stale_sensors() {
  const auto now = now_fn_();
  std::lock_guard<std::mutex> lock(state_mutex_);
  for (auto& [sensor_id, state] : health_by_sensor_) {
    const double age_s = now - state.last_seen;
    if (metrics_ != nullptr) {
      std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
      metrics_->set_gauge("sensor_last_seen_age_s_" + std::to_string(sensor_id), age_s);
    }

    if (age_s > options_.stale_timeout_s && !state.stale_reported) {
      state.stale_reported = true;
      if (metrics_ != nullptr) {
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        metrics_->inc("sensor_stale_total");
      }
      std::cerr << "[health] sensor " << sensor_id << " stale age_s=" << age_s << std::endl;
    }
  }
}

bool IngestPipeline::enqueue_measurement(sensor_fusion::Measurement measurement) {
  if (options_.fault_injector != nullptr &&
      options_.fault_injector->should_drop(options_.queue_fault_drop_prob)) {
    total_drops_.fetch_add(1);
    if (metrics_ != nullptr) {
      std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
      metrics_->inc("ingest_queue_drops_total");
    }
    return false;
  }

  if (options_.fault_injector != nullptr &&
      options_.fault_injector->should_delay(options_.queue_fault_delay_prob)) {
    const double delayed_sent_s =
        measurement.t_sent.to_seconds() +
        std::abs(options_.fault_injector->jitter(options_.queue_fault_jitter_s));
    measurement.t_sent = sensor_fusion::Timestamp::from_seconds(delayed_sent_s);
  }

  std::lock_guard<std::mutex> lock(queue_mutex_);

  bool dropped = false;
  if (queue_.size() >= options_.max_queue) {
    dropped = true;
    if (options_.drop_oldest && !queue_.empty()) {
      queue_.pop_front();
      queue_.push_back(std::move(measurement));
    } else {
      // Drop newest by default if drop_oldest is disabled.
    }
  } else {
    queue_.push_back(std::move(measurement));
  }

  if (dropped) {
    total_drops_.fetch_add(1);
    if (metrics_ != nullptr) {
      std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
      metrics_->inc("ingest_queue_drops_total");
    }
  }

  const size_t depth = queue_.size();
  set_queue_depth_gauge_locked(depth);
  queue_cv_.notify_one();
  return !dropped;
}

void IngestPipeline::worker_loop() {
  auto& topic = bus_->topic<sensor_fusion::Measurement>("measurements");
  while (!stop_.load()) {
    sensor_fusion::Measurement measurement = sensor_fusion::Measurement{
        .t_meas = sensor_fusion::Timestamp::from_seconds(0.0),
        .t_sent = sensor_fusion::Timestamp::from_seconds(0.0),
        .sensor_id = sensor_fusion::SensorId(0),
        .sensor_type = sensor_fusion::SensorType::Radar,
        .measurement_type = sensor_fusion::MeasurementType::RangeBearing2D,
        .z = {},
        .R_rowmajor = {},
        .z_dim = 0,
        .confidence = 0.0,
        .snr = 0.0,
    };
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [&]() { return stop_.load() || !queue_.empty(); });
      if (stop_.load() && queue_.empty()) {
        break;
      }

      measurement = std::move(queue_.front());
      queue_.pop_front();
      set_queue_depth_gauge_locked(queue_.size());
    }

    topic.publish(measurement);
    total_published_.fetch_add(1);
    if (metrics_ != nullptr) {
      std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
      metrics_->inc("ingest_published_total");
    }
  }
}

void IngestPipeline::stale_monitor_loop() {
  while (!stop_.load()) {
    check_stale_sensors();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void IngestPipeline::set_queue_depth_gauge_locked(size_t depth) {
  if (metrics_ != nullptr) {
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    metrics_->set_gauge("ingest_queue_depth", static_cast<double>(depth));
  }
}

size_t IngestPipeline::queue_depth() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return queue_.size();
}

uint64_t IngestPipeline::queue_drops() const {
  return total_drops_.load();
}

uint64_t IngestPipeline::published_total() const {
  return total_published_.load();
}

}  // namespace sensor_fusion::services::telemetry_ingest
