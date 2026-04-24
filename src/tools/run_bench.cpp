#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "observability/metrics_csv.h"
#include "tools/bench_harness.h"
#include "tools/sim_config.h"

namespace {

struct CliOptions {
  std::string out_csv = "results/bench.csv";
  uint64_t seed = 1;
  int runs = 5;
  double deadline_ms = 10.0;
  int profile_every = 0;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--out_csv" && i + 1 < argc) {
      options.out_csv = argv[++i];
    } else if (arg == "--seed" && i + 1 < argc) {
      options.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--runs" && i + 1 < argc) {
      options.runs = std::stoi(argv[++i]);
    } else if (arg == "--deadline_ms" && i + 1 < argc) {
      options.deadline_ms = std::stod(argv[++i]);
    } else if (arg == "--profile_every" && i + 1 < argc) {
      options.profile_every = std::stoi(argv[++i]);
    }
  }
  return options;
}

struct Aggregate {
  int runs = 0;
  double sum_pos_error_mean = 0.0;
  double sum_tracks_confirmed = 0.0;
  double sum_assoc_success_rate = 0.0;
};

double counter_as_double(const sensor_fusion::observability::MetricsSnapshot& snap,
                         const std::string& key) {
  const auto it = snap.counters.find(key);
  return it == snap.counters.end() ? 0.0 : static_cast<double>(it->second);
}

double gauge_as_double(const sensor_fusion::observability::MetricsSnapshot& snap,
                       const std::string& key) {
  const auto it = snap.gauges.find(key);
  return it == snap.gauges.end() ? 0.0 : it->second;
}

double obs_mean(const sensor_fusion::observability::MetricsSnapshot& snap,
                const std::string& key) {
  const auto it = snap.observations.find(key);
  if (it == snap.observations.end() || it->second.count == 0) {
    return 0.0;
  }
  return it->second.sum / static_cast<double>(it->second.count);
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions cli = parse_args(argc, argv);

  const sensor_fusion::tools::SimConfig radar_only_cfg =
      sensor_fusion::tools::load_sim_config_json("config/demo_radar_only.json");
  const sensor_fusion::tools::SimConfig radar_eoir_cfg =
      sensor_fusion::tools::load_sim_config_json("config/demo_radar_eoir.json");
  const sensor_fusion::tools::SimConfig nn_template_cfg =
      sensor_fusion::tools::load_sim_config_json("config/demo_nn_vs_hungarian.json");

  struct Scenario {
    std::string name;
    sensor_fusion::tools::SimConfig cfg;
  };

  sensor_fusion::tools::SimConfig radar_only_h = radar_only_cfg;
  radar_only_h.use_hungarian = 1;
  radar_only_h.enable_eoir = 0;
  radar_only_h.enable_eoir_updates = 0;

  sensor_fusion::tools::SimConfig radar_eoir_h = radar_eoir_cfg;
  radar_eoir_h.use_hungarian = 1;
  radar_eoir_h.enable_eoir = 1;
  radar_eoir_h.enable_eoir_updates = 1;

  sensor_fusion::tools::SimConfig radar_only_nn = nn_template_cfg;
  radar_only_nn.use_hungarian = 0;
  radar_only_nn.enable_eoir = 0;
  radar_only_nn.enable_eoir_updates = 0;

  sensor_fusion::tools::SimConfig radar_eoir_nn = nn_template_cfg;
  radar_eoir_nn.use_hungarian = 0;
  radar_eoir_nn.enable_eoir = 1;
  radar_eoir_nn.enable_eoir_updates = 1;

  const std::vector<Scenario> scenarios = {
      {"radar_only_hungarian", radar_only_h},
      {"radar_eoir_hungarian", radar_eoir_h},
      {"radar_only_nn", radar_only_nn},
      {"radar_eoir_nn", radar_eoir_nn},
  };

  std::map<std::string, Aggregate> aggregates;

  for (const auto& scenario : scenarios) {
    for (int run_idx = 0; run_idx < cli.runs; ++run_idx) {
      const uint64_t run_seed = cli.seed + static_cast<uint64_t>(run_idx);

      const auto result = sensor_fusion::tools::run_bench_case_detailed(
          scenario.cfg, run_seed,
          sensor_fusion::tools::BenchOptions{
              .deadline_ms = cli.deadline_ms,
              .profile_every = cli.profile_every,
          });
      const auto& snap = result.metrics;
      const std::string run_name = scenario.name + "_seed" + std::to_string(run_seed);
      sensor_fusion::observability::write_metrics_csv(cli.out_csv, run_name, snap);

      const double assoc_attempts = counter_as_double(snap, "assoc_attempts");
      const double assoc_success = counter_as_double(snap, "assoc_success");
      const double assoc_success_rate =
          assoc_attempts <= 0.0 ? 0.0 : assoc_success / assoc_attempts;

      auto& agg = aggregates[scenario.name];
      agg.runs += 1;
      agg.sum_pos_error_mean += obs_mean(snap, "pos_error");
      agg.sum_tracks_confirmed += gauge_as_double(snap, "tracks_confirmed");
      agg.sum_assoc_success_rate += assoc_success_rate;
    }
  }

  std::cout << std::left << std::setw(26) << "scenario" << std::setw(18)
            << "avg_pos_error" << std::setw(22) << "avg_tracks_confirmed"
            << "avg_assoc_success_rate" << std::endl;

  for (const auto& [name, agg] : aggregates) {
    const double denom = agg.runs > 0 ? static_cast<double>(agg.runs) : 1.0;
    std::cout << std::left << std::setw(26) << name << std::setw(18)
              << (agg.sum_pos_error_mean / denom) << std::setw(22)
              << (agg.sum_tracks_confirmed / denom)
              << (agg.sum_assoc_success_rate / denom) << std::endl;
  }

  return 0;
}
