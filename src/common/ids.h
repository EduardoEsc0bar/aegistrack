#pragma once

#include <cstdint>
#include <ostream>

namespace sensor_fusion {

struct TrackId {
  uint64_t value{0};

  explicit constexpr TrackId(uint64_t id) : value(id) {}

  constexpr bool operator==(const TrackId& other) const = default;
  constexpr bool operator<(const TrackId& other) const { return value < other.value; }
  constexpr bool operator<=(const TrackId& other) const { return value <= other.value; }
  constexpr bool operator>(const TrackId& other) const { return value > other.value; }
  constexpr bool operator>=(const TrackId& other) const { return value >= other.value; }
};

inline std::ostream& operator<<(std::ostream& os, const TrackId& id) {
  os << id.value;
  return os;
}

struct SensorId {
  uint64_t value{0};

  explicit constexpr SensorId(uint64_t id) : value(id) {}

  constexpr bool operator==(const SensorId& other) const = default;
  constexpr bool operator<(const SensorId& other) const { return value < other.value; }
  constexpr bool operator<=(const SensorId& other) const { return value <= other.value; }
  constexpr bool operator>(const SensorId& other) const { return value > other.value; }
  constexpr bool operator>=(const SensorId& other) const { return value >= other.value; }
};

inline std::ostream& operator<<(std::ostream& os, const SensorId& id) {
  os << id.value;
  return os;
}

}  // namespace sensor_fusion
