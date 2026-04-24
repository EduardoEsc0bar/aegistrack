#include <array>
#include <cmath>

#include <doctest/doctest.h>
#include <Eigen/Dense>

#include "decision/decision_engine.h"
#include "fusion_core/ekf_cv.h"
#include "fusion_core/track.h"

namespace sensor_fusion::decision {
namespace {

sensor_fusion::fusion_core::Track make_track(sensor_fusion::TrackId id,
                                             sensor_fusion::fusion_core::TrackStatus status,
                                             int radar_hits) {
  sensor_fusion::EkfCv ekf;
  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(6);
  x0(0) = 100.0;
  Eigen::MatrixXd P0 = Eigen::MatrixXd::Identity(6, 6);
  ekf.initialize(x0, P0);

  sensor_fusion::fusion_core::Track track(id, ekf, status);
  for (int i = 0; i < radar_hits; ++i) {
    track.mark_radar_hit();
  }
  return track;
}

}  // namespace

TEST_CASE("behavior tree: confirmed high-score track triggers engagement") {
  const auto track = make_track(sensor_fusion::TrackId(1),
                                sensor_fusion::fusion_core::TrackStatus::Confirmed, 4);

  DecisionEngine engine(3.0);
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, true, {100.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId id, const std::array<double, 3>& target) {
        assigned = true;
        CHECK(id == track.id());
        CHECK(std::abs(target[0] - 100.0) < 1e-12);
      });

  CHECK(assigned);
  CHECK(event.decision_type == "engage");
}

TEST_CASE("behavior tree: tentative track does not engage") {
  const auto track = make_track(sensor_fusion::TrackId(2),
                                sensor_fusion::fusion_core::TrackStatus::Tentative, 5);

  DecisionEngine engine(3.0);
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, true, {50.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId, const std::array<double, 3>&) { assigned = true; });

  CHECK(!assigned);
  CHECK(event.decision_type == "acquire");
}

TEST_CASE("behavior tree: unavailable interceptor prevents engagement") {
  const auto track = make_track(sensor_fusion::TrackId(3),
                                sensor_fusion::fusion_core::TrackStatus::Confirmed, 4);

  DecisionEngine engine(3.0);
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, false, {75.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId, const std::array<double, 3>&) { assigned = true; });

  CHECK(!assigned);
  CHECK(event.decision_type == "engage_denied");
  CHECK(event.reason == "interceptor_unavailable");
}

TEST_CASE("behavior tree: no-engage zone denies engagement") {
  auto track = make_track(sensor_fusion::TrackId(4),
                          sensor_fusion::fusion_core::TrackStatus::Confirmed, 4);
  track.mark_radar_hit(0.95, 1);

  DecisionEngine engine(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 1000.0,
      .min_confidence_to_engage = 0.5,
      .no_engage_zone_radius_m = 100.0,
      .engagement_timeout_s = 10.0,
  });
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, true, {20.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId, const std::array<double, 3>&) { assigned = true; });

  CHECK(!assigned);
  CHECK(event.decision_type == "engage_denied");
  CHECK(event.reason == "inside_no_engage_zone");
}

TEST_CASE("behavior tree: out-of-range target denies engagement") {
  auto track = make_track(sensor_fusion::TrackId(5),
                          sensor_fusion::fusion_core::TrackStatus::Confirmed, 4);
  track.mark_radar_hit(0.95, 1);

  DecisionEngine engine(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 50.0,
      .min_confidence_to_engage = 0.5,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
  });
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, true, {150.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId, const std::array<double, 3>&) { assigned = true; });

  CHECK(!assigned);
  CHECK(event.decision_type == "engage_denied");
  CHECK(event.reason == "outside_max_engagement_range");
}

TEST_CASE("behavior tree: low confidence denies engagement") {
  auto track = make_track(sensor_fusion::TrackId(6),
                          sensor_fusion::fusion_core::TrackStatus::Confirmed, 4);
  track.mark_radar_hit(0.1, 1);

  DecisionEngine engine(DecisionConfig{
      .engage_score_threshold = 3.0,
      .max_engagement_range_m = 1000.0,
      .min_confidence_to_engage = 0.8,
      .no_engage_zone_radius_m = 0.0,
      .engagement_timeout_s = 10.0,
  });
  bool assigned = false;
  const DecisionEvent event = engine.decide(
      track, true, {150.0, 0.0, 0.0},
      [&](sensor_fusion::TrackId, const std::array<double, 3>&) { assigned = true; });

  CHECK(!assigned);
  CHECK(event.decision_type == "engage_denied");
  CHECK(event.reason == "low_confidence");
}

}  // namespace sensor_fusion::decision
