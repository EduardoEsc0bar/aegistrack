#include <vector>

#include <doctest/doctest.h>

#include "fusion_core/measurement_buffer.h"

namespace sensor_fusion::fusion_core {
namespace {

sensor_fusion::Measurement make_measurement(double t_sent_s, uint64_t sensor_id = 1) {
  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(0.0),
      .t_sent = sensor_fusion::Timestamp::from_seconds(t_sent_s),
      .sensor_id = sensor_fusion::SensorId(sensor_id),
      .sensor_type = sensor_fusion::SensorType::Radar,
      .measurement_type = sensor_fusion::MeasurementType::RangeBearing2D,
      .z = {10.0, 0.1},
      .R_rowmajor = {1.0, 0.0, 0.0, 1.0},
      .z_dim = 2,
      .confidence = 1.0,
      .snr = 0.0,
  };
}

}  // namespace

TEST_CASE("measurement buffer: out of order insertion sorted on pop") {
  MeasurementBuffer buffer;
  buffer.push(make_measurement(3.0));
  buffer.push(make_measurement(1.0));
  buffer.push(make_measurement(2.0));

  const auto ready = buffer.pop_ready(sensor_fusion::Timestamp::from_seconds(2.5));
  CHECK(ready.size() == 2);
  CHECK(ready[0].t_sent.to_seconds() == 1.0);
  CHECK(ready[1].t_sent.to_seconds() == 2.0);

  const auto late = buffer.pop_ready(sensor_fusion::Timestamp::from_seconds(10.0));
  CHECK(late.size() == 1);
  CHECK(late[0].t_sent.to_seconds() == 3.0);
}

TEST_CASE("measurement buffer: pop threshold keeps late measurements") {
  MeasurementBuffer buffer;
  buffer.push(make_measurement(0.2));
  buffer.push(make_measurement(0.5));
  buffer.push(make_measurement(0.8));

  const auto first = buffer.pop_ready(sensor_fusion::Timestamp::from_seconds(0.5));
  CHECK(first.size() == 2);
  CHECK(first[0].t_sent.to_seconds() == 0.2);
  CHECK(first[1].t_sent.to_seconds() == 0.5);

  const auto second = buffer.pop_ready(sensor_fusion::Timestamp::from_seconds(0.7));
  CHECK(second.empty());

  const auto third = buffer.pop_ready(sensor_fusion::Timestamp::from_seconds(1.0));
  CHECK(third.size() == 1);
  CHECK(third[0].t_sent.to_seconds() == 0.8);
}

TEST_CASE("measurement buffer: deterministic behavior fixed timestamps") {
  MeasurementBuffer a;
  MeasurementBuffer b;

  const std::vector<double> times = {1.1, 0.4, 0.9, 0.2};
  for (double t : times) {
    a.push(make_measurement(t));
    b.push(make_measurement(t));
  }

  const auto ready_a = a.pop_ready(sensor_fusion::Timestamp::from_seconds(1.0));
  const auto ready_b = b.pop_ready(sensor_fusion::Timestamp::from_seconds(1.0));

  CHECK(ready_a.size() == ready_b.size());
  for (size_t i = 0; i < ready_a.size(); ++i) {
    CHECK(ready_a[i].t_sent.to_seconds() == ready_b[i].t_sent.to_seconds());
  }
}

}  // namespace sensor_fusion::fusion_core
