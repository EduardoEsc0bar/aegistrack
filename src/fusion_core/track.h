#pragma once

#include <cstdint>

#include "common/ids.h"
#include "fusion_core/ekf_cv.h"

namespace sensor_fusion::fusion_core {

enum class TrackStatus { Tentative, Confirmed, Deleted };

struct TrackQuality {
  uint32_t age_ticks = 0;
  uint32_t hits = 0;
  uint32_t misses = 0;
  double score = 0.0;
  double confidence = 0.0;
  uint64_t last_trace_id = 0;
};

class Track {
 public:
  Track(sensor_fusion::TrackId id, const sensor_fusion::EkfCv& ekf, TrackStatus status);

  sensor_fusion::TrackId id() const;
  TrackStatus status() const;
  const TrackQuality& quality() const;

  sensor_fusion::EkfCv& filter();
  const sensor_fusion::EkfCv& filter() const;

  void mark_radar_hit(double measurement_confidence = 1.0, uint64_t trace_id = 0);
  void mark_eoir_refinement_hit(double measurement_confidence = 1.0,
                                uint64_t trace_id = 0);
  void mark_hit();
  void mark_miss();
  void set_status(TrackStatus s);

 private:
  sensor_fusion::TrackId id_;
  TrackStatus status_;
  TrackQuality q_;
  sensor_fusion::EkfCv ekf_;
};

}  // namespace sensor_fusion::fusion_core
