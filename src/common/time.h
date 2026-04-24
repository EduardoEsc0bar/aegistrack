#pragma once

#include <chrono>

namespace sensor_fusion {

class Timestamp {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  static Timestamp now();
  static Timestamp from_seconds(double seconds);

  double to_seconds() const;

  double operator-(const Timestamp& other) const;

  bool operator<(const Timestamp& other) const;
  bool operator<=(const Timestamp& other) const;
  bool operator>(const Timestamp& other) const;
  bool operator>=(const Timestamp& other) const;
  bool operator==(const Timestamp& other) const;

 private:
  explicit Timestamp(TimePoint time_point);

  TimePoint time_point_;
};

}  // namespace sensor_fusion
