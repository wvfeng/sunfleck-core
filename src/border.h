#pragma once
#include <sunfleck/ui.h>
#include <array>
#include <algorithm>
#include <cmath>
#include "raster.h"

struct BorderStyle {
    uint32_t background = 0, brush = 0xff000000;
    std::array<double, 4> thickness{}, radius{};
};
namespace border {
void paint(const BorderStyle& style, layout::Rect area, layout::Rect clip, unsigned dpi, std::vector<FillRectCommand>& commands);
inline bool property(ui_property p) { return p >= UI_PROPERTY_BACKGROUND && p <= UI_PROPERTY_CORNER_RADIUS; }
inline ui_value_kind kind(ui_property p) {
    return p <= UI_PROPERTY_BORDER_BRUSH ? UI_VALUE_COLOR : p == UI_PROPERTY_BORDER_THICKNESS ? UI_VALUE_THICKNESS : UI_VALUE_CORNER_RADIUS;
}
inline bool valid(ui_property p, const ui_binding_value& v) {
    if (!property(p) || v.kind != kind(p)) return false;
    if (v.kind == UI_VALUE_COLOR) return v.integer >= 0 && uint64_t(v.integer) <= UINT32_MAX;
    return v.geometry && std::all_of(v.geometry, v.geometry + 4, [](double n) { return std::isfinite(n) && n >= 0 && n <= 4096; });
}
inline ui_binding_value read(const BorderStyle& s, ui_property p) {
    ui_binding_value value{}; value.kind = kind(p);
    if (p <= UI_PROPERTY_BORDER_BRUSH) value.integer = p == UI_PROPERTY_BACKGROUND ? s.background : s.brush;
    else value.geometry = p == UI_PROPERTY_BORDER_THICKNESS ? s.thickness.data() : s.radius.data();
    return value;
}
inline void set(BorderStyle& s, ui_property p, const ui_binding_value& v) {
    if (p == UI_PROPERTY_BACKGROUND) s.background = uint32_t(v.integer);
    else if (p == UI_PROPERTY_BORDER_BRUSH) s.brush = uint32_t(v.integer);
    else std::copy_n(v.geometry, 4, (p == UI_PROPERTY_BORDER_THICKNESS ? s.thickness : s.radius).begin());
}
}
