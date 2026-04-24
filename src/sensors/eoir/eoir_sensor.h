#pragma once

#include <vector>

#include "common/angles.h"
#include "common/rng.h"
#include "messages/types.h"

namespace sensor_fusion::faults {
class FaultInjector;
}

namespace sensor_fusion {

struct EoirConfig {
  SensorId sensor_id;
  double sigma_bearing_rad;
  double sigma_elevation_rad;
  double p_detect;
  double drop_prob;
  double latency_mean_s;
  double latency_jitter_s;
};

class EoirSensor {
 public:
  EoirSensor(EoirConfig cfg, Rng rng, sensor_fusion::faults::FaultInjector* fault_injector = nullptr);

  std::vector<Measurement> sense(const std::vector<TruthState>& truth);

 private:
  bool pass_probability(double p);
  uint64_t next_trace_id();
  double sample_latency_s();
  bool should_drop_measurement();

  EoirConfig cfg_;
  Rng rng_;
  sensor_fusion::faults::FaultInjector* fault_injector_ = nullptr;
  uint64_t sequence_ = 1;
};

}  // namespace sensor_fusion
