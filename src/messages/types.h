#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "common/ids.h"
#include "common/time.h"

namespace sensor_fusion {

enum class SensorType { Radar, EOIR, ADSB };

enum class MeasurementType { RangeBearing2D, BearingElevation, Position3D };

struct TruthState {
  Timestamp t;
  uint64_t target_id;
  std::array<double, 3> position_xyz;
  std::array<double, 3> velocity_xyz;
};

struct Measurement {
  Timestamp t_meas;
  Timestamp t_sent;
  SensorId sensor_id;
  SensorType sensor_type;
  MeasurementType measurement_type;
  std::vector<double> z;
  std::vector<double> R_rowmajor;
  uint32_t z_dim;
  double confidence;
  double snr;
  uint64_t trace_id = 0;
  uint64_t causal_parent_id = 0;
};

inline bool validate_covariance_shape(const Measurement& measurement) {
  const auto dim = static_cast<size_t>(measurement.z_dim);
  return measurement.R_rowmajor.size() == dim * dim;
}

struct Track {
  Timestamp t;
  TrackId track_id;
  std::array<double, 6> x;
  std::array<double, 36> P;
  uint32_t age_ticks;
  uint32_t hits;
  uint32_t misses;
  double score;
};

}  // namespace sensor_fusion
