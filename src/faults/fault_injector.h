#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>

#include "common/rng.h"
#include "observability/metrics.h"

namespace sensor_fusion::faults {

class FaultInjector {
 public:
  struct Config {
    bool enabled = true;
    double global_drop_prob = 0.0;
    double global_delay_prob = 0.0;
  };

  explicit FaultInjector(uint64_t seed) : rng_(seed), cfg_(), metrics_(nullptr) {}

  FaultInjector(uint64_t seed,
                Config cfg,
                sensor_fusion::observability::Metrics* metrics = nullptr)
      : rng_(seed), cfg_(cfg), metrics_(metrics) {}

  void set_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.enabled = enabled;
  }

  void set_global_drop_prob(double p) {
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.global_drop_prob = std::clamp(p, 0.0, 1.0);
  }

  void set_global_delay_prob(double p) {
    std::lock_guard<std::mutex> lock(mutex_);
    cfg_.global_delay_prob = std::clamp(p, 0.0, 1.0);
  }

  void set_observer(std::function<void(const std::string&)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = std::move(observer);
  }

  bool should_drop(double probability) {
    return decide(combine(probability, cfg_.global_drop_prob), "drop");
  }

  bool should_delay(double probability) {
    return decide(combine(probability, cfg_.global_delay_prob), "delay");
  }

  double jitter(double max_jitter) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cfg_.enabled || max_jitter <= 0.0) {
      return 0.0;
    }
    const double value = rng_.uniform(-max_jitter, max_jitter);
    if (value != 0.0) {
      register_fault_locked("jitter");
    }
    return value;
  }

  uint64_t injected_count() const {
    return injected_count_.load();
  }

 private:
  static double combine(double local, double global) {
    const double l = std::clamp(local, 0.0, 1.0);
    const double g = std::clamp(global, 0.0, 1.0);
    return 1.0 - (1.0 - l) * (1.0 - g);
  }

  bool decide(double probability, const char* fault_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cfg_.enabled) {
      return false;
    }
    const double p = std::clamp(probability, 0.0, 1.0);
    if (p <= 0.0) {
      return false;
    }
    if (p >= 1.0 || rng_.uniform(0.0, 1.0) < p) {
      register_fault_locked(fault_type);
      return true;
    }
    return false;
  }

  void register_fault_locked(const std::string& fault_type) {
    injected_count_.fetch_add(1);
    if (metrics_ != nullptr) {
      metrics_->inc("fault_injected_total");
    }
    std::cerr << "[fault] injected " << fault_type << std::endl;
    if (observer_) {
      observer_(fault_type);
    }
  }

  mutable std::mutex mutex_;
  sensor_fusion::Rng rng_;
  Config cfg_;
  sensor_fusion::observability::Metrics* metrics_;
  std::function<void(const std::string&)> observer_;
  std::atomic<uint64_t> injected_count_{0};
};

}  // namespace sensor_fusion::faults
