#include "mcpricer/statistics.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

namespace mcpricer {
namespace {

TEST(Accumulator, MatchesHandComputedValues) {
  BivariateAccumulator acc;
  acc.add(1.0, 2.0);
  acc.add(2.0, 1.0);
  acc.add(6.0, 6.0);
  EXPECT_EQ(acc.count(), 3U);
  EXPECT_DOUBLE_EQ(acc.mean_x(), 3.0);
  EXPECT_DOUBLE_EQ(acc.mean_y(), 3.0);
  EXPECT_DOUBLE_EQ(acc.sum_xx(), 14.0);  // 4 + 1 + 9
  EXPECT_DOUBLE_EQ(acc.sum_yy(), 14.0);  // 1 + 4 + 9
  EXPECT_DOUBLE_EQ(acc.sum_xy(), 13.0);  // 2 + 2 + 9

  const Estimate estimate = estimate_mean(acc);
  EXPECT_DOUBLE_EQ(estimate.mean, 3.0);
  EXPECT_DOUBLE_EQ(estimate.std_error, std::sqrt(14.0 / 2.0 / 3.0));
}

TEST(Accumulator, MergeMatchesSinglePass) {
  BivariateAccumulator whole;
  BivariateAccumulator parts[3];  // Unequal sizes: 100, 1 and 899.
  for (int i = 0; i < 1000; ++i) {
    const double x = std::sin(i) * 10.0 + 5.0;
    const double y = std::cos(i * 0.7) + 0.1 * x;
    whole.add(x, y);
    parts[i < 100 ? 0 : (i == 100 ? 1 : 2)].add(x, y);
  }
  BivariateAccumulator merged;
  for (const auto& part : parts) merged.merge(part);
  EXPECT_EQ(merged.count(), whole.count());
  EXPECT_NEAR(merged.mean_x(), whole.mean_x(), 1e-12);
  EXPECT_NEAR(merged.mean_y(), whole.mean_y(), 1e-12);
  EXPECT_NEAR(merged.sum_xx(), whole.sum_xx(), 1e-8);
  EXPECT_NEAR(merged.sum_yy(), whole.sum_yy(), 1e-8);
  EXPECT_NEAR(merged.sum_xy(), whole.sum_xy(), 1e-8);
}

TEST(Accumulator, MergingEmptyChangesNothing) {
  BivariateAccumulator acc;
  acc.add(1.0, 2.0);
  acc.add(3.0, 5.0);
  const BivariateAccumulator before = acc;
  acc.merge({});
  EXPECT_EQ(acc, before);
}

// The textbook sum of squares loses all significant digits on this input.
TEST(Accumulator, StableForLargeOffset) {
  BivariateAccumulator acc;
  for (const double x : {4.0, 7.0, 13.0, 16.0}) acc.add(1e9 + x, 0.0);
  EXPECT_DOUBLE_EQ(acc.sum_xx() / 3.0, 30.0);
}

TEST(Estimate, RequiresTwoSamples) {
  BivariateAccumulator acc;
  EXPECT_THROW((void)estimate_mean(acc), std::invalid_argument);
  acc.add(1.0, 1.0);
  EXPECT_THROW((void)estimate_mean(acc), std::invalid_argument);
}

}  // namespace
}  // namespace mcpricer
