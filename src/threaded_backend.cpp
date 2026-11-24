#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "mcpricer/execution_backend.hpp"

namespace mcpricer {
namespace {

class ThreadedBackend final : public IExecutionBackend {
 public:
  explicit ThreadedBackend(unsigned threads) : threads_(threads) {}

  BivariateAccumulator run(const SimulationTask& task) const override {
    const std::uint64_t chunks = chunk_count(task.samples);
    std::vector<BivariateAccumulator> results(chunks);
    std::atomic<std::uint64_t> next_chunk{0};

    // Workers claim chunks one at a time instead of receiving equal shares up front: cores can
    // differ in speed (hybrid CPUs, turbo, other processes), and a fixed split would make the
    // fast ones wait. Who runs a chunk cannot change its result, since streams belong to samples.
    const auto work = [&] {
      // Relaxed is enough: the counter only hands out unique indices. The joins below are what
      // makes the results visible to this thread.
      for (std::uint64_t chunk = next_chunk.fetch_add(1, std::memory_order_relaxed); chunk < chunks;
           chunk = next_chunk.fetch_add(1, std::memory_order_relaxed)) {
        results[chunk] = simulate_chunk(task, chunk);  // No other thread touches this slot.
      }
    };
    {
      const auto workers = static_cast<unsigned>(std::min<std::uint64_t>(threads_, chunks));
      std::vector<std::jthread> pool;
      pool.reserve(workers);
      for (unsigned i = 0; i < workers; ++i) pool.emplace_back(work);
    }  // jthread joins here, also if starting a later thread throws.

    BivariateAccumulator total;
    for (const BivariateAccumulator& result : results) total.merge(result);
    return total;
  }

  std::string_view name() const noexcept override { return "threaded"; }

 private:
  unsigned threads_;
};

}  // namespace

std::unique_ptr<IExecutionBackend> make_threaded_backend(unsigned threads) {
  if (threads > kMaxThreads) {
    throw std::invalid_argument("threads must be in [0, " + std::to_string(kMaxThreads) +
                                "], got " + std::to_string(threads));
  }
  // hardware_concurrency() may return 0 when it cannot tell.
  if (threads == 0) threads = std::clamp(std::thread::hardware_concurrency(), 1U, kMaxThreads);
  return std::make_unique<ThreadedBackend>(threads);
}

}  // namespace mcpricer
