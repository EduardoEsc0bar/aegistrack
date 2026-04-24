#pragma once

#include <vector>

#include "common/angles.h"
#include "common/rng.h"
#include "messages/types.h"

namespace sensor_fusion::faults {
class FaultInjector;
}

namespace sensor_fusion {

struct RadarConfig {
  SensorId sensor_id;
  double hz;
  double max_range_m;
  double fov_deg;
  double sigma_range_m;
  double sigma_bearing_rad;
  double p_detect;
  double p_false_alarm;
  double drop_prob;
  double latency_mean_s;
  double latency_jitter_s;
};

class RadarSensor {
 public:
  RadarSensor(RadarConfig cfg, Rng rng, sensor_fusion::faults::FaultInjector* fault_injector = nullptr);

  std::vector<Measurement> sense(const std::vector<TruthState>& truth);

 private:
  bool pass_probability(double p);
  bool within_fov(double bearing_rad) const;
  uint64_t next_trace_id();
  double sample_latency_s();
  bool should_drop_measurement();

  RadarConfig cfg_;
  Rng rng_;
  sensor_fusion::faults::FaultInjector* fault_injector_ = nullptr;
  uint64_t sequence_ = 1;
};

}  // namespace sensor_fusion
