#pragma once

#include <cmath>

namespace sensor_fusion {

inline constexpr double kPi = 3.14159265358979323846;

inline double deg_to_rad(double degrees) {
  return degrees * (kPi / 180.0);
}

inline double rad_to_deg(double radians) {
  return radians * (180.0 / kPi);
}

inline double wrap_angle_pi(double angle_rad) {
  while (angle_rad > kPi) {
    angle_rad -= 2.0 * kPi;
  }
  while (angle_rad < -kPi) {
    angle_rad += 2.0 * kPi;
  }
  return angle_rad;
}

}  // namespace sensor_fusion
