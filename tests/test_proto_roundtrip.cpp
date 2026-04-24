#include <doctest/doctest.h>

#include "detections.pb.h"

namespace {

using aegistrack::telemetry::v1::DetectionBatch;

}  // namespace

TEST_CASE("proto roundtrip: DetectionBatch preserves fields") {
  DetectionBatch batch;
  batch.mutable_header()->set_seq(42);
  batch.mutable_header()->set_t_meas_s(1.25);
  batch.mutable_header()->set_t_sent_s(1.35);
  batch.mutable_header()->set_source_id("sensor_node_1");
  batch.mutable_header()->set_frame_id("world");

  auto* detection = batch.add_detections();
  detection->mutable_header()->set_seq(7);
  detection->mutable_header()->set_t_meas_s(1.25);
  detection->mutable_header()->set_t_sent_s(1.30);
  detection->mutable_header()->set_source_id("radar_1");
  detection->mutable_header()->set_frame_id("world");
  detection->set_sensor_id(1001);
  detection->set_sensor_type("Radar");
  detection->set_measurement_type("RangeBearing2D");
  detection->add_z(123.0);
  detection->add_z(0.2);
  detection->add_r(9.0);
  detection->add_r(0.0);
  detection->add_r(0.0);
  detection->add_r(0.01);
  detection->set_z_dim(2);
  detection->set_confidence(0.9);

  std::string wire;
  CHECK(batch.SerializeToString(&wire));

  DetectionBatch parsed;
  CHECK(parsed.ParseFromString(wire));
  CHECK(parsed.header().seq() == 42);
  CHECK(parsed.header().source_id() == "sensor_node_1");
  CHECK(parsed.detections_size() == 1);

  const auto& parsed_detection = parsed.detections(0);
  CHECK(parsed_detection.sensor_id() == 1001);
  CHECK(parsed_detection.measurement_type() == "RangeBearing2D");
  CHECK(parsed_detection.z_size() == 2);
  CHECK(parsed_detection.r_size() == 4);
  CHECK(parsed_detection.z_dim() == 2);
}
