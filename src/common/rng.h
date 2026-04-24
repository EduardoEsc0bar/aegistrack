#pragma once

#include <cstdint>
#include <random>

namespace sensor_fusion {

class Rng {
 public:
  explicit Rng(uint64_t seed) : engine_(seed) {}

  double uniform(double min, double max) {
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(engine_);
  }

  double normal(double mean, double stddev) {
    std::normal_distribution<double> distribution(mean, stddev);
    return distribution(engine_);
  }

 private:
  std::mt19937_64 engine_;
};

}  // namespace sensor_fusion
