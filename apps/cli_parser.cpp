#include "cli_parser.hpp"

#include <charconv>
#include <stdexcept>
#include <system_error>

namespace mcpricer::cli {
namespace {

// std::from_chars rather than std::stod: it ignores the locale, and it lets us insist that the
// whole argument is a number, so that "0.2x" is an error instead of 0.2.
template <typename T>
T parse_number(std::string_view flag, std::string_view text) {
  T value{};
  const char* const end = text.data() + text.size();
  const auto [parsed_to, error] = std::from_chars(text.data(), end, value);
  if (error == std::errc::result_out_of_range) {
    throw std::invalid_argument(std::string(flag) + " is out of range: '" + std::string(text) +
                                "'");
  }
  if (error != std::errc{} || parsed_to != end) {
    throw std::invalid_argument(std::string(flag) + " expects a number, got '" + std::string(text) +
                                "'");
  }
  return value;
}

void set_type(Options& options, std::string_view name) {
  const auto set = [&](OptionStyle style, OptionRight right) {
    options.option.style = style;
    options.option.right = right;
  };
  if (name == "euro-call") {
    set(OptionStyle::European, OptionRight::Call);
  } else if (name == "euro-put") {
    set(OptionStyle::European, OptionRight::Put);
  } else if (name == "asian-arith" || name == "asian-arith-call") {
    set(OptionStyle::AsianArithmetic, OptionRight::Call);
  } else if (name == "asian-arith-put") {
    set(OptionStyle::AsianArithmetic, OptionRight::Put);
  } else if (name == "asian-geo" || name == "asian-geo-call") {
    set(OptionStyle::AsianGeometric, OptionRight::Call);
  } else if (name == "asian-geo-put") {
    set(OptionStyle::AsianGeometric, OptionRight::Put);
  } else {
    throw std::invalid_argument("unknown --type '" + std::string(name) + "'");
  }
  options.type_name = name;
}

BackendKind parse_backend(std::string_view name) {
  if (name == "serial") return BackendKind::Serial;
  if (name == "threaded") return BackendKind::Threaded;
  if (name == "cuda") return BackendKind::Cuda;
  throw std::invalid_argument("unknown --backend '" + std::string(name) + "'");
}

}  // namespace

Options parse(std::span<const std::string_view> arguments) {
  Options options;
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    const std::string_view flag = arguments[i];
    const auto text = [&] {
      if (i + 1 >= arguments.size()) {
        throw std::invalid_argument(std::string(flag) + " requires a value");
      }
      return arguments[++i];
    };
    const auto number = [&]<typename T>(T& target) { target = parse_number<T>(flag, text()); };

    if (flag == "--help" || flag == "-h") {
      options.show_help = true;
    } else if (flag == "--version") {
      options.show_version = true;
    } else if (flag == "--type") {
      set_type(options, text());
    } else if (flag == "--S0") {
      number(options.market.spot);
    } else if (flag == "--K") {
      number(options.option.strike);
    } else if (flag == "--r") {
      number(options.market.rate);
    } else if (flag == "--q") {
      number(options.market.dividend);
    } else if (flag == "--sigma") {
      number(options.market.volatility);
    } else if (flag == "--T") {
      number(options.option.maturity);
    } else if (flag == "--steps") {
      number(options.option.steps);
    } else if (flag == "--paths") {
      number(options.simulation.paths);
    } else if (flag == "--seed") {
      number(options.simulation.seed);
    } else if (flag == "--threads") {
      number(options.threads);
    } else if (flag == "--backend") {
      options.backend = parse_backend(text());
    } else if (flag == "--antithetic") {
      options.simulation.antithetic = true;
    } else if (flag == "--cv") {
      options.simulation.control_variate = true;
    } else {
      throw std::invalid_argument("unknown argument '" + std::string(flag) + "'");
    }
  }
  if (options.threads != 0 && options.backend != BackendKind::Threaded) {
    throw std::invalid_argument("--threads applies only to --backend threaded");
  }
  return options;
}

std::unique_ptr<IExecutionBackend> make_backend(const Options& options) {
  switch (options.backend) {
    case BackendKind::Serial:
      return make_serial_backend();
    case BackendKind::Threaded:
      return make_threaded_backend(options.threads);
    case BackendKind::Cuda:
      return make_cuda_backend();
  }
  throw std::invalid_argument("unknown backend");
}

std::string_view usage() noexcept {
  return R"(Usage: mcpricer [options]

Contract and market:
  --type NAME      euro-call (default), euro-put, asian-arith[-call], asian-arith-put,
                   asian-geo[-call], asian-geo-put
  --S0 X           spot price (100)
  --K X            strike (100)
  --r X            risk-free rate, continuously compounded (0.05)
  --q X            dividend yield, continuously compounded (0)
  --sigma X        annual volatility (0.2)
  --T X            maturity in years (1)
  --steps N        monitoring dates of Asian options (252)

Simulation:
  --paths N        simulated paths (1000000)
  --seed N         random seed (42)
  --antithetic     use antithetic variates
  --cv             use a control variate (not for asian-geo)
  --backend NAME   serial, threaded (default) or cuda
  --threads N      worker threads of the threaded backend; 0 = all (default)

  --help, --version
)";
}

}  // namespace mcpricer::cli
