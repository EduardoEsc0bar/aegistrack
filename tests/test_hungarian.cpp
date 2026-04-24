#include <algorithm>
#include <vector>

#include <doctest/doctest.h>

#include "fusion_core/association/hungarian.h"

namespace sensor_fusion::fusion_core::association {

TEST_CASE("hungarian: small square matrix optimal assignment") {
  const std::vector<std::vector<double>> cost = {
      {4.0, 1.0, 3.0},
      {2.0, 0.0, 5.0},
      {3.0, 2.0, 2.0},
  };

  const std::vector<int> assignment = hungarian_solve(cost);
  CHECK(assignment.size() == 3);
  CHECK(assignment[0] == 1);
  CHECK(assignment[1] == 0);
  CHECK(assignment[2] == 2);
}

TEST_CASE("hungarian: rectangular more columns than rows") {
  const std::vector<std::vector<double>> cost = {
      {8.0, 1.0, 3.0},
      {1.0, 9.0, 4.0},
  };

  const std::vector<int> assignment = hungarian_solve(cost);
  CHECK(assignment.size() == 2);
  CHECK(assignment[0] == 1);
  CHECK(assignment[1] == 0);
}

TEST_CASE("hungarian: padded costs can produce unassigned rows") {
  const std::vector<std::vector<double>> cost = {
      {1.0, 100.0},
      {2.0, 100.0},
      {3.0, 100.0},
  };

  const std::vector<int> assignment = hungarian_solve(cost);
  CHECK(assignment.size() == 3);

  int unassigned_count = 0;
  std::vector<int> assigned_cols;
  for (int col : assignment) {
    if (col == -1) {
      ++unassigned_count;
    } else {
      assigned_cols.push_back(col);
    }
  }

  CHECK(unassigned_count == 1);
  CHECK(assigned_cols.size() == 2);
  CHECK(assigned_cols[0] != assigned_cols[1]);
}

}  // namespace sensor_fusion::fusion_core::association
