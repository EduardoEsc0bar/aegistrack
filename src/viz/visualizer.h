#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <Eigen/Dense>

namespace sensor_fusion::viz {

struct VizConfig {
  int width = 1100;
  int height = 800;
  double meters_per_pixel = 2.0;
  bool show_covariance = true;
  bool show_truth = true;
  bool show_tracks = true;
  bool show_interceptors = true;
  bool show_engagements = true;
  double protected_zone_radius_m = 200.0;
};

struct VizSnapshot {
  double t_s = 0.0;
  std::vector<std::array<double, 3>> truth_positions;

  struct TrackViz {
    uint64_t id = 0;
    bool confirmed = false;
    std::array<double, 3> pos{0.0, 0.0, 0.0};
    Eigen::Matrix<double, 6, 6> P = Eigen::Matrix<double, 6, 6>::Identity();
  };
  std::vector<TrackViz> tracks;

  struct InterceptorViz {
    uint64_t id = 0;
    bool engaged = false;
    std::array<double, 3> pos{0.0, 0.0, 0.0};
    std::optional<uint64_t> target_track_id;
  };
  std::vector<InterceptorViz> interceptors;

  std::vector<std::pair<std::array<double, 3>, std::array<double, 3>>> engagement_lines;
};

class Visualizer {
 public:
  explicit Visualizer(VizConfig cfg);
  void run(std::function<bool(VizSnapshot& out)> get_snapshot);

 private:
  VizConfig cfg_;
};

}  // namespace sensor_fusion::viz
