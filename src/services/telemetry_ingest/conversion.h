#pragma once

#include "detections.pb.h"
#include "messages/types.h"

namespace sensor_fusion::services::telemetry_ingest {

sensor_fusion::Measurement detection_to_measurement(
    const aegistrack::telemetry::v1::Detection& detection);

}  // namespace sensor_fusion::services::telemetry_ingest
