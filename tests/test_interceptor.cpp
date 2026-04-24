#include <cmath>

#include <doctest/doctest.h>

#include "agents/interceptor/interceptor.h"

namespace sensor_fusion::agents {

TEST_CASE("interceptor: moves toward assigned target") {
  Interceptor interceptor(120.0);
  interceptor.assign_target(sensor_fusion::TrackId(7), {100.0, 0.0, 0.0});
  interceptor.step(0.5);

  const auto& state = interceptor.state();
  CHECK(state.engaged);
  CHECK(state.position[0] > 0.0);
  CHECK(state.velocity[0] > 0.0);
  CHECK(std::abs(state.position[1]) < 1e-9);
  CHECK(std::abs(state.position[2]) < 1e-9);
}

TEST_CASE("interceptor: intercept triggers within reasonable ticks") {
  Interceptor interceptor(150.0);
  const std::array<double, 3> target = {100.0, 0.0, 0.0};
  interceptor.assign_target(sensor_fusion::TrackId(3), target);

  bool intercepted = false;
  for (int i = 0; i < 200; ++i) {
    interceptor.step(0.1);
    if (interceptor.has_intercepted(target)) {
      intercepted = true;
      break;
    }
  }

  CHECK(intercepted);
}

TEST_CASE("interceptor: deterministic stepping") {
  Interceptor a(180.0);
  Interceptor b(180.0);

  a.assign_target(sensor_fusion::TrackId(11), {500.0, -50.0, 30.0});
  b.assign_target(sensor_fusion::TrackId(11), {500.0, -50.0, 30.0});

  for (int i = 0; i < 100; ++i) {
    a.step(0.05);
    b.step(0.05);
  }

  const auto& sa = a.state();
  const auto& sb = b.state();
  for (size_t i = 0; i < 3; ++i) {
    CHECK(std::abs(sa.position[i] - sb.position[i]) < 1e-12);
    CHECK(std::abs(sa.velocity[i] - sb.velocity[i]) < 1e-12);
  }
}

}  // namespace sensor_fusion::agents
