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
  std::uint64_t samples = 0;
};

/// Samples are simulated in fixed-size chunks whose results are merged in order. Floating-point
/// addition is not associative, so fixing both is what makes every CPU backend return the same
/// bits for any number of threads. The size is small enough to balance load and large enough
/// to make the cost of scheduling negligible.
inline constexpr std::uint64_t kChunkSize = 4096;

[[nodiscard]] constexpr std::uint64_t chunk_count(std::uint64_t samples) noexcept {
  return samples / kChunkSize + (samples % kChunkSize != 0 ? 1 : 0);
}

/// Simulates samples [first, first + count). The inner loop of the whole program.
template <Payoff P>
[[nodiscard]] BivariateAccumulator simulate_samples(const SimulationTask& task, const P& payoff,
                                                    std::uint64_t first,
                                                    std::uint64_t count) noexcept {
  BivariateAccumulator accumulator;
  for (std::uint64_t sample = first; sample < first + count; ++sample) {
    NormalStream normals(task.seed, sample);
    PathState path;
    for (std::size_t step = 0; step < task.steps; ++step) {
      // Exact solution of GBM over one step, so there is no time-discretisation bias.
      path.log_return += task.step_drift + task.step_diffusion * normals.next();
      payoff.observe(path);
    }
    const PathResult result = payoff.evaluate(path, task.steps);
    accumulator.add(task.discount * result.payoff, task.discount * result.control);
  }
  return accumulator;
}

/// Resolves the payoff type once per chunk and runs simulate_samples on it.
[[nodiscard]] BivariateAccumulator simulate_chunk(const SimulationTask& task, std::uint64_t chunk);

}  // namespace mcpricer
