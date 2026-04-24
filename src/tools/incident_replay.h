#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "agents/interceptor/interceptor.h"
#include "fusion_core/track.h"

namespace sensor_fusion::tools {

struct IncidentReplayOptions {
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

struct IncidentReplayResult {
  std::vector<sensor_fusion::fusion_core::Track> tracks;
  sensor_fusion::agents::InterceptorState interceptor_state;
  bool had_snapshot = false;
  bool snapshot_match = true;
};

IncidentReplayResult replay_incident_file(const std::string& incident_path,
                                          const IncidentReplayOptions& options,
                                          std::ostream* timeline_out = nullptr);

std::string serialize_replay_snapshot_track(const sensor_fusion::fusion_core::Track& track);
std::string serialize_replay_snapshot_interceptor(
    const sensor_fusion::agents::InterceptorState& state);

}  // namespace sensor_fusion::tools
