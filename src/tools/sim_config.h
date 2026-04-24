#pragma once

#include <cstdint>
#include <string>

namespace sensor_fusion::tools {

struct SimConfig {
  int ticks = 200;
  double hz = 20.0;
  int targets = 5;

  double radar_sigma_range_m = 3.0;
  double radar_sigma_bearing_deg = 1.0;
  double radar_p_detect = 0.9;
  double radar_p_false_alarm = 0.1;
  double radar_drop_prob = 0.1;
  double radar_latency_ms = 100.0;
  double radar_jitter_ms = 50.0;

  double eoir_sigma_bearing_deg = 1.0;
  double eoir_sigma_elevation_deg = 1.0;
  double eoir_p_detect = 0.85;
  double eoir_drop_prob = 0.05;
  double eoir_latency_ms = 100.0;
  double eoir_jitter_ms = 50.0;

  double gate_mahal = 9.21;
  double gate_mahal_eoir = 9.21;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;

  int enable_eoir = 1;
  int enable_eoir_updates = 1;
  int use_hungarian = 1;
};

SimConfig load_sim_config_json(const std::string& path);

}  // namespace sensor_fusion::tools
