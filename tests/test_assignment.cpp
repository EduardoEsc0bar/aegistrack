#include <algorithm>
#include <array>
#include <vector>

#include <doctest/doctest.h>

#include "decision/assignment.h"

namespace sensor_fusion::decision {

TEST_CASE("assignment: one-to-one optimal assignment") {
  std::vector<sensor_fusion::agents::InterceptorState> interceptors = {
      sensor_fusion::agents::InterceptorState{
          .interceptor_id = 1,
          .position = {0.0, 0.0, 0.0},
      },
      sensor_fusion::agents::InterceptorState{
          .interceptor_id = 2,
          .position = {100.0, 0.0, 0.0},
      },
  };

  std::vector<AssignmentTrack> tracks = {
      AssignmentTrack{.track_id = sensor_fusion::TrackId(10),
                      .position = {5.0, 0.0, 0.0},
                      .threat_score = 0.8},
      AssignmentTrack{.track_id = sensor_fusion::TrackId(20),
                      .position = {105.0, 0.0, 0.0},
                      .threat_score = 0.8},
  };

  const auto result = compute_assignments(interceptors, tracks, AssignmentConfig{});
  CHECK(result.size() == 2);
  CHECK(result[0].interceptor_id == 1);
  CHECK(result[0].track_id == sensor_fusion::TrackId(10));
  CHECK(result[1].interceptor_id == 2);
  CHECK(result[1].track_id == sensor_fusion::TrackId(20));
}

TEST_CASE("assignment: higher threat selected when interceptors limited") {
  std::vector<sensor_fusion::agents::InterceptorState> interceptors = {
      sensor_fusion::agents::InterceptorState{
          .interceptor_id = 1,
          .position = {0.0, 0.0, 0.0},
      },
  };

  std::vector<AssignmentTrack> tracks = {
      AssignmentTrack{.track_id = sensor_fusion::TrackId(1),
                      .position = {5.0, 0.0, 0.0},
                      .threat_score = 0.1},
      AssignmentTrack{.track_id = sensor_fusion::TrackId(2),
                      .position = {30.0, 0.0, 0.0},
                      .threat_score = 1.0},
  };

  const auto result = compute_assignments(interceptors, tracks, AssignmentConfig{});
  CHECK(result.size() == 1);
  if (result.size() != 1) {
    return;
  }
  CHECK(result[0].track_id == sensor_fusion::TrackId(2));
}

TEST_CASE("assignment: retask occurs when enabled and commit point not reached") {
  std::vector<sensor_fusion::agents::InterceptorState> interceptors = {
      sensor_fusion::agents::InterceptorState{
          .interceptor_id = 1,
          .position = {0.0, 0.0, 0.0},
          .engaged = true,
          .target_id = sensor_fusion::TrackId(100),
          .assigned_target_position = {200.0, 0.0, 0.0},
      },
  };

  std::vector<AssignmentTrack> tracks = {
      AssignmentTrack{.track_id = sensor_fusion::TrackId(200),
                      .position = {10.0, 0.0, 0.0},
                      .threat_score = 1.0},
  };

  const auto result = compute_assignments(
      interceptors, tracks,
      AssignmentConfig{.allow_multi_engage = false, .retask_enable = true, .commit_distance_m = 50.0});
  CHECK(result.size() == 1);
  if (result.size() != 1) {
    return;
  }
  CHECK(result[0].track_id == sensor_fusion::TrackId(200));
  CHECK(result[0].action == AssignmentAction::Retasked);
}

TEST_CASE("assignment: no retask when commit point reached") {
  std::vector<sensor_fusion::agents::InterceptorState> interceptors = {
      sensor_fusion::agents::InterceptorState{
          .interceptor_id = 1,
          .position = {180.0, 0.0, 0.0},
          .engaged = true,
          .target_id = sensor_fusion::TrackId(100),
          .assigned_target_position = {200.0, 0.0, 0.0},
      },
  };

  std::vector<AssignmentTrack> tracks = {
      AssignmentTrack{.track_id = sensor_fusion::TrackId(200),
                      .position = {10.0, 0.0, 0.0},
                      .threat_score = 1.0},
  };

  const auto result = compute_assignments(
      interceptors, tracks,
      AssignmentConfig{.allow_multi_engage = false, .retask_enable = true, .commit_distance_m = 50.0});
  CHECK(result.empty());
}

}  // namespace sensor_fusion::decision
