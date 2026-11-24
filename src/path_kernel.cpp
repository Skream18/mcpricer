#include "mcpricer/path_kernel.hpp"

#include <algorithm>
#include <variant>

namespace mcpricer {

BivariateAccumulator simulate_chunk(const SimulationTask& task, std::uint64_t chunk) noexcept {
  const std::uint64_t first = chunk * kChunkSize;
  const std::uint64_t count = std::min(kChunkSize, task.samples - first);
  return std::visit(
      [&](const auto& payoff) {
        return task.antithetic ? simulate_samples<true>(task, payoff, first, count)
                               : simulate_samples<false>(task, payoff, first, count);
      },
      task.payoff);
}

}  // namespace mcpricer
