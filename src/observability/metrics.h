#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace sensor_fusion::observability {

struct MetricsSnapshot {
  struct Obs {
    uint64_t count = 0;
    double sum = 0.0;
    double min = 0.0;
    double max = 0.0;
  };

  std::unordered_map<std::string, uint64_t> counters;
  std::unordered_map<std::string, double> gauges;
  std::unordered_map<std::string, Obs> observations;
};

class Metrics {
 public:
  void inc(const std::string& name, uint64_t amount = 1);
  void set_gauge(const std::string& name, double value);
  void observe(const std::string& name, double value);

  void dump() const;
  MetricsSnapshot snapshot() const;

 private:
  struct ObservationStats {
    uint64_t count = 0;
    double sum = 0.0;
    double min = 0.0;
    double max = 0.0;
  };

  std::map<std::string, uint64_t> counters_;
  std::map<std::string, double> gauges_;
  std::map<std::string, ObservationStats> observations_;
};

}  // namespace sensor_fusion::observability
