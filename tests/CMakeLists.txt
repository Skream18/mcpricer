include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE
)
# Use the same MSVC runtime as our own code, otherwise linking fails.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(mcpricer_tests
  test_analytic.cpp
  test_market.cpp
  test_monte_carlo.cpp
  test_payoffs.cpp
  test_philox.cpp
  test_statistics.cpp
  test_version.cpp
)
target_link_libraries(mcpricer_tests PRIVATE mcpricer_core GTest::gtest_main)
target_compile_definitions(mcpricer_tests PRIVATE MCPRICER_EXPECTED_VERSION="${PROJECT_VERSION}")
mcpricer_set_warnings(mcpricer_tests)

include(GoogleTest)
gtest_discover_tests(mcpricer_tests DISCOVERY_MODE PRE_TEST)
