#pragma once
#include <sunfleck/ui.h>
#include <cmath>
#include <iomanip>
#include <locale>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>

namespace sunfleck::metadata {
struct Source { const char *file; int line; };
struct Type {
    ui_component_type descriptor;
    const char *class_name;
    const char *base_type;
    Source source;
    const char *registration = "";
    std::span<const Source> properties = {};
};
inline void string(std::ostream &out, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out << '\\' << char(c); }
        else if (c < 32) { out << "\\u00" << hex[c >> 4] << hex[c & 15]; }
        else { out << char(c); }
    }
    out << '"';
}
inline void string(std::ostream &out, const char *value) { string(out, std::string_view(value ? value : "")); }
inline void source(std::ostream &out, Source value) {
    out << "{\"uri\":"; string(out, value.file);
    out << ",\"line\":" << value.line << ",\"column\":1}";
}
inline const char *nodeName(ui_node_kind kind) {
    static constexpr const char *names[] = {
        "StackPanel", "Flex", "Grid", "ScrollView", "Panel", "Card",
        "Button", "TextBlock", "Image", "CheckBox", "RadioButton",
        "Switch", "Slider", "Separator", "ProgressBar", "Popup", "ComboBox",
        "TextBox", "ItemsControl", "ListView", "ListItem", "TabView", "TabItem",
        "Overlay", "WrapPanel", "DockPanel", "SplitView", "Label", "Icon", "Badge",
        "IconButton", "ToggleButton", "Hyperlink", "SpinBox", "PasswordBox",
        "TextArea", "SearchBox", "TreeView", "DataGrid", "MenuBar", "ToolBar",
        "NavigationView", "Dialog", "Tooltip", "ContextMenu", "Spinner", "InfoBar",
        "TreeItem", "DockHost", "DockSplit", "DockGroup", "DockDocument", "DockTool", "DocumentView"
    };
    static_assert(sizeof(names) / sizeof(*names) == UI_DOCUMENT_VIEW + 1);
    if (kind < UI_STACK_PANEL || kind > UI_DOCUMENT_VIEW) { throw std::invalid_argument("Unknown template part kind"); }
    return names[kind];
}
// Serializes declarations only. Does not call lifecycle/change callbacks or create an App.
inline void write(std::ostream &out, std::span<const Type> types) {
    out.imbue(std::locale::classic());
    out << "{\"version\":1,\"types\":[";
    bool first = true;
    for (const auto &entry : types) {
        const auto &t = entry.descriptor;
        if (!first) { out << ','; } first = false;
        out << "{\"namespace\":"; string(out, t.namespace_uri);
        out << ",\"name\":"; string(out, t.name);
        out << ",\"className\":"; string(out, entry.class_name);
        out << ",\"baseType\":"; string(out, entry.base_type);
        out << ",\"source\":"; source(out, entry.source);
        if (entry.registration && *entry.registration) { out << ",\"register\":"; string(out, entry.registration); }
        out << ",\"templateNamespace\":"; string(out, t.template_namespace);
        out << ",\"template\":"; string(out, t.template_name);
        out << ",\"properties\":[";
        for (size_t i = 0; i < t.property_count; ++i) {
            const auto &p = t.properties[i];
            if (i) { out << ','; }
            out << "{\"name\":"; string(out, p.name);
            static constexpr const char *kinds[] = {"string", "bool", "int", "double", "command", "model", "color", "thickness", "cornerRadius", "point", "size", "matrix", "length", "enum"};
            if (p.default_value.kind < UI_VALUE_STRING || p.default_value.kind > UI_VALUE_ENUM) { throw std::invalid_argument("Unknown property type"); }
            out << ",\"type\":"; string(out, kinds[p.default_value.kind]);
            out << ",\"mode\":"; string(out, p.default_mode == UI_BIND_TWO_WAY ? "TwoWay" : p.default_mode == UI_BIND_ONE_TIME ? "OneTime" : "OneWay");
            out << ",\"readOnly\":" << p.read_only;
            if (p.metadata) {
                if ((p.metadata->validate || p.metadata->coerce) && (!entry.registration || !*entry.registration))
                    throw std::invalid_argument("Property callbacks require an executable component registrar");
                out << ",\"flags\":" << p.metadata->flags;
            }
            if (i < entry.properties.size()) { out << ",\"source\":"; source(out, entry.properties[i]); }
            switch (p.default_value.kind) {
                case UI_VALUE_STRING: out << ",\"default\":"; string(out, p.default_value.string); break;
                case UI_VALUE_BOOL: case UI_VALUE_INT: out << ",\"default\":" << p.default_value.integer; break;
                case UI_VALUE_DOUBLE:
                    if (!std::isfinite(p.default_value.number)) { throw std::invalid_argument("Non-finite property default"); }
                    out << ",\"default\":\"" << std::setprecision(17) << p.default_value.number << '"'; break;
                case UI_VALUE_COLOR:
                    out << ",\"default\":\"#" << std::hex << std::setw(8) << std::setfill('0') << p.default_value.integer << std::dec << std::setfill(' ') << '"'; break;
                case UI_VALUE_ENUM: out << ",\"default\":\"" << p.default_value.integer << '"'; break;
                case UI_VALUE_LENGTH:
                    out << ",\"default\":\"";
                    if (p.default_value.integer == UI_LENGTH_AUTO) out << "Auto";
                    else if (p.default_value.integer == UI_LENGTH_INFINITY) out << "Infinity";
                    else out << std::setprecision(17) << p.default_value.number;
                    out << '"'; break;
                case UI_VALUE_THICKNESS: case UI_VALUE_CORNER_RADIUS: case UI_VALUE_POINT: case UI_VALUE_SIZE: case UI_VALUE_MATRIX: {
                    if (!p.default_value.geometry) throw std::invalid_argument("Missing geometry default");
                    const unsigned count = p.default_value.kind == UI_VALUE_MATRIX ? 6 : p.default_value.kind == UI_VALUE_POINT || p.default_value.kind == UI_VALUE_SIZE ? 2 : 4;
                    out << ",\"default\":\"";
                    for (unsigned j = 0; j < count; ++j) {
                        if (!std::isfinite(p.default_value.geometry[j])) throw std::invalid_argument("Non-finite geometry default");
                        if (j) out << ',';
                        out << std::setprecision(17) << p.default_value.geometry[j];
                    }
                    out << '"'; break;
                }
                default: break;
            }
            out << '}';
        }
        out << "],\"events\":[";
        for (size_t i = 0; i < t.event_count; ++i) { if (i) { out << ','; } string(out, t.events[i]); }
        out << "],\"parts\":[";
        for (size_t i = 0; i < t.part_count; ++i) {
            if (i) { out << ','; }
            out << "{\"name\":"; string(out, t.parts[i].name);
            out << ",\"kind\":"; string(out, nodeName(t.parts[i].kind));
            out << ",\"required\":" << t.parts[i].required << '}';
        }
        out << "]}";
    }
    out << "]}\n";
}
}
