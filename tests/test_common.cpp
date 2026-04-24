#include <cmath>

#include <doctest/doctest.h>

#include "common/ids.h"
#include "common/rng.h"
#include "common/time.h"

namespace sensor_fusion {

TEST_CASE("common: timestamp ordering") {
  const Timestamp t0 = Timestamp::from_seconds(1.0);
  const Timestamp t1 = Timestamp::from_seconds(2.0);

  CHECK(t0 < t1);
  CHECK(t0 <= t1);
  CHECK(t1 > t0);
  CHECK(t1 >= t0);
  CHECK(t0 == Timestamp::from_seconds(1.0));
}

TEST_CASE("common: timestamp subtraction") {
  const Timestamp t0 = Timestamp::from_seconds(1.25);
  const Timestamp t1 = Timestamp::from_seconds(4.5);

  CHECK(std::abs((t1 - t0) - 3.25) < 1e-12);
  CHECK(std::abs((t0 - t1) + 3.25) < 1e-12);
}

TEST_CASE("common: rng same seed same sequence") {
  Rng rng_a(42);
  Rng rng_b(42);

  for (int i = 0; i < 8; ++i) {
    CHECK(rng_a.uniform(-10.0, 10.0) == rng_b.uniform(-10.0, 10.0));
    CHECK(rng_a.normal(0.0, 1.0) == rng_b.normal(0.0, 1.0));
  }
}

TEST_CASE("common: rng different seeds different sequence") {
  Rng rng_a(42);
  Rng rng_b(99);

  bool saw_difference = false;
  for (int i = 0; i < 8; ++i) {
    if (rng_a.uniform(-1.0, 1.0) != rng_b.uniform(-1.0, 1.0)) {
      saw_difference = true;
      break;
    }
  }

  CHECK(saw_difference);
}

TEST_CASE("common: track id equality and ordering") {
  const TrackId a(7);
  const TrackId b(7);
  const TrackId c(9);

  CHECK(a == b);
  CHECK(a < c);
  CHECK(c > a);
  CHECK(a <= b);
  CHECK(c >= b);
}

}  // namespace sensor_fusion
