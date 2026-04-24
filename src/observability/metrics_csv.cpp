#include "observability/metrics_csv.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace sensor_fusion::observability {
namespace {

uint64_t counter_or_zero(const MetricsSnapshot& snap, const std::string& key) {
  const auto it = snap.counters.find(key);
  return it == snap.counters.end() ? 0 : it->second;
}

double gauge_or_zero(const MetricsSnapshot& snap, const std::string& key) {
  const auto it = snap.gauges.find(key);
  return it == snap.gauges.end() ? 0.0 : it->second;
}

MetricsSnapshot::Obs obs_or_zero(const MetricsSnapshot& snap, const std::string& key) {
  const auto it = snap.observations.find(key);
  return it == snap.observations.end() ? MetricsSnapshot::Obs{} : it->second;
}

}  // namespace

void write_metrics_csv(const std::string& path,
                       const std::string& run_name,
                       const MetricsSnapshot& snap) {
  const std::filesystem::path csv_path(path);
  const auto parent = csv_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  const bool write_header = !std::filesystem::exists(csv_path) ||
                            std::filesystem::file_size(csv_path) == 0;

  std::ofstream out(path, std::ios::out | std::ios::app);
  if (!out.is_open()) {
    throw std::runtime_error("failed to open CSV output: " + path);
  }

  if (write_header) {
    out << "run_name,tracks_confirmed,assoc_success,assoc_attempts,meas_radar_total,"
           "radar_updates_total,eoir_updates_total,pos_error_count,pos_error_mean,"
           "pos_error_min,pos_error_max,ekf_update_skipped_total,"
           "covariance_stabilized_total,nonfinite_detected_total\n";
  }

  const auto pos_error = obs_or_zero(snap, "pos_error");
  const double pos_error_mean =
      pos_error.count == 0 ? 0.0 : pos_error.sum / static_cast<double>(pos_error.count);

  out << run_name << ","
      << gauge_or_zero(snap, "tracks_confirmed") << ","
      << counter_or_zero(snap, "assoc_success") << ","
      << counter_or_zero(snap, "assoc_attempts") << ","
      << counter_or_zero(snap, "meas_radar_total") << ","
      << counter_or_zero(snap, "radar_updates_total") << ","
      << counter_or_zero(snap, "eoir_updates_total") << ","
      << pos_error.count << ","
      << pos_error_mean << ","
      << pos_error.min << ","
      << pos_error.max << ","
      << counter_or_zero(snap, "ekf_update_skipped_total") << ","
      << counter_or_zero(snap, "covariance_stabilized_total") << ","
      << counter_or_zero(snap, "nonfinite_detected_total") << "\n";
}

}  // namespace sensor_fusion::observability
