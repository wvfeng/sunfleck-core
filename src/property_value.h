#pragma once
#include <sunfleck/ui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace property_values {
inline unsigned dimensions(ui_value_kind kind) {
    switch (kind) {
    case UI_VALUE_THICKNESS: case UI_VALUE_CORNER_RADIUS: return 4;
    case UI_VALUE_POINT: case UI_VALUE_SIZE: return 2;
    case UI_VALUE_MATRIX: return 6;
    default: return 0;
    }
}
inline bool equal(const ui_binding_value& left, const ui_binding_value& right) {
    if (left.kind != right.kind) return false;
    if (const auto count = dimensions(left.kind))
        return left.geometry && right.geometry && std::equal(left.geometry, left.geometry + count, right.geometry);
    switch (left.kind) {
    case UI_VALUE_STRING: return left.string && right.string && std::strcmp(left.string, right.string) == 0;
    case UI_VALUE_DOUBLE: return left.number == right.number;
    case UI_VALUE_LENGTH: return left.integer == right.integer && (left.integer != UI_LENGTH_PIXELS || left.number == right.number);
    case UI_VALUE_COMMAND: case UI_VALUE_MODEL: return left.command == right.command;
    default: return left.integer == right.integer;
    }
}
}

// Owns borrowed ABI data. Copy/move always repairs pointers, including small
// string storage. Shared by model reads, component defaults and property layers.
struct BindingValue {
    ui_binding_value value{};
    std::string text;
    std::array<double, 6> geometry{};
    BindingValue() = default;
    BindingValue(const BindingValue& other) : value(other.value), text(other.text), geometry(other.geometry) { repoint(); }
    BindingValue(BindingValue&& other) noexcept : value(other.value), text(std::move(other.text)), geometry(other.geometry) { repoint(); }
    BindingValue& operator=(const BindingValue& other) {
        if (this != &other) { BindingValue copy(other); *this = std::move(copy); }
        return *this;
    }
    BindingValue& operator=(BindingValue&& other) noexcept {
        if (this != &other) { value = other.value; text = std::move(other.text); geometry = other.geometry; repoint(); }
        return *this;
    }
    void repoint() {
        if (value.kind == UI_VALUE_STRING) value.string = text.c_str();
        if (property_values::dimensions(value.kind)) value.geometry = geometry.data();
    }
    bool equals(const BindingValue& other) const {
        if (value.kind != other.value.kind) return false;
        if (auto count = property_values::dimensions(value.kind)) return std::equal(geometry.begin(), geometry.begin() + count, other.geometry.begin());
        switch (value.kind) {
        case UI_VALUE_STRING: return text == other.text;
        case UI_VALUE_DOUBLE: return value.number == other.value.number;
        case UI_VALUE_LENGTH: return value.integer == other.value.integer && (value.integer != UI_LENGTH_PIXELS || value.number == other.value.number);
        case UI_VALUE_COMMAND: case UI_VALUE_MODEL: return value.command == other.value.command;
        default: return value.integer == other.value.integer;
        }
    }
};
