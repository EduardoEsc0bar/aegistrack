#include <doctest/doctest.h>
#include <cmath>

#include "detections.pb.h"
#include "messages/types.h"
#include "services/telemetry_ingest/conversion.h"

TEST_CASE("proto conversion: Detection to Measurement") {
  aegistrack::telemetry::v1::Detection detection;
  detection.mutable_header()->set_seq(1);
  detection.mutable_header()->set_t_meas_s(10.0);
  detection.mutable_header()->set_t_sent_s(10.2);
  detection.mutable_header()->set_source_id("sensor_a");
  detection.mutable_header()->set_frame_id("world");
  detection.set_sensor_id(9);
  detection.set_sensor_type("Radar");
  detection.set_measurement_type("RangeBearing2D");
  detection.add_z(100.0);
  detection.add_z(0.1);
  detection.add_r(4.0);
  detection.add_r(0.0);
  detection.add_r(0.0);
  detection.add_r(0.01);
  detection.set_z_dim(2);
  detection.set_confidence(0.8);

  const sensor_fusion::Measurement measurement =
      sensor_fusion::services::telemetry_ingest::detection_to_measurement(detection);

  CHECK(measurement.sensor_id.value == 9);
  CHECK(measurement.measurement_type == sensor_fusion::MeasurementType::RangeBearing2D);
  CHECK(measurement.z_dim == 2);
  CHECK(measurement.z.size() == 2);
  CHECK(measurement.R_rowmajor.size() == 4);
  CHECK(sensor_fusion::validate_covariance_shape(measurement));
  CHECK(std::abs(measurement.t_meas.to_seconds() - 10.0) < 1e-12);
  CHECK(std::abs(measurement.t_sent.to_seconds() - 10.2) < 1e-12);
}
