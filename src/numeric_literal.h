#pragma once
#include <cmath>
#include <string>
#include <string_view>
#include <new>
#include <charconv>
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
#include <cstdlib>
#include <locale.h>
#endif
#ifdef __APPLE__
#include <xlocale.h>
#endif

namespace numeric_literal {
inline bool real_literal(std::string_view part, double& value) {
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
    // Xcode 16.4 and the supported Emscripten libc++ lack floating from_chars.
    // Keep the same decimal grammar and use an explicit C numeric locale,
    // without changing the application's process/thread locale.
    size_t at = 0;
    if (at < part.size() && part[at] == '-') ++at;
    size_t digits = 0;
    bool nonzero = false;
    const auto consume_digits = [&] {
        while (at < part.size() && part[at] >= '0' && part[at] <= '9') {
            nonzero |= part[at] != '0'; ++at; ++digits;
        }
    };
    consume_digits();
    if (at < part.size() && part[at] == '.') { ++at; consume_digits(); }
    if (!digits) return false;
    const bool nonzero_mantissa = nonzero;
    if (at < part.size() && (part[at] == 'e' || part[at] == 'E')) {
        ++at;
        if (at < part.size() && (part[at] == '+' || part[at] == '-')) ++at;
        digits = 0; consume_digits();
        if (!digits) return false;
    }
    if (at != part.size()) return false;
    const std::string terminated(part);
    const auto locale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
    if (!locale) throw std::bad_alloc();
    char* end = nullptr;
    const double parsed = strtod_l(terminated.c_str(), &end, locale);
    freelocale(locale);
    // ERANGE also covers representable subnormals; accept those, but reject
    // underflow to zero, matching from_chars' result_out_of_range behavior.
    if (end != terminated.data() + terminated.size() || !std::isfinite(parsed) ||
        (parsed == 0 && nonzero_mantissa)) return false;
    value = parsed;
    return true;
#else
    const auto result = std::from_chars(part.data(), part.data() + part.size(), value);
    return result.ec == std::errc{} && result.ptr == part.data() + part.size() && std::isfinite(value);
#endif
}
}

