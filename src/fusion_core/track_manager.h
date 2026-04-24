#pragma once

#include <utility>
#include <vector>

#include "fusion_core/track.h"
#include "messages/types.h"

namespace sensor_fusion::observability {
class Metrics;
}

namespace sensor_fusion::fusion_core {

struct TrackDeltas {
  std::vector<Track> created;
  std::vector<Track> updated;
  std::vector<Track> deleted;
};

struct TrackManagerConfig {
  double gate_mahalanobis2 = 9.21;
  double gate_mahal_eoir = 9.21;
  uint32_t confirm_hits = 3;
  uint32_t delete_misses = 5;
  double init_pos_var = 1000.0;
  double init_vel_var = 1000.0;
  bool use_hungarian = true;
  bool enable_eoir_updates = true;
  double unassigned_cost = 1e9;
};

class TrackManager {
 public:
  explicit TrackManager(TrackManagerConfig cfg,
                        sensor_fusion::observability::Metrics* metrics = nullptr);

  void predict_all(double dt);

  std::vector<Track> update_with_measurements(
      const std::vector<sensor_fusion::Measurement>& meas_batch);

  const std::vector<Track>& tracks() const;
  const TrackDeltas& last_deltas() const;

 private:
  TrackManagerConfig cfg_;
  sensor_fusion::observability::Metrics* metrics_;
  std::vector<Track> tracks_;
  TrackDeltas last_tick_deltas_;
  std::vector<size_t> radar_indices_scratch_;
  std::vector<size_t> eoir_indices_scratch_;
  std::vector<bool> track_assigned_scratch_;
  std::vector<bool> meas_assigned_scratch_;
  std::vector<bool> updated_flags_scratch_;
  std::vector<bool> radar_updated_flags_scratch_;
  std::vector<bool> eoir_updated_flags_scratch_;
  uint64_t next_track_id_ = 1;

  Track make_track_from_first_meas(const sensor_fusion::Measurement& m);

  std::vector<std::pair<size_t, size_t>> associate_nearest_neighbor(
      const std::vector<sensor_fusion::Measurement>& meas_batch,
      const std::vector<size_t>& radar_indices,
      std::vector<double>* matched_mahalanobis2);

  std::vector<std::pair<size_t, size_t>> associate_hungarian(
      const std::vector<sensor_fusion::Measurement>& meas_batch,
      const std::vector<size_t>& radar_indices,
      std::vector<double>* matched_mahalanobis2);

  void apply_lifecycle_rules();
};

}  // namespace sensor_fusion::fusion_core
