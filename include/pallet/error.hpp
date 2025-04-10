#pragma once
#include <system_error>
#include <expected>
#include <cstdlib>
#include <cstdio>

namespace pallet {
template <class T>
using Result = std::expected<T, std::error_condition>;

static inline std::unexpected<std::error_condition> error (std::error_condition x) {
  return std::unexpected(x);
}

template <class T>
static inline T unwrap(Result<T>&& in) {
  if (in) {
    return *std::move(in);
  } else {
    auto& error = in.error();
    printf("panic(%d): %s\n", error.value(),
           error.message().c_str());
    abort();
  }
}

template <class T>
static inline T unwrap(Result<T>&& in, const char* msg) {
  if (in) {
    return *std::move(in);
  } else {
    auto& error = in.error();
    printf("panic(%d): %s | %s\n",
           error.value(),
           msg,
           error.message().c_str());
    abort();
  }
}

}

// namespace pallet {
// enum class ErrorCode {
  
// }
// }
