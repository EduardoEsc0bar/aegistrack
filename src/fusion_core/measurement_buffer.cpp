#include "fusion_core/measurement_buffer.h"

namespace sensor_fusion::fusion_core {

void MeasurementBuffer::push(const sensor_fusion::Measurement& m) {
  auto it = buffer_.begin();
  for (; it != buffer_.end(); ++it) {
    if (m.t_sent < it->t_sent) {
      break;
    }
  }
  buffer_.insert(it, m);
}

std::vector<sensor_fusion::Measurement> MeasurementBuffer::pop_ready(
    const sensor_fusion::Timestamp& fusion_time) {
  std::vector<sensor_fusion::Measurement> ready;
  pop_ready(fusion_time, &ready);
  return ready;
}

void MeasurementBuffer::pop_ready(const sensor_fusion::Timestamp& fusion_time,
                                  std::vector<sensor_fusion::Measurement>* out_ready) {
  if (out_ready == nullptr) {
    return;
  }

  out_ready->clear();
  while (!buffer_.empty() && buffer_.front().t_sent <= fusion_time) {
    out_ready->push_back(buffer_.front());
    buffer_.pop_front();
  }
}

}  // namespace sensor_fusion::fusion_core
