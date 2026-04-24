#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include "observability/metrics.h"
#include "observability/metrics_csv.h"

namespace sensor_fusion::observability {

TEST_CASE("metrics csv writes header and row") {
  Metrics metrics;
  metrics.inc("assoc_success", 12);
  metrics.inc("assoc_attempts", 20);
  metrics.inc("meas_radar_total", 33);
  metrics.inc("radar_updates_total", 11);
  metrics.inc("eoir_updates_total", 7);
  metrics.set_gauge("tracks_confirmed", 4.0);
  metrics.observe("pos_error", 1.0);
  metrics.observe("pos_error", 3.0);

  const std::filesystem::path csv_path =
      std::filesystem::temp_directory_path() / "sensor_fusion_metrics_csv_test.csv";

  write_metrics_csv(csv_path.string(), "test_run", metrics.snapshot());

  std::ifstream in(csv_path);
  CHECK(in.is_open());

  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string content = buffer.str();

  CHECK(content.find("run_name,tracks_confirmed,assoc_success") != std::string::npos);
  CHECK(content.find("test_run") != std::string::npos);
  CHECK(content.find(",4,") != std::string::npos);
  CHECK(content.find(",12,") != std::string::npos);
  CHECK(content.find(",20,") != std::string::npos);
  CHECK(content.find(",33,") != std::string::npos);
  CHECK(content.find(",11,") != std::string::npos);
  CHECK(content.find(",7,") != std::string::npos);
  CHECK(content.find(",2,2") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove(csv_path, ec);
}

}  // namespace sensor_fusion::observability
