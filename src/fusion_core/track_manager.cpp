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
  last_tick_deltas_.updated.clear();
  last_tick_deltas_.deleted.clear();

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

  for (size_t i = 0; i < track_count_before; ++i) {
    if (!track_assigned_scratch_[i]) {
      tracks_[i].mark_miss();
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

        if (metrics_ != nullptr) {
          if (gate.numerical_issue) {
            metrics_->inc("nonfinite_detected_total");
          }
          if (std::isfinite(gate.mahalanobis2)) {
            metrics_->observe("mahal", gate.mahalanobis2);
          }
        }

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

    metrics_->inc("radar_updates_total", static_cast<uint64_t>(radar_updates));
    metrics_->inc("eoir_updates_total", static_cast<uint64_t>(eoir_updates));
    metrics_->inc("tracks_with_multisensor_updates",
                  static_cast<uint64_t>(tracks_with_multisensor_updates));

    size_t confirmed_count = 0;
    for (const auto& track : tracks_) {
      if (track.status() == TrackStatus::Confirmed) {
        ++confirmed_count;
      }
    }

    metrics_->set_gauge("tracks_total", static_cast<double>(tracks_.size()));
    metrics_->set_gauge("tracks_confirmed", static_cast<double>(confirmed_count));
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
      if (metrics_ != nullptr) {
        if (gate.numerical_issue) {
          metrics_->inc("nonfinite_detected_total");
        }
        if (std::isfinite(gate.mahalanobis2)) {
          metrics_->observe("mahal", gate.mahalanobis2);
        }
      }
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
      if (metrics_ != nullptr) {
        if (gate.numerical_issue) {
          metrics_->inc("nonfinite_detected_total");
        }
        if (std::isfinite(gate.mahalanobis2)) {
          metrics_->observe("mahal", gate.mahalanobis2);
        }
      }
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
      track.set_status(TrackStatus::Deleted);
      last_tick_deltas_.deleted.push_back(track);
      continue;
    }

    if (track.status() == TrackStatus::Tentative && track.quality().hits >= cfg_.confirm_hits) {
      track.set_status(TrackStatus::Confirmed);
    }
  }

  tracks_.erase(
      std::remove_if(tracks_.begin(), tracks_.end(),
                     [](const Track& track) { return track.status() == TrackStatus::Deleted; }),
      tracks_.end());
}

}  // namespace sensor_fusion::fusion_core
