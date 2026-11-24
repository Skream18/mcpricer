#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "cli_parser.hpp"
#include "mcpricer/analytic.hpp"
#include "mcpricer/version.hpp"

namespace {

void print_report(const mcpricer::cli::Options& options, std::string_view backend,
                  const mcpricer::PricingResult& result) {
  const auto& [spot, rate, dividend, volatility] = options.market;
  std::cout << "Option        " << options.type_name << "  S0=" << spot
            << " K=" << options.option.strike << " r=" << rate << " q=" << dividend
            << " sigma=" << volatility << " T=" << options.option.maturity;
  if (options.option.style != mcpricer::OptionStyle::European) {
    std::cout << " steps=" << options.option.steps;
  }
  std::cout << "\nSimulation    " << result.paths << " paths, " << result.samples
            << " independent samples, seed " << options.simulation.seed << ", " << backend
            << (options.simulation.antithetic ? ", antithetic" : "")
            << (options.simulation.control_variate ? ", control variate" : "") << "\n\n";

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "MC price      " << result.price << '\n'
            << "Std error     " << std::scientific << std::setprecision(3) << result.std_error
            << std::fixed << std::setprecision(6) << '\n'
            << "95% CI        [" << result.ci_low << ", " << result.ci_high << "]\n";
  if (options.simulation.control_variate) {
    std::cout << "CV beta       " << result.control_beta << '\n';
  }
  if (const auto exact = mcpricer::analytic_price(options.market, options.option)) {
    std::cout << "Analytic      " << *exact;
    if (result.std_error > 0.0) {
      std::cout << "  (MC differs by " << std::showpos << std::setprecision(2)
                << (result.price - *exact) / result.std_error << std::noshowpos
                << " standard errors)";
    }
    std::cout << '\n';
  } else {
    std::cout << "Analytic      no closed form\n";
  }
  const double seconds = result.wall_time.count();
  std::cout << std::setprecision(2) << "Wall time     " << seconds * 1e3 << " ms";
  if (seconds > 0.0) {
    std::cout << "  (" << static_cast<double>(result.paths) / seconds / 1e6 << " M paths/s)";
  }
  std::cout << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    const mcpricer::cli::Options options = mcpricer::cli::parse(arguments);
    if (options.show_help) {
      std::cout << mcpricer::cli::usage();
      return 0;
    }
    if (options.show_version) {
      std::cout << "mcpricer " << mcpricer::version() << '\n';
      return 0;
    }
    const auto backend = mcpricer::cli::make_backend(options);
    const mcpricer::PricingResult result =
        mcpricer::price_monte_carlo(options.market, options.option, options.simulation, *backend);
    print_report(options, backend->name(), result);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "mcpricer: " << error.what() << "\nTry 'mcpricer --help'.\n";
    return 1;
  }
}
