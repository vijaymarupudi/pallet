#pragma once
#include <system_error>
#include <expected>
#include <source_location>
#include <cstdlib>
#include <cstdio>


namespace pallet {
template <class T>
using Result = std::expected<T, std::error_condition>;

static inline std::unexpected<std::error_condition> error (std::error_condition x) {
  return std::unexpected(x);
}

template <class T>
static inline T unwrap(Result<T>&& in, std::source_location location = std::source_location::current()) {
  if (in) {
    return *std::move(in);
  } else {
    auto& error = in.error();
    printf("panic(%d): %s | %d:%d:%s:%s\n",
           error.value(),
           error.message().c_str(),
           location.line(),
           location.column(),
           location.function_name(),
           location.file_name());
    abort();
  }
}

}

// namespace pallet {
// enum class ErrorCode {
  
// }
// }
