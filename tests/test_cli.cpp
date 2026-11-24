#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "cli_parser.hpp"

namespace mcpricer::cli {
namespace {

Options parse_args(std::initializer_list<std::string_view> arguments) {
  return parse(std::vector<std::string_view>(arguments));
}

TEST(Cli, ParsesEveryFlag) {
  const Options o = parse_args(
      {"--type",   "asian-arith", "--S0",    "101",          "--K",    "99",  "--r",
       "-0.01",    "--q",         "0.02",    "--sigma",      "0.3",    "--T", "2",
       "--steps",  "52",          "--paths", "5000",         "--seed", "7",   "--backend",
       "threaded", "--threads",   "3",       "--antithetic", "--cv"});
  EXPECT_EQ(o.option.style, OptionStyle::AsianArithmetic);
  EXPECT_EQ(o.option.right, OptionRight::Call);
  EXPECT_EQ(o.market.spot, 101);
  EXPECT_EQ(o.option.strike, 99);
  EXPECT_EQ(o.market.rate, -0.01);
  EXPECT_EQ(o.market.dividend, 0.02);
  EXPECT_EQ(o.market.volatility, 0.3);
  EXPECT_EQ(o.option.maturity, 2);
  EXPECT_EQ(o.option.steps, 52U);
  EXPECT_EQ(o.simulation.paths, 5000U);
  EXPECT_EQ(o.simulation.seed, 7U);
  EXPECT_EQ(o.backend, BackendKind::Threaded);
  EXPECT_EQ(o.threads, 3U);
  EXPECT_TRUE(o.simulation.antithetic);
  EXPECT_TRUE(o.simulation.control_variate);
}

TEST(Cli, MapsTypeNames) {
  const auto check = [](std::string_view name, OptionStyle style, OptionRight right) {
    const Options o = parse_args({"--type", name});
    EXPECT_EQ(o.option.style, style) << name;
    EXPECT_EQ(o.option.right, right) << name;
  };
  check("euro-call", OptionStyle::European, OptionRight::Call);
  check("euro-put", OptionStyle::European, OptionRight::Put);
  check("asian-arith-call", OptionStyle::AsianArithmetic, OptionRight::Call);
  check("asian-arith-put", OptionStyle::AsianArithmetic, OptionRight::Put);
  check("asian-geo", OptionStyle::AsianGeometric, OptionRight::Call);
  check("asian-geo-call", OptionStyle::AsianGeometric, OptionRight::Call);
  check("asian-geo-put", OptionStyle::AsianGeometric, OptionRight::Put);
}

TEST(Cli, DefaultsAreValidAndHelpIsRecognised) {
  const Options o = parse_args({});
  EXPECT_NO_THROW(validate(o.market));
  EXPECT_NO_THROW(validate(o.option));
  EXPECT_NO_THROW(validate(o.simulation));
  EXPECT_TRUE(parse_args({"--help"}).show_help);
  EXPECT_TRUE(parse_args({"--version"}).show_version);
}

TEST(Cli, RejectsMalformedInput) {
  const std::vector<std::vector<std::string_view>> bad{
      {"--unknown"},
      {"stray"},
      {"--K"},                                 // missing value
      {"--K", ""},                             // empty
      {"--sigma", "0.2x"},                     // trailing characters
      {"--sigma", "abc"},                      // not a number
      {"--paths", "-5"},                       // negative count
      {"--paths", "1.5"},                      // fractional count
      {"--paths", "99999999999999999999999"},  // does not fit 64 bits
      {"--S0", "1e999"},                       // does not fit a double
      {"--type", "barrier"},
      {"--backend", "gpu"},
      {"--backend", "serial", "--threads", "4"},  // contradictory
  };
  for (const auto& arguments : bad) {
    EXPECT_THROW((void)parse(arguments), std::invalid_argument) << arguments.front();
  }
}

// from_chars accepts these spellings, so range validation has to reject them.
TEST(Cli, NonFiniteNumbersFailValidation) {
  EXPECT_THROW(validate(parse_args({"--S0", "nan"}).market), std::invalid_argument);
  EXPECT_THROW(validate(parse_args({"--sigma", "inf"}).market), std::invalid_argument);
}

TEST(Cli, SelectsBackend) {
  EXPECT_EQ(make_backend(parse_args({"--backend", "serial"}))->name(), "serial");
  EXPECT_EQ(make_backend(parse_args({"--threads", "2"}))->name(), "threaded");
}

}  // namespace
}  // namespace mcpricer::cli
