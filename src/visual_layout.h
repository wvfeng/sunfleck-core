#pragma once
#include "numeric_literal.h"
#include <sunfleck/ui.h>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>
#include <limits>

// Shared by XML, compiled resources and native property updates.
struct VisualLayout {
    double width = -1, min_width = 0, max_width = std::numeric_limits<double>::infinity();
    double min_height = 0, max_height = std::numeric_limits<double>::infinity();
    std::array<double, 4> margin{}; // left, top, right, bottom
    std::string margin_text = "0";
    unsigned horizontal = 0, vertical = 0; // stretch, start, center, end
    bool horizontal_set = false; // Preserve legacy intrinsic button width until explicitly aligned.
    bool operator==(const VisualLayout&) const = default;
};
namespace visual_layout {
inline constexpr const char* names[] = {"Width", "MinWidth", "MaxWidth", "MinHeight", "MaxHeight", "Margin", "HorizontalAlignment", "VerticalAlignment"};
inline constexpr const char* horizontal[] = {"Stretch", "Left", "Center", "Right"};
inline constexpr const char* vertical[] = {"Stretch", "Top", "Center", "Bottom"};
inline bool property(ui_property p) { return p >= UI_PROPERTY_WIDTH && p <= UI_PROPERTY_VERTICAL_ALIGNMENT; }
inline bool numeric(ui_property p) { return p >= UI_PROPERTY_WIDTH && p <= UI_PROPERTY_MAX_HEIGHT; }
inline bool dimension(ui_property p) { return p==UI_PROPERTY_HEIGHT || numeric(p); }
inline constexpr double maximum_extent=10000000;
inline ui_binding_value length(double number) {
    ui_binding_value result{}; result.kind=UI_VALUE_LENGTH;
    result.integer=number<0?UI_LENGTH_AUTO:std::isinf(number)?UI_LENGTH_INFINITY:UI_LENGTH_PIXELS;
    result.number=result.integer==UI_LENGTH_PIXELS?number:0;
    return result;
}
inline bool scalar_metric(ui_property p) { return p==UI_PROPERTY_SPACING || p==UI_PROPERTY_PADDING; }
inline bool valid_metric(ui_property p,double value) {
    return (p==UI_PROPERTY_SPACING && value==-1) || (std::isfinite(value) && value>=0 && value<=64);
}
inline ui_binding_value normalize(ui_property p,ui_binding_value value) {
    if (scalar_metric(p) && value.kind==UI_VALUE_INT) {
        value.kind=UI_VALUE_DOUBLE; value.number=double(value.integer); value.integer=0;
    }
    if (dimension(p) && (value.kind==UI_VALUE_INT || value.kind==UI_VALUE_DOUBLE)) {
        const double number=value.kind==UI_VALUE_INT?double(value.integer):value.number;
        value.kind=UI_VALUE_LENGTH;
        value.integer=number==-1 && (p==UI_PROPERTY_WIDTH || p==UI_PROPERTY_HEIGHT)?UI_LENGTH_AUTO:UI_LENGTH_PIXELS;
        value.number=value.integer==UI_LENGTH_AUTO?0:number;
    }
    return value;
}
inline bool decode(ui_property p,const ui_binding_value& value,double& number) {
    if (value.kind!=UI_VALUE_LENGTH) return false;
    if (value.integer==UI_LENGTH_AUTO && (p==UI_PROPERTY_WIDTH || p==UI_PROPERTY_HEIGHT)) { number=-1; return true; }
    if (value.integer==UI_LENGTH_INFINITY && (p==UI_PROPERTY_MAX_WIDTH || p==UI_PROPERTY_MAX_HEIGHT)) { number=std::numeric_limits<double>::infinity(); return true; }
    if (value.integer!=UI_LENGTH_PIXELS || !std::isfinite(value.number) || value.number<0 || value.number>maximum_extent) return false;
    number=value.number; return true;
}
inline const char* name(ui_property p) { return property(p) ? names[p - UI_PROPERTY_WIDTH] : "Height"; }
inline ui_property find(std::string_view s) {
    for (unsigned i = 0; i < std::size(names); ++i) if (s == names[i]) return ui_property(UI_PROPERTY_WIDTH + i);
    return UI_PROPERTY_TEXT;
}
inline std::string_view trim(std::string_view s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    return begin == s.npos ? std::string_view{} : s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}
inline constexpr const char* stretch_names[] = {"None","Fill","Uniform","UniformToFill"};
inline bool parse_stretch(std::string_view name, ui_stretch& value) {
    for (unsigned i=0; i<std::size(stretch_names); ++i) if (name == stretch_names[i]) { value = ui_stretch(i); return true; }
    return false;
}
inline bool scroll_policy_name(std::string_view name) {
    return name == "HorizontalScrollBarVisibility" || name == "VerticalScrollBarVisibility";
}
inline constexpr const char* scroll_policies[] = {"Disabled", "Auto", "Hidden", "Visible"};
inline bool parse_scroll_policy(std::string_view text, ui_scroll_policy& value) {
    for (unsigned i=0; i<std::size(scroll_policies); ++i) if (text == scroll_policies[i]) { value = ui_scroll_policy(i); return true; }
    return false;
}
inline bool canvas_coordinate(std::string_view name) { return name == "Canvas.Left" || name == "Canvas.Top"; }
inline bool valid_coordinate(double value) { return std::isfinite(value) && std::abs(value) <= maximum_extent; }
inline bool parse_coordinate(std::string_view text, double& value) {
    text = trim(text);
    return numeric_literal::real_literal(text,value) && valid_coordinate(value);
}
inline bool integer(std::string_view text, int& value, int minimum) {
    text = trim(text);
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && value >= minimum && value <= 4096;
}
inline bool set(VisualLayout& layout, ui_property p, const ui_binding_value& value) {
    if (numeric(p)) {
        double number;
        if (!decode(p,value,number)) return false;
        double* fields[] = {&layout.width, &layout.min_width, &layout.max_width, &layout.min_height, &layout.max_height};
        *fields[p - UI_PROPERTY_WIDTH] = number;
        return true;
    }
    if (p == UI_PROPERTY_MARGIN) {
        if (value.kind!=UI_VALUE_THICKNESS || !value.geometry ||
            !std::all_of(value.geometry,value.geometry+4,[](double n){return std::isfinite(n) && n>=-4096 && n<=4096;})) return false;
        std::ostringstream literal; literal.imbue(std::locale::classic()); literal<<std::setprecision(17);
        for (unsigned i=0;i<4;++i) { if(i) literal<<','; literal<<value.geometry[i]; }
        auto text=literal.str();
        std::copy_n(value.geometry,4,layout.margin.begin());
        layout.margin_text=std::move(text);
        return true;
    }
    if (value.kind != UI_VALUE_STRING || !value.string) return false;
    if (p == UI_PROPERTY_HORIZONTAL_ALIGNMENT || p == UI_PROPERTY_VERTICAL_ALIGNMENT) {
        const auto& choices = p == UI_PROPERTY_HORIZONTAL_ALIGNMENT ? horizontal : vertical;
        for (unsigned i = 0; i < 4; ++i) if (value.string == std::string_view(choices[i])) {
            (p == UI_PROPERTY_HORIZONTAL_ALIGNMENT ? layout.horizontal : layout.vertical) = i;
            if (p == UI_PROPERTY_HORIZONTAL_ALIGNMENT) layout.horizontal_set = true;
            return true;
        }
    }
    return false;
}
inline bool literal(VisualLayout& layout, ui_property p, const std::string& text) {
    ui_binding_value value{};
    if (p==UI_PROPERTY_MARGIN) {
        double geometry[4]{};
        if (ui_value_parse(UI_VALUE_THICKNESS,text.c_str(),&value,geometry,4)!=UI_OK || !set(layout,p,value)) return false;
        layout.margin_text=text;
        return true;
    }
    if (numeric(p)) {
        if (ui_value_parse(UI_VALUE_LENGTH,text.c_str(),&value,nullptr,0)!=UI_OK) return false;
    } else { value.kind = UI_VALUE_STRING; value.string = text.c_str(); }
    return set(layout, p, value);
}
inline bool valid(const VisualLayout& layout) {
    const auto finite=[](double n){return std::isfinite(n) && n>=0 && n<=maximum_extent;};
    const auto maximum=[&](double n){return finite(n) || n==std::numeric_limits<double>::infinity();};
    return (layout.width==-1 || finite(layout.width)) && finite(layout.min_width) && finite(layout.min_height) &&
        maximum(layout.max_width) && maximum(layout.max_height) && layout.min_width <= layout.max_width && layout.min_height <= layout.max_height;
}
}
