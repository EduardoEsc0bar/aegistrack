#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "agents/interceptor/interceptor.h"
#include "fusion_core/track.h"
#include "messages/types.h"

namespace sensor_fusion::decision {
struct DecisionEvent;
struct BtTickResult;
}

namespace sensor_fusion::observability {

class JsonlLogger {
 public:
  explicit JsonlLogger(const std::string& path);

  void write_raw_line(const std::string& line);

  void log_measurement(const sensor_fusion::Measurement& m,
                       uint64_t trace_id = 0,
                       uint64_t causal_parent_id = 0);

  void log_track_event(const std::string& event_type,
                       const sensor_fusion::Timestamp& t,
                       const sensor_fusion::fusion_core::Track& track,
                       uint64_t trace_id = 0,
                       uint64_t causal_parent_id = 0);

  void log_decision_event(const sensor_fusion::Timestamp& t,
                          const sensor_fusion::decision::DecisionEvent& event,
                          uint64_t trace_id = 0,
                          uint64_t causal_parent_id = 0);

  void log_command_event(const sensor_fusion::Timestamp& t,
                         sensor_fusion::TrackId track_id,
                         const std::string& command_type,
                         const std::string& detail,
                         uint64_t trace_id = 0,
                         uint64_t causal_parent_id = 0);

  void log_intercept_event(const sensor_fusion::Timestamp& t,
                           sensor_fusion::TrackId track_id,
                           const std::string& outcome,
                           const std::string& reason,
                           uint64_t trace_id = 0,
                           uint64_t causal_parent_id = 0);

 private:
  std::ofstream out_;
};

std::vector<sensor_fusion::Measurement> load_measurements_from_jsonl(const std::string& path);

std::string serialize_measurement_json(const sensor_fusion::Measurement& m,
                                       uint64_t trace_id = 0,
                                       uint64_t causal_parent_id = 0);

std::string serialize_track_event_json(const std::string& event_type,
                                       const sensor_fusion::Timestamp& t,
                                       const sensor_fusion::fusion_core::Track& track,
                                       uint64_t trace_id = 0,
                                       uint64_t causal_parent_id = 0);

std::string serialize_decision_event_json(const sensor_fusion::Timestamp& t,
                                          const sensor_fusion::decision::DecisionEvent& event,
                                          uint64_t trace_id = 0,
                                          uint64_t causal_parent_id = 0);

std::string serialize_command_event_json(const sensor_fusion::Timestamp& t,
                                         sensor_fusion::TrackId track_id,
                                         const std::string& command_type,
                                         const std::string& detail,
                                         uint64_t trace_id = 0,
                                         uint64_t causal_parent_id = 0);

std::string serialize_intercept_event_json(const sensor_fusion::Timestamp& t,
                                           sensor_fusion::TrackId track_id,
                                           const std::string& outcome,
                                           const std::string& reason,
                                           uint64_t trace_id = 0,
                                           uint64_t causal_parent_id = 0);

std::string serialize_interceptor_state_json(const sensor_fusion::Timestamp& t,
                                             const sensor_fusion::agents::InterceptorState& state,
                                             uint64_t trace_id = 0,
                                             uint64_t causal_parent_id = 0);

std::string serialize_assignment_event_json(const sensor_fusion::Timestamp& t,
                                            uint64_t interceptor_id,
                                            sensor_fusion::TrackId track_id,
                                            const std::string& decision_type,
                                            const std::string& reason,
                                            uint64_t trace_id = 0,
                                            uint64_t causal_parent_id = 0);

std::string serialize_bt_decision_json(const sensor_fusion::Timestamp& t,
                                       const sensor_fusion::decision::BtTickResult& result,
                                       uint64_t trace_id = 0,
                                       uint64_t causal_parent_id = 0);

}  // namespace sensor_fusion::observability
