#include "mcpricer/statistics.hpp"

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

}  // namespace mcpricer
