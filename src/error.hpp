#pragma once
#include <system_error>
#include <expected>

namespace pallet {
template <class T>
using Result = std::expected<T, std::error_condition>;
}

// namespace pallet {
// enum class ErrorCode {
  
// }
// }
