#include "services/telemetry_ingest/conversion.h"

#include <utility>

namespace sensor_fusion::services::telemetry_ingest {
namespace {

sensor_fusion::SensorType sensor_type_from_string(const std::string& value) {
  if (value == "Radar") {
    return sensor_fusion::SensorType::Radar;
  }
  if (value == "EOIR") {
    return sensor_fusion::SensorType::EOIR;
  }
  if (value == "ADSB") {
    return sensor_fusion::SensorType::ADSB;
  }
  return sensor_fusion::SensorType::Radar;
}

sensor_fusion::MeasurementType measurement_type_from_string(const std::string& value) {
  if (value == "RangeBearing2D") {
    return sensor_fusion::MeasurementType::RangeBearing2D;
  }
  if (value == "BearingElevation") {
    return sensor_fusion::MeasurementType::BearingElevation;
  }
  if (value == "Position3D") {
    return sensor_fusion::MeasurementType::Position3D;
  }
  return sensor_fusion::MeasurementType::RangeBearing2D;
}

uint64_t compose_trace_id(uint64_t sensor_id, uint64_t seq) {
  return (sensor_id << 32U) ^ (seq & 0xFFFFFFFFULL);
}

}  // namespace

sensor_fusion::Measurement detection_to_measurement(
    const aegistrack::telemetry::v1::Detection& detection) {
  std::vector<double> z(detection.z().begin(), detection.z().end());
  std::vector<double> R(detection.r().begin(), detection.r().end());

  uint32_t z_dim = detection.z_dim();
  if (z_dim == 0) {
    z_dim = static_cast<uint32_t>(z.size());
  }

  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(detection.header().t_meas_s()),
      .t_sent = sensor_fusion::Timestamp::from_seconds(detection.header().t_sent_s()),
      .sensor_id = sensor_fusion::SensorId(detection.sensor_id()),
      .sensor_type = sensor_type_from_string(detection.sensor_type()),
      .measurement_type = measurement_type_from_string(detection.measurement_type()),
      .z = std::move(z),
      .R_rowmajor = std::move(R),
      .z_dim = z_dim,
      .confidence = detection.confidence(),
      .snr = 0.0,
      .trace_id = compose_trace_id(detection.sensor_id(), detection.header().seq()),
      .causal_parent_id = 0,
  };
}

}  // namespace sensor_fusion::services::telemetry_ingest
