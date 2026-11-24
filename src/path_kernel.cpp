#include "mcpricer/path_kernel.hpp"

#include <algorithm>
#include <variant>

namespace mcpricer {

BivariateAccumulator simulate_chunk(const SimulationTask& task, std::uint64_t chunk) {
  const std::uint64_t first = chunk * kChunkSize;
  const std::uint64_t count = std::min(kChunkSize, task.samples - first);
  return std::visit(
      [&](const auto& payoff) { return simulate_samples(task, payoff, first, count); },
      task.payoff);
}

}  // namespace mcpricer
