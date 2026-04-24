#include "fusion_core/association/hungarian.h"

#include <algorithm>
#include <limits>

namespace sensor_fusion::fusion_core::association {

std::vector<int> hungarian_solve(const std::vector<std::vector<double>>& cost) {
  const size_t rows = cost.size();
  if (rows == 0) {
    return {};
  }

  size_t cols = 0;
  for (const auto& row : cost) {
    cols = std::max(cols, row.size());
  }
  if (cols == 0) {
    return std::vector<int>(rows, -1);
  }

  const size_t n = std::max(rows, cols);
  constexpr double kPadCost = 1e12;

  std::vector<std::vector<double>> a(n + 1, std::vector<double>(n + 1, kPadCost));
  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < cost[i].size(); ++j) {
      a[i + 1][j + 1] = cost[i][j];
    }
  }

  std::vector<double> u(n + 1, 0.0);
  std::vector<double> v(n + 1, 0.0);
  std::vector<size_t> p(n + 1, 0);
  std::vector<size_t> way(n + 1, 0);

  for (size_t i = 1; i <= n; ++i) {
    p[0] = i;
    size_t j0 = 0;
    std::vector<double> minv(n + 1, std::numeric_limits<double>::infinity());
    std::vector<bool> used(n + 1, false);

    do {
      used[j0] = true;
      const size_t i0 = p[j0];
      double delta = std::numeric_limits<double>::infinity();
      size_t j1 = 0;

      for (size_t j = 1; j <= n; ++j) {
        if (used[j]) {
          continue;
        }

        const double cur = a[i0][j] - u[i0] - v[j];
        if (cur < minv[j]) {
          minv[j] = cur;
          way[j] = j0;
        }
        if (minv[j] < delta) {
          delta = minv[j];
          j1 = j;
        }
      }

      for (size_t j = 0; j <= n; ++j) {
        if (used[j]) {
          u[p[j]] += delta;
          v[j] -= delta;
        } else {
          minv[j] -= delta;
        }
      }
      j0 = j1;
    } while (p[j0] != 0);

    do {
      const size_t j1 = way[j0];
      p[j0] = p[j1];
      j0 = j1;
    } while (j0 != 0);
  }

  std::vector<size_t> assignment_row_to_col(n + 1, 0);
  for (size_t j = 1; j <= n; ++j) {
    if (p[j] != 0) {
      assignment_row_to_col[p[j]] = j;
    }
  }

  std::vector<int> result(rows, -1);
  for (size_t i = 1; i <= rows; ++i) {
    const size_t assigned_col = assignment_row_to_col[i];
    if (assigned_col >= 1 && assigned_col <= cols) {
      result[i - 1] = static_cast<int>(assigned_col - 1);
    }
  }

  return result;
}

}  // namespace sensor_fusion::fusion_core::association
