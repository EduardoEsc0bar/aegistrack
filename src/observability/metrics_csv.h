#pragma once

#include <string>

#include "observability/metrics.h"

namespace sensor_fusion::observability {

void write_metrics_csv(const std::string& path,
                       const std::string& run_name,
                       const MetricsSnapshot& snap);

}  // namespace sensor_fusion::observability
