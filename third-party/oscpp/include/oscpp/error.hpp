// oscpp library
//
// Copyright (c) 2004-2018 Stefan Kersten <kaoskorobase@gmail.com>
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef OSCPP_ERROR_HPP_INCLUDED
#define OSCPP_ERROR_HPP_INCLUDED

#include <expected>
#include <system_error>
#include <type_traits>

namespace OSCPP {

// Failures are reported as std::error_condition values of the
// category below, so that no function in this library throws.
enum class Error
{
    // A read ran past the end of the buffer.
    Underrun = 1,
    // A write ran past the end of the buffer.
    Overflow,
    // The input is not a valid OSC packet, or has the wrong type.
    Parse,
    // A pointer that must be 4-byte aligned is not.
    Unaligned,
    // The call is invalid for the packet's state (for example
    // closeBundle() without openBundle()) or a size does not fit.
    InvalidArgument
};

inline const std::error_category& errorCategory() noexcept
{
    struct Category final : std::error_category
    {
        const char* name() const noexcept override
        {
            return "oscpp";
        }

        std::string message(int value) const override
        {
            switch (static_cast<Error>(value))
            {
                case Error::Underrun:
                    return "buffer underrun";
                case Error::Overflow:
                    return "buffer overflow";
                case Error::Parse:
                    return "parse error";
                case Error::Unaligned:
                    return "unaligned pointer";
                case Error::InvalidArgument:
                    return "invalid argument";
            }
            return "unknown oscpp error";
        }
    };
    static const Category category;
    return category;
}

inline std::error_condition make_error_condition(Error error) noexcept
{
    return std::error_condition(static_cast<int>(error), errorCategory());
}

} // namespace OSCPP

template <>
struct std::is_error_condition_enum<OSCPP::Error> : std::true_type
{};

namespace OSCPP {

template <class T> using Result = std::expected<T, std::error_condition>;

inline std::unexpected<std::error_condition> fail(Error error) noexcept
{
    return std::unexpected(make_error_condition(error));
}

} // namespace OSCPP

#endif // OSCPP_ERROR_HPP_INCLUDED
