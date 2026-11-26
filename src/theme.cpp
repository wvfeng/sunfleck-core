#include "theme.h"
#include "fleck_schema.h"

#include <charconv>
#include <string_view>

namespace {
std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}

using fleck::schema::colors;
using fleck::schema::palette_metrics;

bool parse_number(std::string_view text, std::uint32_t& number, int base)
{
    if (text.empty()) {
        return false;
    }
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, number, base);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool apply(ui_theme& theme, std::string_view name, std::string_view value)
{
    for (std::size_t i = 0; i < std::size(colors); ++i) {
        if (name == colors[i]) {
            return value.size() == 7 && value.front() == '#' &&
                parse_number(value.substr(1), theme.colors[i], 16);
        }
    }
    for (const auto& metric : palette_metrics) {
        if (name == metric.name) {
            auto& field = theme.*metric.field;
            return parse_number(value, field, 10) && field >= metric.low && field <= metric.high;
        }
    }
    return false;
}
}

ui_result ui_theme_apply_overrides(ui_theme* theme, const char* resource)
{
    if (!theme || theme->struct_size < sizeof(ui_theme) || !resource) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    std::string_view input(resource);
    if (input.size() > 4096) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto staged = *theme;
    while (!input.empty()) {
        const auto newline = input.find('\n');
        const auto line = trim(input.substr(0, newline));
        if (!line.empty()) {
            const auto equals = line.find('=');
            if (equals == std::string_view::npos ||
                !apply(staged, trim(line.substr(0, equals)), trim(line.substr(equals + 1)))) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
        }
        input = newline == std::string_view::npos ? std::string_view{} : input.substr(newline + 1);
    }
    *theme = staged;
    return UI_OK;
}

const char* ui_builtin_theme_name(ui_builtin_theme theme)
{
    const auto index = static_cast<unsigned>(theme);
    return index < UI_BUILTIN_THEME_COUNT ? builtin_themes[index].name : nullptr;
}
