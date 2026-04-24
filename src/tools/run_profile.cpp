#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "tools/bench_harness.h"
#include "tools/sim_config.h"

namespace {

struct CliOptions {
  std::string config_path = "config/demo_radar_eoir.json";
  std::string out_json = "results/profile.json";
  uint64_t seed = 1;
  double deadline_ms = 10.0;
  int profile_every = 50;
  int ticks = -1;
};

CliOptions parse_args(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      options.config_path = argv[++i];
    } else if (arg == "--out" && i + 1 < argc) {
      options.out_json = argv[++i];
    } else if (arg == "--seed" && i + 1 < argc) {
      options.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--deadline_ms" && i + 1 < argc) {
      options.deadline_ms = std::stod(argv[++i]);
    } else if (arg == "--profile_every" && i + 1 < argc) {
      options.profile_every = std::stoi(argv[++i]);
    } else if (arg == "--ticks" && i + 1 < argc) {
      options.ticks = std::stoi(argv[++i]);
    }
  }
  return options;
}

double counter_as_double(const sensor_fusion::observability::MetricsSnapshot& snap,
                         const std::string& key) {
  const auto it = snap.counters.find(key);
  return it == snap.counters.end() ? 0.0 : static_cast<double>(it->second);
}

}  // namespace

int main(int argc, char** argv) {
  const CliOptions options = parse_args(argc, argv);

  sensor_fusion::tools::SimConfig config;
  try {
    config = sensor_fusion::tools::load_sim_config_json(options.config_path);
  } catch (const std::exception& ex) {
    std::cerr << "warning: failed to load config '" << options.config_path
              << "', using defaults: " << ex.what() << std::endl;
  }

  if (options.ticks > 0) {
    config.ticks = options.ticks;
  }

  const auto result = sensor_fusion::tools::run_bench_case_detailed(
      config, options.seed,
      sensor_fusion::tools::BenchOptions{
          .deadline_ms = options.deadline_ms,
          .profile_every = options.profile_every,
      });

  const auto& snap = result.metrics;
  const double ticks_per_sec =
      result.elapsed_s > 0.0 ? static_cast<double>(result.tick_stats.ticks) / result.elapsed_s
                             : 0.0;
  const double assoc_attempts = counter_as_double(snap, "assoc_attempts");
  const double assoc_success = counter_as_double(snap, "assoc_success");
  const double assoc_rate = assoc_attempts > 0.0 ? assoc_success / assoc_attempts : 0.0;
  const double queue_drops = counter_as_double(snap, "ingest_queue_drops_total");

  std::cout << "profile ticks=" << result.tick_stats.ticks
            << " ticks_per_sec=" << ticks_per_sec
            << " mean_tick_ms=" << result.tick_stats.mean_ms
            << " max_tick_ms=" << result.tick_stats.max_ms
            << " deadline_misses=" << result.tick_stats.deadline_misses
            << " assoc_success_rate=" << assoc_rate
            << " queue_drops=" << queue_drops << std::endl;

  const std::filesystem::path out_path(options.out_json);
  if (out_path.has_parent_path()) {
    std::filesystem::create_directories(out_path.parent_path());
  }

  std::ofstream out(options.out_json);
  if (!out.is_open()) {
    std::cerr << "failed to open profile report path: " << options.out_json << std::endl;
    return 1;
  }

  out << std::fixed << std::setprecision(6);
  out << "{\n";
  out << "  \"config\": \"" << options.config_path << "\",\n";
  out << "  \"seed\": " << options.seed << ",\n";
  out << "  \"ticks\": " << result.tick_stats.ticks << ",\n";
  out << "  \"elapsed_s\": " << result.elapsed_s << ",\n";
  out << "  \"ticks_per_sec\": " << ticks_per_sec << ",\n";
  out << "  \"mean_tick_ms\": " << result.tick_stats.mean_ms << ",\n";
  out << "  \"max_tick_ms\": " << result.tick_stats.max_ms << ",\n";
  out << "  \"min_tick_ms\": " << result.tick_stats.min_ms << ",\n";
  out << "  \"deadline_misses\": " << result.tick_stats.deadline_misses << ",\n";
  out << "  \"assoc_success_rate\": " << assoc_rate << ",\n";
  out << "  \"queue_drops\": " << queue_drops << "\n";
  out << "}\n";

  return 0;
}
