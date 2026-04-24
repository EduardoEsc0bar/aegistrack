#include <array>
#include <vector>

#include <doctest/doctest.h>

#include "agents/interceptor/interceptor_manager.h"

namespace sensor_fusion::agents {

TEST_CASE("interceptor manager: stepping updates all interceptors") {
  InterceptorManager manager({
      InterceptorConfig{.interceptor_id = 1, .max_speed_mps = 100.0, .start_position = {0.0, 0.0, 0.0}},
      InterceptorConfig{.interceptor_id = 2, .max_speed_mps = 100.0, .start_position = {0.0, 20.0, 0.0}},
  });

  manager.update_track_positions({
      {sensor_fusion::TrackId(10), {100.0, 0.0, 0.0}},
      {sensor_fusion::TrackId(20), {100.0, 20.0, 0.0}},
  });

  manager.assign({
      {1, sensor_fusion::TrackId(10)},
      {2, sensor_fusion::TrackId(20)},
  });

  const auto before = manager.states();
  manager.step(0.5);
  const auto after = manager.states();

  CHECK(before.size() == 2);
  CHECK(after.size() == 2);
  CHECK(after[0].position[0] > before[0].position[0]);
  CHECK(after[1].position[0] > before[1].position[0]);
}

TEST_CASE("interceptor manager: intercept success frees interceptor") {
  InterceptorManager manager({
      InterceptorConfig{.interceptor_id = 1, .max_speed_mps = 100.0, .start_position = {0.0, 0.0, 0.0}},
  });

  manager.update_track_positions({
      {sensor_fusion::TrackId(10), {100.0, 0.0, 0.0}},
  });

  manager.assign({
      {1, sensor_fusion::TrackId(10)},
  });

  CHECK(manager.states()[0].engaged);
  manager.handle_intercepts({
      {1, sensor_fusion::TrackId(10)},
  });
  CHECK(!manager.states()[0].engaged);
}

}  // namespace sensor_fusion::agents
