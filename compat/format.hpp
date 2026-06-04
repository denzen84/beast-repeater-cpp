#pragma once

// Compatibility header: provides std::format for compilers without <format>.
//
// GCC 13+ / Clang 14+ with libc++: use the native implementation.
// GCC 12 and earlier:               use a minimal polyfill that handles
//                                   only the basic {} placeholder syntax
//                                   used throughout this project.

#if __has_include(<format>)
#  include <format>

#else  // ── Polyfill ────────────────────────────────────────────────────────

#include <sstream>
#include <string>
#include <string_view>

namespace std {

namespace _compat_fmt {

// Base case: no more arguments, write the rest of the format string.
inline void _apply(std::ostringstream& os, std::string_view s) {
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// Recursive case: locate next {}, write the prefix, format one argument,
// then recurse on the remainder of the format string.
template<typename T, typename... Ts>
void _apply(std::ostringstream& os, std::string_view s, T&& arg, Ts&&... rest) {
    const auto pos = s.find("{}");
    if (pos == std::string_view::npos) {
        os.write(s.data(), static_cast<std::streamsize>(s.size()));
        return;
    }
    os.write(s.data(), static_cast<std::streamsize>(pos));
    os << std::forward<T>(arg);
    _apply(os, s.substr(pos + 2), std::forward<Ts>(rest)...);
}

} // namespace _compat_fmt

// Minimal std::format polyfill.
// Supports {} positional placeholders; does not support format specifiers.
template<typename... Args>
std::string format(std::string_view fmt_str, Args&&... args) {
    std::ostringstream oss;
    _compat_fmt::_apply(oss, fmt_str, std::forward<Args>(args)...);
    return oss.str();
}

} // namespace std

#endif // __has_include(<format>)
