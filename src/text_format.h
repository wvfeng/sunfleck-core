#pragma once
#include <sunfleck/ui.h>
#include <string>
#include <string_view>
#include <cmath>
#include "font_resources.h"

struct TextFormat {
    windows_text::FontResources resources;
    std::string family = "Microsoft YaHei UI";
    double size = 14;
    int64_t weight = 400, style = 0, stretch = 5;
    int64_t alignment = 0, wrapping = 1, trimming = 0, decorations = 0, flow = 0;
    double line_height = 0;
    uint32_t foreground = 0xff000000;
    bool foreground_set = false, trimming_set = false;
    bool operator==(const TextFormat&) const = default;
    bool same_metrics(const TextFormat& other) const {
        return resources==other.resources && family==other.family && size==other.size && weight==other.weight && style==other.style && stretch==other.stretch &&
            alignment==other.alignment && wrapping==other.wrapping && trimming==other.trimming && trimming_set==other.trimming_set &&
            line_height==other.line_height && flow==other.flow;
    }
};
inline const TextFormat& default_text_format() { static const TextFormat value; return value; }
inline thread_local const TextFormat* current_text_format = nullptr;
inline const TextFormat& active_text_format() { return current_text_format ? *current_text_format : default_text_format(); }
struct TextFormatScope {
    const TextFormat* previous;
    explicit TextFormatScope(const TextFormat& value):previous(current_text_format) { current_text_format=&value; }
    ~TextFormatScope() { current_text_format=previous; }
};
struct ui_node;
namespace text_format {
inline bool property(ui_property p) { return p >= UI_PROPERTY_FONT_FAMILY && p <= UI_PROPERTY_FLOW_DIRECTION; }
inline bool inherits(ui_property p) { return p != UI_PROPERTY_TEXT_WRAPPING && p != UI_PROPERTY_TEXT_TRIMMING; }
inline ui_value_kind kind(ui_property p) { return p == UI_PROPERTY_FONT_FAMILY ? UI_VALUE_STRING : p == UI_PROPERTY_FOREGROUND ? UI_VALUE_COLOR : p == UI_PROPERTY_FONT_SIZE || p == UI_PROPERTY_LINE_HEIGHT ? UI_VALUE_DOUBLE : UI_VALUE_ENUM; }
inline bool valid(ui_property p,const ui_binding_value& v) {
    if (!property(p) || v.kind!=kind(p)) return false;
    if (p==UI_PROPERTY_FONT_FAMILY) return v.string && *v.string && std::string_view(v.string).size()<=256;
    if (p==UI_PROPERTY_FONT_SIZE) return std::isfinite(v.number) && v.number>0 && v.number<=4096;
    if (p==UI_PROPERTY_LINE_HEIGHT) return std::isfinite(v.number) && v.number>=0 && v.number<=4096;
    if (p==UI_PROPERTY_FOREGROUND) return v.integer>=0 && uint64_t(v.integer)<=UINT32_MAX;
    if (p==UI_PROPERTY_TEXT_ALIGNMENT || p==UI_PROPERTY_TEXT_DECORATIONS) return v.integer>=0 && v.integer<=3;
    if (p==UI_PROPERTY_TEXT_WRAPPING || p==UI_PROPERTY_FLOW_DIRECTION) return v.integer>=0 && v.integer<=1;
    if (p==UI_PROPERTY_TEXT_TRIMMING) return v.integer>=0 && v.integer<=2;
    if (p==UI_PROPERTY_FONT_WEIGHT) return v.integer>=1 && v.integer<=999;
    if (p==UI_PROPERTY_FONT_STYLE) return v.integer>=0 && v.integer<=2;
    return v.integer>=1 && v.integer<=9;
}
inline ui_binding_value read(const TextFormat& f,ui_property p) {
    ui_binding_value v{}; v.kind=kind(p);
    if (p==UI_PROPERTY_FONT_FAMILY) v.string=f.family.c_str();
    else if (p==UI_PROPERTY_FONT_SIZE) v.number=f.size;
    else if (p==UI_PROPERTY_LINE_HEIGHT) v.number=f.line_height;
    else if (p==UI_PROPERTY_FOREGROUND) v.integer=f.foreground;
    else if (p==UI_PROPERTY_TEXT_ALIGNMENT) v.integer=f.alignment;
    else if (p==UI_PROPERTY_TEXT_WRAPPING) v.integer=f.wrapping;
    else if (p==UI_PROPERTY_TEXT_TRIMMING) v.integer=f.trimming;
    else if (p==UI_PROPERTY_TEXT_DECORATIONS) v.integer=f.decorations;
    else if (p==UI_PROPERTY_FLOW_DIRECTION) v.integer=f.flow;
    else v.integer=p==UI_PROPERTY_FONT_WEIGHT?f.weight:p==UI_PROPERTY_FONT_STYLE?f.style:f.stretch;
    return v;
}
inline void set(TextFormat& f,ui_property p,const ui_binding_value& v) {
    if (p==UI_PROPERTY_FONT_FAMILY) f.family=v.string;
    else if (p==UI_PROPERTY_FONT_SIZE) f.size=v.number;
    else if (p==UI_PROPERTY_LINE_HEIGHT) f.line_height=v.number;
    else if (p==UI_PROPERTY_FOREGROUND) { f.foreground=uint32_t(v.integer); f.foreground_set=true; }
    else if (p==UI_PROPERTY_TEXT_ALIGNMENT) f.alignment=v.integer;
    else if (p==UI_PROPERTY_TEXT_WRAPPING) f.wrapping=v.integer;
    else if (p==UI_PROPERTY_TEXT_TRIMMING) { f.trimming=v.integer; f.trimming_set=true; }
    else if (p==UI_PROPERTY_TEXT_DECORATIONS) f.decorations=v.integer;
    else if (p==UI_PROPERTY_FLOW_DIRECTION) f.flow=v.integer;
    else if (p==UI_PROPERTY_FONT_WEIGHT) f.weight=v.integer;
    else if (p==UI_PROPERTY_FONT_STYLE) f.style=v.integer;
    else f.stretch=v.integer;
}
const ui_node* owner(const ui_node& node,ui_property property,bool parent_only=false);
TextFormat effective(const ui_node& node);
bool parse(ui_property property,const std::string& text,ui_binding_value& value);
std::string literal(const TextFormat& format,ui_property property);
}
