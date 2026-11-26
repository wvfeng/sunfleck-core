#pragma once
#include <sunfleck/ui.h>
#include <string_view>
#include <charconv>

namespace visual_state {
inline bool property(ui_property p) { return p == UI_PROPERTY_VISIBILITY || p == UI_PROPERTY_IS_HIT_TEST_VISIBLE || p == UI_PROPERTY_CLIP_TO_BOUNDS || p == UI_PROPERTY_IS_TAB_STOP || p == UI_PROPERTY_TAB_INDEX || p == UI_PROPERTY_IS_FOCUS_SCOPE; }
inline bool trigger_source(ui_property p) { return p == UI_PROPERTY_IS_ENABLED || p == UI_PROPERTY_VALUE || property(p); }
inline ui_value_kind kind(ui_property p) { return p == UI_PROPERTY_VISIBILITY ? UI_VALUE_ENUM : p == UI_PROPERTY_TAB_INDEX ? UI_VALUE_INT : UI_VALUE_BOOL; }
inline bool valid(ui_property p, const ui_binding_value& v) {
    return property(p) && v.kind == kind(p) && v.integer >= 0 && v.integer <= (p == UI_PROPERTY_VISIBILITY ? 2 : p == UI_PROPERTY_TAB_INDEX ? INT32_MAX : 1);
}
inline const char* name(int64_t value) { return value == UI_HIDDEN ? "Hidden" : value == UI_COLLAPSED ? "Collapsed" : "Visible"; }
inline bool parse(ui_property p, std::string_view text, ui_binding_value& v) {
    v = {}; v.kind = kind(p);
    if (p == UI_PROPERTY_VISIBILITY) {
        if (text == "Visible") v.integer = UI_VISIBLE;
        else if (text == "Hidden") v.integer = UI_HIDDEN;
        else if (text == "Collapsed") v.integer = UI_COLLAPSED;
        else return false;
    } else if (p == UI_PROPERTY_TAB_INDEX) {
        auto parsed = std::from_chars(text.data(),text.data()+text.size(),v.integer);
        return parsed.ec == std::errc{} && parsed.ptr == text.data()+text.size() && valid(p,v);
    } else if (text == "true") v.integer = 1;
    else if (text != "false") return false;
    return true;
}
}
