#pragma once
#include "visual_layout.h"
#include "brush.h"
#include "timeline.h"
#include <sunfleck/ui.h>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include "devtools.h"

namespace fleck {
struct Conversion {
    std::string converter;
    std::vector<std::string> inputs;
};
bool parse_conversion(std::string_view encoded, Conversion& conversion);
std::string conversion_path(std::string_view encoded);
struct Binding {
    ui_property property = UI_PROPERTY_TEXT;
    std::string path, format;
    ui_binding_mode mode = UI_BIND_DEFAULT;
    ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED;
    enum Source { Context, Self, TemplatedParent, NamedElement, Ancestor } source = Context;
    std::string target;
};
struct Element {
    std::string type_namespace, type_name, template_namespace, template_name, source_uri;
    std::map<std::string, std::string> properties;
    ui_node_kind kind = UI_STACK_PANEL;
    std::string text;
    std::string id;
    std::string anchor;
    bool async_command = false;
    std::vector<Binding> bindings;
    bool enabled = true;
    unsigned value = 0;
    double height = -1;
    VisualLayout layout;
    unsigned weight = 1;
    ui_dock_edge dock = UI_DOCK_LEFT;
    unsigned dock_capabilities = UI_DOCK_DEFAULT_CAPABILITIES;
    unsigned dock_min_width = 96, dock_min_height = 64;
    std::string dock_icon;
    bool has_value = false;
    double spacing = -1;
    double padding = 0;
    std::size_t offset = 0;
    devtools::Location location;
    std::vector<Element> children;
    // Expansion keeps the caller declaration for later template replacement.
    std::vector<Element> supplied_content;
    bool projected = false;
};
inline bool range_template(ui_node_kind kind) {
    return kind == UI_SLIDER || kind == UI_SPIN_BOX || kind == UI_PROGRESS_BAR;
}
inline bool supports_template(ui_node_kind kind) {
    return kind == UI_BUTTON || kind == UI_ICON_BUTTON || kind == UI_HYPERLINK ||
        kind == UI_LIST_ITEM || range_template(kind) || devtools::boolean_target(kind);
}
inline bool native_template(const Element& node) {
    return node.type_name.empty() && !node.template_name.empty() &&
        supports_template(node.kind);
}
struct Page {
    std::vector<Page> resources;
    std::string component_manifest;
    std::string document_kind = "Window", class_name, type_namespace, target_namespace, target_name, source_uri;
    std::string title = "Sunfleck";
    std::string model;
    int width = 400;
    int height = 240;
    Element root;
};
bool read_drawing(std::string_view xml,drawing::Brush* brush,ui_effect_desc* effect);
bool trigger_setter(ui_node_kind kind, std::string_view key, const std::string& literal,
                    unsigned& order, std::map<ui_property, unsigned>& conditions, ui_property& property, BindingValue& value);
bool dynamic_style_target(ui_node_kind kind, std::string_view name, std::string_view key, ui_property& property);
bool style_value(ui_node_kind kind, std::string_view name, const std::string& literal, ui_property& property, BindingValue& value);
bool read_storyboard(std::string_view xml,ui_node_kind kind,std::vector<animation::Track>& tracks);
bool valid_binding_name(std::string_view name);
using DictionarySources = std::map<std::string,std::string,std::less<>>;
using PointerBindingKey = std::pair<ui_pointer_action,ui_event_phase>;
std::optional<PointerBindingKey> pointer_binding(std::string_view action,std::string_view phase);
std::optional<PointerBindingKey> pointer_binding(std::string_view property);
// Keep legacy bubble gesture codes stable in compiled pages.
inline unsigned gesture_code(unsigned key, unsigned modifiers, ui_event_phase phase) {
    const unsigned route = phase == UI_EVENT_PREVIEW ? 1 : phase == UI_EVENT_DIRECT ? 2 : 0;
    return (route << 11) | (modifiers << 8) | key;
}
std::optional<unsigned> key_binding_code(std::string_view property);
bool key_gesture(std::string_view text,unsigned& key,unsigned& modifiers);
bool valid_gesture(unsigned key, unsigned modifiers);
bool is_dictionary(std::string_view input);
bool parse(std::string_view input, Page& page, ui_fleck_diagnostic& diagnostic,
           const DictionarySources& dictionaries = {}, std::string_view source = {});
ui_result can_load(ui_app* app);
ui_result install(ui_app* app, Page&& page, std::string_view input, ui_fleck_diagnostic& diagnostic,
                  bool resolve_bindings = true, const ui_fleck_data_context* context = nullptr, ui_model* model = nullptr);
std::string compile_resource(const Page& page);
bool has_components(const Page& page);
std::string compile_components(const Page& page);
Page read_components(std::string_view bytes);
ui_result check_resource_version(std::string_view bytes, ui_fleck_diagnostic& diagnostic);
ui_result install_components(ui_app* app, Page page, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic& diagnostic);
std::string source_map(const Page& page, std::string_view uri);
std::string fingerprint(const Page& page);
uint64_t resource_fingerprint(const Page& page);
void report(std::string_view input, std::size_t offset, ui_fleck_error code,
            const char* message, ui_fleck_diagnostic& diagnostic);
}
