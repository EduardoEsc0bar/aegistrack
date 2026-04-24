#include "sensors/radar/radar_sensor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "faults/fault_injector.h"

namespace sensor_fusion {

RadarSensor::RadarSensor(RadarConfig cfg,
                         Rng rng,
                         sensor_fusion::faults::FaultInjector* fault_injector)
    : cfg_(cfg), rng_(std::move(rng)), fault_injector_(fault_injector) {}

bool RadarSensor::pass_probability(double p) {
  if (p <= 0.0) {
    return false;
  }
  if (p >= 1.0) {
    return true;
  }
  return rng_.uniform(0.0, 1.0) < p;
}

bool RadarSensor::within_fov(double bearing_rad) const {
  const double half_fov_rad = deg_to_rad(cfg_.fov_deg) * 0.5;
  return std::abs(bearing_rad) <= half_fov_rad;
}

uint64_t RadarSensor::next_trace_id() {
  const uint64_t sensor_bits = (cfg_.sensor_id.value & 0xFFFFFFFFULL) << 32U;
  const uint64_t seq_bits = sequence_++ & 0xFFFFFFFFULL;
  return sensor_bits ^ seq_bits;
}

double RadarSensor::sample_latency_s() {
  const double jitter =
      fault_injector_ != nullptr ? fault_injector_->jitter(cfg_.latency_jitter_s)
                                 : rng_.uniform(-cfg_.latency_jitter_s, cfg_.latency_jitter_s);
  double latency = cfg_.latency_mean_s + jitter;
  if (fault_injector_ != nullptr && fault_injector_->should_delay(0.0)) {
    latency += std::abs(fault_injector_->jitter(cfg_.latency_jitter_s));
  }
  return std::max(0.0, latency);
}

bool RadarSensor::should_drop_measurement() {
  if (fault_injector_ != nullptr) {
    return fault_injector_->should_drop(cfg_.drop_prob);
  }
  return pass_probability(cfg_.drop_prob);
}

std::vector<Measurement> RadarSensor::sense(const std::vector<TruthState>& truth) {
  std::vector<Measurement> measurements;

  auto maybe_add_measurement = [&](Measurement measurement) {
    if (!should_drop_measurement()) {
      measurements.push_back(std::move(measurement));
    }
  };

  for (const auto& state : truth) {
    const double x = state.position_xyz[0];
    const double y = state.position_xyz[1];
    const double range = std::sqrt(x * x + y * y);
    const double bearing = std::atan2(y, x);

    if (range > cfg_.max_range_m || !within_fov(bearing)) {
      continue;
    }

    if (!pass_probability(cfg_.p_detect)) {
      continue;
    }

    const double range_meas = range + rng_.normal(0.0, cfg_.sigma_range_m);
    const double bearing_meas = wrap_angle_pi(bearing + rng_.normal(0.0, cfg_.sigma_bearing_rad));
    const double latency = sample_latency_s();

    maybe_add_measurement(Measurement{
        .t_meas = state.t,
        .t_sent = Timestamp::from_seconds(state.t.to_seconds() + latency),
        .sensor_id = cfg_.sensor_id,
        .sensor_type = SensorType::Radar,
        .measurement_type = MeasurementType::RangeBearing2D,
        .z = {range_meas, bearing_meas},
        .R_rowmajor = {cfg_.sigma_range_m * cfg_.sigma_range_m, 0.0, 0.0,
                       cfg_.sigma_bearing_rad * cfg_.sigma_bearing_rad},
        .z_dim = 2,
        .confidence = cfg_.p_detect,
        .snr = 0.0,
        .trace_id = next_trace_id(),
        .causal_parent_id = 0,
    });
  }

  if (pass_probability(cfg_.p_false_alarm)) {
    const double half_fov_rad = deg_to_rad(cfg_.fov_deg) * 0.5;
    const double range_meas = rng_.uniform(0.0, cfg_.max_range_m);
    const double bearing_meas =
        wrap_angle_pi(rng_.uniform(-half_fov_rad, half_fov_rad));

    const Timestamp t_meas = truth.empty() ? Timestamp::from_seconds(0.0) : truth.front().t;
    const double latency = sample_latency_s();

    maybe_add_measurement(Measurement{
        .t_meas = t_meas,
        .t_sent = Timestamp::from_seconds(t_meas.to_seconds() + latency),
        .sensor_id = cfg_.sensor_id,
        .sensor_type = SensorType::Radar,
        .measurement_type = MeasurementType::RangeBearing2D,
        .z = {range_meas, bearing_meas},
        .R_rowmajor = {cfg_.sigma_range_m * cfg_.sigma_range_m, 0.0, 0.0,
                       cfg_.sigma_bearing_rad * cfg_.sigma_bearing_rad},
        .z_dim = 2,
        .confidence = 0.2,
        .snr = 0.0,
        .trace_id = next_trace_id(),
        .causal_parent_id = 0,
    });
  }

  return measurements;
}

}  // namespace sensor_fusion
