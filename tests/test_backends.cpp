#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <thread>
#include <vector>

#include "mcpricer/execution_backend.hpp"
#include "mcpricer/monte_carlo.hpp"

namespace mcpricer {
namespace {

PricingResult price(const IExecutionBackend& backend, const OptionSpec& option,
                    const SimulationConfig& config) {
  return price_monte_carlo({.spot = 100, .rate = 0.05, .volatility = 0.2}, option, config, backend);
}

TEST(Chunks, CoverEverySampleOnce) {
  EXPECT_EQ(chunk_count(0), 0U);
  EXPECT_EQ(chunk_count(1), 1U);
  EXPECT_EQ(chunk_count(kChunkSize), 1U);
  EXPECT_EQ(chunk_count(kChunkSize + 1), 2U);

  SimulationTask task{.payoff = EuropeanPayoff{{OptionRight::Call, 100, 100}}};
  task.samples = kChunkSize + 5;
  EXPECT_EQ(simulate_chunk(task, 0).count(), kChunkSize);
  EXPECT_EQ(simulate_chunk(task, 1).count(), 5U);
}

// Stronger than statistical agreement: exactly equal doubles, for any number of threads.
TEST(ThreadedBackend, BitIdenticalToSerial) {
  const auto serial = make_serial_backend();
  const std::vector<unsigned> thread_counts{1, 2, 3, 8, std::thread::hardware_concurrency(), 0};
  // Fewer samples than a chunk, whole chunks, and a partial last chunk.
  for (const std::uint64_t paths : {std::uint64_t{100}, 4 * kChunkSize, 20 * kChunkSize + 124}) {
    for (const OptionStyle style : {OptionStyle::European, OptionStyle::AsianArithmetic}) {
      for (const bool reduce : {false, true}) {
        const OptionSpec option{.style = style, .steps = 4};
        const SimulationConfig config{
            .paths = paths, .seed = 9, .antithetic = reduce, .control_variate = reduce};
        const PricingResult expected = price(*serial, option, config);
        for (const unsigned threads : thread_counts) {
          const PricingResult actual = price(*make_threaded_backend(threads), option, config);
          EXPECT_EQ(actual.price, expected.price) << threads << " threads, " << paths << " paths";
          EXPECT_EQ(actual.std_error, expected.std_error);
          EXPECT_EQ(actual.control_beta, expected.control_beta);
          EXPECT_EQ(actual.samples, expected.samples);
        }
      }
    }
  }
}

TEST(ThreadedBackend, RepeatedRunsAgree) {
  const auto backend = make_threaded_backend(0);
  const SimulationConfig config{.paths = 100'000, .seed = 3};
  const double first = price(*backend, OptionSpec{}, config).price;
  for (int i = 0; i < 20; ++i) EXPECT_EQ(price(*backend, OptionSpec{}, config).price, first);
}

TEST(ThreadedBackend, AgreesStatisticallyWithSerialOnAnotherSeed) {
  const PricingResult serial =
      price(*make_serial_backend(), OptionSpec{}, {.paths = 200'000, .seed = 1});
  const PricingResult threaded =
      price(*make_threaded_backend(0), OptionSpec{}, {.paths = 200'000, .seed = 2});
  EXPECT_NE(serial.price, threaded.price);
  EXPECT_NEAR(serial.price, threaded.price, 3.0 * std::hypot(serial.std_error, threaded.std_error));
}

TEST(ThreadedBackend, RejectsAbsurdThreadCounts) {
  EXPECT_THROW((void)make_threaded_backend(kMaxThreads + 1), std::invalid_argument);
}

TEST(Backends, HaveNames) {
  EXPECT_EQ(make_serial_backend()->name(), "serial");
  EXPECT_EQ(make_threaded_backend(2)->name(), "threaded");
}

TEST(CudaBackend, FailsClearlyUntilImplemented) {
  const auto use_cuda = [] { (void)price(*make_cuda_backend(), OptionSpec{}, {.paths = 100}); };
  EXPECT_THROW(use_cuda(), std::runtime_error);
}

}  // namespace
}  // namespace mcpricer
