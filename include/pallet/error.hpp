#pragma once
#include <system_error>
#include <expected>

namespace pallet {
template <class T>
using Result = std::expected<T, std::error_condition>;

static inline std::unexpected<std::error_condition> error (std::error_condition x) {
  return std::unexpected(x);
}
}

// namespace pallet {
// enum class ErrorCode {
  
// }
// }
