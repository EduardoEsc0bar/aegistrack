#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "common/fixed_rate_loop.h"
#include "scenario/truth_generator.h"

namespace sensor_fusion {

TEST_CASE("scenario: truth generator constant velocity") {
  TruthGenerator generator({TruthGenerator::TargetInit{
      .target_id = 1,
      .position_xyz = {0.0, 0.0, 0.0},
      .velocity_xyz = {1.0, 2.0, 3.0},
  }});

  const auto step1 = generator.step(1.0);
  CHECK(step1.size() == 1);
  CHECK(step1[0].position_xyz[0] == 1.0);
  CHECK(step1[0].position_xyz[1] == 2.0);
  CHECK(step1[0].position_xyz[2] == 3.0);
  CHECK(step1[0].velocity_xyz[0] == 1.0);
  CHECK(step1[0].velocity_xyz[1] == 2.0);
  CHECK(step1[0].velocity_xyz[2] == 3.0);

  const auto step2 = generator.step(2.0);
  CHECK(step2.size() == 1);
  CHECK(step2[0].position_xyz[0] == 3.0);
  CHECK(step2[0].position_xyz[1] == 6.0);
  CHECK(step2[0].position_xyz[2] == 9.0);
  CHECK(step2[0].velocity_xyz[0] == 1.0);
  CHECK(step2[0].velocity_xyz[1] == 2.0);
  CHECK(step2[0].velocity_xyz[2] == 3.0);
}

TEST_CASE("scenario: fixed rate loop dt") {
  const FixedRateLoop loop(20.0);
  CHECK(std::abs(loop.tick_dt() - 0.05) < 1e-12);
}

TEST_CASE("scenario: deterministic target stepping") {
  const std::vector<TruthGenerator::TargetInit> initial_targets{
      TruthGenerator::TargetInit{
          .target_id = 1,
          .position_xyz = {0.0, 0.0, 0.0},
          .velocity_xyz = {3.0, 1.0, 0.0},
      },
      TruthGenerator::TargetInit{
          .target_id = 2,
          .position_xyz = {10.0, -5.0, 2.0},
          .velocity_xyz = {1.0, -2.0, 0.5},
      },
  };

  TruthGenerator generator_a(initial_targets);
  TruthGenerator generator_b(initial_targets);

  for (int i = 0; i < 10; ++i) {
    const auto states_a = generator_a.step(0.1);
    const auto states_b = generator_b.step(0.1);

    CHECK(states_a.size() == states_b.size());
    for (size_t j = 0; j < states_a.size(); ++j) {
      CHECK(states_a[j].target_id == states_b[j].target_id);
      for (size_t k = 0; k < states_a[j].position_xyz.size(); ++k) {
        CHECK(std::abs(states_a[j].position_xyz[k] - states_b[j].position_xyz[k]) < 1e-12);
      }
    }
  }
}

}  // namespace sensor_fusion
