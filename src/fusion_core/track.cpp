#include "fusion_core/track.h"

#include <algorithm>

namespace sensor_fusion::fusion_core {

Track::Track(sensor_fusion::TrackId id, const sensor_fusion::EkfCv& ekf, TrackStatus status)
    : id_(id), status_(status), ekf_(ekf) {}

sensor_fusion::TrackId Track::id() const {
  return id_;
}

TrackStatus Track::status() const {
  return status_;
}

const TrackQuality& Track::quality() const {
  return q_;
}

sensor_fusion::EkfCv& Track::filter() {
  return ekf_;
}

const sensor_fusion::EkfCv& Track::filter() const {
  return ekf_;
}

void Track::mark_radar_hit(double measurement_confidence, uint64_t trace_id) {
  ++q_.age_ticks;
  ++q_.hits;
  q_.score += 1.0;
  const double clamped_confidence = std::clamp(measurement_confidence, 0.0, 1.0);
  if (q_.hits == 1 && q_.misses == 0) {
    q_.confidence = clamped_confidence;
  } else {
    q_.confidence = std::clamp(0.7 * q_.confidence + 0.3 * clamped_confidence, 0.0, 1.0);
  }
  if (trace_id != 0) {
    q_.last_trace_id = trace_id;
  }
}

void Track::mark_eoir_refinement_hit(double measurement_confidence, uint64_t trace_id) {
  q_.score += 0.5;
  const double clamped_confidence = std::clamp(measurement_confidence, 0.0, 1.0);
  q_.confidence = std::clamp(0.85 * q_.confidence + 0.15 * clamped_confidence, 0.0, 1.0);
  if (trace_id != 0) {
    q_.last_trace_id = trace_id;
  }
}

void Track::mark_hit() {
  mark_radar_hit(1.0, 0);
}

void Track::mark_miss() {
  ++q_.age_ticks;
  ++q_.misses;
  q_.score -= 0.5;
  q_.confidence = std::clamp(q_.confidence * 0.9, 0.0, 1.0);
}

void Track::set_status(TrackStatus s) {
  status_ = s;
}

}  // namespace sensor_fusion::fusion_core
