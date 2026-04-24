#include "observability/incident_capture.h"

#include <filesystem>
#include <fstream>

namespace sensor_fusion::observability {

IncidentCapture::IncidentCapture(double window_seconds, size_t max_events)
    : window_seconds_(window_seconds > 0.0 ? window_seconds : 5.0),
      max_events_(max_events > 0 ? max_events : 1) {}

void IncidentCapture::add_event(double t_s, const std::string& json_line) {
  events_.push_back(Event{.t_s = t_s, .line = json_line});

  const double cutoff = t_s - window_seconds_;
  while (!events_.empty() && events_.front().t_s < cutoff) {
    events_.pop_front();
  }
  while (events_.size() > max_events_) {
    events_.pop_front();
  }
}

bool IncidentCapture::dump_to_file(const std::string& path, double end_time_s) const {
  std::filesystem::path output(path);
  const auto parent = output.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }

  const double cutoff = end_time_s - window_seconds_;
  for (const auto& event : events_) {
    if (event.t_s >= cutoff && event.t_s <= end_time_s) {
      out << event.line << "\n";
    }
  }

  return true;
}

}  // namespace sensor_fusion::observability
