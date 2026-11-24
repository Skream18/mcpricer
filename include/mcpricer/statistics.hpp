#pragma once

#include <cstdint>

namespace mcpricer {

/// Running means and centred second moments of paired samples (x, y). Here x is the discounted
/// payoff and y its control variate.
///
/// Uses Welford's update, because the textbook sum(x^2) - n * mean^2 subtracts two nearly equal
/// numbers and loses precision. Two accumulators can be merged (Chan, Golub and LeVeque), which
/// is what makes a parallel reduction possible.
class BivariateAccumulator {
 public:
  void add(double x, double y) noexcept {
    ++count_;
    const double n = static_cast<double>(count_);
    const double dx = x - mean_x_;
    const double dy = y - mean_y_;
    mean_x_ += dx / n;
    mean_y_ += dy / n;
    // One factor uses the old mean and the other the new one; this is not a typo.
    sum_xx_ += dx * (x - mean_x_);
    sum_yy_ += dy * (y - mean_y_);
    sum_xy_ += dx * (y - mean_y_);
  }

  void merge(const BivariateAccumulator& other) noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
  [[nodiscard]] double mean_x() const noexcept { return mean_x_; }
  [[nodiscard]] double mean_y() const noexcept { return mean_y_; }
  /// Centred sums, not yet divided by the degrees of freedom.
  [[nodiscard]] double sum_xx() const noexcept { return sum_xx_; }
  [[nodiscard]] double sum_yy() const noexcept { return sum_yy_; }
  [[nodiscard]] double sum_xy() const noexcept { return sum_xy_; }

  /// Exact comparison, used to check that backends agree bit for bit.
  friend bool operator==(const BivariateAccumulator&, const BivariateAccumulator&) = default;

 private:
  std::uint64_t count_ = 0;
  double mean_x_ = 0.0;
  double mean_y_ = 0.0;
  double sum_xx_ = 0.0;
  double sum_yy_ = 0.0;
  double sum_xy_ = 0.0;
};

struct Estimate {
  double mean = 0.0;
  double std_error = 0.0;
  double beta = 0.0;  ///< Control variate coefficient; zero if no control was used.
};

/// Sample mean of x and its standard error. Requires at least two samples.
[[nodiscard]] Estimate estimate_mean(const BivariateAccumulator& samples);

/// Estimates E[x] as mean(x) - beta * (mean(y) - control_mean), where control_mean is the exact
/// E[y] and beta = Cov(x, y) / Var(y) minimises the variance. Requires at least four samples.
///
/// Beta is estimated from the same samples. The resulting bias is O(1/n), negligible next to the
/// O(1/sqrt(n)) standard error, and a separate pilot run is not needed.
[[nodiscard]] Estimate estimate_with_control(const BivariateAccumulator& samples,
                                             double control_mean);

}  // namespace mcpricer
