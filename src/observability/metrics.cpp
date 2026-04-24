#include "observability/metrics.h"

#include <iostream>

namespace sensor_fusion::observability {

void Metrics::inc(const std::string& name, uint64_t amount) {
  counters_[name] += amount;
}

void Metrics::set_gauge(const std::string& name, double value) {
  gauges_[name] = value;
}

void Metrics::observe(const std::string& name, double value) {
  auto& stats = observations_[name];
  if (stats.count == 0) {
    stats.min = value;
    stats.max = value;
  } else {
    if (value < stats.min) {
      stats.min = value;
    }
    if (value > stats.max) {
      stats.max = value;
    }
  }

  ++stats.count;
  stats.sum += value;
}

void Metrics::dump() const {
  std::cout << "[metrics] counters" << std::endl;
  for (const auto& [name, value] : counters_) {
    std::cout << "  " << name << "=" << value << std::endl;
  }

  std::cout << "[metrics] gauges" << std::endl;
  for (const auto& [name, value] : gauges_) {
    std::cout << "  " << name << "=" << value << std::endl;
  }

  std::cout << "[metrics] observations" << std::endl;
  for (const auto& [name, stats] : observations_) {
    const double avg = stats.count == 0 ? 0.0 : stats.sum / static_cast<double>(stats.count);
    std::cout << "  " << name << " count=" << stats.count << " avg=" << avg
              << " min=" << stats.min << " max=" << stats.max << std::endl;
  }
}

MetricsSnapshot Metrics::snapshot() const {
  MetricsSnapshot snap;
  snap.counters.insert(counters_.begin(), counters_.end());
  snap.gauges.insert(gauges_.begin(), gauges_.end());

  for (const auto& [name, stats] : observations_) {
    snap.observations[name] = MetricsSnapshot::Obs{
        .count = stats.count,
        .sum = stats.sum,
        .min = stats.min,
        .max = stats.max,
    };
  }
  return snap;
}

}  // namespace sensor_fusion::observability
