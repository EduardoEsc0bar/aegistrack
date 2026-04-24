#include "decision/assignment.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fusion_core/association/hungarian.h"

namespace sensor_fusion::decision {
namespace {

double distance3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  const double dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

std::vector<AssignmentDecision> compute_assignments(
    const std::vector<sensor_fusion::agents::InterceptorState>& interceptors,
    const std::vector<AssignmentTrack>& tracks,
    const AssignmentConfig& cfg) {
  std::vector<AssignmentDecision> result;
  if (interceptors.empty() || tracks.empty()) {
    return result;
  }

  std::vector<AssignmentTrack> sorted_tracks = tracks;
  std::sort(sorted_tracks.begin(), sorted_tracks.end(), [](const AssignmentTrack& a,
                                                           const AssignmentTrack& b) {
    if (a.threat_score != b.threat_score) {
      return a.threat_score > b.threat_score;
    }
    return a.track_id.value < b.track_id.value;
  });

  std::unordered_map<uint64_t, size_t> track_index_by_id;
  for (size_t i = 0; i < sorted_tracks.size(); ++i) {
    track_index_by_id.emplace(sorted_tracks[i].track_id.value, i);
  }

  std::vector<size_t> eligible_rows;
  eligible_rows.reserve(interceptors.size());

  for (size_t i = 0; i < interceptors.size(); ++i) {
    const auto& interceptor = interceptors[i];
    if (!interceptor.engaged) {
      eligible_rows.push_back(i);
      continue;
    }

    if (!cfg.retask_enable) {
      continue;
    }

    const double dist_to_current =
        distance3(interceptor.position, interceptor.assigned_target_position);
    if (dist_to_current > cfg.commit_distance_m) {
      eligible_rows.push_back(i);
    }
  }

  if (eligible_rows.empty()) {
    return result;
  }

  const double kUnassignedCost = 1e12;
  std::vector<std::vector<double>> cost(
      eligible_rows.size(), std::vector<double>(sorted_tracks.size(), kUnassignedCost));

  for (size_t row = 0; row < eligible_rows.size(); ++row) {
    const auto& interceptor = interceptors[eligible_rows[row]];
    for (size_t col = 0; col < sorted_tracks.size(); ++col) {
      const auto& track = sorted_tracks[col];
      const double dist = distance3(interceptor.position, track.position);
      const double threat = std::max(0.05, track.threat_score);
      cost[row][col] = dist / threat;
    }
  }

  const std::vector<int> assignment =
      sensor_fusion::fusion_core::association::hungarian_solve(cost);

  std::vector<bool> track_used(sorted_tracks.size(), false);
  for (size_t row = 0; row < assignment.size() && row < eligible_rows.size(); ++row) {
    const int col_i = assignment[row];
    if (col_i < 0) {
      continue;
    }
    const size_t col = static_cast<size_t>(col_i);
    if (col >= sorted_tracks.size()) {
      continue;
    }
    if (cost[row][col] >= kUnassignedCost) {
      continue;
    }
    if (!cfg.allow_multi_engage && track_used[col]) {
      continue;
    }

    track_used[col] = true;
    const auto& interceptor = interceptors[eligible_rows[row]];
    const auto& target_track = sorted_tracks[col];
    const AssignmentAction action =
        (interceptor.engaged && interceptor.target_id != target_track.track_id)
            ? AssignmentAction::Retasked
            : AssignmentAction::Assigned;

    result.push_back(AssignmentDecision{
        .interceptor_id = interceptor.interceptor_id,
        .track_id = target_track.track_id,
        .action = action,
        .cost = cost[row][col],
    });
  }

  if (cfg.allow_multi_engage && !sorted_tracks.empty()) {
    const auto best_track = sorted_tracks.front().track_id;
    for (size_t row = 0; row < eligible_rows.size(); ++row) {
      const uint64_t interceptor_id = interceptors[eligible_rows[row]].interceptor_id;
      const bool already_assigned =
          std::any_of(result.begin(), result.end(), [&](const AssignmentDecision& decision) {
            return decision.interceptor_id == interceptor_id;
          });
      if (!already_assigned) {
        result.push_back(AssignmentDecision{
            .interceptor_id = interceptor_id,
            .track_id = best_track,
            .action = AssignmentAction::Assigned,
            .cost = 0.0,
        });
      }
    }
  }

  std::sort(result.begin(), result.end(), [](const AssignmentDecision& a,
                                             const AssignmentDecision& b) {
    return a.interceptor_id < b.interceptor_id;
  });
  return result;
}

}  // namespace sensor_fusion::decision
