#pragma once

#include <cstddef>
#include <cstdint>

#include "mcpricer/payoffs.hpp"
#include "mcpricer/philox.hpp"
#include "mcpricer/statistics.hpp"

namespace mcpricer {

/// Everything a backend needs, with all per-path constants precomputed.
struct SimulationTask {
  AnyPayoff payoff;
  std::size_t steps = 1;
  double step_drift = 0.0;      ///< (r - q - sigma^2 / 2) * dt
  double step_diffusion = 0.0;  ///< sigma * sqrt(dt)
  double discount = 1.0;
  std::uint64_t seed = 0;
  std::uint64_t samples = 0;  ///< Independent observations; one is two paths if antithetic.
  bool antithetic = false;
};

/// Samples are simulated in fixed-size chunks whose results are merged in order. Floating-point
/// addition is not associative, so fixing both is what makes every CPU backend return the same
/// bits for any number of threads. The size is small enough to balance load and large enough
/// to make the cost of scheduling negligible.
inline constexpr std::uint64_t kChunkSize = 1024;

[[nodiscard]] constexpr std::uint64_t chunk_count(std::uint64_t samples) noexcept {
  return samples / kChunkSize + (samples % kChunkSize != 0 ? 1 : 0);
}

/// Simulates samples [first, first + count). The inner loop of the whole program.
template <bool Antithetic, Payoff P>
[[nodiscard]] BivariateAccumulator simulate_samples(const SimulationTask& task, const P& payoff,
                                                    std::uint64_t first,
                                                    std::uint64_t count) noexcept {
  BivariateAccumulator accumulator;
  for (std::uint64_t sample = first; sample < first + count; ++sample) {
    NormalStream normals(task.seed, sample);
    PathState path;
    PathState mirror;  // Driven by -Z. Unused, and optimised away, without antithetics.
    for (std::size_t step = 0; step < task.steps; ++step) {
      // Exact solution of GBM over one step, so there is no time-discretisation bias.
      const double shock = task.step_diffusion * normals.next();
      path.log_return += task.step_drift + shock;
      payoff.observe(path);
      if constexpr (Antithetic) {
        mirror.log_return += task.step_drift - shock;
        payoff.observe(mirror);
      }
    }
    PathResult result = payoff.evaluate(path, task.steps);
    if constexpr (Antithetic) {
      // The two halves are dependent, so their average is one observation, not two. Counting
      // them separately would report a wrong standard error.
      const PathResult other = payoff.evaluate(mirror, task.steps);
      result.payoff = 0.5 * (result.payoff + other.payoff);
      result.control = 0.5 * (result.control + other.control);
    }
    accumulator.add(task.discount * result.payoff, task.discount * result.control);
  }
  return accumulator;
}

/// Resolves the payoff type and the antithetic flag once per chunk, then runs simulate_samples.
/// It cannot throw: the payoffs are trivially copyable, so the variant always holds a value.
[[nodiscard]] BivariateAccumulator simulate_chunk(const SimulationTask& task,
                                                  std::uint64_t chunk) noexcept;

}  // namespace mcpricer
