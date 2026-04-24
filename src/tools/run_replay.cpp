#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "fusion_core/track_manager.h"
#include "observability/jsonl_logger.h"

namespace {

struct CliOptions {
  std::string input = "logs/run.jsonl";
  int use_hungarian = 1;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;
  double gate_mahal = 9.21;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--input" && i + 1 < argc) {
      options.input = argv[++i];
    } else if (arg == "--use_hungarian" && i + 1 < argc) {
      options.use_hungarian = std::stoi(argv[++i]);
    } else if (arg == "--confirm_hits" && i + 1 < argc) {
      options.confirm_hits = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--delete_misses" && i + 1 < argc) {
      options.delete_misses = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--gate_mahal" && i + 1 < argc) {
      options.gate_mahal = std::stod(argv[++i]);
    }
  }

  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  std::vector<sensor_fusion::Measurement> measurements =
      sensor_fusion::observability::load_measurements_from_jsonl(options.input);

  std::stable_sort(measurements.begin(), measurements.end(),
                   [](const sensor_fusion::Measurement& a, const sensor_fusion::Measurement& b) {
                     if (a.t_sent < b.t_sent) {
                       return true;
                     }
                     if (b.t_sent < a.t_sent) {
                       return false;
                     }
                     return a.t_meas < b.t_meas;
                   });

  sensor_fusion::fusion_core::TrackManager manager(
      sensor_fusion::fusion_core::TrackManagerConfig{
          .gate_mahalanobis2 = options.gate_mahal,
          .confirm_hits = options.confirm_hits,
          .delete_misses = options.delete_misses,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = options.use_hungarian != 0,
          .unassigned_cost = 1e9,
      });

  if (!measurements.empty()) {
    double prev_fusion_s = measurements.front().t_sent.to_seconds();
    size_t i = 0;
    while (i < measurements.size()) {
      const double current_t_sent_s = measurements[i].t_sent.to_seconds();
      double dt = current_t_sent_s - prev_fusion_s;
      if (dt < 0.0) {
        dt = 0.0;
      }

      manager.predict_all(dt);

      std::vector<sensor_fusion::Measurement> batch;
      while (i < measurements.size() &&
             std::abs(measurements[i].t_sent.to_seconds() - current_t_sent_s) < 1e-12) {
        batch.push_back(measurements[i]);
        ++i;
      }

      manager.update_with_measurements(batch);
      prev_fusion_s = current_t_sent_s;
    }
  }

  for (const auto& track : manager.tracks()) {
    if (track.status() != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
      continue;
    }

    const auto x = track.filter().state();
    std::cout << "track_id=" << track.id() << " px=" << x(0) << " py=" << x(1)
              << " hits=" << track.quality().hits << " misses=" << track.quality().misses
              << std::endl;
  }

  return 0;
}
