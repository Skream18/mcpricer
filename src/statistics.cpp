#include "mcpricer/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mcpricer {

void BivariateAccumulator::merge(const BivariateAccumulator& other) noexcept {
  if (other.count_ == 0) return;
  if (count_ == 0) {
    *this = other;
    return;
  }
  const double n = static_cast<double>(count_);
  const double m = static_cast<double>(other.count_);
  const double dx = other.mean_x_ - mean_x_;
  const double dy = other.mean_y_ - mean_y_;
  const double weight = n * m / (n + m);
  sum_xx_ += other.sum_xx_ + dx * dx * weight;
  sum_yy_ += other.sum_yy_ + dy * dy * weight;
  sum_xy_ += other.sum_xy_ + dx * dy * weight;
  mean_x_ += dx * (m / (n + m));
  mean_y_ += dy * (m / (n + m));
  count_ += other.count_;
}

Estimate estimate_mean(const BivariateAccumulator& samples) {
  if (samples.count() < 2) {
    throw std::invalid_argument("a standard error requires at least two samples");
  }
  const double n = static_cast<double>(samples.count());
  return {.mean = samples.mean_x(), .std_error = std::sqrt(samples.sum_xx() / (n - 1.0) / n)};
}

Estimate estimate_with_control(const BivariateAccumulator& samples, double control_mean) {
  if (samples.count() < 4) {
    throw std::invalid_argument("a control variate requires at least four samples");
  }
  const double n = static_cast<double>(samples.count());
  // A constant control carries no information, so fall back to the plain estimator.
  const double beta = samples.sum_yy() > 0.0 ? samples.sum_xy() / samples.sum_yy() : 0.0;
  // Residual sum of squares of regressing x on y. Two parameters were fitted, so two degrees of
  // freedom are lost. Rounding can make it slightly negative under perfect correlation.
  const double residual = std::max(samples.sum_xx() - beta * samples.sum_xy(), 0.0);
  return {.mean = samples.mean_x() - beta * (samples.mean_y() - control_mean),
          .std_error = std::sqrt(residual / (n - 2.0) / n),
          .beta = beta};
}

}  // namespace mcpricer
