#include <vector>

#include <doctest/doctest.h>

#include "common/time.h"
#include "messages/types.h"
#include "transport/message_bus.h"

namespace sensor_fusion {

TEST_CASE("messages: covariance validation") {
  const Measurement valid{
      .t_meas = Timestamp::from_seconds(1.0),
      .t_sent = Timestamp::from_seconds(1.1),
      .sensor_id = SensorId(10),
      .sensor_type = SensorType::Radar,
      .measurement_type = MeasurementType::RangeBearing2D,
      .z = {1000.0, 0.15},
      .R_rowmajor = {1.0, 0.0, 0.0, 1.0},
      .z_dim = 2,
      .confidence = 0.9,
      .snr = 20.0,
  };

  const Measurement invalid{
      .t_meas = Timestamp::from_seconds(2.0),
      .t_sent = Timestamp::from_seconds(2.1),
      .sensor_id = SensorId(11),
      .sensor_type = SensorType::EOIR,
      .measurement_type = MeasurementType::BearingElevation,
      .z = {0.05, 0.1},
      .R_rowmajor = {1.0, 0.0, 0.0},
      .z_dim = 2,
      .confidence = 0.7,
      .snr = 0.0,
  };

  CHECK(validate_covariance_shape(valid));
  CHECK(!validate_covariance_shape(invalid));
}

TEST_CASE("bus: publish order") {
  MessageBus bus;
  auto& topic = bus.topic<int>("ints");

  std::vector<int> received;
  topic.subscribe([&received](const int& value) { received.push_back(value); });

  for (int i = 1; i <= 100; ++i) {
    topic.publish(i);
  }

  CHECK(received.size() == 100);
  for (int i = 0; i < 100; ++i) {
    CHECK(received[static_cast<size_t>(i)] == i + 1);
  }
}

TEST_CASE("bus: multi subscriber fanout") {
  MessageBus bus;
  auto& topic = bus.topic<int>("fanout");

  std::vector<int> a;
  std::vector<int> b;

  topic.subscribe([&a](const int& value) { a.push_back(value); });
  topic.subscribe([&b](const int& value) { b.push_back(value); });

  topic.publish(10);
  topic.publish(20);
  topic.publish(30);

  const std::vector<int> expected{10, 20, 30};
  CHECK(a == expected);
  CHECK(b == expected);
}

TEST_CASE("bus: unsubscribe") {
  MessageBus bus;
  auto& topic = bus.topic<int>("unsubscribe");

  std::vector<int> a;
  std::vector<int> b;

  const uint64_t sub_a = topic.subscribe([&a](const int& value) { a.push_back(value); });
  topic.subscribe([&b](const int& value) { b.push_back(value); });

  topic.publish(1);
  topic.publish(2);

  topic.unsubscribe(sub_a);

  topic.publish(3);
  topic.publish(4);

  const std::vector<int> expected_a{1, 2};
  const std::vector<int> expected_b{1, 2, 3, 4};

  CHECK(a == expected_a);
  CHECK(b == expected_b);
}

}  // namespace sensor_fusion
