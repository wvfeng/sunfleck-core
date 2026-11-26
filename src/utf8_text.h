#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace utf8_text {
inline bool scalar(unsigned value)
{
    return value <= 0x10ffff && (value < 0xd800 || value > 0xdfff);
}

inline std::wstring decode(std::string_view bytes)
{
    std::wstring result;
    for (std::size_t i = 0; i < bytes.size();) {
        const auto first = static_cast<unsigned char>(bytes[i++]);
        if (first < 0x80) {
            result.push_back(first);
            continue;
        }
        if (first < 0xc2 || first > 0xf4) {
            throw std::range_error("Invalid UTF-8 lead byte");
        }
        const unsigned tail = first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
        unsigned value = first & (0x7f >> (tail + 1));
        for (unsigned j = 0; j < tail; ++j) {
            if (i >= bytes.size() || (static_cast<unsigned char>(bytes[i]) & 0xc0) != 0x80) {
                throw std::range_error("Invalid UTF-8 continuation");
            }
            value = (value << 6) | (static_cast<unsigned char>(bytes[i++]) & 0x3f);
        }
        constexpr unsigned minimum[] = {0, 0x80, 0x800, 0x10000};
        if (value < minimum[tail] || !scalar(value)) {
            throw std::range_error("Invalid Unicode scalar");
        }
        if constexpr (sizeof(wchar_t) == 2) {
            if (value > 0xffff) {
                value -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xd800 + (value >> 10)));
                result.push_back(static_cast<wchar_t>(0xdc00 + (value & 0x3ff)));
                continue;
            }
        }
        result.push_back(static_cast<wchar_t>(value));
    }
    return result;
}

inline std::string encode(std::wstring_view text)
{
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned value = static_cast<unsigned>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (value >= 0xd800 && value <= 0xdbff) {
                if (++i == text.size()) { throw std::range_error("Incomplete UTF-16 surrogate pair"); }
                const unsigned low = static_cast<unsigned>(text[i]);
                if (low < 0xdc00 || low > 0xdfff) { throw std::range_error("Invalid UTF-16 surrogate pair"); }
                value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
            }
        }
        if (!scalar(value)) {
            throw std::range_error("Invalid Unicode scalar");
        }
        if (value < 0x80) {
            result.push_back(static_cast<char>(value));
            continue;
        }
        const int tail = value < 0x800 ? 1 : value < 0x10000 ? 2 : 3;
        constexpr unsigned prefixes[] = {0, 0xc0, 0xe0, 0xf0};
        result.push_back(static_cast<char>(prefixes[tail] | (value >> (6 * tail))));
        for (int shift = 6 * (tail - 1); shift >= 0; shift -= 6) {
            result.push_back(static_cast<char>(0x80 | ((value >> shift) & 0x3f)));
        }
    }
    return result;
}
}
