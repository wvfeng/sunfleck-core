#include "devtools.h"
#include "ui_tree.h"
#include "fleck.h"
#include "dock_json.h"
#include "dock.h"
#include "fleck_schema.h"
#include "utf8_text.h"
#include "components.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <limits>
#include <cmath>
#include <cstdio>
#include <utility>

namespace devtools {
namespace {
constexpr const char* names[] = {
    "StackPanel", "Flex", "Grid", "ScrollView", "Panel", "Card", "Button", "TextBlock", "Image",
    "CheckBox", "RadioButton", "Switch", "Slider", "Separator", "ProgressBar", "Popup", "ComboBox",
    "TextBox", "ItemsControl", "ListView", "ListItem", "TabView", "TabItem", "Overlay", "WrapPanel",
    "DockPanel", "SplitView", "Label", "Icon", "Badge", "IconButton", "ToggleButton", "Hyperlink",
    "SpinBox", "PasswordBox", "TextArea", "SearchBox", "TreeView", "DataGrid", "MenuBar", "ToolBar",
    "NavigationView", "Dialog", "Tooltip", "ContextMenu", "Spinner", "InfoBar", "TreeItem", "DockHost",
    "DockSplit", "DockGroup", "DockDocument", "DockTool", "DocumentView", "Border", "Run", "Span", "Canvas", "ContentControl", "ContentPresenter", "Viewbox", "Rectangle", "Ellipse", "Line", "Path"
};
static_assert(std::size(names) == UI_PATH + 1);
using docking::json::quote;
void strings_json(std::ostream& out, std::span<const std::string_view> values)
{
    out << '[';
    bool comma = false;
    for (const auto value : values) {
        if (std::exchange(comma, true)) { out << ','; }
        out << quote(value);
    }
    out << ']';
}
void attributes_json(std::ostream& out, std::span<const fleck::schema::Attribute> attributes, bool button = false)
{
    out << '[';
    bool comma = false;
    for (const auto& attribute : attributes) {
        if (std::exchange(comma, true)) { out << ','; }
        const bool role = button && attribute.name == "Role";
        out << "{\"name\":" << quote(attribute.name) << ",\"type\":" << quote(attribute.type);
        if (attribute.required || role) { out << ",\"required\":true"; }
        if (attribute.minimum >= 0) { out << ",\"minimum\":" << attribute.minimum << ",\"maximum\":" << attribute.maximum; }
        if (!attribute.values.empty()) {
            out << ",\"values\":";
            strings_json(out, role ? attribute.values.first(4) : attribute.values);
        }
        if (attribute.name == "State") { out << ",\"separator\":\"|\""; }
        out << '}';
    }
    out << ']';
}
}

SourceIndex::SourceIndex(std::string_view source) : positions_(source.size() + 1)
{
    uint32_t line = 1, column = 1;
    for (size_t i = 0; i < source.size();) {
        const auto first = static_cast<unsigned char>(source[i]);
        size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        length = std::min(length, source.size() - i);
        for (size_t j = 0; j < length; ++j) { positions_[i + j] = {line, column}; }
        if (first == '\r' || (first == '\n' && (i == 0 || source[i - 1] != '\r'))) { ++line; column = 1; }
        else if (first != '\n' && !(i == 0 && source.substr(0, 3) == "\xef\xbb\xbf")) { column += length == 4 ? 2u : 1u; }
        i += length;
    }
    positions_.back() = {line, column};
}
Span SourceIndex::span(size_t offset, size_t length) const
{
    if (offset >= positions_.size() || length > positions_.size() - 1 - offset) { throw std::range_error("Invalid source range"); }
    const auto [line, column] = positions_[offset];
    const auto [end_line, end_column] = positions_[offset + length];
    return {offset, length, line, column, end_line, end_column};
}
std::string span_json(const Span& span)
{
    return "{\"offset\":" + std::to_string(span.offset) + ",\"length\":" + std::to_string(span.length) +
        ",\"line\":" + std::to_string(span.line) + ",\"column\":" + std::to_string(span.column) +
        ",\"endLine\":" + std::to_string(span.end_line) + ",\"endColumn\":" + std::to_string(span.end_column) + "}";
}
std::string location_json(const Source* source, const Location& location)
{
    std::string result = "{\"uri\":" + quote(!location.source_uri.empty() ? location.source_uri : source ? source->uri : "") + ",\"range\":" + span_json(location.element) + ",\"attributes\":[";
    bool comma = false;
    for (const auto& attribute : location.attributes) {
        if (std::exchange(comma, true)) { result += ','; }
        result += "{\"name\":" + quote(attribute.name) + ",\"range\":" + span_json(attribute.range) + ",\"value\":" + span_json(attribute.value) + "}";
    }
    return result + "],\"model\":" + quote(source ? source->model : "") + ",\"modelId\":\"" +
        std::to_string(source ? source->model_id : 0) + "\"}";
}
ui_result copy_json(const std::string& json, char* output, size_t* size)
{
    if (!size) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto available = *size;
    *size = json.size() + 1;
    if (output) {
        if (available < *size) { return UI_ERROR_INVALID_ARGUMENT; }
        std::memcpy(output, json.c_str(), *size);
    }
    return UI_OK;
}
const char* control_name(ui_node_kind kind)
{
    return kind >= UI_STACK_PANEL && kind <= UI_PATH ? names[kind] : "Unknown";
}
const char* value_type(ui_value_kind kind)
{
    constexpr const char* types[] = {"string", "bool", "integer", "number", "command", "model", "color", "thickness", "cornerRadius", "point", "size", "matrix", "length", "enum"};
    return kind >= UI_VALUE_STRING && kind <= UI_VALUE_ENUM ? types[kind] : "unknown";
}
bool text_target(ui_node_kind kind)
{
    return (has_content(kind) || !is_container(kind) || kind == UI_COMBO_BOX || kind == UI_TAB_ITEM || kind == UI_TREE_ITEM ||
        kind == UI_DOCK_DOCUMENT || kind == UI_DOCK_TOOL) && kind != UI_IMAGE && kind != UI_ICON &&
        kind != UI_SEPARATOR && kind != UI_SLIDER && kind != UI_PROGRESS_BAR;
}
bool numeric_target(ui_node_kind kind)
{
    return kind == UI_SLIDER || kind == UI_PROGRESS_BAR || kind == UI_SPIN_BOX || kind == UI_SPLIT_VIEW;
}
bool boolean_target(ui_node_kind kind)
{
    return kind == UI_CHECK_BOX || kind == UI_RADIO_BUTTON || kind == UI_SWITCH || kind == UI_TOGGLE_BUTTON;
}
bool indexed(ui_node_kind kind) { return is_selector(kind) || kind == UI_DOCK_GROUP; }
bool numeric(ui_node_kind kind) { return numeric_target(kind) || kind == UI_SPINNER || kind == UI_DOCK_SPLIT; }
bool has_value(ui_node_kind kind)
{
    return indexed(kind) || is_popup(kind) || numeric(kind) || kind == UI_TREE_ITEM || kind == UI_BUTTON ||
        (kind >= UI_CHECK_BOX && kind <= UI_SLIDER) || (kind >= UI_ICON_BUTTON && kind <= UI_HYPERLINK);
}
bool accepts_child(ui_node_kind parent, size_t count, ui_node_kind child)
{
    if (inline_container(parent)) return is_inline(child);
    if (is_inline(child)) return false;
    if (parent == UI_DOCK_HOST) { return count == 0 && (child == UI_DOCK_SPLIT || child == UI_DOCK_GROUP); }
    if (parent == UI_DOCK_SPLIT) { return count < 2 && (child == UI_DOCK_SPLIT || child == UI_DOCK_GROUP); }
    if (parent == UI_DOCK_GROUP) { return child == UI_DOCK_DOCUMENT || child == UI_DOCK_TOOL; }
    if (child >= UI_DOCK_SPLIT && child <= UI_DOCK_TOOL) { return false; }
    if ((parent == UI_BORDER || parent == UI_VIEWBOX || has_content(parent)) && count != 0) return false;
    return is_container(parent) && ((parent == UI_LIST_VIEW || parent == UI_COMBO_BOX) == (child == UI_LIST_ITEM)) &&
        ((parent == UI_TAB_VIEW || parent == UI_NAVIGATION_VIEW) == (child == UI_TAB_ITEM)) &&
        ((parent == UI_TREE_VIEW || parent == UI_TREE_ITEM) == (child == UI_TREE_ITEM)) &&
        (parent != UI_DATA_GRID || child == UI_GRID) && (parent != UI_SPLIT_VIEW || count < 2);
}
bool attribute_allowed(ui_node_kind kind, std::string_view name)
{
    if (name=="Fill" || name=="Stroke" || name=="StrokeThickness") return drawing::shape_kind(kind);
    if (name=="Data") return kind==UI_PATH;
    if (name=="DragText") return kind==UI_BUTTON || kind==UI_ICON_BUTTON;
    if (name=="AllowDrop") return is_editor(kind);
    if (name == "Stretch") return kind == UI_IMAGE || kind == UI_ICON || kind == UI_VIEWBOX;
    if (name == "Source") return kind == UI_IMAGE;
    if (visual_layout::scroll_policy_name(name)) return kind == UI_SCROLL_VIEW;
    if (name == "Orientation" && (kind == UI_STACK_PANEL || kind == UI_WRAP_PANEL || kind == UI_SEPARATOR)) return true;
    if (name == "Panel.ZIndex" || visual_layout::canvas_coordinate(name)) return true;
    if (grid_layout::attribute(name)) return kind == UI_GRID || name.starts_with("Grid.");
    if (name == "ItemsSource") return kind == UI_LIST_VIEW || kind == UI_ITEMS_CONTROL || kind == UI_COMBO_BOX || kind == UI_TREE_VIEW || kind == UI_TREE_ITEM || kind == UI_DATA_GRID;
    if (name == "Template") return fleck::supports_template(kind);
    if (name == "Style") return true;
    if (name == "LayoutTransform" || name == "RenderTransform" || name == "RenderTransformOrigin" || name == "Opacity" || name == "Visibility" || name == "TabIndex" || name == "IsFocusScope" || name == "IsTabStop" || name == "ClipToBounds" || name == "IsHitTestVisible" || name == "FontFamily" || name == "FontSize" || name == "FontWeight" || name == "FontStyle" || name == "FontStretch" || name == "Foreground" || name == "TextAlignment" || name == "TextWrapping" || name == "TextTrimming" || name == "LineHeight" || name == "TextDecorations" || name == "FlowDirection") return true;
    ui_property appearance;
    if (properties::find(name, appearance) && border::property(appearance)) return kind == UI_BORDER;
    if (visual_layout::property(visual_layout::find(name))) return true;
    if (name == "Id" || name == "IsEnabled" || name == "Height" || name == "Weight" || name == "Dock" || name == "DataContext") { return true; }
    if (name == "Anchor") { return kind == UI_LABEL || is_popup(kind); }
    if (name == "Text") { return text_target(kind); }
    if (name == "Content") { return has_content(kind); }
    if (name == "Command" || name == "CommandMode") { return is_interactive(kind); }
    if (name == "Value") { return has_value(kind); }
    if (name == "Spacing" || name == "Padding") { return is_container(kind); }
    if (kind == UI_DOCK_SPLIT && (name == "Orientation" || name == "Ratio")) { return true; }
    return (kind == UI_DOCK_DOCUMENT || kind == UI_DOCK_TOOL) &&
        (name == "Title" || name == "Icon" || name == "CanClose" || name == "CanFloat" || name == "CanAutoHide" || name == "MinWidth" || name == "MinHeight");
}
inline constexpr const char* visual_attributes[] = {"Id", "Anchor", "IsEnabled", "DragText", "AllowDrop", "DataContext", "Text", "Content", "Command", "CommandMode", "Value", "Spacing", "Padding",
        "Style", "Template", "ItemsSource", "Height", "Width", "MaxWidth", "MaxHeight", "Margin", "HorizontalAlignment", "VerticalAlignment", "Weight", "Dock", "Orientation", "Ratio", "Title", "Icon", "CanClose", "CanFloat", "CanAutoHide", "MinWidth", "MinHeight", "Background", "BorderBrush", "BorderThickness", "CornerRadius", "Visibility", "LayoutTransform", "RenderTransform", "RenderTransformOrigin", "Opacity", "TabIndex", "IsFocusScope", "IsTabStop", "ClipToBounds", "IsHitTestVisible", "FontFamily", "FontSize", "FontWeight", "FontStyle", "FontStretch", "Foreground", "TextAlignment", "TextWrapping", "TextTrimming", "LineHeight", "TextDecorations", "FlowDirection", "RowDefinitions", "ColumnDefinitions", "Grid.Row", "Grid.Column", "Grid.RowSpan", "Grid.ColumnSpan", "Panel.ZIndex", "Canvas.Left", "Canvas.Top", "HorizontalScrollBarVisibility", "VerticalScrollBarVisibility", "Source","Stretch","Fill","Stroke","StrokeThickness","Data"};


std::string property_value(const ui_node& node, std::string_view name)
{
    if (name == "Source") return quote(node.image_source);
    const auto number = [](auto n) { return std::to_string(n); };
    ui_property fontProperty;
    if (properties::find(name,fontProperty) && text_format::property(fontProperty)) {
        const auto value = text_format::effective(node);
        const auto literal = text_format::literal(value,fontProperty);
        return fontProperty == UI_PROPERTY_FONT_SIZE || fontProperty == UI_PROPERTY_LINE_HEIGHT ? literal : quote(literal);
    }
    if (visual_layout::scroll_policy_name(name))
        return quote(visual_layout::scroll_policies[name == "HorizontalScrollBarVisibility" ? node.horizontal_scroll : node.vertical_scroll]);
    if (name == "Stretch") return quote(visual_layout::stretch_names[node.stretch.value_or(UI_STRETCH_UNIFORM)]);
    if (name=="Fill" || name=="Stroke") {
        const auto& brush=node.brushes[name=="Fill" ? UI_BRUSH_BACKGROUND : UI_BRUSH_OUTLINE];
        char value[10]; std::snprintf(value,sizeof(value),"#%08X",brush ? brush->color : 0u);
        return quote(value);
    }
    if (name=="Data") return quote(node.geometry ? node.geometry->data : "");
    if (name=="StrokeThickness") return number(node.stroke_thickness);
    if (name == "Canvas.Left") return number(node.canvas_left);
    if (name == "Canvas.Top") return number(node.canvas_top);
    if (name == "Panel.ZIndex") return number(node.z_index);
    if (name == "Grid.Row") return number(node.grid.cell.row);
    if (name == "Grid.Column") return number(node.grid.cell.column);
    if (name == "Grid.RowSpan") return number(node.grid.cell.rows);
    if (name == "Grid.ColumnSpan") return number(node.grid.cell.columns);
    if (name == "RowDefinitions" || name == "ColumnDefinitions") {
        const auto& tracks = name == "RowDefinitions" ? node.grid.rows : node.grid.columns;
        std::ostringstream literal;
        literal.imbue(std::locale::classic());
        for (size_t i=0; i<tracks.size(); ++i) {
            if (i) literal << ',';
            const auto& track = tracks[i];
            if (track.unit == grid_layout::Unit::automatic) literal << "Auto";
            else if (track.unit == grid_layout::Unit::pixels) literal << track.value;
            else { if (track.value != 1) literal << track.value; literal << '*'; }
            if (!track.group.empty()) literal << '@' << track.group;
        }
        return quote(tracks.empty() ? "*" : literal.str());
    }
    if (name == "Visibility") return quote(visual_state::name(node.visibility));
    if (name == "LayoutTransform" || name == "RenderTransform" || name == "RenderTransformOrigin") {
        const auto& m = name == "LayoutTransform" ? node.layout_transform : node.render_transform;
        std::ostringstream value;
        value.imbue(std::locale::classic());
        if (name != "RenderTransformOrigin") value << m.m11 << ',' << m.m12 << ',' << m.m21 << ',' << m.m22 << ',' << m.offset_x << ',' << m.offset_y;
        else value << node.transform_origin.x << ',' << node.transform_origin.y;
        return quote(value.str());
    }
    if (name == "DragText") return quote(node.drag_text);
    if (name == "AllowDrop") return node.allow_text_drop ? "true" : "false";
    if (name == "Opacity") return number(node.opacity);
    if (name == "TabIndex") return std::to_string(node.tab_index);
    if (name == "IsFocusScope") return node.is_focus_scope ? "true" : "false";
    if (name == "IsTabStop") return node.is_tab_stop ? "true" : "false";
    if (name == "ClipToBounds") return node.clip_to_bounds ? "true" : "false";
    if (name == "IsHitTestVisible") return node.hit_test_visible ? "true" : "false";
    const auto boolean = [](bool value) { return std::string(value ? "true" : "false"); };
    ui_property appearance;
    if (properties::find(name, appearance) && border::property(appearance)) {
        const auto v = border::read(node.border, appearance);
        if (v.kind == UI_VALUE_COLOR) {
            char literal[10]; std::snprintf(literal, sizeof(literal), "#%08X", unsigned(v.integer)); return quote(literal);
        }
        std::ostringstream out; out << '[';
        for (int i = 0; i < 4; ++i) { if (i) out << ','; out << v.geometry[i]; }
        out << ']'; return out.str();
    }
    if (name == "Template" && node.component && node.component->is_control)
        return quote(node.component->selected_template.first + ":" + node.component->selected_template.second);
    if (name == "Style") return quote(node.style_key);
    if (name == "Id") return quote(node.fleck_id);
    if (name == "DataContext") {
        const auto* model = binding_data_context(node);
        return model ? quote("Model " + std::to_string(model->debug_id)) : "null";
    }
    if (name == "Anchor") return quote(node.anchor ? node.anchor->fleck_id : "");
    if (name == "IsEnabled") return boolean(node.enabled);
    if (name == "Text" || name == "Content") return quote(node.kind == UI_PASSWORD_BOX ? "" : utf8_text::encode(node.display_text()));
    if (name == "Command") return node.command ? quote("Attached") : "null";
    if (name == "ItemsSource") return node.items_source ? quote("Attached") : "null";
    if (name == "CommandMode") return quote(node.command_mode == UI_COMMAND_ASYNC ? "Async" : "Sync");
    if (name == "Value" || name == "Ratio") return number(node.value);
    if (name == "Spacing") return node.spacing < 0 ? quote("Theme") : number(node.spacing);
    if (name == "Padding") return number(node.padding);
    if (name == "Width") return node.layout.width < 0 ? quote("Auto") : number(node.layout.width);
    if (name == "Height") return node.height < 0 ? quote("Auto") : number(node.height);
    if (name == "MinWidth") return number(node.layout.min_width);
    if (name == "MaxWidth") return std::isinf(node.layout.max_width)?quote("Infinity"):number(node.layout.max_width);
    if (name == "MinHeight") return number(node.layout.min_height);
    if (name == "MaxHeight") return std::isinf(node.layout.max_height)?quote("Infinity"):number(node.layout.max_height);
    if (name == "Margin") {
        std::ostringstream out; out.imbue(std::locale::classic()); out<<std::setprecision(17)<<'[';
        for (size_t i=0;i<4;++i) { if(i) out<<','; out<<node.layout.margin[i]; }
        out<<']'; return out.str();
    }
    if (name == "HorizontalAlignment") return quote(visual_layout::horizontal[node.layout.horizontal]);
    if (name == "VerticalAlignment") return quote(visual_layout::vertical[node.layout.vertical]);
    if (name == "Weight") return number(node.weight);
    if (name == "Dock") { constexpr const char* edges[] = {"Left", "Top", "Right", "Bottom"}; return quote(edges[node.dock]); }
    if (name == "Orientation") return quote((node.kind==UI_DOCK_SPLIT ? node.dock==UI_DOCK_TOP : node.vertical()) ? "Vertical" : "Horizontal");
    if (name == "Title") return quote(node.dock_title);
    if (name == "Icon") return quote(node.dock_icon);
    if (name == "CanClose") return boolean(node.dock_capabilities & UI_DOCK_CAN_CLOSE);
    if (name == "CanFloat") return boolean(node.dock_capabilities & UI_DOCK_CAN_FLOAT);
    if (name == "CanAutoHide") return boolean(node.dock_capabilities & UI_DOCK_CAN_AUTO_HIDE);
    return "null";
}
std::string default_property(ui_node_kind kind, std::string_view name)
{
    ui_node node(nullptr, kind);
    if (kind == UI_SPLIT_VIEW || kind == UI_DOCK_SPLIT) node.value = 50;
    return property_value(node, name);
}
const char* property_effect(std::string_view name)
{
    if (name == "Weight") return "Relative space allocation in Grid/Flex; not a width. Auto is only supported by Width/Height.";
    if (name == "Dock") return "Placement in DockPanel; the final child fills the remaining area.";
    if (name == "Spacing") return "Container-specific gap; Theme uses the current theme. Overlay has no gaps.";
    if (name == "Width" || name == "Height") return "Requested logical size; Auto measures content or stretches in the parent slot. Min/Max still apply.";
    if (name == "IsEnabled") return "Local value; an ancestor can disable the effective state.";
    if (name == "Foreground") return "Inherited ARGB text color; an unset value uses the control theme.";
    if (name == "TextAlignment") return "Left/Right are absolute horizontal alignment, including right-to-left text.";
    if (name == "TextWrapping") return "Wrap or NoWrap; noninherited. Single-line controls never wrap.";
    if (name == "TextTrimming") return "Explicit overflow trimming; noninherited. Editing controls keep all text available.";
    if (name == "LineHeight") return "Inherited logical line height in 0..4096; zero uses font metrics.";
    if (name == "TextDecorations") return "Inherited underline and/or strikethrough.";
    if (name == "FlowDirection") return "Inherited paragraph reading direction; does not mirror the visual layout.";
    return "";
}
std::string node_properties_json(const ui_node& node)
{
    std::ostringstream out;
    out << '['; bool comma = false;
    for (const auto* name : visual_attributes) {
        if (!attribute_allowed(node.kind, name)) continue;
        if (node.component && (std::string_view(name) == "Spacing" || std::string_view(name) == "Padding")) continue;
        if (std::exchange(comma, true)) out << ',';
        const bool declared = std::any_of(node.location.attributes.begin(), node.location.attributes.end(),
            [&](const auto& a) { return a.name == name; });
        out << "{\"name\":" << quote(name) << ",\"value\":" << property_value(node, name)
            << ",\"defaultValue\":" << default_property(node.kind, name) << ",\"declared\":" << (declared ? "true" : "false")
            << ",\"description\":" << quote(property_effect(name));
        ui_property property;
        ui_property_source source;
        if (properties::find(name, property) && ui_node_get_property_source(&node, property, &source) == UI_OK) {
            out << ",\"valueSource\":" << quote(properties::source_name(source));
            out << ",\"propertyId\":" << quote(std::to_string(ui_property_id(property) + 1));
            if (property == UI_PROPERTY_VISIBILITY) out << ",\"effectiveVisible\":" << (node.is_visible() ? "true" : "false");
            if (property == UI_PROPERTY_IS_HIT_TEST_VISIBLE) out << ",\"acceptsPointer\":" << (node.accepts_input() ? "true" : "false");
            if (property == UI_PROPERTY_IS_ENABLED) out << ",\"effectiveValue\":" << (node.is_enabled() ? "true" : "false");
        }
        out << '}';
    }
    if (node.component) {
        if (node.component->is_control && !fleck::supports_template(node.kind))
            out << ",{\"name\":\"Template\",\"value\":" << quote(node.component->selected_template.first + ":" + node.component->selected_template.second) << '}';
        const auto value_json = [](const components::Property& property) -> std::string {
            const auto& value = property.value;
            switch (value.kind) {
            case UI_VALUE_STRING: return quote(property.text);
            case UI_VALUE_BOOL: return value.integer ? "true" : "false";
            case UI_VALUE_INT: case UI_VALUE_ENUM: case UI_VALUE_COLOR: return std::to_string(value.integer);
            case UI_VALUE_DOUBLE: return std::isfinite(value.number) ? std::to_string(value.number) : "null";
            case UI_VALUE_COMMAND: return value.command ? quote("Attached") : "null";
            case UI_VALUE_MODEL: return value.model ? quote("Attached") : "null";
            case UI_VALUE_LENGTH: return value.integer == UI_LENGTH_AUTO ? quote("Auto") : value.integer == UI_LENGTH_INFINITY ? quote("Infinity") : std::to_string(value.number);
            default: {
                std::string result = "[";
                const auto count = property_values::dimensions(value.kind);
                if (!count) return "null";
                for (unsigned i = 0; i < count; ++i) { if (i) result += ','; result += std::to_string(property.geometry[i]); }
                return result + "]";
            }
            }
        };
        for (const auto& property : node.component->type->properties) {
            const auto found = node.component->values.find(property.name);
            if (found == node.component->values.end()) continue;
            if (std::exchange(comma, true)) out << ',';
            ui_property_source source = UI_PROPERTY_SOURCE_DEFAULT;
            (void)ui_component_get_property_source(node.component.get(), property.name.c_str(), &source);
            out << "{\"name\":" << quote(property.name) << ",\"value\":" << value_json(found->second)
                << ",\"valueSource\":" << quote(properties::source_name(source))
                << ",\"propertyFlags\":" << property.metadata.flags
                << ",\"propertyId\":" << quote(std::to_string(property.id))
                << ",\"ownerNamespace\":" << quote(node.component->type->key.first)
                << ",\"ownerName\":" << quote(node.component->type->key.second)
                << ",\"defaultValue\":" << value_json(property) << ",\"readOnly\":" << (property.read_only ? "true" : "false") << '}';
        }
    }
    out << ']'; return out.str();
}
std::string schema_json()
{
    std::ostringstream out;
    out << R"({"version":1,"namespace":"urn:sunfleck:v1","transformObjects":{)";
    bool first_transform = true;
    const auto object = [&](std::string_view name,std::initializer_list<const char*> attrs) {
        if (!first_transform) out << ',';
        first_transform = false;
        out << quote(name) << ":{\"name\":" << quote(name) << ",\"attributes\":[";
        bool first = true;
        for (auto attr : attrs) {
            if (!first) out << ',';
            first = false;
            const std::string_view key=attr;
            const auto type = key=="Matrix" ? "matrix" : key=="Color" ? "color" :
                key=="StartPoint" || key=="EndPoint" || key=="Center" || (name=="DropShadowEffect" && key=="Offset") ? "point" :
                key=="AutoReverse" || key=="HoldEnd" ? "bool" :
                key=="RepeatCount" || key=="DurationMs" || key=="DelayMs" ? "integer" :
                key=="Name" || key=="Gesture" || key=="Command" || key=="Executed" || key=="Action" || key=="Phase" || key=="Source" || key=="Binding" || key=="Path" || key=="Converter" || key=="Header" || key=="ItemsSource" || key=="TargetType" || key=="Key" || key=="BasedOn" || key=="Stretch" || key=="Rgba" || key=="Easing" || key=="Property" || key=="Value" ? "string" : "number";
            out << "{\"name\":" << quote(attr) << ",\"type\":" << quote(type);
            if (key == "Phase") out << R"(,"values":["Preview","Direct","Bubble"])";
            out << '}';

        }
        out << "]}";
    };
    object("MatrixTransform",{"Matrix"});
    object("TranslateTransform",{"X","Y"});
    object("ScaleTransform",{"ScaleX","ScaleY","CenterX","CenterY"});
    object("RotateTransform",{"Angle","CenterX","CenterY"});
    object("SkewTransform",{"AngleX","AngleY","CenterX","CenterY"});
    object("TransformGroup",{});
    out << "},\"itemObjects\":{";
    first_transform=true;
    object("DataTemplate",{"Key"});
    object("HierarchicalDataTemplate",{"ItemsSource"});
    object("DataGridTextColumn",{"Header","Binding","Width"});
    object("MultiBinding",{"Converter"});
    object("Binding",{"Path"});
    out << "},\"drawingObjects\":{";
    first_transform=true;
    object("SolidColorBrush",{"Color"});
    object("LinearGradientBrush",{"StartPoint","EndPoint"});
    object("RadialGradientBrush",{"Center","RadiusX","RadiusY"});
    object("GradientStop",{"Offset","Color"});
    object("ImageBrush",{"Width","Height","Rgba","Stretch"});
    object("BlurEffect",{"Radius"});
    object("DropShadowEffect",{"Radius","Color","Offset"});
    out << "},\"animationObjects\":{";
    first_transform=true;
    object("CommandBinding",{"Command","Executed"});
    object("PointerBinding",{"Action","Phase","Command"});
    object("KeyBinding",{"Gesture","Phase","Command"});
    object("VisualStateGroup",{"Name"});
    object("VisualState",{"Name"});
    object("Storyboard",{});
    object("Timeline",{"Property","DurationMs","DelayMs","RepeatCount","AutoReverse","HoldEnd"});
    object("KeyFrame",{"Position","Value","Easing"});
    out << R"(},"visualStateGroups":{"CommonStates":["Normal","MouseOver","Pressed","Disabled"],)"
           R"("FocusStates":["Focused","Unfocused"],"CheckStates":["Checked","Unchecked"],)"
           R"("SelectionStates":["Selected","Unselected"],"ValidationStates":["Valid","InvalidFocused","InvalidUnfocused"])";
    out << "},\"styleObjects\":{";
    first_transform=true;
    object("ResourceDictionary",{"Source"});
    object("Resource",{"Key","Value"});
    object("Style",{"Key","TargetType","BasedOn"});
    object("Setter",{"Property","Value"});
    object("Trigger",{"Property","Value"});
    object("MultiTrigger",{});
    object("DataTrigger",{"Binding","Value"});
    object("Condition",{"Property","Value"});
    const auto legacy_element = [](ui_node_kind kind) { return kind == UI_ICON || kind == UI_ICON_BUTTON; };
    const auto element_json = [&](ui_node_kind kind) {
        out << "{\"kind\":" << static_cast<int>(kind) << ",\"name\":" << quote(control_name(kind)) << ",\"container\":" << (is_container(kind) || inline_container(kind) ? "true" : "false") << ",\"attributes\":[";
        bool comma = false;
        for (const auto* attribute : visual_attributes) {
            if (!attribute_allowed(kind, attribute)) { continue; }
            if (std::exchange(comma, true)) { out << ','; }
            const std::string_view name(attribute);
            ui_property property;
            ui_property_descriptor descriptor{}; descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
            const bool described = properties::find(name, property) && ui_property_describe(kind, property, &descriptor) == UI_OK;
            const bool bindable = described && (descriptor.flags & UI_PROPERTY_BINDABLE);
            const bool two_way = described && (descriptor.flags & UI_PROPERTY_TWO_WAY);
            const bool boolean = name == "AllowDrop" || name == "IsFocusScope" || name == "IsTabStop" || name == "ClipToBounds" || name == "IsHitTestVisible" || name == "IsEnabled" || name == "CanClose" || name == "CanFloat" || name == "CanAutoHide";
            const bool integer = name == "TabIndex" || name == "Panel.ZIndex" || name.starts_with("Grid.") || visual_layout::numeric(visual_layout::find(name)) || name == "Value" || name == "Spacing" || name == "Padding" || name == "Height" || name == "Weight" || name == "Ratio" || name == "MinWidth" || name == "MinHeight";
            const auto type = name=="Fill" || name=="Stroke" ? "color" : name=="StrokeThickness" ? "number" : name == "LayoutTransform" || name == "RenderTransform" ? "matrix" : name == "RenderTransformOrigin" ? "point" : name == "Opacity" || visual_layout::canvas_coordinate(name) ? "number" : described && visual_layout::dimension(property) ? "length" : name == "Margin" ? "thickness" : name == "Foreground" ? "color" : name == "TextAlignment" ? "textAlignment" : name == "TextWrapping" ? "textWrapping" : name == "TextTrimming" ? "textTrimming" : name == "TextDecorations" ? "textDecorations" : name == "FlowDirection" ? "flowDirection" : name == "FontSize" || name == "LineHeight" || name == "Spacing" || name == "Padding" ? "number" : name == "FontWeight" ? "fontWeight" : name == "FontStyle" ? "fontStyle" : name == "FontStretch" ? "fontStretch" : name == "Visibility" ? "visibility" : described && border::property(property) ? value_type(descriptor.kind) : boolean ? "bool" : integer ? "integer" : name == "Command" ? "command" : (name == "DataContext" || name == "ItemsSource") ? "model" : "string";
            out << "{\"name\":" << quote(name) << ",\"type\":" << quote(type) << ",\"binding\":" << (bindable ? "true" : "false")
                << ",\"animatable\":" << (described && (descriptor.flags & UI_PROPERTY_ANIMATABLE) ? "true" : "false")
                << ",\"propertyFlags\":" << (described ? descriptor.flags : 0)
                << ",\"twoWay\":" << (two_way ? "true" : "false") << ",\"defaultMode\":" << quote(two_way ? "TwoWay" : "OneWay") << ",\"values\":[";
            if (boolean) { out << "\"true\",\"false\""; }
            else if (name == "TextAlignment") { out << "\"Left\",\"Center\",\"Right\",\"Justify\""; }
            else if (name == "TextWrapping") { out << "\"NoWrap\",\"Wrap\""; }
            else if (name == "TextTrimming") { out << "\"None\",\"CharacterEllipsis\",\"WordEllipsis\""; }
            else if (name == "TextDecorations") { out << "\"None\",\"Underline\",\"Strikethrough\",\"Underline,Strikethrough\""; }
            else if (name == "FlowDirection") { out << "\"LeftToRight\",\"RightToLeft\""; }
            else if (name == "FontWeight") { out << "\"Thin\",\"ExtraLight\",\"Light\",\"SemiLight\",\"Normal\",\"Medium\",\"SemiBold\",\"Bold\",\"ExtraBold\",\"Black\",\"ExtraBlack\""; }
            else if (name == "FontStyle") { out << "\"Normal\",\"Oblique\",\"Italic\""; }
            else if (name == "FontStretch") { out << "\"UltraCondensed\",\"ExtraCondensed\",\"Condensed\",\"SemiCondensed\",\"Normal\",\"SemiExpanded\",\"Expanded\",\"ExtraExpanded\",\"UltraExpanded\""; }
            else if (name == "Visibility") { out << "\"Visible\",\"Hidden\",\"Collapsed\""; }
            else if (name == "DataContext") { out << "\"null\""; }
            else if (name == "Dock") { out << "\"Left\",\"Top\",\"Right\",\"Bottom\""; }
            else if (name == "Stretch") { out << "\"None\",\"Fill\",\"Uniform\",\"UniformToFill\""; }
            else if (visual_layout::scroll_policy_name(name)) { out << "\"Disabled\",\"Auto\",\"Hidden\",\"Visible\""; }
            else if (name == "Orientation") { out << "\"Horizontal\",\"Vertical\""; }
            else if (name == "CommandMode") { out << "\"Sync\",\"Async\""; }
            else if (name == "HorizontalAlignment") { out << "\"Stretch\",\"Left\",\"Center\",\"Right\""; }
            else if (name == "VerticalAlignment") { out << "\"Stretch\",\"Top\",\"Center\",\"Bottom\""; }
            out << ']';
            if (name == "Spacing") { out << ",\"autoValue\":\"Theme\""; }
            if (name == "Height" || name == "Width") { out << ",\"autoValue\":\"Auto\""; }
            if (name == "MaxHeight" || name == "MaxWidth") { out << ",\"autoValue\":\"Infinity\""; }
            out << ",\"defaultValue\":" << default_property(kind, name) << ",\"description\":" << quote(property_effect(name));
            if (described && visual_state::trigger_source(property)) out << ",\"triggerSource\":true";
            if (name == "Value") {
                if (fleck::range_template(kind)) out << ",\"templateBindingSource\":true";
                out << ",\"minimum\":0";
                if (indexed(kind)) { out << ",\"maximumFrom\":\"children.count-1\""; }
                else { out << ",\"maximum\":" << (numeric(kind) ? 100 : 1); }
            }
            if (name == "Grid.RowSpan" || name == "Grid.ColumnSpan") out << ",\"minimum\":1,\"maximum\":256";
            if (name == "Grid.Row" || name == "Grid.Column") out << ",\"minimum\":0,\"maximum\":255";
            if (name == "Ratio") { out << ",\"minimum\":1,\"maximum\":99"; }
            else if (name == "Spacing" || name == "Padding") { out << ",\"minimum\":0,\"maximum\":64"; }
            else if (described && visual_layout::dimension(property)) { out << ",\"minimum\":0,\"maximum\":10000000"; }
            else if (name == "Weight") { out << ",\"minimum\":0,\"maximum\":1024"; }

            out << '}';
        }
        out << "],\"children\":[";
        comma = false;
        for (int c = UI_STACK_PANEL; c <= UI_PATH; ++c) {
            const auto child = static_cast<ui_node_kind>(c);
            if (legacy_element(child) || !accepts_child(kind, 0, child)) { continue; }
            if (std::exchange(comma, true)) { out << ','; }
            out << quote(control_name(child));
        }
        out << "]}";
    };
    out << "},\"elements\":[";
    bool element_comma = false;
    for (int k = UI_STACK_PANEL; k <= UI_PATH; ++k) {
        const auto kind = static_cast<ui_node_kind>(k);
        if (legacy_element(kind)) { continue; }
        if (std::exchange(element_comma, true)) { out << ','; }
        element_json(kind);
    }
    // Code wrappers still use these declarations for programmatic construction.
    out << "],\"legacyElements\":[";
    element_json(UI_ICON);
    out << ',';
    element_json(UI_ICON_BUTTON);
    out << "],\"documents\":[{\"name\":\"Window\",\"attributes\":";
    attributes_json(out, fleck::schema::window_attributes);
    out << ",\"maxChildren\":1},{\"name\":\"UserControl\",\"attributes\":";
    attributes_json(out, fleck::schema::component_attributes);
    out << ",\"maxChildren\":1},{\"name\":\"ControlTemplate\",\"attributes\":";
    attributes_json(out, fleck::schema::component_attributes);
    out << ",\"maxChildren\":1},{\"name\":\"Theme\",\"attributes\":";
    attributes_json(out, fleck::schema::theme_attributes);
    out << ",\"children\":[\"Resource\",\"DockTemplate\"]},{\"name\":\"Resource\",\"attributes\":";
    attributes_json(out, fleck::schema::resource_attributes);
    out << ",\"children\":[]},{\"name\":\"DockTemplate\",\"attributes\":";
    attributes_json(out, fleck::schema::dock_template_attributes);
    out << ",\"minChildren\":1,\"maxChildren\":1,\"children\":";
    strings_json(out, fleck::schema::template_names);
    out << "}],\"templateElements\":{\"names\":";
    strings_json(out, fleck::schema::template_names);
    out << ",\"attributes\":";
    attributes_json(out, fleck::schema::template_attributes);
    out << ",\"elements\":[";
    bool comma = false;
    for (const auto name : fleck::schema::template_names) {
        if (std::exchange(comma, true)) { out << ','; }
        out << "{\"name\":" << quote(name) << ",\"attributes\":";
        attributes_json(out, fleck::schema::template_attributes, name == "Button");
        out << ",\"children\":";
        strings_json(out, fleck::schema::template_names);
        out << '}';
    }
    out << "]},\"themeResources\":{\"colors\":";
    strings_json(out, fleck::schema::colors);
    out << ",\"colorPrefixes\":[\"dock.\",\"custom.\"],\"colorPattern\":\"^#[0-9A-Fa-f]{6}$\",\"metrics\":";
    std::vector<fleck::schema::Attribute> metrics;
    for (const auto& metric : fleck::schema::palette_metrics) {
        metrics.push_back({metric.name, "integer", {}, static_cast<int>(metric.low), static_cast<int>(metric.high)});
    }
    metrics.insert(metrics.end(), std::begin(fleck::schema::dock_metrics), std::end(fleck::schema::dock_metrics));
    metrics.insert(metrics.end(), std::begin(fleck::schema::shape_metrics), std::end(fleck::schema::shape_metrics));
    attributes_json(out, metrics);
    out << R"(},"binding":{"options":["Path","Mode","UpdateSourceTrigger","StringFormat","Converter","ElementName","RelativeSource","AncestorType"],"modes":["Default","OneWay","OneTime","TwoWay"],"triggers":["PropertyChanged","LostFocus"]}})";
    return out.str();
}
std::string model_json(ui_model* model, const char* name)
{
    std::ostringstream out;
    out << "{\"version\":1,\"id\":\"" << model->debug_id << "\",\"name\":" << quote(name ? name : "") << ",\"properties\":[";
    bool comma = false;
    for (const auto& [path, property] : model->properties) {
        if (std::exchange(comma, true)) { out << ','; }
        out << "{\"path\":" << quote(path) << ",\"type\":" << quote(value_type(property.kind)) << ",\"writable\":" << (property.set ? "true" : "false")
            << ",\"source\":{\"uri\":" << quote(property.source_uri) << ",\"line\":" << property.source_line << ",\"column\":" << property.source_column << "}";
        if (property.kind == UI_VALUE_COMMAND && property.command_modes) {
            out << ",\"commandModes\":[";
            if (property.command_modes & UI_COMMAND_SUPPORTS_SYNC) { out << "\"Sync\""; }
            if (property.command_modes == (UI_COMMAND_SUPPORTS_SYNC | UI_COMMAND_SUPPORTS_ASYNC)) { out << ','; }
            if (property.command_modes & UI_COMMAND_SUPPORTS_ASYNC) { out << "\"Async\""; }
            out << ']';
        }
        out << '}';
    }
    return out.str() + "]}";
}
}

ui_result ui_fleck_get_schema(char* output, size_t* size)
{
    try { return devtools::copy_json(devtools::schema_json(), output, size); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_fleck_validate(const char* text, size_t size, ui_fleck_diagnostic* diagnostic)
{
    ui_fleck_diagnostic local{};
    auto& result = diagnostic ? *diagnostic : local;
    result = {};
    if (!text) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        const std::string_view input(text, size);
        fleck::Page page;
        if (!fleck::parse(input, page, result)) {
            ui_fleck_diagnostic theme_diagnostic{};
            try { (void)docking::parse_theme(input, theme_diagnostic); result = {}; return UI_OK; }
            catch (const std::range_error&) {
                if (input.find("<Theme") != std::string_view::npos) { result = theme_diagnostic; }
                return UI_ERROR_INVALID_ARGUMENT;
            }
        }
        const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
        ui_app* app = nullptr;
        const auto created = ui_app_create(&options, &app);
        if (created != UI_OK) { return created; }
        std::unique_ptr<ui_app, decltype(&ui_app_destroy)> owner(app, ui_app_destroy);
        page.document_kind = "Window";
        return fleck::install(app, std::move(page), input, result, false);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_describe(ui_model* model, const char* name, char* output, size_t* size)
{
    if (!model) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    try {
        if (name) { (void)utf8_text::decode(name); }
        return devtools::copy_json(devtools::model_json(model, name), output, size);
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_set_property_source(ui_model* model, const char* path, const char* uri, uint32_t line, uint32_t column)
{
    if (!model || !path || !uri) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (model->callbacks || model->notifying) { return UI_ERROR_INVALID_STATE; }
    try {
        (void)utf8_text::decode(uri);
        const auto found = model->properties.find(path);
        if (found == model->properties.end()) { return UI_ERROR_INVALID_ARGUMENT; }
        found->second.source_uri = uri;
        found->second.source_line = line;
        found->second.source_column = column;
        return UI_OK;
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_add_properties(ui_model* model, const ui_model_property* properties, size_t count)
{
    if (!model || (count && !properties)) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (model->callbacks || model->notifying || count > 512 - model->properties.size()) { return UI_ERROR_INVALID_STATE; }
    try {
        auto previous = model->properties;
        for (size_t i = 0; i < count; ++i) {
            const auto status = ui_model_add_property(model, &properties[i]);
            if (status != UI_OK) { model->properties.swap(previous); return status; }
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_set_command_modes(ui_model* model, const char* path, uint32_t modes)
{
    if (!model || !path || (modes & ~(UI_COMMAND_SUPPORTS_SYNC | UI_COMMAND_SUPPORTS_ASYNC))) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    if (model->callbacks || model->notifying) { return UI_ERROR_INVALID_STATE; }
    const auto found = model->properties.find(path);
    if (found == model->properties.end() || found->second.kind != UI_VALUE_COMMAND) { return UI_ERROR_INVALID_ARGUMENT; }
    found->second.command_modes = modes;
    return UI_OK;
}

std::string fleck::fingerprint(const Page& page)
{
    // Content identity for stale-sidecar detection; not a cryptographic signature.
    std::ostringstream output;
    output << std::hex << resource_fingerprint(page);
    return output.str();
}
std::string fleck::source_map(const Page& page, std::string_view uri)
{
    using docking::json::quote;
    std::string result = "{\"version\":1,\"uri\":" + quote(uri) + ",\"model\":" + quote(page.model) +
        ",\"fingerprint\":" + quote(fingerprint(page)) + ",\"nodes\":[";
    bool comma = false;
    const auto append = [&](const auto& self, const Element& element) -> void {
        if (std::exchange(comma, true)) { result += ','; }
        auto location = element.location; location.source_uri = element.source_uri;
        result += "{\"kind\":" + std::to_string(element.kind) + ",\"id\":" + quote(element.id) +
            ",\"location\":" + devtools::location_json(nullptr, location) + "}";
        for (const auto& child : element.children) { self(self, child); }
    };
    append(append, page.root);
    return result + "]}";
}
