#include "property.h"
#include "ui_tree.h"
#include "utf8_text.h"
#include <algorithm>
#include <limits>

namespace {
constexpr const char* names[] = {"Text", "Content", "IsEnabled", "Value", "Command", "Height", "Width",
    "MinWidth", "MaxWidth", "MinHeight", "MaxHeight", "Margin", "HorizontalAlignment", "VerticalAlignment", "Spacing", "Padding", "Weight", "DataContext", "Background", "BorderBrush", "BorderThickness", "CornerRadius", "Visibility", "IsHitTestVisible", "FontFamily", "FontSize", "FontWeight", "FontStyle", "FontStretch", "Foreground", "TextAlignment", "TextWrapping", "TextTrimming", "LineHeight", "TextDecorations", "FlowDirection", "ClipToBounds", "Opacity", "RenderTransform", "RenderTransformOrigin", "LayoutTransform", "IsTabStop", "TabIndex", "IsFocusScope", "ItemsSource", "Source"};
static_assert(std::size(names) == properties::count);
ui_binding_value floating(double number) { ui_binding_value value{}; value.kind=UI_VALUE_DOUBLE; value.number=number; return value; }
ui_binding_value integer(int64_t number) { ui_binding_value value{}; value.kind = UI_VALUE_INT; value.integer = number; return value; }
ui_binding_value string(const char* text) { ui_binding_value value{}; value.kind = UI_VALUE_STRING; value.string = text; return value; }
ui_result describe_node(const ui_node* node, ui_property property, ui_property_descriptor& descriptor) {
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    if (auto result = binding_app_check(node->owner); result != UI_OK) return result;
    descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
    return ui_property_describe(node->kind, property, &descriptor);
}
}

bool properties::valid(ui_property property) { return property >= UI_PROPERTY_TEXT && property <= UI_PROPERTY_SOURCE; }
const char* properties::name(ui_property property) { return valid(property) ? names[property] : "Unknown"; }
bool properties::find(std::string_view name, ui_property& property) {
    for (unsigned i = 0; i < count; ++i) if (name == names[i]) { property = ui_property(i); return true; }
    return false;
}
void properties::written(ui_node& node, ui_property property) {
    if (!node.applying_property && valid(property)) node.local_properties |= uint64_t{1} << canonical(property);
}
void properties::initialize(ui_node& node) {
    // Resources retain explicit declarations, including explicit defaults.
    // Native construction can also initialize fields without source locations;
    // capture its nondefault values once, never on subsequent reads.
    for (const auto& attribute : node.location.attributes) {
        ui_property property;
        if (find(attribute.name, property)) written(node, property);
    }
    if (!node.text.empty()) written(node, UI_PROPERTY_TEXT);
    if (!node.enabled) written(node, UI_PROPERTY_IS_ENABLED);
    if (node.height != -1) written(node, UI_PROPERTY_HEIGHT);
    if (node.value != ((node.kind == UI_SPLIT_VIEW || node.kind == UI_DOCK_SPLIT) ? 50u : 0u)) written(node, UI_PROPERTY_VALUE);
    const auto& layout = node.layout;
    const double values[] = {layout.width, layout.min_width, layout.max_width, layout.min_height, layout.max_height};
    const double defaults[] = {-1, 0, std::numeric_limits<double>::infinity(), 0, std::numeric_limits<double>::infinity()};
    for (unsigned i = 0; i < std::size(values); ++i) if (values[i] != defaults[i]) written(node, ui_property(UI_PROPERTY_WIDTH + i));
    if (layout.margin != std::array<double, 4>{}) written(node, UI_PROPERTY_MARGIN);
    if (layout.horizontal_set) written(node, UI_PROPERTY_HORIZONTAL_ALIGNMENT);
    if (layout.vertical) written(node, UI_PROPERTY_VERTICAL_ALIGNMENT);
    if (node.spacing != -1) written(node, UI_PROPERTY_SPACING);
    if (node.padding != 0) written(node, UI_PROPERTY_PADDING);
    if (node.weight != 1) written(node, UI_PROPERTY_WEIGHT);
    if (node.has_local_data_context) written(node, UI_PROPERTY_DATA_CONTEXT);
}
const char* properties::source_name(ui_property_source source) {
    switch (source) {
    case UI_PROPERTY_SOURCE_LOCAL: return "Local";
    case UI_PROPERTY_SOURCE_BINDING: return "Binding";
    case UI_PROPERTY_SOURCE_INHERITED: return "Inherited";
    case UI_PROPERTY_SOURCE_THEME: return "Theme";
    case UI_PROPERTY_SOURCE_STYLE: return "Style";
    case UI_PROPERTY_SOURCE_TRIGGER: return "Trigger";
    case UI_PROPERTY_SOURCE_TEMPLATE: return "Template";
    case UI_PROPERTY_SOURCE_ANIMATION: return "Animation";
    default: return "Default";
    }
}

ui_result ui_property_describe(ui_node_kind kind, ui_property property, ui_property_descriptor* descriptor) {
    if (!descriptor || descriptor->struct_size != sizeof(*descriptor) || descriptor->version != 1 ||
        kind < UI_STACK_PANEL || kind > UI_PATH || !properties::valid(property) ||
        (!devtools::attribute_allowed(kind, names[property]) && property != UI_PROPERTY_DATA_CONTEXT && !(property == UI_PROPERTY_COMMAND && devtools::indexed(kind)))) return UI_ERROR_INVALID_ARGUMENT;
    ui_property_descriptor result{};
    result.struct_size = sizeof(result); result.version = 1; result.property = property; result.name = names[property];
    result.flags = UI_PROPERTY_BINDABLE;
    if (property == UI_PROPERTY_OPACITY) {
        result.kind = UI_VALUE_DOUBLE;
        result.default_value = floating(1);
        result.flags |= UI_PROPERTY_ANIMATABLE | UI_PROPERTY_AFFECTS_RENDER;
        *descriptor = result; return UI_OK;
    }
    if (property == UI_PROPERTY_LAYOUT_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN) {
        static constexpr double identity[] = {1,0,0,1,0,0}, zero[] = {0,0};
        const bool matrix = property != UI_PROPERTY_RENDER_TRANSFORM_ORIGIN;
        if (property == UI_PROPERTY_LAYOUT_TRANSFORM) result.flags |= UI_PROPERTY_AFFECTS_MEASURE;
        result.kind = result.default_value.kind = matrix ? UI_VALUE_MATRIX : UI_VALUE_POINT;
        result.default_value.geometry = matrix ? identity : zero;
        result.flags |= UI_PROPERTY_ANIMATABLE | UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT;
        *descriptor = result; return UI_OK;
    }
    if (border::property(property)) {
        static const double zero[4]{};
        result.default_value = border::read(BorderStyle{}, property);
        if (property >= UI_PROPERTY_BORDER_THICKNESS) result.default_value.geometry = zero;
        result.flags |= UI_PROPERTY_ANIMATABLE | UI_PROPERTY_AFFECTS_RENDER;
        if (property == UI_PROPERTY_BORDER_THICKNESS) result.flags |= UI_PROPERTY_AFFECTS_MEASURE;
        result.kind = result.default_value.kind; *descriptor = result; return UI_OK;
    }
    if (text_format::property(property)) {
        result.kind = text_format::kind(property); result.default_value = text_format::read(default_text_format(),property);
        result.flags |= UI_PROPERTY_AFFECTS_RENDER;
        if (text_format::inherits(property)) result.flags |= UI_PROPERTY_INHERITS;
        if (property != UI_PROPERTY_FOREGROUND && property != UI_PROPERTY_TEXT_DECORATIONS) result.flags |= UI_PROPERTY_AFFECTS_MEASURE;
        if (property == UI_PROPERTY_FONT_SIZE || property == UI_PROPERTY_LINE_HEIGHT || property == UI_PROPERTY_FOREGROUND) result.flags |= UI_PROPERTY_ANIMATABLE;
        *descriptor = result; return UI_OK;
    }
    if (visual_state::property(property)) {
        result.kind = result.default_value.kind = visual_state::kind(property);
        result.default_value.integer = property == UI_PROPERTY_VISIBILITY ? UI_VISIBLE : property == UI_PROPERTY_TAB_INDEX ? INT32_MAX : property == UI_PROPERTY_IS_FOCUS_SCOPE ? 0 : 1;
        result.flags |= UI_PROPERTY_AFFECTS_INPUT;
        if (property == UI_PROPERTY_CLIP_TO_BOUNDS) result.flags |= UI_PROPERTY_AFFECTS_RENDER;
        if (property == UI_PROPERTY_VISIBILITY) result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_AFFECTS_RENDER;
        *descriptor = result; return UI_OK;
    }
    switch (property) {
    case UI_PROPERTY_SOURCE:
        result.default_value = string("");
        result.flags |= UI_PROPERTY_AFFECTS_RENDER;
        break;
    case UI_PROPERTY_ITEMS_SOURCE:
        result.default_value.kind = UI_VALUE_MODEL;
        result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_AFFECTS_RENDER;
        break;
    case UI_PROPERTY_DATA_CONTEXT:
        result.default_value.kind = UI_VALUE_MODEL;
        result.flags = UI_PROPERTY_INHERITS | UI_PROPERTY_BINDABLE;
        break;
    case UI_PROPERTY_TEXT: case UI_PROPERTY_CONTENT:
        result.default_value = string("");
        result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_AFFECTS_RENDER;
        if (property == UI_PROPERTY_TEXT && is_editor(kind)) result.flags |= UI_PROPERTY_TWO_WAY | UI_PROPERTY_AFFECTS_INPUT;
        break;
    case UI_PROPERTY_IS_ENABLED:
        result.default_value.kind = UI_VALUE_BOOL; result.default_value.integer = 1;
        result.flags |= UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT;
        break;
    case UI_PROPERTY_VALUE:
        result.default_value = integer(kind == UI_SPLIT_VIEW || kind == UI_DOCK_SPLIT ? 50 : 0);
        result.flags = UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT;
        if (devtools::numeric_target(kind)) result.flags |= UI_PROPERTY_ANIMATABLE;
        if (devtools::numeric_target(kind) || devtools::boolean_target(kind)) result.flags |= UI_PROPERTY_BINDABLE;
        if ((result.flags & UI_PROPERTY_BINDABLE) && kind != UI_PROGRESS_BAR) result.flags |= UI_PROPERTY_TWO_WAY;
        if (kind == UI_SPLIT_VIEW || kind == UI_DOCK_SPLIT || kind == UI_TREE_ITEM || is_popup(kind)) result.flags |= UI_PROPERTY_AFFECTS_MEASURE;
        break;
    case UI_PROPERTY_COMMAND:
        result.default_value.kind = UI_VALUE_COMMAND;
        result.flags |= UI_PROPERTY_AFFECTS_RENDER | UI_PROPERTY_AFFECTS_INPUT;
        break;
    case UI_PROPERTY_HEIGHT: case UI_PROPERTY_WIDTH:
        result.default_value = visual_layout::length(-1); result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    case UI_PROPERTY_MIN_WIDTH: case UI_PROPERTY_MIN_HEIGHT:
        result.default_value = visual_layout::length(0); result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    case UI_PROPERTY_MAX_WIDTH: case UI_PROPERTY_MAX_HEIGHT:
        result.default_value = visual_layout::length(std::numeric_limits<double>::infinity()); result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    case UI_PROPERTY_MARGIN: {
        static const double zero[4]{};
        result.default_value.kind=UI_VALUE_THICKNESS; result.default_value.geometry=zero;
        result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    }
    case UI_PROPERTY_HORIZONTAL_ALIGNMENT: case UI_PROPERTY_VERTICAL_ALIGNMENT:
        result.default_value = string("Stretch"); result.flags |= UI_PROPERTY_AFFECTS_ARRANGE; break;
    case UI_PROPERTY_SPACING: case UI_PROPERTY_PADDING:
        result.default_value = floating(property == UI_PROPERTY_SPACING ? -1 : 0);
        result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    case UI_PROPERTY_WEIGHT:
        result.default_value = integer(1);
        result.flags |= UI_PROPERTY_AFFECTS_MEASURE | UI_PROPERTY_ANIMATABLE; break;
    default: return UI_ERROR_INVALID_ARGUMENT;
    }
    result.kind = result.default_value.kind;
    *descriptor = result;
    return UI_OK;
}

ui_result ui_node_get_property_source(const ui_node* node, ui_property property, ui_property_source* source) {
    if (!source) return UI_ERROR_INVALID_ARGUMENT;
    ui_property_descriptor descriptor{};
    if (auto result = describe_node(node, property, descriptor); result != UI_OK) return result;
    property = properties::canonical(property);
    const auto slot = node->property_slots.find(property);
    if (slot != node->property_slots.end() && !slot->second.values.empty()) {
        const auto priority = slot->second.values.rbegin()->first;
        constexpr ui_property_source sources[] = {UI_PROPERTY_SOURCE_INHERITED, UI_PROPERTY_SOURCE_THEME,
            UI_PROPERTY_SOURCE_STYLE, UI_PROPERTY_SOURCE_TRIGGER, UI_PROPERTY_SOURCE_TEMPLATE,
            UI_PROPERTY_SOURCE_LOCAL, UI_PROPERTY_SOURCE_ANIMATION};
        if (priority != UI_PRECEDENCE_LOCAL) { *source = sources[priority]; return UI_OK; }
    }
    if (std::any_of(node->declared_bindings.begin(), node->declared_bindings.end(), [&](const auto& binding) {
        return properties::canonical(binding.property) == property;
    }) || (property == UI_PROPERTY_TEXT && node->text_source)) *source = UI_PROPERTY_SOURCE_BINDING;
    else if (property == UI_PROPERTY_DATA_CONTEXT) {
        *source = UI_PROPERTY_SOURCE_DEFAULT;
        for (auto* current = node; current; current = current->inheritance_parent()) {
            if (!current->has_local_data_context) continue;
            *source = current == node ? UI_PROPERTY_SOURCE_LOCAL : UI_PROPERTY_SOURCE_INHERITED;
            break;
        }
    } else if (text_format::property(property)) {
        const auto* owner = text_format::owner(*node,property);
        *source = owner == node ? UI_PROPERTY_SOURCE_LOCAL : owner ? UI_PROPERTY_SOURCE_INHERITED : UI_PROPERTY_SOURCE_DEFAULT;
    } else *source = node->local_properties & (uint64_t{1} << property) ? UI_PROPERTY_SOURCE_LOCAL : UI_PROPERTY_SOURCE_DEFAULT;
    return UI_OK;
}
static ui_result apply_property(ui_node* node, ui_property property, const ui_binding_value* value) {
    if (!value) return UI_ERROR_INVALID_ARGUMENT;
    ui_property_descriptor descriptor{};
    if (auto result = describe_node(node, property, descriptor); result != UI_OK) return result;
    if (value->kind != descriptor.kind) return UI_ERROR_INVALID_ARGUMENT;
    if (property == UI_PROPERTY_LAYOUT_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN) {
        const auto* v = value->geometry;
        if (!v) return UI_ERROR_INVALID_ARGUMENT;
        if (property == UI_PROPERTY_LAYOUT_TRANSFORM) {
            node->layout_transform = {v[0],v[1],v[2],v[3],v[4],v[5]};
            invalidate_layout(*node,descriptor.flags);
        } else if (property == UI_PROPERTY_RENDER_TRANSFORM) node->render_transform = {v[0],v[1],v[2],v[3],v[4],v[5]};
        else node->transform_origin = {v[0],v[1]};
        binding_app_dirty(node->owner);
        return UI_OK;
    }
    if (property == UI_PROPERTY_OPACITY) {
        if (!(value->number >= 0 && value->number <= 1)) return UI_ERROR_INVALID_ARGUMENT;
        node->opacity = value->number; binding_app_dirty(node->owner); return UI_OK;
    }
    if (text_format::property(property)) {
        if (!text_format::valid(property,*value)) return UI_ERROR_INVALID_ARGUMENT;
        text_format::set(node->font,property,*value);
        invalidate_layout(*node,descriptor.flags); binding_app_dirty(node->owner); return UI_OK;
    }
    if (visual_state::property(property)) {
        if (!visual_state::valid(property, *value)) return UI_ERROR_INVALID_ARGUMENT;
        if (property == UI_PROPERTY_VISIBILITY) node->visibility = ui_visibility(value->integer);
        else if (property == UI_PROPERTY_IS_FOCUS_SCOPE) { node->is_focus_scope = value->integer != 0; node->logical_focus_id = 0; }
        else if (property == UI_PROPERTY_IS_TAB_STOP) node->is_tab_stop = value->integer != 0;
        else if (property == UI_PROPERTY_TAB_INDEX) node->tab_index = static_cast<int>(value->integer);
        else if (property == UI_PROPERTY_CLIP_TO_BOUNDS) node->clip_to_bounds = value->integer != 0;
        else node->hit_test_visible = value->integer != 0;
        invalidate_layout(*node, descriptor.flags); reconcile_input_state(*node); binding_app_dirty(node->owner);
        return UI_OK;
    }
    if (border::property(property)) {
        if (!border::valid(property, *value)) return UI_ERROR_INVALID_ARGUMENT;
        border::set(node->border, property, *value);
        invalidate_layout(*node, descriptor.flags);
        binding_app_dirty(node->owner);
        return UI_OK;
    }
    if (visual_layout::property(property)) return ui_node_set_layout_value(node, property, value);
    switch (property) {
    case UI_PROPERTY_SOURCE:
        node->image_source = value->string;
        binding_app_dirty(node->owner);
        return UI_OK;
    case UI_PROPERTY_TEXT: case UI_PROPERTY_CONTENT: return ui_node_set_text(node, value->string);
    case UI_PROPERTY_IS_ENABLED:
        if (value->integer < 0 || value->integer > 1) return UI_ERROR_INVALID_ARGUMENT;
        return ui_node_set_enabled(node, uint32_t(value->integer));
    case UI_PROPERTY_VALUE:
        if (value->integer < 0 || value->integer > std::numeric_limits<uint32_t>::max()) return UI_ERROR_INVALID_ARGUMENT;
        return ui_node_set_value(node, uint32_t(value->integer));
    case UI_PROPERTY_HEIGHT: {
        double height;
        if (!visual_layout::decode(property,*value,height)) return UI_ERROR_INVALID_ARGUMENT;
        node->height=height; invalidate_layout(*node,descriptor.flags); binding_app_dirty(node->owner); return UI_OK;
    }
    case UI_PROPERTY_COMMAND: return ui_node_bind_command(node, value->command);
    case UI_PROPERTY_SPACING: case UI_PROPERTY_PADDING:
        (property==UI_PROPERTY_SPACING ? node->spacing : node->padding)=value->number;
        invalidate_layout(*node,descriptor.flags); binding_app_dirty(node->owner); return UI_OK;
    case UI_PROPERTY_WEIGHT: return ui_node_set_weight(node, uint32_t(value->integer));
    default: return UI_ERROR_INVALID_ARGUMENT;
    }
}
ui_result ui_node_get_property(ui_node* node, ui_property property, ui_binding_value* value) {
    if (!value) return UI_ERROR_INVALID_ARGUMENT;
    ui_property_descriptor descriptor{};
    if (auto result = describe_node(node, property, descriptor); result != UI_OK) return result;
    if (property == UI_PROPERTY_LAYOUT_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN) {
        const auto& m = property == UI_PROPERTY_LAYOUT_TRANSFORM ? node->layout_transform : node->render_transform;
        if (property != UI_PROPERTY_RENDER_TRANSFORM_ORIGIN)
            node->property_geometry = {m.m11,m.m12,m.m21,m.m22,m.offset_x,m.offset_y};
        else node->property_geometry = {node->transform_origin.x,node->transform_origin.y};
        *value = {}; value->kind = descriptor.kind; value->geometry = node->property_geometry.data();
        return UI_OK;
    }
    if (property == UI_PROPERTY_OPACITY) { *value = floating(node->opacity); return UI_OK; }
    if (text_format::property(property)) {
        const auto* owner = text_format::owner(*node,property);
        *value = text_format::read(owner ? owner->font : default_text_format(),property); return UI_OK;
    }
    if (visual_state::property(property)) {
        *value = {}; value->kind = visual_state::kind(property);
        value->integer = property == UI_PROPERTY_VISIBILITY ? node->visibility : property == UI_PROPERTY_IS_FOCUS_SCOPE ? node->is_focus_scope : property == UI_PROPERTY_IS_TAB_STOP ? node->is_tab_stop : property == UI_PROPERTY_TAB_INDEX ? node->tab_index : property == UI_PROPERTY_CLIP_TO_BOUNDS ? node->clip_to_bounds : node->hit_test_visible; return UI_OK;
    }
    if (border::property(property)) { *value = border::read(node->border, property); return UI_OK; }
    if (visual_layout::property(property)) return ui_node_get_layout_value(node, property, value);
    ui_binding_value result{}; result.kind = descriptor.kind;
    try {
        switch (property) {
        case UI_PROPERTY_SOURCE: result.string = node->image_source.c_str(); break;
        case UI_PROPERTY_ITEMS_SOURCE: result.model = node->items_source; break;
        case UI_PROPERTY_DATA_CONTEXT: result.model = binding_data_context(*node); break;
        case UI_PROPERTY_TEXT: case UI_PROPERTY_CONTENT:
            node->property_text = utf8_text::encode(node->display_text()); result.string = node->property_text.c_str(); break;
        case UI_PROPERTY_IS_ENABLED: result.integer = node->is_enabled(); break;
        case UI_PROPERTY_VALUE: result.integer = node->value; break;
        case UI_PROPERTY_HEIGHT: result = visual_layout::length(node->height); break;
        case UI_PROPERTY_COMMAND: result.command = node->command; break;
        case UI_PROPERTY_SPACING: result.number = node->spacing; break;
        case UI_PROPERTY_PADDING: result.number = node->padding; break;
        case UI_PROPERTY_WEIGHT: result.integer = node->weight; break;
        default: return UI_ERROR_INVALID_ARGUMENT;
        }
        *value = result; return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_STATE; }
}
namespace {
ui_result validate_value(ui_node& node, ui_property property, const ui_binding_value& value, std::wstring* decoded = nullptr) {
    if (text_format::property(property)) {
        if (!text_format::valid(property,value)) return UI_ERROR_INVALID_ARGUMENT;
        if (property == UI_PROPERTY_FONT_FAMILY) (void)utf8_text::decode(value.string);
        return UI_OK;
    }
    if (property == UI_PROPERTY_LAYOUT_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN) {
        const bool matrix = property != UI_PROPERTY_RENDER_TRANSFORM_ORIGIN;
        return value.kind == (matrix ? UI_VALUE_MATRIX : UI_VALUE_POINT) && value.geometry &&
               std::all_of(value.geometry,value.geometry+(matrix ? 6 : 2),[](double v){return std::isfinite(v);})
               ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    }
    if (property == UI_PROPERTY_OPACITY) return value.kind == UI_VALUE_DOUBLE && value.number >= 0 && value.number <= 1 ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    if (visual_state::property(property)) return visual_state::valid(property,value) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    if (border::property(property)) return border::valid(property, value) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    if (visual_layout::scalar_metric(property)) return visual_layout::valid_metric(property,value.number) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    if (property==UI_PROPERTY_WEIGHT) return value.integer>=0 && value.integer<=1024 ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    if (visual_layout::property(property)) {
        auto layout = node.layout;
        return visual_layout::set(layout, property, value) && visual_layout::valid(layout) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    }
    switch (property) {
    case UI_PROPERTY_SOURCE:
        if (!value.string || std::string_view(value.string).size() > 1024) return UI_ERROR_INVALID_ARGUMENT;
        (void)utf8_text::decode(value.string);
        return UI_OK;
    case UI_PROPERTY_ITEMS_SOURCE:
        if (!node.items_source && !node.children.empty() && !node.children.front()->collection_item_id) return UI_ERROR_INVALID_STATE;
        return !value.model || (value.model->owner == node.owner && value.model->collection) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    case UI_PROPERTY_DATA_CONTEXT:
        return !value.model || value.model->owner == node.owner ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    case UI_PROPERTY_TEXT: case UI_PROPERTY_CONTENT: {
        auto text = utf8_text::decode(value.string);
        if (text.size() > 4096 || (is_editor(node.kind) && node.kind != UI_TEXT_AREA && text.find_first_of(L"\r\n") != text.npos)) return UI_ERROR_INVALID_ARGUMENT;
        if (decoded) *decoded = std::move(text);
        return UI_OK;
    }
    case UI_PROPERTY_HEIGHT: { double height; return visual_layout::decode(property,value,height)?UI_OK:UI_ERROR_INVALID_ARGUMENT; }
    case UI_PROPERTY_VALUE:
        return value.integer >= 0 && (devtools::indexed(node.kind) ? uint64_t(value.integer) < node.item_count() :
            value.integer <= (devtools::numeric(node.kind) ? 100 : 1)) ? UI_OK : UI_ERROR_INVALID_ARGUMENT;
    case UI_PROPERTY_COMMAND:
        if (value.command && (value.command->owner != node.owner ||
            (node.command_mode == UI_COMMAND_ASYNC && !value.command->asynchronous()) ||
            (node.command_mode == UI_COMMAND_SYNC && value.command->asynchronous()))) return UI_ERROR_INVALID_ARGUMENT;
        return UI_OK;
    default: return UI_OK;
    }
}
ui_result apply_effective(ui_node* node, ui_property property, ui_value_precedence precedence,
                          const ui_binding_value& effective, bool selected, bool clearing, const std::wstring* decoded = nullptr) {
    if (property == UI_PROPERTY_ITEMS_SOURCE) {
        if (node->items_source != effective.model) {
            node->items_source = effective.model;
            node->realized_source_id = 0;
            queue_items_refresh(*node);
        }
        return UI_OK;
    }
    if (property == UI_PROPERTY_DATA_CONTEXT) return binding_set_data_context(node, effective.model, selected);
    ui_binding_value current{};
    if (!decoded) {
        if (auto status = ui_node_get_property(node, property, &current); status != UI_OK) return status;
        if (property == UI_PROPERTY_IS_ENABLED) current.integer = node->enabled;
    }
    const bool needsApply = text_format::property(property) || (decoded ? node->display_text() != *decoded : !property_values::equal(current, effective)) ||
        (property == UI_PROPERTY_TEXT && node->text_source && precedence == UI_PRECEDENCE_LOCAL) ||
        (property == UI_PROPERTY_HORIZONTAL_ALIGNMENT && !node->layout.horizontal_set && selected);
    if (!needsApply) return UI_OK;
    struct Guard {
        ui_node& node; bool property, binding;
        ~Guard() { node.applying_property = property; node.applying_binding = binding; }
    } guard{*node, std::exchange(node->applying_property, true), std::exchange(node->applying_binding, true)};
    if (decoded) return binding_apply_text(*node, *decoded);
    if (clearing && property == UI_PROPERTY_VALUE && devtools::indexed(node->kind) && node->children.empty() && effective.integer == 0) {
        node->value = 0; binding_app_dirty(node->owner);
        return UI_OK;
    }
    return apply_property(node, property, &effective);
}
ui_result change_layer(ui_node* node, ui_property property, ui_value_precedence precedence, const ui_binding_value* input) {
    ui_property_descriptor descriptor{};
    if (auto result = describe_node(node, property, descriptor); result != UI_OK) return result;
    if (precedence < UI_PRECEDENCE_INHERITED || precedence > UI_PRECEDENCE_ANIMATION) return UI_ERROR_INVALID_ARGUMENT;
    if (precedence == UI_PRECEDENCE_ANIMATION && !(descriptor.flags & UI_PROPERTY_ANIMATABLE)) return UI_ERROR_INVALID_ARGUMENT;
    property = properties::canonical(property);
    try {
        BindingValue copied;
        std::wstring decoded;
        if (input) {
            if (auto status = binding_copy_value(visual_layout::normalize(property,*input), copied, descriptor.kind); status != UI_OK) return status;
            if (auto status = validate_value(*node, property, copied.value, &decoded); status != UI_OK) return status;
        }
        auto [where, inserted] = node->property_slots.try_emplace(property);
        struct Cleanup {
            ui_node& node; ui_property property; bool remove;
            ~Cleanup() { if (remove) node.property_slots.erase(property); }
        } cleanup{*node, property, inserted};
        // The common binding/local-value update does not need to clone a map.
        // Validate and apply first, then publish with a noexcept owning move.
        // Other layers retain the general transactional path below.
        if (input && precedence == UI_PRECEDENCE_LOCAL && !inserted && where->second.values.size() == 1 &&
            where->second.values.begin()->first == UI_PRECEDENCE_LOCAL) {
            if (auto status = apply_effective(node, property, precedence, copied.value, true, false,
                property == UI_PROPERTY_TEXT ? &decoded : nullptr); status != UI_OK) return status;
            where->second.values.begin()->second = std::move(copied);
            binding_detach(*node, property);
            properties::written(*node, property);
            if (property == UI_PROPERTY_DATA_CONTEXT && !node->applying_binding) node->data_context_edited = true;
            binding_element_changed(*node,property);
            return UI_OK;
        }
        auto next = where->second;
        if (inserted && (node->local_properties & (uint64_t{1} << property))) {
            ui_binding_value initial{};
            if (auto status = ui_node_get_property(node, property, &initial); status != UI_OK) return status;
            if (property == UI_PROPERTY_IS_ENABLED) initial.integer = node->enabled;
            if (auto status = binding_copy_value(initial, next.values[UI_PRECEDENCE_LOCAL], descriptor.kind); status != UI_OK) return status;
        }
        if (input) next.values.insert_or_assign(precedence, std::move(copied));
        else next.values.erase(precedence);
        const auto* selected = next.effective();
        auto fallback = descriptor.default_value;
        if (!selected && text_format::property(property)) {
            if (const auto* parent = text_format::owner(*node,property,true)) fallback = text_format::read(parent->font,property);
        }
        const auto& effective = selected ? selected->value : fallback;
        const bool selectedInput = input && selected && next.values.rbegin()->first == precedence;
        if (auto status = apply_effective(node, property, precedence, effective, selected != nullptr, !input,
            property == UI_PROPERTY_TEXT && selectedInput ? &decoded : nullptr); status != UI_OK) return status;
        where->second = std::move(next);
        cleanup.remove = false;
        if (precedence == UI_PRECEDENCE_LOCAL) {
            binding_detach(*node, property);
            if (input) properties::written(*node, property);
            else node->local_properties &= ~(uint64_t{1} << property);
        }
        if (property == UI_PROPERTY_DATA_CONTEXT && !node->applying_binding) node->data_context_edited = true;
        if (property == UI_PROPERTY_HORIZONTAL_ALIGNMENT && where->second.values.empty() && node->layout.horizontal_set) {
            node->layout.horizontal_set = false;
            binding_app_dirty(node->owner);
        }
        binding_element_changed(*node,property);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
}
}
ui_result properties::validate(ui_node& node, ui_property property, const ui_binding_value& value) {
    ui_property_descriptor descriptor{};
    if (auto status = describe_node(&node, property, descriptor); status != UI_OK) return status;
    try {
        BindingValue copy;
        if (auto status = binding_copy_value(visual_layout::normalize(property,value), copy, descriptor.kind); status != UI_OK) return status;
        return validate_value(node, property, copy.value);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
}
ui_result properties::set_metric_pair(ui_node& node, ui_property first, double first_value, ui_property second, double second_value) {
    if (!visual_layout::scalar_metric(first) || !visual_layout::scalar_metric(second) || first == second) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = binding_app_check(node.owner); status != UI_OK) return status;
    try {
        const ui_property keys[] = {first, second};
        const double requested[] = {first_value, second_value};
        double effective[2]{};
        auto next = node.property_slots;
        bool changed = false;
        for (unsigned i = 0; i < 2; ++i) {
            const auto key = keys[i]; const auto input = floating(requested[i]);
            if (auto status = validate(node, key, input); status != UI_OK) return status;
            ui_binding_value current{};
            if (auto status = ui_node_get_property(&node, key, &current); status != UI_OK) return status;
            auto& slot = next[key];
            slot.values.insert_or_assign(UI_PRECEDENCE_LOCAL, BindingValue{});
            slot.values[UI_PRECEDENCE_LOCAL].value = input;
            effective[i] = slot.effective()->value.number;
            changed |= current.number != effective[i];
        }
        // Every allocation and validation finished. Scalar assignment, map swap
        // and detaching old bindings below cannot fail halfway through the pair.
        node.property_slots.swap(next);
        for (unsigned i = 0; i < 2; ++i) {
            switch (keys[i]) {
            case UI_PROPERTY_SPACING: node.spacing = effective[i]; break;
            case UI_PROPERTY_PADDING: node.padding = effective[i]; break;
            default: break;
            }
            binding_detach(node, keys[i]); written(node, keys[i]);
        }
        if (changed) binding_app_dirty(node.owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result properties::update_triggers(ui_node& node) {
    if (node.triggers.empty()) return UI_OK;
    std::array<const BindingValue*, properties::count> active{};
    uint64_t targets = 0;
    for (const auto& trigger : node.triggers) {
        targets |= uint64_t{1} << trigger.property;
        if (!trigger.data_path.empty() && !trigger.data_matches) continue;
        const auto matches = [&](const auto& condition) {
            if (condition.first == UI_PROPERTY_IS_ENABLED) return node.is_enabled() == condition.second;
            ui_binding_value actual{};
            return ui_node_get_property(&node,condition.first,&actual) == UI_OK && actual.integer == condition.second;
        };
        if (std::all_of(trigger.conditions.begin(),trigger.conditions.end(),matches)) active[trigger.property] = &trigger.value;
    }
    for (unsigned index = 0; index < active.size(); ++index) {
        if (!(targets & (uint64_t{1} << index))) continue;
        auto property = ui_property(index);
        const auto* value = active[index];
        const auto slot = node.property_slots.find(property);
        if (!value && (slot == node.property_slots.end() || !slot->second.values.contains(UI_PRECEDENCE_TRIGGER))) continue;
        if (auto status = change_layer(&node,property,UI_PRECEDENCE_TRIGGER,value ? &value->value : nullptr); status != UI_OK) return status;
    }
    return UI_OK;
}

static ui_result update_enabled_triggers(ui_node& node) {
    if (auto status = properties::update_triggers(node); status != UI_OK) return status;
    for (auto* child : node.children)
        if (auto status = update_enabled_triggers(*child); status != UI_OK) return status;
    return UI_OK;
}
static ui_result set_or_clear_layer(ui_node* node, ui_property property, ui_value_precedence precedence, const ui_binding_value* value) {
    auto status = change_layer(node,property,precedence,value);
    if (status == UI_OK && !node->applying_resource && !node->resource_references.empty())
        std::erase_if(node->resource_references,[&](const auto& reference) {
            return reference.property == property && reference.layer == precedence;
        });
    if (status == UI_OK && property == UI_PROPERTY_IS_ENABLED) return update_enabled_triggers(*node);
    if (status == UI_OK && (property == UI_PROPERTY_VALUE || visual_state::property(property))) return properties::update_triggers(*node);
    return status;
}
ui_result ui_node_set_property_layer(ui_node* node, ui_property property, ui_value_precedence precedence, const ui_binding_value* value) {
    return value ? set_or_clear_layer(node, property, precedence, value) : UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_node_clear_property_layer(ui_node* node, ui_property property, ui_value_precedence precedence) {
    return set_or_clear_layer(node, property, precedence, nullptr);
}
ui_result ui_node_set_property(ui_node* node, ui_property property, const ui_binding_value* value) {
    return ui_node_set_property_layer(node, property, UI_PRECEDENCE_LOCAL, value);
}
ui_result ui_node_clear_property(ui_node* node, ui_property property) {
    return ui_node_clear_property_layer(node, property, UI_PRECEDENCE_LOCAL);
}
ui_result ui_node_get_base_property(ui_node* node, ui_property property, ui_binding_value* value) {
    if (!value) return UI_ERROR_INVALID_ARGUMENT;
    ui_property_descriptor descriptor{};
    if (auto result = describe_node(node, property, descriptor); result != UI_OK) return result;
    const auto found = node->property_slots.find(properties::canonical(property));
    if (found == node->property_slots.end()) return ui_node_get_property(node, property, value);
    const auto* base = found->second.effective(true);
    if (!base && property == UI_PROPERTY_DATA_CONTEXT) return ui_node_get_property(node, property, value);
    if (!base && text_format::property(property)) {
        const auto* parent = text_format::owner(*node,property,true);
        *value = text_format::read(parent ? parent->font : default_text_format(),property); return UI_OK;
    }
    *value = base ? base->value : descriptor.default_value;
    return UI_OK;
}
void properties::capture(ui_node& node, ui_property property) {
    written(node, property);
    if (!node.property_slots.contains(canonical(property))) {
        binding_element_changed(node,property);
        if (property == UI_PROPERTY_VALUE) binding_app_error(node.owner,update_triggers(node));
        return;
    }
    ui_binding_value value{};
    if (ui_node_get_property(&node, property, &value) != UI_OK) return;
    if (canonical(property) == UI_PROPERTY_TEXT && node.text_source) {
        try {
            auto& local = node.property_slots.at(UI_PROPERTY_TEXT).values[UI_PRECEDENCE_LOCAL];
            binding_app_error(node.owner, binding_copy_value(value, local, UI_VALUE_STRING));
        } catch (const std::bad_alloc&) { binding_app_error(node.owner, UI_ERROR_OUT_OF_MEMORY); }
        return;
    }
    struct Guard { ui_node& node; bool previous; ~Guard() { node.applying_binding = previous; } } guard{node, std::exchange(node.applying_binding, true)};
    binding_app_error(node.owner, ui_node_set_property(&node, property, &value));
}
