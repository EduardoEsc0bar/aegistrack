#include "tools/incident_replay.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "decision/decision_engine.h"
#include "fusion_core/track_manager.h"
#include "observability/jsonl_logger.h"

namespace sensor_fusion::tools {
namespace {

struct SnapshotTrack {
  sensor_fusion::fusion_core::TrackStatus status = sensor_fusion::fusion_core::TrackStatus::Tentative;
  uint32_t hits = 0;
  uint32_t misses = 0;
  std::vector<double> x;
  std::vector<double> P;
};

struct SnapshotInterceptor {
  bool present = false;
  bool engaged = false;
  sensor_fusion::TrackId target_id{0};
  std::array<double, 3> position{0.0, 0.0, 0.0};
  std::array<double, 3> velocity{0.0, 0.0, 0.0};
};

size_t find_key_value_start(const std::string& line, const std::string& key) {
  const std::string token = std::string("\"") + key + "\":";
  const size_t key_pos = line.find(token);
  if (key_pos == std::string::npos) {
    throw std::runtime_error("missing key: " + key);
  }
  size_t pos = key_pos + token.size();
  while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) {
    ++pos;
  }
  return pos;
}

std::string parse_json_string(const std::string& line, const std::string& key) {
  const size_t pos = find_key_value_start(line, key);
  if (line[pos] != '"') {
    throw std::runtime_error("expected string field: " + key);
  }
  const size_t end = line.find('"', pos + 1);
  if (end == std::string::npos) {
    throw std::runtime_error("unterminated string field: " + key);
  }
  return line.substr(pos + 1, end - pos - 1);
}

double parse_json_number(const std::string& line, const std::string& key) {
  const size_t pos = find_key_value_start(line, key);
  size_t end = pos;
  while (end < line.size() && line[end] != ',' && line[end] != '}') {
    ++end;
  }
  return std::stod(line.substr(pos, end - pos));
}

std::vector<double> parse_json_number_array(const std::string& line, const std::string& key) {
  size_t pos = find_key_value_start(line, key);
  if (line[pos] != '[') {
    throw std::runtime_error("expected array field: " + key);
  }
  ++pos;
  const size_t end = line.find(']', pos);
  if (end == std::string::npos) {
    throw std::runtime_error("unterminated array field: " + key);
  }

  const std::string inner = line.substr(pos, end - pos);
  std::vector<double> values;
  if (inner.empty()) {
    return values;
  }

  std::stringstream ss(inner);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (!token.empty()) {
      values.push_back(std::stod(token));
    }
  }
  return values;
}

sensor_fusion::fusion_core::TrackStatus parse_status(const std::string& status) {
  if (status == "Confirmed") {
    return sensor_fusion::fusion_core::TrackStatus::Confirmed;
  }
  if (status == "Deleted") {
    return sensor_fusion::fusion_core::TrackStatus::Deleted;
  }
  return sensor_fusion::fusion_core::TrackStatus::Tentative;
}

void parse_snapshot_lines(const std::string& path,
                          std::unordered_map<uint64_t, SnapshotTrack>* track_snapshots,
                          SnapshotInterceptor* interceptor_snapshot,
                          bool* had_snapshot) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open incident file: " + path);
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    const std::string type = parse_json_string(line, "type");
    if (type == "replay_snapshot_track") {
      const uint64_t track_id = static_cast<uint64_t>(parse_json_number(line, "track_id"));
      (*track_snapshots)[track_id] = SnapshotTrack{
          .status = parse_status(parse_json_string(line, "status")),
          .hits = static_cast<uint32_t>(parse_json_number(line, "hits")),
          .misses = static_cast<uint32_t>(parse_json_number(line, "misses")),
          .x = parse_json_number_array(line, "x"),
          .P = parse_json_number_array(line, "P"),
      };
      *had_snapshot = true;
      continue;
    }

    if (type == "replay_snapshot_interceptor") {
      interceptor_snapshot->present = true;
      interceptor_snapshot->engaged = parse_json_number(line, "engaged") > 0.5;
      interceptor_snapshot->target_id =
          sensor_fusion::TrackId(static_cast<uint64_t>(parse_json_number(line, "target_id")));

      const auto pos = parse_json_number_array(line, "position");
      const auto vel = parse_json_number_array(line, "velocity");
      if (pos.size() >= 3) {
        interceptor_snapshot->position = {pos[0], pos[1], pos[2]};
      }
      if (vel.size() >= 3) {
        interceptor_snapshot->velocity = {vel[0], vel[1], vel[2]};
      }
      *had_snapshot = true;
    }
  }
}

bool approx_equal(double a, double b, double epsilon = 1e-6) {
  return std::abs(a - b) <= epsilon;
}

}  // namespace

IncidentReplayResult replay_incident_file(const std::string& incident_path,
                                          const IncidentReplayOptions& options,
                                          std::ostream* timeline_out) {
  std::vector<sensor_fusion::Measurement> measurements =
      sensor_fusion::observability::load_measurements_from_jsonl(incident_path);

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
          .gate_mahal_eoir = options.gate_mahal,
          .confirm_hits = options.confirm_hits,
          .delete_misses = options.delete_misses,
          .init_pos_var = 1000.0,
          .init_vel_var = 1000.0,
          .use_hungarian = options.use_hungarian != 0,
          .enable_eoir_updates = true,
          .unassigned_cost = 1e9,
      });

  sensor_fusion::decision::DecisionEngine decision_engine(sensor_fusion::decision::DecisionConfig{
      .engage_score_threshold = options.engage_score_threshold,
      .max_engagement_range_m = options.max_engagement_range_m,
      .min_confidence_to_engage = options.min_confidence_to_engage,
      .no_engage_zone_radius_m = options.no_engage_zone_radius_m,
      .engagement_timeout_s = options.engagement_timeout_s,
  });
  sensor_fusion::agents::Interceptor interceptor(options.interceptor_speed);
  std::unordered_map<uint64_t, double> engagement_time_by_track;

  if (!measurements.empty()) {
    double prev_time_s = measurements.front().t_sent.to_seconds();
    size_t i = 0;
    uint64_t tick = 0;

    while (i < measurements.size()) {
      const double time_s = measurements[i].t_sent.to_seconds();
      const double dt = std::max(0.0, time_s - prev_time_s);
      manager.predict_all(dt);

      std::vector<sensor_fusion::Measurement> batch;
      while (i < measurements.size() &&
             std::abs(measurements[i].t_sent.to_seconds() - time_s) < 1e-12) {
        batch.push_back(measurements[i]);
        ++i;
      }

      const auto tracks = manager.update_with_measurements(batch);
      std::unordered_map<uint64_t, std::array<double, 3>> confirmed_positions;
      for (const auto& track : tracks) {
        if (track.status() != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
          continue;
        }

        const auto x = track.filter().state();
        const std::array<double, 3> target_pos = {x(0), x(1), x(2)};
        confirmed_positions.emplace(track.id().value, target_pos);

        const bool was_engaged = interceptor.state().engaged;
        const auto decision = decision_engine.decide(
            track, !interceptor.state().engaged, target_pos,
            [&](sensor_fusion::TrackId id, const std::array<double, 3>& assigned_pos) {
              interceptor.assign_target(id, assigned_pos);
            });
        if (timeline_out != nullptr) {
          *timeline_out << "tick=" << tick << " t=" << time_s << " track=" << track.id().value
                        << " decision=" << decision.decision_type << " reason="
                        << decision.reason << "\n";
        }

        if (!was_engaged && interceptor.state().engaged) {
          engagement_time_by_track[interceptor.state().target_id.value] = time_s;
        }
      }

      if (interceptor.state().engaged) {
        const uint64_t target_id = interceptor.state().target_id.value;
        const auto pos_it = confirmed_positions.find(target_id);
        if (pos_it != confirmed_positions.end()) {
          interceptor.assign_target(interceptor.state().target_id, pos_it->second);
          interceptor.step(dt);
          if (interceptor.has_intercepted(pos_it->second)) {
            interceptor.clear_engagement();
            engagement_time_by_track.erase(target_id);
          } else {
            const auto engage_it = engagement_time_by_track.find(target_id);
            if (engage_it != engagement_time_by_track.end() &&
                (time_s - engage_it->second) > decision_engine.config().engagement_timeout_s) {
              interceptor.clear_engagement();
              engagement_time_by_track.erase(engage_it);
            }
          }
        } else {
          interceptor.clear_engagement();
          engagement_time_by_track.erase(target_id);
        }
      }

      if (timeline_out != nullptr) {
        *timeline_out << "tick=" << tick << " tracks=" << tracks.size()
                      << " interceptor_engaged=" << (interceptor.state().engaged ? 1 : 0)
                      << "\n";
      }

      prev_time_s = time_s;
      ++tick;
    }
  }

  IncidentReplayResult result{
      .tracks = manager.tracks(),
      .interceptor_state = interceptor.state(),
      .had_snapshot = false,
      .snapshot_match = true,
  };

  std::unordered_map<uint64_t, SnapshotTrack> snapshot_tracks;
  SnapshotInterceptor snapshot_interceptor;
  parse_snapshot_lines(incident_path, &snapshot_tracks, &snapshot_interceptor, &result.had_snapshot);
  if (!result.had_snapshot) {
    return result;
  }

  std::unordered_map<uint64_t, sensor_fusion::fusion_core::Track> actual_tracks;
  for (const auto& track : result.tracks) {
    actual_tracks.emplace(track.id().value, track);
  }

  for (const auto& [id, snapshot_track] : snapshot_tracks) {
    const auto it = actual_tracks.find(id);
    if (it == actual_tracks.end()) {
      result.snapshot_match = false;
      continue;
    }

    const auto& actual = it->second;
    if (actual.status() != snapshot_track.status || actual.quality().hits != snapshot_track.hits ||
        actual.quality().misses != snapshot_track.misses) {
      result.snapshot_match = false;
    }

    const auto x = actual.filter().state();
    const auto P = actual.filter().covariance();
    if (snapshot_track.x.size() != static_cast<size_t>(x.size()) ||
        snapshot_track.P.size() != static_cast<size_t>(P.size())) {
      result.snapshot_match = false;
      continue;
    }

    for (int k = 0; k < x.size(); ++k) {
      if (!approx_equal(snapshot_track.x[static_cast<size_t>(k)], x(k))) {
        result.snapshot_match = false;
      }
    }
    for (int k = 0; k < P.size(); ++k) {
      if (!approx_equal(snapshot_track.P[static_cast<size_t>(k)], P.data()[k])) {
        result.snapshot_match = false;
      }
    }
  }

  if (snapshot_interceptor.present) {
    if (result.interceptor_state.engaged != snapshot_interceptor.engaged ||
        result.interceptor_state.target_id != snapshot_interceptor.target_id) {
      result.snapshot_match = false;
    }
    for (size_t i = 0; i < 3; ++i) {
      if (!approx_equal(result.interceptor_state.position[i], snapshot_interceptor.position[i]) ||
          !approx_equal(result.interceptor_state.velocity[i], snapshot_interceptor.velocity[i])) {
        result.snapshot_match = false;
      }
    }
  }

  return result;
}

std::string serialize_replay_snapshot_track(const sensor_fusion::fusion_core::Track& track) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"type\":\"replay_snapshot_track\",\"track_id\":" << track.id().value
      << ",\"status\":\""
      << (track.status() == sensor_fusion::fusion_core::TrackStatus::Confirmed
              ? "Confirmed"
              : (track.status() == sensor_fusion::fusion_core::TrackStatus::Tentative ? "Tentative"
                                                                                       : "Deleted"))
      << "\",\"hits\":" << track.quality().hits << ",\"misses\":"
      << track.quality().misses << ",\"x\":";

  const auto x = track.filter().state();
  out << "[";
  for (int i = 0; i < x.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << x(i);
  }

  const auto P = track.filter().covariance();
  out << "],\"P\":[";
  for (int i = 0; i < P.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << P.data()[i];
  }
  out << "]}";
  return out.str();
}

std::string serialize_replay_snapshot_interceptor(
    const sensor_fusion::agents::InterceptorState& state) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"type\":\"replay_snapshot_interceptor\",\"engaged\":"
      << (state.engaged ? 1 : 0) << ",\"target_id\":" << state.target_id.value
      << ",\"position\":[" << state.position[0] << "," << state.position[1] << ","
      << state.position[2] << "],\"velocity\":[" << state.velocity[0] << ","
      << state.velocity[1] << "," << state.velocity[2] << "]}";
  return out.str();
}

}  // namespace sensor_fusion::tools
