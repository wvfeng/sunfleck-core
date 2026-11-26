#pragma once

#include "utf8_text.h"
#include <charconv>
#include <map>
#include <vector>
#include <variant>

/* A bounded JSON reader for layout documents. No application code or resources execute. */
namespace docking::json {
struct Value {
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;
    std::variant<std::nullptr_t, int64_t, std::string, Object, Array> data;
    bool has(const char* key) const
    {
        const auto* object = std::get_if<Object>(&data); return object && object->contains(key);
    }
    const Value& at(const char* key) const
    {
        const auto* object = std::get_if<Object>(&data);
        if (!object || !object->contains(key)) { throw std::range_error("Missing layout property"); }
        return object->at(key);
    }
    int number(int low, int high) const
    {
        const auto* value = std::get_if<int64_t>(&data);
        if (!value || *value < low || *value > high) { throw std::range_error("Invalid layout number"); }
        return static_cast<int>(*value);
    }
    const std::string& string() const
    {
        const auto* value = std::get_if<std::string>(&data);
        if (!value) { throw std::range_error("Expected layout string"); }
        return *value;
    }
    const Array& array() const
    {
        const auto* value = std::get_if<Array>(&data);
        if (!value) { throw std::range_error("Expected layout array"); }
        return *value;
    }
};
inline std::string quote(std::string_view text)
{
    std::string out = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char ch : text) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += static_cast<char>(ch); }
        else if (ch < 32) { out += "\\u00"; out += hex[ch >> 4]; out += hex[ch & 15]; }
        else { out += static_cast<char>(ch); }
    }
    return out + '"';
}
class Reader {
    std::string_view input;
    size_t offset = 0, count = 0;
    size_t string_limit = 16384;
    [[noreturn]] void fail() const { throw std::range_error("Malformed layout JSON"); }
    void space() { while (offset < input.size() && (input[offset] == ' ' || input[offset] == '\n' || input[offset] == '\r' || input[offset] == '\t')) { ++offset; } }
    bool take(char ch) { space(); if (offset < input.size() && input[offset] == ch) { ++offset; return true; } return false; }
    unsigned hex()
    {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            if (offset == input.size()) { fail(); }
            const char c = input[offset++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) { fail(); }
            value = value * 16 + digit;
        }
        return value;
    }
    std::string text()
    {
        if (!take('"')) { fail(); }
        std::string result;
        bool closed = false;
        while (offset < input.size()) {
            const unsigned char c = input[offset++];
            if (c == '"') { closed = true; break; }
            if (c < 32) { fail(); }
            if (c != '\\') { result += static_cast<char>(c); continue; }
            if (offset == input.size()) { fail(); }
            switch (input[offset++]) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u': {
                unsigned scalar = hex();
                if (scalar >= 0xd800 && scalar <= 0xdbff) {
                    if (input.substr(offset, 2) != "\\u") { fail(); }
                    offset += 2;
                    const unsigned low = hex();
                    if (low < 0xdc00 || low > 0xdfff) { fail(); }
                    scalar = 0x10000 + ((scalar - 0xd800) << 10) + low - 0xdc00;
                }
                if (!utf8_text::scalar(scalar) || scalar == 0) { fail(); }
                if (scalar < 0x80) { result += static_cast<char>(scalar); }
                else {
                    const int tail = scalar < 0x800 ? 1 : scalar < 0x10000 ? 2 : 3;
                    result += static_cast<char>((tail == 1 ? 0xc0 : tail == 2 ? 0xe0 : 0xf0) | (scalar >> (tail * 6)));
                    for (int shift = (tail - 1) * 6; shift >= 0; shift -= 6) { result += static_cast<char>(0x80 | ((scalar >> shift) & 63)); }
                }
                break;
            }
            default: fail();
            }
            if (result.size() > string_limit) { fail(); }
        }
        if (!closed || result.size() > string_limit) { fail(); }
        (void)utf8_text::decode(result);
        return result;
    }
    Value read(unsigned depth)
    {
        if (depth > 128 || ++count > 400000) { fail(); }
        space();
        if (offset == input.size()) { fail(); }
        if (input[offset] == '"') { return Value{text()}; }
        if (take('{')) {
            Value::Object result;
            if (!take('}')) {
                do {
                    auto key = text();
                    if (!take(':')) { fail(); }
                    auto value = read(depth + 1);
                    if (!result.emplace(std::move(key), std::move(value)).second) { fail(); }
                } while (take(','));
                if (!take('}')) { fail(); }
            }
            return Value{std::move(result)};
        }
        if (take('[')) {
            Value::Array result;
            if (!take(']')) {
                do { result.push_back(read(depth + 1)); } while (take(','));
                if (!take(']')) { fail(); }
            }
            return Value{std::move(result)};
        }
        if (input.substr(offset, 4) == "null") { offset += 4; return Value{nullptr}; }
        const auto start = offset;
        if (input[offset] == '-') { ++offset; }
        if (offset == input.size() || input[offset] < '0' || input[offset] > '9') { fail(); }
        if (input[offset] == '0') { ++offset; }
        else { while (offset < input.size() && input[offset] >= '0' && input[offset] <= '9') { ++offset; } }
        int64_t value = 0;
        const auto parsed = std::from_chars(input.data() + start, input.data() + offset, value);
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + offset) { fail(); }
        return Value{value};
    }
public:
    explicit Reader(std::string_view text, size_t max_string = 16384) : input(text), string_limit(max_string) {}
    Value parse()
    {
        if (input.empty() || input.size() > 8 * 1024 * 1024) { fail(); }
        auto result = read(0);
        space();
        if (offset != input.size()) { fail(); }
        return result;
    }
};
}
