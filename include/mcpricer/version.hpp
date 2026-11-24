#pragma once

#include <string_view>

namespace mcpricer {

/// Library version, taken from the CMake project so that it is defined in one place only.
[[nodiscard]] std::string_view version() noexcept;

}  // namespace mcpricer
