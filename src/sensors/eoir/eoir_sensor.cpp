#include "sensors/eoir/eoir_sensor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "faults/fault_injector.h"

namespace sensor_fusion {

EoirSensor::EoirSensor(EoirConfig cfg,
                       Rng rng,
                       sensor_fusion::faults::FaultInjector* fault_injector)
    : cfg_(cfg), rng_(std::move(rng)), fault_injector_(fault_injector) {}

bool EoirSensor::pass_probability(double p) {
  if (p <= 0.0) {
    return false;
  }
  if (p >= 1.0) {
    return true;
  }
  return rng_.uniform(0.0, 1.0) < p;
}

uint64_t EoirSensor::next_trace_id() {
  const uint64_t sensor_bits = (cfg_.sensor_id.value & 0xFFFFFFFFULL) << 32U;
  const uint64_t seq_bits = sequence_++ & 0xFFFFFFFFULL;
  return sensor_bits ^ seq_bits;
}

double EoirSensor::sample_latency_s() {
  const double jitter =
      fault_injector_ != nullptr ? fault_injector_->jitter(cfg_.latency_jitter_s)
                                 : rng_.uniform(-cfg_.latency_jitter_s, cfg_.latency_jitter_s);
  double latency = cfg_.latency_mean_s + jitter;
  if (fault_injector_ != nullptr && fault_injector_->should_delay(0.0)) {
    latency += std::abs(fault_injector_->jitter(cfg_.latency_jitter_s));
  }
  return std::max(0.0, latency);
}

bool EoirSensor::should_drop_measurement() {
  if (fault_injector_ != nullptr) {
    return fault_injector_->should_drop(cfg_.drop_prob);
  }
  return pass_probability(cfg_.drop_prob);
}

std::vector<Measurement> EoirSensor::sense(const std::vector<TruthState>& truth) {
  std::vector<Measurement> measurements;

  for (const auto& state : truth) {
    if (!pass_probability(cfg_.p_detect)) {
      continue;
    }

    const double x = state.position_xyz[0];
    const double y = state.position_xyz[1];
    const double z = state.position_xyz[2];

    const double ground_range = std::sqrt(x * x + y * y);
    const double bearing = std::atan2(y, x);
    const double elevation = std::atan2(z, ground_range);

    const double bearing_meas =
        wrap_angle_pi(bearing + rng_.normal(0.0, cfg_.sigma_bearing_rad));
    const double elevation_meas = elevation + rng_.normal(0.0, cfg_.sigma_elevation_rad);

    const double latency = sample_latency_s();

    Measurement measurement{
        .t_meas = state.t,
        .t_sent = Timestamp::from_seconds(state.t.to_seconds() + latency),
        .sensor_id = cfg_.sensor_id,
        .sensor_type = SensorType::EOIR,
        .measurement_type = MeasurementType::BearingElevation,
        .z = {bearing_meas, elevation_meas},
        .R_rowmajor = {cfg_.sigma_bearing_rad * cfg_.sigma_bearing_rad, 0.0, 0.0,
                       cfg_.sigma_elevation_rad * cfg_.sigma_elevation_rad},
        .z_dim = 2,
        .confidence = cfg_.p_detect,
        .snr = 0.0,
        .trace_id = next_trace_id(),
        .causal_parent_id = 0,
    };

    if (!should_drop_measurement()) {
      measurements.push_back(std::move(measurement));
    }
  }

  return measurements;
}

}  // namespace sensor_fusion
