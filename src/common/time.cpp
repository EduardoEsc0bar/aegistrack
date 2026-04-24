#include "common/time.h"

namespace sensor_fusion {

Timestamp::Timestamp(TimePoint time_point) : time_point_(time_point) {}

Timestamp Timestamp::now() {
  return Timestamp(Clock::now());
}

Timestamp Timestamp::from_seconds(double seconds) {
  const auto duration = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(seconds));
  return Timestamp(TimePoint(duration));
}

double Timestamp::to_seconds() const {
  return std::chrono::duration<double>(time_point_.time_since_epoch()).count();
}

double Timestamp::operator-(const Timestamp& other) const {
  return std::chrono::duration<double>(time_point_ - other.time_point_).count();
}

bool Timestamp::operator<(const Timestamp& other) const {
  return time_point_ < other.time_point_;
}

bool Timestamp::operator<=(const Timestamp& other) const {
  return time_point_ <= other.time_point_;
}

bool Timestamp::operator>(const Timestamp& other) const {
  return time_point_ > other.time_point_;
}

bool Timestamp::operator>=(const Timestamp& other) const {
  return time_point_ >= other.time_point_;
}

bool Timestamp::operator==(const Timestamp& other) const {
  return time_point_ == other.time_point_;
}

}  // namespace sensor_fusion
