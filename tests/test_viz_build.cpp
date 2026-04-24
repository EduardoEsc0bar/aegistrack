#include <doctest/doctest.h>

#include "viz/visualizer.h"

namespace sensor_fusion::viz {

TEST_CASE("viz: visualizer constructs") {
  VizConfig cfg;
  Visualizer visualizer(cfg);
  CHECK(cfg.width > 0);
  CHECK(cfg.height > 0);
  (void)visualizer;
}

}  // namespace sensor_fusion::viz
