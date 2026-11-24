#include "mcpricer/version.hpp"

#include <gtest/gtest.h>

// Smoke test: proves that the library, the test framework and CTest are wired together.
TEST(Version, MatchesCMakeProjectVersion) {
  EXPECT_EQ(mcpricer::version(), MCPRICER_EXPECTED_VERSION);
}
