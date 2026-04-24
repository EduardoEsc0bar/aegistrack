#pragma once

#include <cstddef>
#include <deque>
#include <string>

namespace sensor_fusion::observability {

class IncidentCapture {
 public:
  IncidentCapture(double window_seconds, size_t max_events = 20000);

  void add_event(double t_s, const std::string& json_line);
  bool dump_to_file(const std::string& path, double end_time_s) const;

 private:
  struct Event {
    double t_s;
    std::string line;
  };

  double window_seconds_;
  size_t max_events_;
  std::deque<Event> events_;
};

}  // namespace sensor_fusion::observability
