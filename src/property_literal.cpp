#include "binding.h"
#include "numeric_literal.h"

ui_result binding_parse_value(std::string_view text, ui_value_kind kind, BindingValue& output) {
    BindingValue parsed; parsed.value.kind = kind;
    auto real = [](std::string_view part, double& value) {
        return numeric_literal::real_literal(part, value);
    };
    auto integral = [](std::string_view part, int64_t& value, int base = 10) {
        const auto result = std::from_chars(part.data(), part.data() + part.size(), value, base);
        return result.ec == std::errc{} && result.ptr == part.data() + part.size();
    };
    if (auto count = property_values::dimensions(kind)) {
        unsigned n = 0;
        while (!text.empty() && n < count) {
            auto comma = text.find(',');
            auto part = text.substr(0, comma);
            while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
            while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
            if (!real(part, parsed.geometry[n++])) return UI_ERROR_INVALID_ARGUMENT;
            if (comma == text.npos) { text = {}; break; }
            text.remove_prefix(comma + 1);
            if (text.empty()) return UI_ERROR_INVALID_ARGUMENT;
        }
        if (!text.empty()) return UI_ERROR_INVALID_ARGUMENT;
        if (n == 1 && (kind == UI_VALUE_THICKNESS || kind == UI_VALUE_CORNER_RADIUS)) parsed.geometry.fill(parsed.geometry[0]);
        else if (n == 2 && kind == UI_VALUE_THICKNESS) { parsed.geometry[2] = parsed.geometry[0]; parsed.geometry[3] = parsed.geometry[1]; }
        else if (n != count) return UI_ERROR_INVALID_ARGUMENT;
    } else switch (kind) {
    case UI_VALUE_STRING: parsed.text = text; break;
    case UI_VALUE_BOOL:
        if (text != "true" && text != "false") return UI_ERROR_INVALID_ARGUMENT;
        parsed.value.integer = text == "true"; break;
    case UI_VALUE_INT: case UI_VALUE_ENUM:
        if (!integral(text, parsed.value.integer)) return UI_ERROR_INVALID_ARGUMENT;
        break;
    case UI_VALUE_DOUBLE:
        if (!real(text, parsed.value.number)) return UI_ERROR_INVALID_ARGUMENT;
        break;
    case UI_VALUE_COLOR:
        if ((text.size() != 7 && text.size() != 9) || text.front() != '#' || !integral(text.substr(1), parsed.value.integer, 16)) return UI_ERROR_INVALID_ARGUMENT;
        if (text.size() == 7) parsed.value.integer |= 0xff000000;
        break;
    case UI_VALUE_LENGTH:
        if (text == "Auto") parsed.value.integer = UI_LENGTH_AUTO;
        else if (text == "Infinity") parsed.value.integer = UI_LENGTH_INFINITY;
        else if (!real(text, parsed.value.number)) return UI_ERROR_INVALID_ARGUMENT;
        break;
    default: return UI_ERROR_INVALID_ARGUMENT;
    }
    parsed.repoint();
    return binding_copy_value(parsed.value, output, kind);
}
