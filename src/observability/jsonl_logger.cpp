#include "observability/jsonl_logger.h"

#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "decision/decision_engine.h"
#include "decision/mission_behavior_tree.h"

namespace sensor_fusion::observability {
namespace {

std::string sensor_type_to_string(sensor_fusion::SensorType type) {
  switch (type) {
    case sensor_fusion::SensorType::Radar:
      return "Radar";
    case sensor_fusion::SensorType::EOIR:
      return "EOIR";
    case sensor_fusion::SensorType::ADSB:
      return "ADSB";
  }
  return "Radar";
}

std::string measurement_type_to_string(sensor_fusion::MeasurementType type) {
  switch (type) {
    case sensor_fusion::MeasurementType::RangeBearing2D:
      return "RangeBearing2D";
    case sensor_fusion::MeasurementType::BearingElevation:
      return "BearingElevation";
    case sensor_fusion::MeasurementType::Position3D:
      return "Position3D";
  }
  return "RangeBearing2D";
}

sensor_fusion::SensorType sensor_type_from_string(const std::string& type) {
  if (type == "Radar") {
    return sensor_fusion::SensorType::Radar;
  }
  if (type == "EOIR") {
    return sensor_fusion::SensorType::EOIR;
  }
  if (type == "ADSB") {
    return sensor_fusion::SensorType::ADSB;
  }
  throw std::runtime_error("unknown sensor_type: " + type);
}

sensor_fusion::MeasurementType measurement_type_from_string(const std::string& type) {
  if (type == "RangeBearing2D") {
    return sensor_fusion::MeasurementType::RangeBearing2D;
  }
  if (type == "BearingElevation") {
    return sensor_fusion::MeasurementType::BearingElevation;
  }
  if (type == "Position3D") {
    return sensor_fusion::MeasurementType::Position3D;
  }
  throw std::runtime_error("unknown measurement_type: " + type);
}

std::string track_status_to_string(sensor_fusion::fusion_core::TrackStatus status) {
  switch (status) {
    case sensor_fusion::fusion_core::TrackStatus::Tentative:
      return "Tentative";
    case sensor_fusion::fusion_core::TrackStatus::Confirmed:
      return "Confirmed";
    case sensor_fusion::fusion_core::TrackStatus::Deleted:
      return "Deleted";
  }
  return "Tentative";
}

void write_number_vector(std::ostream& os, const std::vector<double>& values) {
  os << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      os << ",";
    }
    os << values[i];
  }
  os << "]";
}

void write_number_array(std::ostream& os, const double* values, size_t n) {
  os << "[";
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      os << ",";
    }
    os << values[i];
  }
  os << "]";
}

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
  if (pos >= line.size()) {
    throw std::runtime_error("malformed key: " + key);
  }
  return pos;
}

std::string parse_json_string(const std::string& line, const std::string& key) {
  size_t pos = find_key_value_start(line, key);
  if (line[pos] != '"') {
    throw std::runtime_error("expected string for key: " + key);
  }
  ++pos;
  const size_t end = line.find('"', pos);
  if (end == std::string::npos) {
    throw std::runtime_error("unterminated string for key: " + key);
  }
  return line.substr(pos, end - pos);
}

double parse_json_number(const std::string& line, const std::string& key) {
  size_t pos = find_key_value_start(line, key);
  size_t end = pos;
  while (end < line.size() && line[end] != ',' && line[end] != '}') {
    ++end;
  }
  return std::stod(line.substr(pos, end - pos));
}

double parse_json_number_optional(const std::string& line,
                                  const std::string& key,
                                  double default_value) {
  const std::string token = std::string("\"") + key + "\":";
  if (line.find(token) == std::string::npos) {
    return default_value;
  }
  return parse_json_number(line, key);
}

std::vector<double> parse_json_number_array(const std::string& line, const std::string& key) {
  size_t pos = find_key_value_start(line, key);
  if (line[pos] != '[') {
    throw std::runtime_error("expected array for key: " + key);
  }
  ++pos;

  const size_t end = line.find(']', pos);
  if (end == std::string::npos) {
    throw std::runtime_error("unterminated array for key: " + key);
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

std::string serialize_common_trace_fields(uint64_t trace_id, uint64_t causal_parent_id) {
  std::ostringstream os;
  os << ",\"trace_id\":" << trace_id << ",\"causal_parent_id\":" << causal_parent_id;
  return os.str();
}

}  // namespace

JsonlLogger::JsonlLogger(const std::string& path) {
  const std::filesystem::path log_path(path);
  const auto parent = log_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  out_.open(path, std::ios::out | std::ios::trunc);
  if (!out_.is_open()) {
    throw std::runtime_error("failed to open log file: " + path);
  }

  out_ << std::setprecision(17);
}

void JsonlLogger::write_raw_line(const std::string& line) {
  out_ << line << std::endl;
}

void JsonlLogger::log_measurement(const sensor_fusion::Measurement& m,
                                  uint64_t trace_id,
                                  uint64_t causal_parent_id) {
  write_raw_line(serialize_measurement_json(m, trace_id, causal_parent_id));
}

void JsonlLogger::log_track_event(const std::string& event_type,
                                  const sensor_fusion::Timestamp& t,
                                  const sensor_fusion::fusion_core::Track& track,
                                  uint64_t trace_id,
                                  uint64_t causal_parent_id) {
  write_raw_line(serialize_track_event_json(event_type, t, track, trace_id, causal_parent_id));
}

void JsonlLogger::log_decision_event(const sensor_fusion::Timestamp& t,
                                     const sensor_fusion::decision::DecisionEvent& event,
                                     uint64_t trace_id,
                                     uint64_t causal_parent_id) {
  write_raw_line(serialize_decision_event_json(t, event, trace_id, causal_parent_id));
}

void JsonlLogger::log_command_event(const sensor_fusion::Timestamp& t,
                                    sensor_fusion::TrackId track_id,
                                    const std::string& command_type,
                                    const std::string& detail,
                                    uint64_t trace_id,
                                    uint64_t causal_parent_id) {
  write_raw_line(
      serialize_command_event_json(t, track_id, command_type, detail, trace_id, causal_parent_id));
}

void JsonlLogger::log_intercept_event(const sensor_fusion::Timestamp& t,
                                      sensor_fusion::TrackId track_id,
                                      const std::string& outcome,
                                      const std::string& reason,
                                      uint64_t trace_id,
                                      uint64_t causal_parent_id) {
  write_raw_line(
      serialize_intercept_event_json(t, track_id, outcome, reason, trace_id, causal_parent_id));
}

std::string serialize_measurement_json(const sensor_fusion::Measurement& m,
                                       uint64_t trace_id,
                                       uint64_t causal_parent_id) {
  const uint64_t effective_trace_id = trace_id == 0 ? m.trace_id : trace_id;
  const uint64_t effective_parent_id =
      causal_parent_id == 0 ? m.causal_parent_id : causal_parent_id;

  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"measurement\","
      << "\"t_meas_s\":" << m.t_meas.to_seconds() << ","
      << "\"t_sent_s\":" << m.t_sent.to_seconds() << ","
      << "\"sensor_id\":" << m.sensor_id.value << ","
      << "\"sensor_type\":\"" << sensor_type_to_string(m.sensor_type) << "\","
      << "\"measurement_type\":\"" << measurement_type_to_string(m.measurement_type)
      << "\","
      << "\"z_dim\":" << m.z_dim << ","
      << "\"z\":";
  write_number_vector(out, m.z);
  out << ",\"R\":";
  write_number_vector(out, m.R_rowmajor);
  out << ",\"confidence\":" << m.confidence
      << serialize_common_trace_fields(effective_trace_id, effective_parent_id) << "}";
  return out.str();
}

std::string serialize_track_event_json(const std::string& event_type,
                                       const sensor_fusion::Timestamp& t,
                                       const sensor_fusion::fusion_core::Track& track,
                                       uint64_t trace_id,
                                       uint64_t causal_parent_id) {
  const uint64_t effective_trace_id = trace_id == 0 ? track.quality().last_trace_id : trace_id;

  const auto x = track.filter().state();
  const auto P = track.filter().covariance();

  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"track_event\","
      << "\"event\":\"" << event_type << "\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"track_id\":" << track.id().value << ","
      << "\"status\":\"" << track_status_to_string(track.status()) << "\","
      << "\"x\":";
  write_number_array(out, x.data(), static_cast<size_t>(x.size()));
  out << ",\"P\":";
  write_number_array(out, P.data(), static_cast<size_t>(P.size()));
  out << ",\"age_ticks\":" << track.quality().age_ticks << ","
      << "\"hits\":" << track.quality().hits << ","
      << "\"misses\":" << track.quality().misses << ","
      << "\"score\":" << track.quality().score << ","
      << "\"confidence\":" << track.quality().confidence
      << serialize_common_trace_fields(effective_trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_track_stability_event_json(
    const sensor_fusion::Timestamp& t,
    const std::string& event_type,
    const sensor_fusion::fusion_core::Track& track,
    const std::string& reason,
    sensor_fusion::TrackId related_track_id,
    double distance_m,
    uint64_t trace_id,
    uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"track_stability_event\","
      << "\"event\":\"" << event_type << "\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"track_id\":" << track.id().value << ","
      << "\"status\":\"" << track_status_to_string(track.status()) << "\","
      << "\"age_ticks\":" << track.quality().age_ticks << ","
      << "\"hits\":" << track.quality().hits << ","
      << "\"misses\":" << track.quality().misses << ","
      << "\"score\":" << track.quality().score << ","
      << "\"confidence\":" << track.quality().confidence << ","
      << "\"reason\":\"" << reason << "\"";
  if (related_track_id.value != 0) {
    out << ",\"related_track_id\":" << related_track_id.value;
  }
  if (distance_m >= 0.0) {
    out << ",\"distance_m\":" << distance_m;
  }
  out << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_decision_event_json(const sensor_fusion::Timestamp& t,
                                          const sensor_fusion::decision::DecisionEvent& event,
                                          uint64_t trace_id,
                                          uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"decision_event\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"track_id\":" << event.track_id.value << ","
      << "\"decision_type\":\"" << event.decision_type << "\","
      << "\"reason\":\"" << event.reason << "\""
      << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_command_event_json(const sensor_fusion::Timestamp& t,
                                         sensor_fusion::TrackId track_id,
                                         const std::string& command_type,
                                         const std::string& detail,
                                         uint64_t trace_id,
                                         uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"command_event\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"track_id\":" << track_id.value << ","
      << "\"command_type\":\"" << command_type << "\","
      << "\"detail\":\"" << detail << "\""
      << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_intercept_event_json(const sensor_fusion::Timestamp& t,
                                           sensor_fusion::TrackId track_id,
                                           const std::string& outcome,
                                           const std::string& reason,
                                           uint64_t trace_id,
                                           uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"intercept_event\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"track_id\":" << track_id.value << ","
      << "\"outcome\":\"" << outcome << "\","
      << "\"reason\":\"" << reason << "\""
      << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_interceptor_state_json(const sensor_fusion::Timestamp& t,
                                             const sensor_fusion::agents::InterceptorState& state,
                                             uint64_t trace_id,
                                             uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"interceptor_state\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"interceptor_id\":" << state.interceptor_id << ","
      << "\"engaged\":" << (state.engaged ? 1 : 0) << ","
      << "\"track_id\":" << state.target_id.value << ","
      << "\"position\":["
      << state.position[0] << "," << state.position[1] << "," << state.position[2] << "],"
      << "\"velocity\":["
      << state.velocity[0] << "," << state.velocity[1] << "," << state.velocity[2] << "],"
      << "\"engagement_time_s\":" << state.engagement_time_s
      << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_assignment_event_json(const sensor_fusion::Timestamp& t,
                                            uint64_t interceptor_id,
                                            sensor_fusion::TrackId track_id,
                                            const std::string& decision_type,
                                            const std::string& reason,
                                            uint64_t trace_id,
                                            uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"assignment_event\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"interceptor_id\":" << interceptor_id << ","
      << "\"track_id\":" << track_id.value << ","
      << "\"decision_type\":\"" << decision_type << "\","
      << "\"reason\":\"" << reason << "\""
      << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::string serialize_bt_decision_json(const sensor_fusion::Timestamp& t,
                                       const sensor_fusion::decision::BtTickResult& result,
                                       uint64_t trace_id,
                                       uint64_t causal_parent_id) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{"
      << "\"type\":\"bt_decision\","
      << "\"t_s\":" << t.to_seconds() << ","
      << "\"tick\":" << result.tick << ","
      << "\"mode\":\"" << sensor_fusion::decision::mission_mode_to_string(result.mode)
      << "\","
      << "\"track_id\":" << result.event.track_id.value << ","
      << "\"decision_type\":\"" << result.event.decision_type << "\","
      << "\"reason\":\"" << result.event.reason << "\","
      << "\"engagement_commands\":" << result.engagement_commands.size() << ","
      << "\"active_engagements\":" << result.active_engagement_count << ","
      << "\"idle_interceptors\":" << result.idle_interceptor_count << ","
      << "\"selected_track_id\":" << result.selected_track_id.value << ","
      << "\"selected_interceptor_id\":" << result.selected_interceptor_id << ","
      << "\"selected_engagement_score\":" << result.selected_engagement_score << ","
      << "\"selected_estimated_intercept_time_s\":"
      << result.selected_estimated_intercept_time_s << ","
      << "\"assignment_reason\":\"" << result.assignment_reason << "\","
      << "\"events\":[";
  for (size_t i = 0; i < result.events.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    const auto& event = result.events[i];
    out << "{"
        << "\"event\":\"" << event.event << "\","
        << "\"track_id\":" << event.track_id.value << ","
        << "\"interceptor_id\":" << event.interceptor_id << ","
        << "\"reason\":\"" << event.reason << "\""
        << "}";
  }
  out << "],"
      << "\"nodes\":[";
  for (size_t i = 0; i < result.node_trace.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    const auto& trace = result.node_trace[i];
    out << "{"
        << "\"node\":\"" << trace.node << "\","
        << "\"status\":\"" << sensor_fusion::decision::bt_status_to_string(trace.status)
        << "\","
        << "\"detail\":\"" << trace.detail << "\""
        << "}";
  }
  out << "]" << serialize_common_trace_fields(trace_id, causal_parent_id) << "}";
  return out.str();
}

std::vector<sensor_fusion::Measurement> load_measurements_from_jsonl(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open JSONL input: " + path);
  }

  std::vector<sensor_fusion::Measurement> result;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    const std::string type = parse_json_string(line, "type");
    if (type != "measurement") {
      continue;
    }

    const auto measurement_type =
        measurement_type_from_string(parse_json_string(line, "measurement_type"));
    if (measurement_type != sensor_fusion::MeasurementType::RangeBearing2D &&
        measurement_type != sensor_fusion::MeasurementType::BearingElevation) {
      continue;
    }

    sensor_fusion::Measurement m{
        .t_meas = sensor_fusion::Timestamp::from_seconds(parse_json_number(line, "t_meas_s")),
        .t_sent = sensor_fusion::Timestamp::from_seconds(parse_json_number(line, "t_sent_s")),
        .sensor_id = sensor_fusion::SensorId(
            static_cast<uint64_t>(parse_json_number(line, "sensor_id"))),
        .sensor_type = sensor_type_from_string(parse_json_string(line, "sensor_type")),
        .measurement_type = measurement_type,
        .z = parse_json_number_array(line, "z"),
        .R_rowmajor = parse_json_number_array(line, "R"),
        .z_dim = static_cast<uint32_t>(parse_json_number(line, "z_dim")),
        .confidence = parse_json_number(line, "confidence"),
        .snr = 0.0,
        .trace_id =
            static_cast<uint64_t>(parse_json_number_optional(line, "trace_id", 0.0)),
        .causal_parent_id = static_cast<uint64_t>(
            parse_json_number_optional(line, "causal_parent_id", 0.0)),
    };

    result.push_back(std::move(m));
  }

  return result;
}

}  // namespace sensor_fusion::observability
