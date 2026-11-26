#include "text_format.h"
#include "ui_tree.h"
#include <charconv>
#include <cstdio>
#include "utf8_text.h"

namespace text_format {
namespace {
struct Name { const char* name; int value; };
constexpr Name weights[]={{"Thin",100},{"ExtraLight",200},{"Light",300},{"SemiLight",350},{"Normal",400},{"Medium",500},{"SemiBold",600},{"Bold",700},{"ExtraBold",800},{"Black",900},{"ExtraBlack",950}};
constexpr Name styles[]={{"Normal",0},{"Oblique",1},{"Italic",2}};
constexpr Name stretches[]={{"UltraCondensed",1},{"ExtraCondensed",2},{"Condensed",3},{"SemiCondensed",4},{"Normal",5},{"SemiExpanded",6},{"Expanded",7},{"ExtraExpanded",8},{"UltraExpanded",9}};
constexpr Name alignments[]={{"Left",0},{"Center",1},{"Right",2},{"Justify",3}};
constexpr Name wrappings[]={{"NoWrap",0},{"Wrap",1}};
constexpr Name trimmings[]={{"None",0},{"CharacterEllipsis",1},{"WordEllipsis",2}};
constexpr Name decorations[]={{"None",0},{"Underline",1},{"Strikethrough",2},{"Underline,Strikethrough",3}};
constexpr Name flows[]={{"LeftToRight",0},{"RightToLeft",1}};
std::span<const Name> names(ui_property p) {
    switch(p) {
    case UI_PROPERTY_FONT_WEIGHT:return weights; case UI_PROPERTY_FONT_STYLE:return styles; case UI_PROPERTY_FONT_STRETCH:return stretches;
    case UI_PROPERTY_TEXT_ALIGNMENT:return alignments; case UI_PROPERTY_TEXT_WRAPPING:return wrappings; case UI_PROPERTY_TEXT_TRIMMING:return trimmings;
    case UI_PROPERTY_TEXT_DECORATIONS:return decorations; case UI_PROPERTY_FLOW_DIRECTION:return flows; default:return {};
    }
}
}
const ui_node* owner(const ui_node& node,ui_property property,bool parent_only) {
    if (parent_only && !inherits(property)) return nullptr;
    for (auto* n=parent_only?node.inheritance_parent():&node;n;n=inherits(property)?n->inheritance_parent():nullptr) {
        const auto found=n->property_slots.find(property);
        if (n->applying_property || (n->local_properties & (uint64_t{1} << property)) || (found!=n->property_slots.end() && !found->second.values.empty())) return n;
    }
    return nullptr;
}
TextFormat effective(const ui_node& node) {
    TextFormat result;
    result.resources=windows_text::app_fonts(node.owner);
    for (int p=UI_PROPERTY_FONT_FAMILY;p<=UI_PROPERTY_FLOW_DIRECTION;++p) {
        const auto property=ui_property(p);
        if (const auto* n=owner(node,property)) set(result,property,read(n->font,property));
    }
    return result;
}
bool parse(ui_property p,const std::string& text,ui_binding_value& value) {
    value={}; value.kind=kind(p);
    if (p==UI_PROPERTY_FONT_FAMILY) {
        try { (void)utf8_text::decode(text); } catch(const std::range_error&) { return false; }
        value.string=text.data(); return !text.empty() && text.size()<=256 && text.find('\0')==text.npos;
    }
    if (p==UI_PROPERTY_FOREGROUND) {
        BindingValue parsed;
        if (binding_parse_value(text,UI_VALUE_COLOR,parsed)!=UI_OK) return false;
        value=parsed.value; return true;
    }
    if (p==UI_PROPERTY_FONT_SIZE || p==UI_PROPERTY_LINE_HEIGHT) {
        BindingValue parsed;
        if (binding_parse_value(text,UI_VALUE_DOUBLE,parsed)!=UI_OK || !valid(p,parsed.value)) return false;
        value=parsed.value; return true;
    }
    for (const auto& entry:names(p)) if (entry.name==text) { value.integer=entry.value; return true; }
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value.integer);
    return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && valid(p,value);
}
std::string literal(const TextFormat& f,ui_property p) {
    if (p==UI_PROPERTY_FONT_FAMILY) return f.family;
    if (p==UI_PROPERTY_FONT_SIZE) return std::to_string(f.size);
    if (p==UI_PROPERTY_LINE_HEIGHT) return std::to_string(f.line_height);
    if (p==UI_PROPERTY_FOREGROUND) { char buffer[10]; std::snprintf(buffer,sizeof(buffer),"#%08X",f.foreground); return buffer; }
    const auto v=read(f,p).integer;
    for (const auto& entry:names(p)) if (entry.value==v) return entry.name;
    return std::to_string(v);
}
}
