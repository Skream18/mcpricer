add_executable(mcpricer_bench bench_main.cpp)
target_link_libraries(mcpricer_bench PRIVATE mcpricer_core)
mcpricer_set_warnings(mcpricer_bench)

# Recorded with every result, so that a CSV file can be interpreted on its own. The CPU is that of
# the build machine: build and run benchmarks on the same one. CMake's PROCESSOR_DESCRIPTION is
# not used because it misidentifies recent CPUs.
set(cpu_name "unknown")
if(CMAKE_HOST_WIN32)
  cmake_host_system_information(RESULT cpu_name QUERY WINDOWS_REGISTRY
    "HKLM/HARDWARE/DESCRIPTION/System/CentralProcessor/0" VALUE ProcessorNameString)
elseif(EXISTS /proc/cpuinfo)
  file(STRINGS /proc/cpuinfo cpu_line REGEX "^model name" LIMIT_COUNT 1)
  string(REGEX REPLACE "^model name[ \t]*:[ \t]*" "" cpu_name "${cpu_line}")
endif()
string(STRIP "${cpu_name}" cpu_name)
string(STRIP "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}" release_flags)
target_compile_definitions(mcpricer_bench PRIVATE
  MCPRICER_CPU="${cpu_name}"
  MCPRICER_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
  MCPRICER_FLAGS="${release_flags}"
)
