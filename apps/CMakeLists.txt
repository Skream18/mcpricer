# The parser is a library so that the tests can exercise it without starting a process.
add_library(mcpricer_cli STATIC cli_parser.cpp)
target_include_directories(mcpricer_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mcpricer_cli PUBLIC mcpricer_core)
mcpricer_set_warnings(mcpricer_cli)

add_executable(mcpricer mcpricer.cpp)
target_link_libraries(mcpricer PRIVATE mcpricer_cli)
mcpricer_set_warnings(mcpricer)
