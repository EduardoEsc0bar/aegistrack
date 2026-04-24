#pragma once

#include <vector>

namespace sensor_fusion::fusion_core::association {

// Returns assignment for rows -> cols.
// Row i is unassigned when result[i] == -1.
// Supports rectangular cost matrices.
std::vector<int> hungarian_solve(const std::vector<std::vector<double>>& cost);

}  // namespace sensor_fusion::fusion_core::association
