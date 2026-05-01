#include <cmath>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "common/angles.h"
#include "fusion_core/track_manager.h"
#include "observability/jsonl_logger.h"
#include "observability/metrics.h"

namespace sensor_fusion::fusion_core {
namespace {

sensor_fusion::Measurement make_meas(double range,
                                     double bearing,
                                     double confidence = 0.9,
                                     double range_var = 1.0,
                                     double bearing_var = 0.01,
                                     double t = 0.0) {
  return sensor_fusion::Measurement{
      .t_meas = sensor_fusion::Timestamp::from_seconds(t),
      .t_sent = sensor_fusion::Timestamp::from_seconds(t),
      .sensor_id = sensor_fusion::SensorId(1),
      .sensor_type = sensor_fusion::SensorType::Radar,
      .measurement_type = sensor_fusion::MeasurementType::RangeBearing2D,
      .z = {range, bearing},
      .R_rowmajor = {range_var, 0.0, 0.0, bearing_var},
      .z_dim = 2,
      .confidence = confidence,
      .snr = 0.0,
  };
}

}  // namespace

TEST_CASE("track manager: creates track from measurement") {
  TrackManager manager(TrackManagerConfig{});

  manager.predict_all(0.1);
  const auto snapshot = manager.update_with_measurements({make_meas(100.0, 0.0)});

  CHECK(snapshot.size() == 1);
  CHECK(snapshot[0].status() == TrackStatus::Tentative);
  CHECK(snapshot[0].quality().hits == 1);
}

TEST_CASE("track manager: confirms after N hits") {
  TrackManagerConfig cfg;
  cfg.confirm_hits = 3;
  TrackManager manager(cfg);

  for (uint32_t i = 0; i < cfg.confirm_hits; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({make_meas(100.0, 0.0, 0.9, 4.0, 0.01)});
  }

  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].status() == TrackStatus::Confirmed);
}

TEST_CASE("track manager: deletes after misses") {
  TrackManagerConfig cfg;
  cfg.delete_misses = 4;
  TrackManager manager(cfg);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});

  for (uint32_t i = 0; i < cfg.delete_misses; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({});
  }

  CHECK(manager.tracks().empty());
}

TEST_CASE("track manager: confirmed tracks survive short missed-update windows") {
  TrackManagerConfig cfg;
  cfg.confirm_hits = 1;
  cfg.delete_misses = 2;
  cfg.confirmed_delete_misses = 5;
  sensor_fusion::observability::Metrics metrics;
  TrackManager manager(cfg, &metrics);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});
  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].status() == TrackStatus::Confirmed);

  for (uint32_t i = 0; i < cfg.delete_misses; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({});
  }

  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].status() == TrackStatus::Confirmed);
  CHECK(manager.tracks()[0].quality().misses == cfg.delete_misses);

  const auto snapshot = metrics.snapshot();
  CHECK(std::abs(snapshot.gauges.at("tentative_track_deletion_threshold") - 2.0) < 1e-12);
  CHECK(std::abs(snapshot.gauges.at("confirmed_track_deletion_threshold") - 5.0) < 1e-12);
  CHECK(snapshot.counters.at("confirmed_track_coast_ticks") == 2);

  for (uint32_t i = cfg.delete_misses; i < cfg.confirmed_delete_misses; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({});
  }

  CHECK(manager.tracks().empty());
}

TEST_CASE("track manager: tentative tracks still use strict deletion threshold") {
  TrackManagerConfig cfg;
  cfg.confirm_hits = 3;
  cfg.delete_misses = 2;
  cfg.confirmed_delete_misses = 5;
  TrackManager manager(cfg);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});
  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].status() == TrackStatus::Tentative);

  for (uint32_t i = 0; i < cfg.delete_misses; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({});
  }

  CHECK(manager.tracks().empty());
}

TEST_CASE("track manager: lifetime and deletion metrics are recorded") {
  TrackManagerConfig cfg;
  cfg.delete_misses = 2;
  sensor_fusion::observability::Metrics metrics;
  TrackManager manager(cfg, &metrics);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});

  for (uint32_t i = 0; i < cfg.delete_misses; ++i) {
    manager.predict_all(0.1);
    manager.update_with_measurements({});
  }

  const auto snapshot = metrics.snapshot();
  CHECK(snapshot.counters.at("tracks_deleted_total") == 1);
  CHECK(snapshot.counters.at("track_coasts_total") == 2);
  CHECK(snapshot.observations.count("track_lifetime_ticks") == 1);
  CHECK(snapshot.observations.at("track_lifetime_ticks").count == 1);
  CHECK(std::abs(snapshot.observations.at("track_lifetime_ticks").max - 3.0) < 1e-12);
}

TEST_CASE("track manager: fragmentation warning increments when new track replaces deleted track") {
  TrackManagerConfig cfg;
  cfg.confirm_hits = 1;
  cfg.delete_misses = 1;
  cfg.confirmed_delete_misses = 1;
  cfg.fragmentation_warning_distance_m = 25.0;
  cfg.fragmentation_recent_delete_window_ticks = 3;
  sensor_fusion::observability::Metrics metrics;
  TrackManager manager(cfg, &metrics);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});
  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].status() == TrackStatus::Confirmed);

  manager.predict_all(0.1);
  manager.update_with_measurements({});
  CHECK(manager.tracks().empty());

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(101.0, 0.0)});

  const auto& deltas = manager.last_deltas();
  CHECK(deltas.fragmentation_warnings.size() == 1);
  CHECK(deltas.fragmentation_warnings[0].new_track_id == sensor_fusion::TrackId(2));
  const auto snapshot = metrics.snapshot();
  CHECK(snapshot.counters.at("track_fragmentation_warnings_total") == 1);
  CHECK(snapshot.counters.at("possible_id_switch_total") == 1);
  CHECK(snapshot.counters.at("reacquisition_candidates_total") == 1);
}

TEST_CASE("track manager: gating rejects bad measurement") {
  TrackManagerConfig cfg;
  cfg.gate_mahalanobis2 = 9.21;
  TrackManager manager(cfg);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0, 0.9, 1.0, 0.001)});

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(300.0, deg_to_rad(90.0), 0.1, 0.01, 0.0001)});

  CHECK(manager.tracks().size() == 1);
  CHECK(manager.tracks()[0].quality().misses == 1);
}

TEST_CASE("track manager: accepted and rejected association mahalanobis metrics are separated") {
  TrackManagerConfig cfg;
  cfg.gate_mahalanobis2 = 9.21;
  sensor_fusion::observability::Metrics metrics;
  TrackManager manager(cfg, &metrics);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0, 0.9, 1.0, 0.001)});

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(101.0, 0.0, 0.9, 1.0, 0.001)});

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(300.0, deg_to_rad(90.0), 0.9, 0.01, 0.0001)});

  const auto snapshot = metrics.snapshot();
  CHECK(snapshot.observations.count("assoc_accepted_mahalanobis2") == 1);
  CHECK(snapshot.observations.count("assoc_rejected_mahalanobis2") == 1);
  CHECK(snapshot.observations.at("assoc_accepted_mahalanobis2").count >= 1);
  CHECK(snapshot.observations.at("assoc_rejected_mahalanobis2").count >= 1);
  CHECK(snapshot.counters.at("assoc_gated_out_measurements") >= 1);
}

TEST_CASE("track stability JSONL includes compact lifecycle fields") {
  TrackManager manager(TrackManagerConfig{});
  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0)});
  CHECK(manager.tracks().size() == 1);

  const std::string line =
      sensor_fusion::observability::serialize_track_stability_event_json(
          sensor_fusion::Timestamp::from_seconds(0.1), "track_created", manager.tracks()[0],
          "test_reason", sensor_fusion::TrackId(7), 12.5, 99, 0);

  CHECK(line.find("\"type\":\"track_stability_event\"") != std::string::npos);
  CHECK(line.find("\"event\":\"track_created\"") != std::string::npos);
  CHECK(line.find("\"track_id\":1") != std::string::npos);
  CHECK(line.find("\"age_ticks\":1") != std::string::npos);
  CHECK(line.find("\"related_track_id\":7") != std::string::npos);
  CHECK(line.find("\"distance_m\":12.5") != std::string::npos);
}

TEST_CASE("track manager: nearest neighbor uniqueness") {
  TrackManagerConfig cfg;
  cfg.confirm_hits = 10;
  TrackManager manager(cfg);

  manager.predict_all(0.1);
  manager.update_with_measurements({make_meas(100.0, 0.0), make_meas(200.0, 0.0)});

  manager.predict_all(0.1);
  manager.update_with_measurements(
      {make_meas(101.0, 0.01, 0.9, 2.0, 0.01), make_meas(199.0, -0.01, 0.9, 2.0, 0.01)});

  CHECK(manager.tracks().size() == 2);

  const auto& t0 = manager.tracks()[0];
  const auto& t1 = manager.tracks()[1];
  CHECK(t0.quality().hits == 2);
  CHECK(t1.quality().hits == 2);
  CHECK(t0.quality().misses == 0);
  CHECK(t1.quality().misses == 0);

  CHECK(t0.filter().state()(0) < t1.filter().state()(0));
  CHECK(std::abs(t0.filter().state()(0) - 101.0) < std::abs(t0.filter().state()(0) - 199.0));
  CHECK(std::abs(t1.filter().state()(0) - 199.0) < std::abs(t1.filter().state()(0) - 101.0));
}

}  // namespace sensor_fusion::fusion_core
