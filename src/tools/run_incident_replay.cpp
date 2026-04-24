#include <iostream>
#include <string>

#include "tools/incident_replay.h"

namespace {

struct CliOptions {
  std::string incident = "incidents/run1.json";
  int use_hungarian = 1;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;
  double gate_mahal = 9.21;
  double engage_score_threshold = 3.0;
  double max_engagement_range_m = 5000.0;
  double min_confidence_to_engage = 0.6;
  double no_engage_zone_radius_m = 0.0;
  double engagement_timeout_s = 10.0;
  double interceptor_speed = 150.0;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--incident" && i + 1 < argc) {
      options.incident = argv[++i];
    } else if (arg == "--use_hungarian" && i + 1 < argc) {
      options.use_hungarian = std::stoi(argv[++i]);
    } else if (arg == "--confirm_hits" && i + 1 < argc) {
      options.confirm_hits = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--delete_misses" && i + 1 < argc) {
      options.delete_misses = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--gate_mahal" && i + 1 < argc) {
      options.gate_mahal = std::stod(argv[++i]);
    } else if (arg == "--engage_score_threshold" && i + 1 < argc) {
      options.engage_score_threshold = std::stod(argv[++i]);
    } else if (arg == "--max_engagement_range_m" && i + 1 < argc) {
      options.max_engagement_range_m = std::stod(argv[++i]);
    } else if (arg == "--min_confidence_to_engage" && i + 1 < argc) {
      options.min_confidence_to_engage = std::stod(argv[++i]);
    } else if (arg == "--no_engage_zone_radius_m" && i + 1 < argc) {
      options.no_engage_zone_radius_m = std::stod(argv[++i]);
    } else if (arg == "--engagement_timeout_s" && i + 1 < argc) {
      options.engagement_timeout_s = std::stod(argv[++i]);
    } else if (arg == "--interceptor_speed" && i + 1 < argc) {
      options.interceptor_speed = std::stod(argv[++i]);
    }
  }
  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions cli = parse_args(argc, argv);

  const auto result = sensor_fusion::tools::replay_incident_file(
      cli.incident,
      sensor_fusion::tools::IncidentReplayOptions{
          .use_hungarian = cli.use_hungarian,
          .confirm_hits = cli.confirm_hits,
          .delete_misses = cli.delete_misses,
          .gate_mahal = cli.gate_mahal,
          .engage_score_threshold = cli.engage_score_threshold,
          .max_engagement_range_m = cli.max_engagement_range_m,
          .min_confidence_to_engage = cli.min_confidence_to_engage,
          .no_engage_zone_radius_m = cli.no_engage_zone_radius_m,
          .engagement_timeout_s = cli.engagement_timeout_s,
          .interceptor_speed = cli.interceptor_speed,
      },
      &std::cout);

  std::cout << "final_tracks=" << result.tracks.size() << " interceptor_engaged="
            << (result.interceptor_state.engaged ? 1 : 0) << std::endl;

  if (result.had_snapshot) {
    std::cout << "snapshot_match=" << (result.snapshot_match ? 1 : 0) << std::endl;
    return result.snapshot_match ? 0 : 2;
  }

  return 0;
}
