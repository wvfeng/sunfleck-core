#include "components.h"
#include "dock_json.h"
#include "ui_tree.h"
#include <bit>
#include <stdexcept>

namespace {
constexpr char resource_header[]={'F','L','E','C','K',char(UI_FLECK_RESOURCE_VERSION)};
constexpr std::string_view resource_magic(resource_header,sizeof(resource_header));
template<bool HashOnly = false> struct Writer {
    std::string bytes;
    uint64_t hash = 14695981039346656037ull;
    size_t length = 0;
    void append(std::string_view value) {
        if constexpr (HashOnly) {
            length += value.size();
            for (unsigned char byte : value) hash = (hash ^ byte) * 1099511628211ull;
        } else bytes.append(value);
    }
    void number(uint32_t value) {
        char encoded[4];
        for (unsigned n = 0; n < 32; n += 8) {
            encoded[n / 8] = char(value >> n);
        }
        append({encoded, 4});
    }
    void string(const std::string &value) {
        if (value.size() > 1024 * 1024) {
            throw std::range_error("Component resource string limit");
        }
        number(uint32_t(value.size()));
        append(value);
    }
    void real(double value) {
        const auto bits=std::bit_cast<uint64_t>(value); number(uint32_t(bits)); number(uint32_t(bits>>32));
    }
    void span(const devtools::Span &s) {
        number(uint32_t(s.offset));
        number(uint32_t(s.length));
        number(s.line);
        number(s.column);
        number(s.end_line);
        number(s.end_column);
    }
    void node(const fleck::Element &n) {
        number(n.kind);
        string(n.text);
        string(n.id);
        string(n.anchor);
        number(n.value);
        number(n.has_value);
        number(n.enabled);
        number(n.async_command);
        real(n.height);
        real(n.layout.width);
        real(n.layout.min_width); real(n.layout.max_width);
        real(n.layout.min_height); real(n.layout.max_height);
        string(n.layout.margin_text);
        number(n.layout.horizontal | (n.layout.horizontal_set ? 4u : 0u)); number(n.layout.vertical);
        number(n.weight);
        real(n.spacing);
        real(n.padding);
        number(n.dock);
        number(n.dock_capabilities);
        number(n.dock_min_width);
        number(n.dock_min_height);
        string(n.dock_icon);
        string(n.type_namespace);
        string(n.type_name);
        string(n.template_namespace);
        string(n.template_name);
        string(n.source_uri);
        number(uint32_t(n.offset));
        span(n.location.element);
        number(uint32_t(n.location.attributes.size()));
        for (const auto &a : n.location.attributes) {
            string(a.name);
            span(a.range);
            span(a.value);
        }
        number(uint32_t(n.properties.size()));
        for (const auto &[name, value] : n.properties) {
            string(name);
            string(value);
        }
        number(uint32_t(n.bindings.size()));
        for (const auto &b : n.bindings) {
            number(b.property);
            string(b.path);
            string(b.format);
            number(b.mode);
            number(b.trigger);
            number(b.source);
            string(b.target);
        }
        number(uint32_t(n.children.size()));
        for (const auto &child : n.children) {
            node(child);
        }
    }
    void page(const fleck::Page &p) {
        string(p.document_kind);
        string(p.class_name);
        string(p.type_namespace);
        string(p.target_namespace);
        string(p.target_name);
        string(p.source_uri);
        string(p.component_manifest);
        string(p.title);
        string(p.model);
        number(p.width);
        number(p.height);
        node(p.root);
        number(uint32_t(p.resources.size()));
        for (const auto &resource : p.resources) {
            page(resource);
        }
    }
};
struct Reader {
    std::string_view bytes;
    bool has_height = false, has_layout = false;
    unsigned nodes = 0, documents = 0;
    uint32_t number() {
        if (bytes.size() < 4) {
            throw std::range_error("Truncated component resource");
        }
        uint32_t value = 0;
        for (unsigned n = 0; n < 4; ++n) {
            value |= uint32_t((unsigned char)bytes[n]) << (8 * n);
        }
        bytes.remove_prefix(4);
        return value;
    }
    uint32_t count(uint32_t limit) {
        const auto value = number();
        if (value > limit || value > bytes.size()) {
            throw std::range_error("Component resource limit exceeded");
        }
        return value;
    }
    double real() {
        const uint64_t low=number(),high=number();
        return std::bit_cast<double>(low|(high<<32));
    }
    std::string string(size_t limit = 16384) {
        auto size = number();
        if (size > limit || size > bytes.size() ||
            bytes.substr(0, size).find('\0') != bytes.npos) {
            throw std::range_error("Invalid component resource string");
        }
        std::string value(bytes.substr(0, size));
        bytes.remove_prefix(size);
        return value;
    }
    bool flag() {
        auto value = number();
        if (value > 1) {
            throw std::range_error("Invalid resource boolean");
        }
        return value != 0;
    }
    devtools::Span span() {
        devtools::Span s;
        s.offset = number();
        s.length = number();
        s.line = number();
        s.column = number();
        s.end_line = number();
        s.end_column = number();
        return s;
    }
    fleck::Element node(unsigned depth) {
        if (depth > 64 || ++nodes > 16384) {
            throw std::range_error("Component node limit exceeded");
        }
        fleck::Element n;
        auto kind = number();
        if (kind > UI_PATH) {
            throw std::range_error("Unknown resource node kind");
        }
        n.kind = ui_node_kind(kind);
        n.text = string();
        n.id = string(64);
        n.anchor = string(64);
        n.value = number();
        n.has_value = flag();
        n.enabled = flag();
        n.async_command = flag();
        n.height = has_height ? real() : -1;
        if (has_layout) {
            n.layout.width = real();
            n.layout.min_width = real(); n.layout.max_width = real();
            n.layout.min_height = real(); n.layout.max_height = real();
            auto margin = string(128);
            const auto horizontal = number();
            n.layout.horizontal = horizontal & 3u; n.layout.horizontal_set = (horizontal & 4u) != 0;
            n.layout.vertical = number();
            if (!visual_layout::valid(n.layout) ||
                horizontal > 7 || n.layout.vertical > 3 || !visual_layout::literal(n.layout, UI_PROPERTY_MARGIN, margin))
                throw std::range_error("Invalid resource layout");
        }
        n.weight = number();
        n.spacing = real();
        n.padding = real();
        auto dock = number();
        if (!(n.height==-1 || (std::isfinite(n.height) && n.height>=0 && n.height<=visual_layout::maximum_extent)) || n.weight > 1024 || !visual_layout::valid_metric(UI_PROPERTY_SPACING,n.spacing) ||
            !visual_layout::valid_metric(UI_PROPERTY_PADDING,n.padding) ||
            dock > UI_DOCK_BOTTOM) {
            throw std::range_error("Invalid resource layout");
        }
        n.dock = ui_dock_edge(dock);
        n.dock_capabilities = number();
        n.dock_min_width = number();
        n.dock_min_height = number();
        n.dock_icon = string(64);
        if (n.dock_capabilities > 7 || n.dock_min_width < 32 ||
            n.dock_min_width > visual_layout::maximum_extent || n.dock_min_height < 32 ||
            n.dock_min_height > visual_layout::maximum_extent) {
            throw std::range_error("Invalid resource Dock options");
        }
        n.type_namespace = string();
        n.type_name = string(64);
        n.template_namespace = string();
        n.template_name = string(64);
        n.source_uri = string();
        n.offset = number();
        n.location.element = span();
        for (auto size = count(256); size; --size) {
            devtools::Attribute a;
            a.name = string(128);
            a.range = span();
            a.value = span();
            n.location.attributes.push_back(std::move(a));
        }
        for (auto size = count(16384); size; --size) {
            auto name = string(256);
            auto value = string();
            if (!n.properties.emplace(name, value).second) {
                throw std::range_error("Duplicate component property");
            }
        }
        std::set<std::string> targets;
        for (auto size = count(256); size; --size) {
            fleck::Binding b;
            auto property = number();
            b.path = string(128);
            b.format = string();
            auto mode = number();
            auto trigger = number();
            auto source = number();
            b.target = string(128);
            if (property > UI_PROPERTY_SOURCE || mode > UI_BIND_DEFAULT ||
                trigger > UI_UPDATE_LOST_FOCUS ||
                source > fleck::Binding::Ancestor ||
                !binding_path(b.path)) {
                throw std::range_error("Invalid resource binding");
            }
            b.property = ui_property(property);
            b.mode = ui_binding_mode(mode);
            b.trigger = ui_update_source_trigger(trigger);
            b.source = fleck::Binding::Source(source);
            auto key = b.target.empty()
                           ? std::to_string(property == UI_PROPERTY_CONTENT
                                                ? UI_PROPERTY_TEXT
                                                : property)
                           : b.target;
            if (!targets.insert(key).second) {
                throw std::range_error("Duplicate binding target");
            }
            n.bindings.push_back(std::move(b));
        }
        for (auto size = count(16384); size; --size) {
            n.children.push_back(node(depth + 1));
        }
        if ((!n.id.empty() && !fleck::valid_binding_name(n.id)) ||
            (!n.anchor.empty() && !fleck::valid_binding_name(n.anchor)) ||
            (n.type_name.empty() != n.type_namespace.empty()) ||
            (!n.type_name.empty() &&
             (n.kind != UI_PANEL || n.children.size() > 1)) ||
            (n.type_name.empty() &&
             ((!n.template_name.empty() && !fleck::native_template(n)) || std::any_of(n.properties.begin(), n.properties.end(), [&](const auto& property) {
                 if (drawing::shape_kind(n.kind)) {
                     if (property.first=="Data") return n.kind!=UI_PATH || !drawing::parse_path(property.second);
                     if (property.first=="Fill" || property.first=="Stroke") {
                         BindingValue color; return binding_parse_value(property.second,UI_VALUE_COLOR,color)!=UI_OK;
                     }
                     if (property.first==".Fill" || property.first==".Stroke") {
                         drawing::Brush brush; return !fleck::read_drawing(property.second,&brush,nullptr);
                     }
                     if (property.first=="StrokeThickness") {
                         double width; return !numeric_literal::real_literal(property.second,width) || width<0 || width>4096;
                     }
                 }
                 if (property.first.starts_with(".DataPath.") || property.first.starts_with(".DataValue.")) {
                     const bool path = property.first.starts_with(".DataPath.");
                     auto index = std::string_view(property.first).substr(path ? 10 : 11);
                     unsigned order = 0;
                     auto result = std::from_chars(index.data(),index.data()+index.size(),order);
                     return result.ec != std::errc{} || result.ptr != index.data()+index.size() || order >= 512 ||
                            (path && !binding_path(property.second));
                 }
                 if (property.first.starts_with(".Trigger.")) {
                     ui_node::TriggerSetter trigger{};
                     return !fleck::trigger_setter(n.kind,std::string_view(property.first).substr(9),property.second,
                         trigger.order,trigger.conditions,trigger.property,trigger.value);
                 }
                 if (property.first.starts_with(".Resource.")) return !fleck::valid_binding_name(std::string_view(property.first).substr(10));
                 if (property.first.starts_with(".DynamicStyle.") || property.first.starts_with(".DynamicLocal.")) {
                     ui_property target;
                     return !fleck::dynamic_style_target(n.kind,std::string_view(property.first).substr(14),property.second,target);
                 }
                 if (property.first == ".StyleKey") return !fleck::valid_binding_name(property.second);
                 if (property.first.starts_with(".Style.")) {
                     ui_property target;
                     BindingValue value;
                     return !fleck::style_value(n.kind, std::string_view(property.first).substr(7), property.second, target, value);
                 }
                 if (property.first.starts_with(".CommandBinding.")) return !fleck::valid_binding_name(std::string_view(property.first).substr(16)) || !binding_path(property.second);
                 if (property.first.starts_with(".PointerBinding.")) return !fleck::pointer_binding(property.first) || !fleck::valid_binding_name(property.second);
                 if (property.first.starts_with(".KeyBinding.")) return !fleck::key_binding_code(property.first) || !fleck::valid_binding_name(property.second);
                 if (property.first == ".RoutedCommand") return !is_interactive(n.kind) || !fleck::valid_binding_name(property.second);
                 if (property.first.starts_with(".State.")) {
                     const auto key = std::string_view(property.first).substr(7);
                     const auto split = key.find('/');
                     std::vector<animation::Track> tracks;
                     return split == key.npos || !fleck::valid_binding_name(key.substr(0,split)) ||
                         !fleck::valid_binding_name(key.substr(split+1)) ||
                         (!property.second.empty() && !fleck::read_storyboard(property.second,n.kind,tracks));
                 }
                 if (property.first.starts_with(".Column")) {
                     auto dot = property.first.rfind('.');
                     if (dot == 0 || n.kind!=UI_DATA_GRID) return true;
                     auto index = std::string_view(property.first).substr(dot+1);
                     uint32_t column;
                     auto parsed = std::from_chars(index.data(),index.data()+index.size(),column);
                     if (parsed.ec!=std::errc{} || parsed.ptr!=index.data()+index.size() || column>=512) return true;
                     auto name = property.first.substr(0,dot);
                     if (name==".ColumnPath") return !binding_path(property.second);
                     if (name==".ColumnHeader") return false;
                     if (name==".ColumnReadOnly") return property.second!="true";
                     double width;
                     return name!=".ColumnWidth" || !numeric_literal::real_literal(property.second,width) || width<16 || width>4096;
                 }
                 if (property.first==".ChildrenPath" || property.first==".TreeLabelPath") return n.kind!=UI_TREE_VIEW || !binding_path(property.second);
                 if (property.first==".ItemTemplate") return !devtools::attribute_allowed(n.kind,"ItemsSource");
                 if (property.first.starts_with(".BindingSource.")) {
                     return !fleck::valid_binding_name(property.second) || std::none_of(n.bindings.begin(),n.bindings.end(),[&](const auto& b){
                         return b.source>=fleck::Binding::NamedElement && property.first==".BindingSource."+std::to_string(b.property);
                     });
                 }
                 if (property.first.starts_with(".Conversion.")) {
                     fleck::Conversion conversion;
                     return property.first.substr(12)!=fleck::conversion_path(property.second) || !fleck::parse_conversion(property.second,conversion);
                 }
                 if (property.first.starts_with(".ItemTemplateKey.")) return property.first.size()==17 || (n.kind!=UI_LIST_VIEW && n.kind!=UI_ITEMS_CONTROL);
                 if (property.first==".Storyboard") {
                     std::vector<animation::Track> tracks;
                     return !fleck::read_storyboard(property.second,n.kind,tracks);
                 }
                 if (property.first==".Effect") {
                     ui_effect_desc effect{};
                     return !fleck::read_drawing(property.second,nullptr,&effect);
                 }
                 if (property.first==".OpacityMask" || property.first==".Background" || property.first==".BorderBrush") {
                     drawing::Brush brush;
                     return (property.first!=".OpacityMask" && n.kind!=UI_BORDER) || !fleck::read_drawing(property.second,&brush,nullptr);
                 }
                 if (property.first == "Orientation")
                     return (n.kind != UI_STACK_PANEL && n.kind != UI_WRAP_PANEL && n.kind != UI_SEPARATOR) ||
                         (property.second != "Vertical" && property.second != "Horizontal");
                 if (property.first == "LayoutTransform" || property.first == "RenderTransform" || property.first == "RenderTransformOrigin") {
                     BindingValue value;
                     return binding_parse_value(property.second,property.first != "RenderTransformOrigin" ? UI_VALUE_MATRIX : UI_VALUE_POINT,value) != UI_OK;
                 }
                 if (property.first == "Stretch") {
                     ui_stretch value;
                     return (n.kind != UI_VIEWBOX && n.kind != UI_IMAGE && n.kind != UI_ICON) || !visual_layout::parse_stretch(property.second,value);
                 }
                 if (property.first == "DragText") return !devtools::attribute_allowed(n.kind,"DragText");
                 if (property.first == "AllowDrop") return !is_editor(n.kind) || (property.second!="true" && property.second!="false");
                 if (property.first == "Opacity") {
                     ui_binding_value parsed{};
                     return ui_value_parse(UI_VALUE_DOUBLE,property.second.c_str(),&parsed,nullptr,0) != UI_OK || parsed.number < 0 || parsed.number > 1;
                 }
                 if (visual_layout::scroll_policy_name(property.first)) {
                     ui_scroll_policy value;
                     return n.kind != UI_SCROLL_VIEW || !visual_layout::parse_scroll_policy(property.second,value);
                 }
                 if (visual_layout::canvas_coordinate(property.first)) {
                     double value;
                     return !visual_layout::parse_coordinate(property.second,value);
                 }
                 if (property.first == "Panel.ZIndex") {
                     int32_t value;
                     return !grid_layout::parse_z_index(property.second,value);
                 }
                 if (grid_layout::attribute(property.first)) {
                     grid_layout::Settings parsed;
                     return !grid_layout::assign(parsed,n.kind==UI_GRID,property.first,property.second);
                 }
                 if (property.first == "DataContext") return property.second != "null";
                 if (property.first == "Source") return n.kind != UI_IMAGE || property.second.size() > 1024;
                 if ((n.kind == UI_DOCK_DOCUMENT || n.kind == UI_DOCK_TOOL) &&
                     (property.first == "MinWidth" || property.first == "MinHeight")) {
                     auto declared=n.layout;
                     return !visual_layout::literal(declared,visual_layout::find(property.first),property.second) || declared!=n.layout;
                 }
                 ui_property target; BindingValue parsed;
                 if (properties::find(property.first,target) && text_format::property(target)) {
                     ui_binding_value font{}; return !text_format::parse(target,property.second,font);
                 }
                 if (properties::find(property.first,target) && visual_state::property(target)) {
                     ui_binding_value state{}; return !visual_state::parse(target,property.second,state);
                 }
                 return n.kind != UI_BORDER || !properties::find(property.first, target) || !border::property(target) ||
                     binding_parse_value(property.second, border::kind(target), parsed) != UI_OK || !border::valid(target, parsed.value);
             }) || (std::any_of(n.bindings.begin(), n.bindings.end(), [&](const auto& binding) {
                 return n.properties.contains(properties::name(binding.property));
             }))))) {
            throw std::range_error("Invalid component declaration");
        }
        return n;
    }
    fleck::Page page(unsigned depth = 0) {
        if (depth > 1 || ++documents > 1025) {
            throw std::range_error("Component document limit exceeded");
        }
        fleck::Page p;
        p.document_kind = string(32);
        p.class_name = string(128);
        p.type_namespace = string();
        p.target_namespace = string();
        p.target_name = string(64);
        p.source_uri = string();
        p.component_manifest = string(1024 * 1024);
        p.title = string();
        p.model = string(128);
        p.width = int(number());
        p.height = int(number());
        if (p.width < 1 || p.height < 1 || p.width > 4096 || p.height > 4096 ||
            (p.document_kind != "Window" && p.document_kind != "UserControl" &&
             p.document_kind != "ControlTemplate")) {
            throw std::range_error("Invalid component document");
        }
        if (p.document_kind != "Window" &&
            (!binding_path(p.class_name) || p.type_namespace.empty())) {
            throw std::range_error("Missing component identity");
        }
        p.root = node(0);
        for (auto size = count(1024); size; --size) {
            p.resources.push_back(page(depth + 1));
        }
        return p;
    }
};
} // namespace
bool fleck::has_components(const Page &page) {
    if (page.document_kind != "Window" || !page.resources.empty()) {
        return true;
    }
    auto visit = [&](auto &self, const Element &n) -> bool {
        const bool explicit_default = std::any_of(n.location.attributes.begin(), n.location.attributes.end(), [&](const auto& attribute) {
            ui_property property;
            if (!properties::find(attribute.name, property)) return false;
            if (std::any_of(n.bindings.begin(), n.bindings.end(), [&](const auto& binding) {
                return properties::canonical(binding.property) == properties::canonical(property);
            })) return false; // Binding declarations already survive the legacy format.
            if (property >= UI_PROPERTY_HEIGHT || property == UI_PROPERTY_IS_ENABLED) return true;
            return (property == UI_PROPERTY_TEXT || property == UI_PROPERTY_CONTENT) && n.text.empty() &&
                std::none_of(n.bindings.begin(), n.bindings.end(), [](const auto& binding) {
                    return properties::canonical(binding.property) == UI_PROPERTY_TEXT;
                });
        });
        if (!n.properties.empty() || explicit_default || !n.type_name.empty() || native_template(n) || n.height >= 0 || n.layout != VisualLayout{} || std::any_of(n.bindings.begin(), n.bindings.end(), [](const auto& b) { return b.property >= UI_PROPERTY_HEIGHT; })) {
            return true;
        }
        for (const auto &b : n.bindings) {
            if (b.source != Binding::Context) {
                return true;
            }
        }
        for (const auto &c : n.children) {
            if (self(self, c)) {
                return true;
            }
        }
        return false;
    };
    return visit(visit, page.root);
}
std::string fleck::compile_components(const Page &page) {
    Writer<> writer;
    writer.append(resource_magic);
    writer.page(page);
    if (writer.bytes.size() > 9 * 1024 * 1024) {
        throw std::range_error("Component resource exceeds 9 MiB");
    }
    return writer.bytes;
}
uint64_t fleck::resource_fingerprint(const Page& page) {
    Writer<true> writer;
    writer.append(resource_magic);
    writer.page(page);
    if (writer.length > 9 * 1024 * 1024) throw std::range_error("Component resource exceeds 9 MiB");
    return writer.hash;
}
fleck::Page fleck::read_components(std::string_view bytes) {
    if (!bytes.starts_with(resource_magic) || bytes.size() > 9 * 1024 * 1024) {
        throw std::range_error("Invalid component resource");
    }
    Reader reader{bytes.substr(6), true, true};
    auto page = reader.page();
    if (!reader.bytes.empty()) {
        throw std::range_error("Trailing component resource bytes");
    }
    return page;
}
ui_result fleck::install_components(ui_app *app, Page page, ui_model *model,
                                    const ui_fleck_data_context *context,
                                    ui_fleck_diagnostic &diagnostic) {
    auto &registry = components::registry(app);
    auto previous = registry.documents;
    auto previous_types = registry.types;
    try {
        if (!page.component_manifest.empty()) {
            const auto manifest =
                docking::json::Reader(page.component_manifest).parse();
            for (const auto &type : manifest.at("types").array()) {
                if (type.has("register") &&
                    !registry.types.contains({type.at("namespace").string(),
                                              type.at("name").string()})) {
                    throw std::range_error("Component behavior must be "
                                           "registered before loading: " +
                                           type.at("name").string());
                }
            }
        }
        if (!page.component_manifest.empty() &&
            ui_app_register_component_manifest(
                app, page.component_manifest.data(),
                page.component_manifest.size(), &diagnostic) != UI_OK) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        std::set<components::Key> seen;
        for (auto &document : page.resources) {
            if (document.document_kind == "Window") {
                throw std::range_error("Window cannot be a component resource");
            }
            components::Key key{document.type_namespace,
                                document.class_name.substr(
                                    document.class_name.find_last_of('.') + 1)};
            if (!seen.insert(key).second ||
                (registry.documents.contains(key) &&
                 registry.documents.at(key).source_uri !=
                     document.source_uri)) {
                throw std::range_error("Duplicate component identity");
            }
            registry.documents.insert_or_assign(key, std::move(document));
        }
        page.resources.clear();
        auto status = fleck::install(app, std::move(page), {}, diagnostic, true,
                                     context, model);
        if (status != UI_OK) {
            registry.documents.swap(previous);
            registry.types.swap(previous_types);
        }
        return status;
    } catch (...) {
        registry.documents.swap(previous);
        registry.types.swap(previous_types);
        throw;
    }
}

ui_result ui_app_register_component_resource(ui_app *app, const void *bytes,
                                             size_t size,
                                             ui_fleck_diagnostic *diagnostic) {
    if (auto status = binding_app_check(app); status != UI_OK) {
        return status;
    }
    if (!bytes) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    ui_fleck_diagnostic local{};
    if (!diagnostic) {
        diagnostic = &local;
    }
    *diagnostic = {};
    if (auto status = fleck::check_resource_version({static_cast<const char*>(bytes), size}, *diagnostic); status != UI_OK) return status;
    try {
        auto document =
            fleck::read_components({static_cast<const char *>(bytes), size});
        if (document.document_kind == "Window") {
            throw std::range_error("Expected a compiled component definition");
        }
        auto next = components::registry(app).documents;
        auto add = [&](fleck::Page page) {
            if (page.document_kind == "Window") {
                throw std::range_error("Invalid component resource dependency");
            }
            components::Key key{
                page.type_namespace,
                page.class_name.substr(page.class_name.find_last_of('.') + 1)};
            if (next.contains(key) &&
                next.at(key).source_uri != page.source_uri) {
                throw std::range_error("Duplicate component resource");
            }
            page.resources.clear();
            next.insert_or_assign(key, std::move(page));
        };
        for (const auto &resource : document.resources) {
            add(resource);
        }
        add(std::move(document));
        components::registry(app).documents.swap(next);
        return UI_OK;
    } catch (const std::bad_alloc &) {
        return UI_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception &error) {
        fleck::report({}, 0, UI_FLECK_VALUE, error.what(), *diagnostic);
        return UI_ERROR_INVALID_ARGUMENT;
    }
}
