#include "fusion_core/track_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <Eigen/Dense>

#include "fusion_core/association/hungarian.h"
#include "fusion_core/gating.h"
#include "observability/metrics.h"

namespace sensor_fusion::fusion_core {

namespace {

bool measurement_to_range_bearing(const sensor_fusion::Measurement& m,
                                  double& range,
                                  double& bearing,
                                  Eigen::Matrix2d& R) {
  if (m.measurement_type != sensor_fusion::MeasurementType::RangeBearing2D) {
    return false;
  }
  if (m.z_dim != 2 || m.z.size() < 2 || !sensor_fusion::validate_covariance_shape(m) ||
      m.R_rowmajor.size() < 4) {
    return false;
  }

  range = m.z[0];
  bearing = m.z[1];
  R << m.R_rowmajor[0], m.R_rowmajor[1], m.R_rowmajor[2], m.R_rowmajor[3];
  return true;
}

bool measurement_to_bearing_elevation(const sensor_fusion::Measurement& m,
                                      double& bearing,
                                      double& elevation,
                                      Eigen::Matrix2d& R) {
  if (m.measurement_type != sensor_fusion::MeasurementType::BearingElevation) {
    return false;
  }
  if (m.z_dim != 2 || m.z.size() < 2 || !sensor_fusion::validate_covariance_shape(m) ||
      m.R_rowmajor.size() < 4) {
    return false;
  }

  bearing = m.z[0];
  elevation = m.z[1];
  R << m.R_rowmajor[0], m.R_rowmajor[1], m.R_rowmajor[2], m.R_rowmajor[3];
  return true;
}

std::array<double, 3> track_position(const Track& track) {
  const auto x = track.filter().state();
  return {x(0), x(1), x(2)};
}

double distance3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  const double dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void observe_gate_metric(sensor_fusion::observability::Metrics* metrics,
                         const GateResult& gate) {
  if (metrics == nullptr) {
    return;
  }
  if (gate.numerical_issue) {
    metrics->inc("nonfinite_detected_total");
  }
  if (!std::isfinite(gate.mahalanobis2)) {
    return;
  }

  metrics->observe("mahal", gate.mahalanobis2);
  metrics->observe("assoc_candidate_mahalanobis2", gate.mahalanobis2);
  if (gate.pass) {
    metrics->observe("assoc_passed_gate_mahalanobis2", gate.mahalanobis2);
  } else {
    metrics->inc("assoc_gated_out_measurements");
    metrics->observe("assoc_rejected_mahalanobis2", gate.mahalanobis2);
  }
}

}  // namespace

TrackManager::TrackManager(TrackManagerConfig cfg,
                           sensor_fusion::observability::Metrics* metrics)
    : cfg_(cfg), metrics_(metrics) {
#if defined(AEGISTRACK_REALTIME)
  tracks_.reserve(256);
  last_tick_deltas_.created.reserve(64);
  last_tick_deltas_.updated.reserve(128);
  last_tick_deltas_.deleted.reserve(64);
  radar_indices_scratch_.reserve(512);
  eoir_indices_scratch_.reserve(512);
  track_assigned_scratch_.reserve(256);
  meas_assigned_scratch_.reserve(512);
  updated_flags_scratch_.reserve(256);
  radar_updated_flags_scratch_.reserve(256);
  eoir_updated_flags_scratch_.reserve(256);
#endif
}

void TrackManager::predict_all(double dt) {
  for (auto& track : tracks_) {
    track.filter().predict(dt);
  }
}

std::vector<Track> TrackManager::update_with_measurements(
    const std::vector<sensor_fusion::Measurement>& meas_batch) {
  last_tick_deltas_.created.clear();
  last_tick_deltas_.confirmed.clear();
  last_tick_deltas_.updated.clear();
  last_tick_deltas_.coasted.clear();
  last_tick_deltas_.deleted.clear();
  last_tick_deltas_.fragmentation_warnings.clear();
  age_recently_deleted_tracks();

  const size_t track_count_before = tracks_.size();

  radar_indices_scratch_.clear();
  eoir_indices_scratch_.clear();
  radar_indices_scratch_.reserve(meas_batch.size());
  eoir_indices_scratch_.reserve(meas_batch.size());
  for (size_t i = 0; i < meas_batch.size(); ++i) {
    if (meas_batch[i].measurement_type == sensor_fusion::MeasurementType::RangeBearing2D) {
      radar_indices_scratch_.push_back(i);
    } else if (meas_batch[i].measurement_type ==
               sensor_fusion::MeasurementType::BearingElevation) {
      eoir_indices_scratch_.push_back(i);
    }
  }

  if (metrics_ != nullptr) {
    metrics_->inc("meas_radar_total", static_cast<uint64_t>(radar_indices_scratch_.size()));
    metrics_->inc("assoc_attempts", static_cast<uint64_t>(track_count_before));
  }

  std::vector<double> matched_mahalanobis2;
  std::vector<std::pair<size_t, size_t>> radar_matches;
  if (cfg_.use_hungarian) {
    radar_matches =
        associate_hungarian(meas_batch, radar_indices_scratch_, &matched_mahalanobis2);
  } else {
    radar_matches =
        associate_nearest_neighbor(meas_batch, radar_indices_scratch_, &matched_mahalanobis2);
  }

  track_assigned_scratch_.assign(track_count_before, false);
  meas_assigned_scratch_.assign(meas_batch.size(), false);
  updated_flags_scratch_.assign(track_count_before, false);
  radar_updated_flags_scratch_.assign(track_count_before, false);

  size_t radar_updates = 0;
  for (const auto& [track_index, meas_index] : radar_matches) {
    if (track_index >= track_count_before || meas_index >= meas_batch.size()) {
      continue;
    }

    double range = 0.0;
    double bearing = 0.0;
    Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
    if (!measurement_to_range_bearing(meas_batch[meas_index], range, bearing, R)) {
      continue;
    }

    const bool updated = tracks_[track_index].filter().update_range_bearing(range, bearing, R);
    if (!updated) {
      continue;
    }
    tracks_[track_index].mark_radar_hit(meas_batch[meas_index].confidence,
                                        meas_batch[meas_index].trace_id);

    track_assigned_scratch_[track_index] = true;
    meas_assigned_scratch_[meas_index] = true;
    updated_flags_scratch_[track_index] = true;
    radar_updated_flags_scratch_[track_index] = true;
    ++radar_updates;
  }

  if (metrics_ != nullptr) {
    for (const double mahalanobis2 : matched_mahalanobis2) {
      metrics_->observe("assoc_accepted_mahalanobis2", mahalanobis2);
    }
  }

  for (size_t i = 0; i < track_count_before; ++i) {
    if (!track_assigned_scratch_[i]) {
      tracks_[i].mark_miss();
      last_tick_deltas_.coasted.push_back(tracks_[i]);
      if (metrics_ != nullptr) {
        metrics_->inc("track_coasts_total");
        metrics_->observe("track_coast_misses", static_cast<double>(tracks_[i].quality().misses));
      }
    }
  }

  for (size_t k = 0; k < radar_indices_scratch_.size(); ++k) {
    const size_t meas_index = radar_indices_scratch_[k];
    if (meas_assigned_scratch_[meas_index]) {
      continue;
    }
    if (meas_batch[meas_index].confidence < 0.3) {
      continue;
    }

    double range = 0.0;
    double bearing = 0.0;
    Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
    if (!measurement_to_range_bearing(meas_batch[meas_index], range, bearing, R)) {
      continue;
    }

    Track created = make_track_from_first_meas(meas_batch[meas_index]);
    record_fragmentation_warning_if_needed(created);
    last_tick_deltas_.created.push_back(created);

    tracks_.push_back(std::move(created));
    updated_flags_scratch_.push_back(false);
    radar_updated_flags_scratch_.push_back(false);
  }

  eoir_updated_flags_scratch_.assign(tracks_.size(), false);
  size_t eoir_updates = 0;
  if (cfg_.enable_eoir_updates) {
    for (size_t k = 0; k < eoir_indices_scratch_.size(); ++k) {
      const size_t meas_index = eoir_indices_scratch_[k];

      double bearing = 0.0;
      double elevation = 0.0;
      Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
      if (!measurement_to_bearing_elevation(meas_batch[meas_index], bearing, elevation, R)) {
        continue;
      }

      size_t best_track_index = std::numeric_limits<size_t>::max();
      double best_mahal2 = std::numeric_limits<double>::infinity();

      for (size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
        const GateResult gate = gate_bearing_elevation(
            tracks_[track_index].filter(), bearing, elevation, R, cfg_.gate_mahal_eoir);
        observe_gate_metric(metrics_, gate);

        if (gate.pass && gate.mahalanobis2 < best_mahal2) {
          best_mahal2 = gate.mahalanobis2;
          best_track_index = track_index;
        }
      }

      if (best_track_index == std::numeric_limits<size_t>::max()) {
        continue;
      }

      const bool updated =
          tracks_[best_track_index].filter().update_bearing_elevation(bearing, elevation, R);
      if (!updated) {
        continue;
      }
      tracks_[best_track_index].mark_eoir_refinement_hit(meas_batch[meas_index].confidence,
                                                         meas_batch[meas_index].trace_id);
      eoir_updated_flags_scratch_[best_track_index] = true;
      updated_flags_scratch_[best_track_index] = true;
      ++eoir_updates;
    }
  }

  size_t tracks_with_multisensor_updates = 0;
  for (size_t i = 0; i < tracks_.size(); ++i) {
    if (radar_updated_flags_scratch_[i] && eoir_updated_flags_scratch_[i]) {
      ++tracks_with_multisensor_updates;
    }
  }

  for (size_t i = 0; i < tracks_.size(); ++i) {
    if (updated_flags_scratch_[i]) {
      last_tick_deltas_.updated.push_back(tracks_[i]);
    }
  }

  apply_lifecycle_rules();

  if (metrics_ != nullptr) {
    metrics_->inc("assoc_success", static_cast<uint64_t>(radar_updates));
    metrics_->inc("assoc_unassigned_tracks",
                  static_cast<uint64_t>(track_count_before - radar_updates));
    metrics_->inc("assoc_unassigned_meas",
                  static_cast<uint64_t>(radar_indices_scratch_.size() - radar_updates));
    metrics_->inc("assoc_unassigned_measurements",
                  static_cast<uint64_t>(radar_indices_scratch_.size() - radar_updates));

    const double assoc_success_rate =
        track_count_before == 0
            ? 0.0
            : static_cast<double>(radar_updates) / static_cast<double>(track_count_before);
    metrics_->set_gauge("assoc_success_rate", assoc_success_rate);

    metrics_->inc("radar_updates_total", static_cast<uint64_t>(radar_updates));
    metrics_->inc("eoir_updates_total", static_cast<uint64_t>(eoir_updates));
    metrics_->inc("tracks_with_multisensor_updates",
                  static_cast<uint64_t>(tracks_with_multisensor_updates));

    size_t confirmed_count = 0;
    uint64_t confirmed_age_ticks_sum = 0;
    for (const auto& track : tracks_) {
      if (track.status() == TrackStatus::Confirmed) {
        ++confirmed_count;
        confirmed_age_ticks_sum += track.quality().age_ticks;
      }
    }

    metrics_->set_gauge("tracks_total", static_cast<double>(tracks_.size()));
    metrics_->set_gauge("tracks_confirmed", static_cast<double>(confirmed_count));
    metrics_->set_gauge(
        "tracks_confirmed_avg_age_ticks",
        confirmed_count == 0
            ? 0.0
            : static_cast<double>(confirmed_age_ticks_sum) / static_cast<double>(confirmed_count));
  }

  return tracks_;
}

const std::vector<Track>& TrackManager::tracks() const {
  return tracks_;
}

const TrackDeltas& TrackManager::last_deltas() const {
  return last_tick_deltas_;
}

Track TrackManager::make_track_from_first_meas(const sensor_fusion::Measurement& m) {
  const double range = m.z[0];
  const double bearing = m.z[1];

  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = range * std::cos(bearing);
  x0(1) = range * std::sin(bearing);
  x0(2) = 0.0;

  Eigen::MatrixXd P0 = Eigen::MatrixXd::Zero(6, 6);
  P0(0, 0) = cfg_.init_pos_var;
  P0(1, 1) = cfg_.init_pos_var;
  P0(2, 2) = cfg_.init_pos_var;
  P0(3, 3) = cfg_.init_vel_var;
  P0(4, 4) = cfg_.init_vel_var;
  P0(5, 5) = cfg_.init_vel_var;

  sensor_fusion::EkfCv ekf;
  ekf.set_metrics(metrics_);
  ekf.initialize(x0, P0);

  Track track(sensor_fusion::TrackId(next_track_id_++), ekf, TrackStatus::Tentative);
  track.mark_radar_hit(m.confidence, m.trace_id);
  return track;
}

void TrackManager::age_recently_deleted_tracks() {
  for (auto& deleted : recently_deleted_tracks_) {
    if (deleted.remaining_ticks > 0) {
      --deleted.remaining_ticks;
    }
  }
  recently_deleted_tracks_.erase(
      std::remove_if(recently_deleted_tracks_.begin(), recently_deleted_tracks_.end(),
                     [](const RecentlyDeletedTrack& deleted) {
                       return deleted.remaining_ticks == 0;
                     }),
      recently_deleted_tracks_.end());
}

void TrackManager::record_fragmentation_warning_if_needed(const Track& created) {
  if (cfg_.fragmentation_warning_distance_m <= 0.0) {
    return;
  }

  const auto created_position = track_position(created);
  for (const auto& deleted : recently_deleted_tracks_) {
    if (!deleted.was_confirmed) {
      continue;
    }

    const double distance_m = distance3(created_position, deleted.position);
    if (distance_m > cfg_.fragmentation_warning_distance_m) {
      continue;
    }

    last_tick_deltas_.fragmentation_warnings.push_back(TrackDeltas::FragmentationWarning{
        .original_track_id = deleted.track_id,
        .new_track_id = created.id(),
        .distance_m = distance_m,
    });
    if (metrics_ != nullptr) {
      metrics_->inc("track_fragmentation_warnings_total");
      metrics_->inc("possible_id_switch_total");
      metrics_->observe("track_fragmentation_distance_m", distance_m);
    }
    return;
  }
}

std::vector<std::pair<size_t, size_t>> TrackManager::associate_nearest_neighbor(
    const std::vector<sensor_fusion::Measurement>& meas_batch,
    const std::vector<size_t>& radar_indices,
    std::vector<double>* matched_mahalanobis2) {
  std::vector<std::pair<size_t, size_t>> matches;
  std::vector<bool> meas_assigned(radar_indices.size(), false);

  for (size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
    size_t best_k = std::numeric_limits<size_t>::max();
    double best_mahal2 = std::numeric_limits<double>::infinity();

    for (size_t k = 0; k < radar_indices.size(); ++k) {
      if (meas_assigned[k]) {
        continue;
      }

      const size_t meas_index = radar_indices[k];
      double range = 0.0;
      double bearing = 0.0;
      Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
      if (!measurement_to_range_bearing(meas_batch[meas_index], range, bearing, R)) {
        continue;
      }

      const GateResult gate = gate_range_bearing(
          tracks_[track_index].filter(), range, bearing, R, cfg_.gate_mahalanobis2);
      observe_gate_metric(metrics_, gate);
      if (gate.pass && gate.mahalanobis2 < best_mahal2) {
        best_mahal2 = gate.mahalanobis2;
        best_k = k;
      }
    }

    if (best_k != std::numeric_limits<size_t>::max()) {
      meas_assigned[best_k] = true;
      matches.emplace_back(track_index, radar_indices[best_k]);
      if (matched_mahalanobis2 != nullptr) {
        matched_mahalanobis2->push_back(best_mahal2);
      }
    }
  }

  return matches;
}

std::vector<std::pair<size_t, size_t>> TrackManager::associate_hungarian(
    const std::vector<sensor_fusion::Measurement>& meas_batch,
    const std::vector<size_t>& radar_indices,
    std::vector<double>* matched_mahalanobis2) {
  std::vector<std::pair<size_t, size_t>> matches;
  if (tracks_.empty() || radar_indices.empty()) {
    return matches;
  }

  std::vector<std::vector<double>> cost(
      tracks_.size(), std::vector<double>(radar_indices.size(), cfg_.unassigned_cost));

  for (size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
    for (size_t k = 0; k < radar_indices.size(); ++k) {
      const size_t meas_index = radar_indices[k];

      double range = 0.0;
      double bearing = 0.0;
      Eigen::Matrix2d R = Eigen::Matrix2d::Zero();
      if (!measurement_to_range_bearing(meas_batch[meas_index], range, bearing, R)) {
        continue;
      }

      const GateResult gate = gate_range_bearing(
          tracks_[track_index].filter(), range, bearing, R, cfg_.gate_mahalanobis2);
      observe_gate_metric(metrics_, gate);
      if (gate.pass) {
        cost[track_index][k] = gate.mahalanobis2;
      }
    }
  }

  const std::vector<int> assignment = association::hungarian_solve(cost);
  for (size_t track_index = 0; track_index < assignment.size(); ++track_index) {
    const int assigned_k = assignment[track_index];
    if (assigned_k < 0) {
      continue;
    }

    const size_t k = static_cast<size_t>(assigned_k);
    if (k >= radar_indices.size()) {
      continue;
    }

    if (cost[track_index][k] >= cfg_.unassigned_cost) {
      continue;
    }

    matches.emplace_back(track_index, radar_indices[k]);
    if (matched_mahalanobis2 != nullptr) {
      matched_mahalanobis2->push_back(cost[track_index][k]);
    }
  }

  return matches;
}

void TrackManager::apply_lifecycle_rules() {
  for (auto& track : tracks_) {
    if (track.quality().misses >= cfg_.delete_misses) {
      const bool was_confirmed = track.status() == TrackStatus::Confirmed;
      track.set_status(TrackStatus::Deleted);
      last_tick_deltas_.deleted.push_back(track);
      recently_deleted_tracks_.push_back(RecentlyDeletedTrack{
          .track_id = track.id(),
          .position = track_position(track),
          .was_confirmed = was_confirmed,
          .remaining_ticks = cfg_.fragmentation_recent_delete_window_ticks,
      });
      if (metrics_ != nullptr) {
        metrics_->inc("tracks_deleted_total");
        metrics_->observe("track_lifetime_ticks", static_cast<double>(track.quality().age_ticks));
        metrics_->observe("track_deleted_misses", static_cast<double>(track.quality().misses));
      }
      continue;
    }

    if (track.status() == TrackStatus::Tentative && track.quality().hits >= cfg_.confirm_hits) {
      track.set_status(TrackStatus::Confirmed);
      last_tick_deltas_.confirmed.push_back(track);
      if (metrics_ != nullptr) {
        metrics_->inc("tracks_confirmed_created_total");
      }
    }
  }

  tracks_.erase(
      std::remove_if(tracks_.begin(), tracks_.end(),
                     [](const Track& track) { return track.status() == TrackStatus::Deleted; }),
      tracks_.end());
}

}  // namespace sensor_fusion::fusion_core
