#include "tools/sim_config.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace sensor_fusion::tools {
namespace {

double parse_number_or_default(const std::string& content,
                               const std::string& key,
                               double current) {
  const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+)");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    return std::stod(match[1].str());
  }
  return current;
}

int parse_int_or_default(const std::string& content, const std::string& key, int current) {
  const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([-+]?[0-9]+)");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    return std::stoi(match[1].str());
  }
  return current;
}

uint32_t parse_u32_or_default(const std::string& content,
                              const std::string& key,
                              uint32_t current) {
  const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    return static_cast<uint32_t>(std::stoul(match[1].str()));
  }
  return current;
}

}  // namespace

SimConfig load_sim_config_json(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open config: " + path);
  }

  std::stringstream ss;
  ss << in.rdbuf();
  const std::string content = ss.str();

  SimConfig cfg;
  cfg.ticks = parse_int_or_default(content, "ticks", cfg.ticks);
  cfg.hz = parse_number_or_default(content, "hz", cfg.hz);
  cfg.targets = parse_int_or_default(content, "targets", cfg.targets);

  cfg.radar_sigma_range_m =
      parse_number_or_default(content, "radar_sigma_range_m", cfg.radar_sigma_range_m);
  cfg.radar_sigma_bearing_deg =
      parse_number_or_default(content, "radar_sigma_bearing_deg", cfg.radar_sigma_bearing_deg);
  cfg.radar_p_detect = parse_number_or_default(content, "radar_p_detect", cfg.radar_p_detect);
  cfg.radar_p_false_alarm =
      parse_number_or_default(content, "radar_p_false_alarm", cfg.radar_p_false_alarm);
  cfg.radar_drop_prob =
      parse_number_or_default(content, "radar_drop_prob", cfg.radar_drop_prob);
  cfg.radar_latency_ms =
      parse_number_or_default(content, "radar_latency_ms", cfg.radar_latency_ms);
  cfg.radar_jitter_ms = parse_number_or_default(content, "radar_jitter_ms", cfg.radar_jitter_ms);

  cfg.eoir_sigma_bearing_deg =
      parse_number_or_default(content, "eoir_sigma_bearing_deg", cfg.eoir_sigma_bearing_deg);
  cfg.eoir_sigma_elevation_deg =
      parse_number_or_default(content, "eoir_sigma_elevation_deg", cfg.eoir_sigma_elevation_deg);
  cfg.eoir_p_detect = parse_number_or_default(content, "eoir_p_detect", cfg.eoir_p_detect);
  cfg.eoir_drop_prob = parse_number_or_default(content, "eoir_drop_prob", cfg.eoir_drop_prob);
  cfg.eoir_latency_ms = parse_number_or_default(content, "eoir_latency_ms", cfg.eoir_latency_ms);
  cfg.eoir_jitter_ms = parse_number_or_default(content, "eoir_jitter_ms", cfg.eoir_jitter_ms);

  cfg.gate_mahal = parse_number_or_default(content, "gate_mahal", cfg.gate_mahal);
  cfg.gate_mahal_eoir = parse_number_or_default(content, "gate_mahal_eoir", cfg.gate_mahal_eoir);
  cfg.confirm_hits = parse_u32_or_default(content, "confirm_hits", cfg.confirm_hits);
  cfg.delete_misses = parse_u32_or_default(content, "delete_misses", cfg.delete_misses);

  cfg.enable_eoir = parse_int_or_default(content, "enable_eoir", cfg.enable_eoir);
  cfg.enable_eoir_updates =
      parse_int_or_default(content, "enable_eoir_updates", cfg.enable_eoir_updates);
  cfg.use_hungarian = parse_int_or_default(content, "use_hungarian", cfg.use_hungarian);

  return cfg;
}

}  // namespace sensor_fusion::tools
