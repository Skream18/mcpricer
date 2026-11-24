// Measures throughput and parallel speedup, and the convergence of each variance reduction
// method. Usage: mcpricer_bench [output directory] [--quick]
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "mcpricer/execution_backend.hpp"
#include "mcpricer/monte_carlo.hpp"

namespace {

using mcpricer::IExecutionBackend;
using mcpricer::OptionSpec;
using mcpricer::OptionStyle;
using mcpricer::PricingResult;
using mcpricer::SimulationConfig;

const mcpricer::MarketParams kMarket{.spot = 100, .rate = 0.05, .volatility = 0.2};
const OptionSpec kEuropean{.style = OptionStyle::European};
const OptionSpec kAsian{.style = OptionStyle::AsianArithmetic, .steps = 252};

struct Timing {
  double median;
  double min;
  double max;
  PricingResult result;
};

/// One untimed warm-up, then `repeats` timed runs. The median is robust against interruptions
/// by the operating system, which make individual runs slower but never faster.
Timing measure(const IExecutionBackend& backend, const OptionSpec& option,
               const SimulationConfig& config, int repeats) {
  PricingResult result = price_monte_carlo(kMarket, option, config, backend);
  std::vector<double> seconds;
  for (int i = 0; i < repeats; ++i) {
    result = price_monte_carlo(kMarket, option, config, backend);
    seconds.push_back(result.wall_time.count());
  }
  std::sort(seconds.begin(), seconds.end());
  return {seconds[seconds.size() / 2], seconds.front(), seconds.back(), result};
}

std::ofstream open_csv(const std::filesystem::path& path) {
  std::ofstream file(path);
  if (!file) throw std::runtime_error("cannot write " + path.string());
  file << std::setprecision(9);
  return file;
}

std::string timestamp() {
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  std::ostringstream text;
  text << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return text.str();
}

/// 1, 2, 4, ... up to and including the number of hardware threads.
std::vector<unsigned> thread_counts() {
  const unsigned available = std::max(std::thread::hardware_concurrency(), 1U);
  std::vector<unsigned> counts;
  for (unsigned n = 1; n < available; n *= 2) counts.push_back(n);
  counts.push_back(available);
  return counts;
}

void benchmark_scaling(const std::filesystem::path& directory, int repeats, std::uint64_t divisor) {
  std::ofstream csv = open_csv(directory / "scaling.csv");
  csv << "timestamp,cpu,compiler,flags,option,steps,paths,backend,threads,median_s,min_s,max_s,"
         "paths_per_s,speedup\n";
  const std::string prefix =
      timestamp() + ",\"" MCPRICER_CPU "\",\"" MCPRICER_COMPILER "\",\"" MCPRICER_FLAGS "\",";

  const auto run = [&](std::string_view name, const OptionSpec& option, std::uint64_t paths) {
    const auto report = [&](std::string_view backend, unsigned threads, const Timing& timing,
                            double speedup) {
      const double rate = static_cast<double>(paths) / timing.median;
      csv << prefix << name << ',' << option.steps << ',' << paths << ',' << backend << ','
          << threads << ',' << timing.median << ',' << timing.min << ',' << timing.max << ','
          << rate << ',' << speedup << '\n';
      std::cout << std::left << std::setw(12) << name << std::right << std::setw(10) << paths
                << std::setw(10) << backend << std::setw(4) << threads << std::fixed
                << std::setprecision(3) << std::setw(12) << timing.median * 1e3 << " ms"
                << std::setw(10) << rate / 1e6 << " M paths/s" << std::setprecision(2)
                << std::setw(8) << speedup << "x\n";
    };
    const SimulationConfig config{.paths = paths};
    const Timing serial = measure(*mcpricer::make_serial_backend(), option, config, repeats);
    report("serial", 1, serial, 1.0);
    for (const unsigned threads : thread_counts()) {
      const Timing timing =
          measure(*mcpricer::make_threaded_backend(threads), option, config, repeats);
      if (timing.result.price != serial.result.price) {
        throw std::logic_error("backends disagree; the benchmark would be meaningless");
      }
      report("threaded", threads, timing, serial.median / timing.median);
    }
  };
  for (const std::uint64_t paths : {10'000U, 100'000U, 1'000'000U}) {
    run("asian-arith", kAsian, paths / divisor);
  }
  for (const std::uint64_t paths : {1'000'000U, 10'000'000U}) {
    run("euro-call", kEuropean, paths / divisor);
  }
}

void benchmark_convergence(const std::filesystem::path& directory, int repeats,
                           std::uint64_t divisor) {
  std::ofstream csv = open_csv(directory / "convergence.csv");
  // Efficiency is 1 / (variance * time): it charges each method for its extra cost.
  csv << "option,method,paths,price,std_error,median_s,efficiency\n";
  const auto serial = mcpricer::make_serial_backend();
  const auto run = [&](std::string_view name, const OptionSpec& option) {
    for (std::uint64_t paths = 1'000; paths <= 256'000 / divisor; paths *= 4) {
      for (const bool control : {false, true}) {
        for (const bool antithetic : {false, true}) {
          const std::string_view method =
              control ? (antithetic ? "both" : "control") : (antithetic ? "antithetic" : "plain");
          const Timing timing = measure(
              *serial, option,
              {.paths = paths, .antithetic = antithetic, .control_variate = control}, repeats);
          const PricingResult& result = timing.result;
          csv << name << ',' << method << ',' << paths << ',' << result.price << ','
              << result.std_error << ',' << timing.median << ','
              << 1.0 / (result.std_error * result.std_error * timing.median) << '\n';
        }
      }
    }
  };
  run("asian-arith", kAsian);
  run("euro-call", kEuropean);
}

}  // namespace

int main(int argc, char** argv) {
  try {
#ifndef NDEBUG
    std::cerr << "warning: this is a Debug build; the timings are meaningless\n";
#endif
    std::filesystem::path directory = "bench/results";
    bool quick = false;
    for (const std::string_view argument : std::vector<std::string_view>(argv + 1, argv + argc)) {
      if (argument == "--quick") {
        quick = true;
      } else if (argument.starts_with("-")) {
        throw std::invalid_argument("unknown option " + std::string(argument));
      } else {
        directory = argument;
      }
    }
    std::filesystem::create_directories(directory);
    std::cout << MCPRICER_CPU << " | " << MCPRICER_COMPILER << " | " << MCPRICER_FLAGS << '\n';
    const int repeats = quick ? 1 : 5;
    const std::uint64_t divisor = quick ? 10 : 1;
    benchmark_scaling(directory, repeats, divisor);
    benchmark_convergence(directory, repeats, divisor);
    std::cout << "Results written to " << directory.string() << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "mcpricer_bench: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
