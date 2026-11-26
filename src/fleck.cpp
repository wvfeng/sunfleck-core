#include "fleck.h"
#include "utf8_text.h"
#include "ui_tree.h"
#include "dock.h"
#include "fleck_schema.h"

#include <algorithm>
#include <charconv>
#include <cstdio>

namespace fleck {
bool parse_conversion(std::string_view encoded, Conversion& conversion)
{
    while (!encoded.empty()) {
        auto end = encoded.find(' ');
        auto part = encoded.substr(0,end);
        if (!binding_path(part)) return false;
        if (conversion.converter.empty()) conversion.converter = part;
        else conversion.inputs.emplace_back(part);
        if (conversion.inputs.size()>64) return false;
        if (end==encoded.npos) break;
        encoded.remove_prefix(end+1);
    }
    return !conversion.inputs.empty();
}
std::string conversion_path(std::string_view encoded)
{
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : encoded) hash = (hash ^ c) * 1099511628211ull;
    return "_FleckConvert_" + std::to_string(hash);
}
namespace {
struct Failure {
    std::size_t offset;
    ui_fleck_error code;
    const char* message;
};

bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool name_char(char c)
{
    return letter(c) || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == ':';
}

struct Attribute {
    std::string_view name;
    std::string value;
    std::size_t offset;
    size_t length = 0, value_offset = 0, value_length = 0;
};

Binding parse_binding(const Attribute& attribute, ui_property property, Element* owner=nullptr)
{
    auto reject = [&]() -> void { throw Failure{attribute.offset, UI_FLECK_VALUE, "Invalid Binding expression or option"}; };
    std::string expression = attribute.value;
    if (expression.starts_with("{TemplateBinding ") && expression.back() == '}') {
        expression = "{Binding " + expression.substr(17, expression.size() - 18) + ", RelativeSource=TemplatedParent, Mode=OneWay}";
    }
    std::string_view input = expression;
    if (!input.starts_with("{Binding") || input.size() < 10 || input.back() != '}' || !space(input[8])) { reject(); }
    input = input.substr(9, input.size() - 10);
    const auto trim = [](std::string_view s) {
        while (!s.empty() && space(s.front())) { s.remove_prefix(1); }
        while (!s.empty() && space(s.back())) { s.remove_suffix(1); }
        return s;
    };
    Binding result;
    std::string converter;
    std::string element_name, ancestor_type;
    result.property = property;
    unsigned seen = 0;
    while (!input.empty()) {
        char quote = 0;
        bool escape = false;
        size_t end = 0;
        for (; end < input.size(); ++end) {
            const char c = input[end];
            if (escape) { escape = false; continue; }
            if (c == '\\') { escape = true; continue; }
            if (quote) { if (c == quote) { quote = 0; } }
            else if (c == '\'' || c == '"') { quote = c; }
            else if (c == ',') { break; }
        }
        if (quote || escape) { reject(); }
        auto part = trim(input.substr(0, end));
        const auto equal = part.find('=');
        auto key = equal == part.npos ? std::string_view("Path") : trim(part.substr(0, equal));
        auto raw = trim(equal == part.npos ? part : part.substr(equal + 1));
        if (raw.empty()) { reject(); }
        if (raw.front() == '\'' || raw.front() == '"') {
            if (raw.size() < 2 || raw.back() != raw.front()) { reject(); }
            raw = raw.substr(1, raw.size() - 2);
        }
        std::string value;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\') { if (++i == raw.size()) { reject(); } }
            value += raw[i];
        }
        unsigned flag = 0;
        if (key == "Path") { flag = 1; result.path = value; }
        else if (key == "Mode") {
            flag = 2;
            if (value == "Default") { result.mode = UI_BIND_DEFAULT; }
            else if (value == "OneWay") { result.mode = UI_BIND_ONE_WAY; }
            else if (value == "OneTime") { result.mode = UI_BIND_ONE_TIME; }
            else if (value == "TwoWay") { result.mode = UI_BIND_TWO_WAY; }
            else { reject(); }
        } else if (key == "UpdateSourceTrigger") {
            flag = 4;
            if (value == "PropertyChanged") { result.trigger = UI_UPDATE_PROPERTY_CHANGED; }
            else if (value == "LostFocus") { result.trigger = UI_UPDATE_LOST_FOCUS; }
            else { reject(); }
        } else if (key == "RelativeSource") {
            flag = 16;
            if (value == "Self") { result.source = Binding::Self; }
            else if (value == "TemplatedParent") { result.source = Binding::TemplatedParent; }
            else if (value == "FindAncestor") { result.source = Binding::Ancestor; }
            else { reject(); }
        } else if (key=="ElementName" && owner && valid_binding_name(value)) {
            flag=64; element_name=value;
        } else if (key=="AncestorType" && owner && valid_binding_name(value)) {
            flag=128; ancestor_type=value;
        } else if (key == "Converter" && owner && binding_path(value)) { flag = 32; converter = value; }
        else if (key == "StringFormat") { flag = 8; result.format = value; }
        else { reject(); }
        if (seen & flag) { reject(); }
        seen |= flag;
        if (end == input.size()) { input = {}; }
        else { input.remove_prefix(end + 1); if (trim(input).empty()) { reject(); } }
    }
    if (!binding_path(result.path)) { reject(); }
    if (!element_name.empty()) {
        if (seen & 16) reject();
        result.source=Binding::NamedElement;
    }
    if ((result.source==Binding::Ancestor)!=!ancestor_type.empty()) reject();
    if (result.source==Binding::Ancestor || result.source==Binding::NamedElement) {
        if (!owner) reject();
        owner->properties[".BindingSource."+std::to_string(property)] = element_name.empty() ? ancestor_type : element_name;
    }
    if (!converter.empty()) {
        if (result.mode==UI_BIND_TWO_WAY || result.source!=Binding::Context) reject();
        auto encoded=converter+" "+result.path;
        result.path=conversion_path(encoded);
        owner->properties[".Conversion."+result.path]=std::move(encoded);
        if (result.mode==UI_BIND_DEFAULT) result.mode=UI_BIND_ONE_WAY;
    }
    return result;
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input), source_index_(input) {}

    std::string_view document_root()
    {
        if (take("\xef\xbb\xbf")) {
            // The UTF-8 BOM is optional.
        }
        if (take("<?xml")) {
            if (offset_ == input_.size() || !space(input_[offset_])) {
                fail(UI_FLECK_SYNTAX, "Invalid XML declaration");
            }
            const auto declaration = attributes();
            if (declaration.empty() || declaration.front().name != "version" ||
                declaration.front().value != "1.0") {
                fail(UI_FLECK_VALUE, "Expected XML version 1.0");
            }
            for (std::size_t i = 1; i < declaration.size(); ++i) {
                const auto& a = declaration[i];
                if (a.name != "encoding" || (a.value != "UTF-8" && a.value != "utf-8")) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Fleck requires UTF-8"};
                }
            }
            expect("?>");
        }
        trivia();
        open_tag();
        return name();
    }
    const DictionarySources* dictionaries = nullptr;
    std::string_view source;
    size_t* imported_bytes = nullptr;
    const Parser* importer = nullptr;

    Page document()
    {
        Page page;
        const auto root_tag = document_root();
        if (root_tag == "ResourceDictionary") {
            page.document_kind = root_tag;
            std::map<std::string,std::string> declared;
            declared_literals_ = &declared;
            dictionary_document(1);
            return page;
        }
        if (root_tag != "Window" && root_tag != "UserControl" && root_tag != "ControlTemplate") {
            fail(UI_FLECK_ELEMENT, "Expected Window, UserControl or ControlTemplate root");
        }
        page.document_kind = root_tag;
        std::string target;
        bool has_namespace = false;
        for (const auto& a : attributes()) {
            if (a.name == "xmlns") {
                if (a.value != "urn:sunfleck:v1") {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Unsupported Fleck namespace"};
                }
                has_namespace = true;
                namespaces_[""] = a.value;
            } else if (a.name.starts_with("xmlns:")) {
                namespaces_[std::string(a.name.substr(6))] = a.value;
            } else if (root_tag != "Window" && a.name == "Class") {
                if (!binding_path(a.value)) { fail(UI_FLECK_VALUE, "Invalid component Class"); }
                page.class_name = a.value;
            } else if (root_tag != "Window" && a.name == "Namespace") {
                page.type_namespace = a.value;
            } else if (root_tag == "ControlTemplate" && a.name == "TargetType") {
                target = a.value;
            } else if (a.name == "Model") {
                if (!valid_binding_name(a.value)) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Invalid model name"};
                }
                page.model = a.value;
            } else if (a.name == "Title") {
                page.title = a.value;
            } else if (a.name == "Width") {
                page.width = number(a, schema::window_attributes);
            } else if (a.name == "Height") {
                page.height = number(a, schema::window_attributes);
            } else {
                throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown Window attribute"};
            }
        }
        if (!has_namespace) {
            fail(UI_FLECK_ATTRIBUTE, "Window requires xmlns");
        }
        expect(">");
        trivia();
        if (root_tag != "Window" && (page.class_name.empty() || page.type_namespace.empty())) {
            fail(UI_FLECK_ATTRIBUTE, "Component requires Class and Namespace");
        }
        if (root_tag == "ControlTemplate") {
            auto qualified = qualify(target);
            page.target_namespace = qualified.first; page.target_name = qualified.second;
        }
        std::map<std::string,std::string> page_literals;
        resources_property(root_tag,page_literals);
        page.root = element(2);
        for (const auto& [key,value] : page_literals) page.root.properties.try_emplace(".Resource."+key,value);
        if (is_inline(page.root.kind)) fail(UI_FLECK_ELEMENT,"Run and Span require a TextBlock or Span parent");
        trivia();
        close_tag(root_tag);
        trivia();
        if (offset_ != input_.size()) {
            fail(UI_FLECK_SYNTAX, "Unexpected data after Window");
        }
        return page;
    }

    docking::Theme theme_document()
    {
        docking::Theme theme;
        if (take("\xef\xbb\xbf")) { }
        if (take("<?xml")) {
            if (offset_ == input_.size() || !space(input_[offset_])) { fail(UI_FLECK_SYNTAX, "Invalid XML declaration"); }
            const auto declaration = attributes();
            if (declaration.empty() || declaration.front().name != "version" || declaration.front().value != "1.0") { fail(UI_FLECK_VALUE, "Expected XML version 1.0"); }
            for (size_t i = 1; i < declaration.size(); ++i) {
                if (declaration[i].name != "encoding" || (declaration[i].value != "UTF-8" && declaration[i].value != "utf-8")) { fail(UI_FLECK_VALUE, "Fleck requires UTF-8"); }
            }
            expect("?>");
        }
        trivia(); open_tag();
        if (name() != "Theme") { fail(UI_FLECK_ELEMENT, "Expected Theme root"); }
        ui_builtin_theme base = UI_BUILTIN_SUNFLECK_LITE;
        bool dark = false, compact = false;
        for (const auto& a : attributes()) {
            if (a.name == "Version") {
                if (a.value != "1") { throw Failure{a.offset, UI_FLECK_VALUE, "Unsupported theme version"}; }
            } else if (a.name == "Base") {
                const auto& bases = schema::bases;
                const auto found = std::find(std::begin(bases), std::end(bases), a.value);
                if (found == std::end(bases)) { throw Failure{a.offset, UI_FLECK_VALUE, "Unknown base theme"}; }
                base = static_cast<ui_builtin_theme>(found - std::begin(bases));
            } else if (a.name == "Dark" || a.name == "Compact") {
                if (a.value != "true" && a.value != "false") { throw Failure{a.offset, UI_FLECK_VALUE, "Expected true or false"}; }
                (a.name == "Dark" ? dark : compact) = a.value == "true";
            } else if (a.name == "Skin") {
                if (!valid_binding_name(a.value)) { throw Failure{a.offset, UI_FLECK_VALUE, "Invalid skin name"}; } theme.skin = a.value;
            } else if (a.name != "xmlns" || a.value != "urn:sunfleck:v1") { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown Theme attribute"}; }
        }
        theme.palette = builtin_palette(base, dark, compact); theme.shape = builtin_themes[base].shape;
        if (take("/>")) { trivia(); if (offset_ != input_.size()) { fail(UI_FLECK_SYNTAX, "Trailing theme data"); } return theme; }
        expect(">"); trivia();
        std::set<std::string> resources;
        while (!input_.substr(offset_).starts_with("</")) {
            if (++nodes_ > 512) { fail(UI_FLECK_LIMIT, "Too many theme resources"); }
            open_tag(); const auto tag = name();
            if (tag == "Resource") {
                std::string key, value;
                for (const auto& a : attributes()) {
                    if (a.name == "Key") { key = a.value; }
                    else if (a.name == "Value") { value = a.value; }
                    else { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown Resource attribute"}; }
                }
                if (key.empty() || value.empty() || !resources.insert(key).second) { fail(UI_FLECK_VALUE, "Resource requires unique Key and Value"); }
                if (key.starts_with("dock.") || key.starts_with("custom.")) {
                    if (value.size() == 7 && value.front() == '#') {
                        uint32_t rgb = 0; const auto parsed = std::from_chars(value.data() + 1, value.data() + value.size(), rgb, 16);
                        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) { fail(UI_FLECK_VALUE, "Invalid theme color"); }
                        theme.colors[key] = rgb;
                    } else {
                        const auto* metric = schema::find(schema::dock_metrics, key);
                        if (!metric) { fail(UI_FLECK_VALUE, "Unknown Dock metric"); }
                        const Attribute number_attribute{"Value", value, offset_};
                        theme.metrics[key] = number(number_attribute, metric->minimum, metric->maximum);
                    }
                } else if (const auto* metric = schema::find(schema::shape_metrics, key)) {
                    const Attribute number_attribute{"Value", value, offset_}; const int radius = number(number_attribute, metric->minimum, metric->maximum);
                    if (key == "button_radius") { theme.shape.button_radius = radius; }
                    else if (key == "input_radius") { theme.shape.input_radius = radius; }
                    else if (key == "card_radius") { theme.shape.card_radius = radius; }
                    else { theme.shape.popup_radius = radius; }
                } else if (ui_theme_apply_overrides(&theme.palette, (key + '=' + value).c_str()) != UI_OK) { fail(UI_FLECK_VALUE, "Invalid theme resource"); }
                expect("/>");
            } else if (tag == "DockTemplate") {
                ui_dock_part part = UI_DOCK_PART_COUNT; unsigned state = 0;
                for (const auto& a : attributes()) {
                    if (a.name == "Part") {
                        const auto& names = schema::parts;
                        const auto found = std::find(std::begin(names), std::end(names), a.value);
                        if (found == std::end(names)) { throw Failure{a.offset, UI_FLECK_VALUE, "Unknown Dock template part"}; }
                        part = static_cast<ui_dock_part>(found - std::begin(names));
                    } else if (a.name == "State") {
                        size_t start = 0;
                        while (start < a.value.size()) {
                            const auto end = a.value.find('|', start); const auto name = a.value.substr(start, end == std::string::npos ? end : end - start);
                            const auto found = std::find(std::begin(schema::states), std::end(schema::states), name);
                            if (found == std::end(schema::states)) { throw Failure{a.offset, UI_FLECK_VALUE, "Unknown template state"}; }
                            state |= schema::state_flags[found - std::begin(schema::states)];
                            if (end == std::string::npos) { break; } start = end + 1;
                        }
                    } else { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown DockTemplate attribute"}; }
                }
                if (part == UI_DOCK_PART_COUNT) { fail(UI_FLECK_ATTRIBUTE, "DockTemplate requires Part"); }
                expect(">"); trivia(); auto element = template_element(0); trivia(); close_tag("DockTemplate");
                if (!theme.templates.emplace(std::pair{part, state}, std::move(element)).second) { fail(UI_FLECK_VALUE, "Duplicate Dock template"); }
            } else { fail(UI_FLECK_ELEMENT, "Unknown theme element"); }
            trivia();
        }
        close_tag("Theme"); trivia(); if (offset_ != input_.size()) { fail(UI_FLECK_SYNTAX, "Trailing theme data"); }
        const auto validate = [&](const auto& self, const docking::TemplateElement& node) -> void {
            for (const auto* resource : {&node.background, &node.foreground}) {
                if (resource->empty() || *resource == "{Background}" || *resource == "{Foreground}" || *resource == "{Border}") { continue; }
                if (resource->front() == '#') {
                    unsigned value = 0;
                    const auto parsed = std::from_chars(resource->data() + 1, resource->data() + resource->size(), value, 16);
                    if (resource->size() != 7 || parsed.ec != std::errc{} || parsed.ptr != resource->data() + resource->size()) { fail(UI_FLECK_VALUE, "Invalid template color"); }
                    continue;
                }
                const auto& colors = schema::colors;
                if (!theme.colors.contains(*resource) && std::find(std::begin(colors), std::end(colors), *resource) == std::end(colors)) { fail(UI_FLECK_VALUE, "Template references an unknown resource"); }
            }
            for (const auto& child : node.children) { self(self, child); }
        };
        for (const auto& [key, element] : theme.templates) { (void)key; validate(validate, element); }
        return theme;
    }

    void drawing_fragment(drawing::Brush* brush,ui_effect_desc* effect) {
        drawing_object(brush,effect);
        trivia();
        if (offset_!=input_.size()) fail(UI_FLECK_SYNTAX,"Unexpected content after drawing object");
    }
    void storyboard_fragment(ui_node_kind kind,std::vector<animation::Track>& tracks) {
        storyboard_object(kind,tracks);
        trivia();
        if (offset_ != input_.size()) fail(UI_FLECK_SYNTAX,"Unexpected content after Storyboard");
    }
private:
    std::string_view input_;
    devtools::SourceIndex source_index_;
    std::size_t offset_ = 0;
    unsigned nodes_ = 0;
    std::map<std::string, std::string> namespaces_;
    struct Style {
        std::string key, target_type, based_on;
        std::map<std::string,std::string> setters;
        struct Trigger {
            std::string data_path, data_value;
            std::map<std::string,std::string> conditions;
            std::map<std::string,std::string> setters;
        };
        std::vector<Trigger> triggers;
    };
    std::map<std::string,Style> styles_;
    std::pair<std::string, std::string> qualify(std::string_view name) {
        const auto colon = name.find(':');
        if (name.empty() || colon == 0 || colon + 1 == name.size()) { fail(UI_FLECK_VALUE, "Expected a namespace-qualified type"); }
        const auto found = namespaces_.find(colon == name.npos ? std::string() : std::string(name.substr(0, colon)));
        if (found == namespaces_.end()) { fail(UI_FLECK_VALUE, "Unknown component namespace prefix"); }
        return {found->second, std::string(colon == name.npos ? name : name.substr(colon + 1))};
    }

    [[noreturn]] void fail(ui_fleck_error code, const char* message) const
    {
        throw Failure{offset_, code, message};
    }

    bool take(std::string_view token)
    {
        if (!input_.substr(offset_).starts_with(token)) {
            return false;
        }
        offset_ += token.size();
        return true;
    }

    void expect(std::string_view token)
    {
        if (!take(token)) {
            fail(UI_FLECK_SYNTAX, "Unexpected XML token");
        }
    }

    void whitespace()
    {
        while (offset_ < input_.size() && space(input_[offset_])) {
            ++offset_;
        }
    }

    void trivia()
    {
        for (;;) {
            whitespace();
            if (!take("<!--")) {
                break;
            }
            const auto end = input_.find("--", offset_);
            if (end == std::string_view::npos || !input_.substr(end).starts_with("-->")) {
                fail(UI_FLECK_SYNTAX, "Invalid XML comment");
            }
            offset_ = end + 3;
        }
    }

    void open_tag()
    {
        if (input_.substr(offset_).starts_with("<!") || input_.substr(offset_).starts_with("<?")) {
            fail(UI_FLECK_FORBIDDEN, "DTD, declarations and processing instructions are forbidden here");
        }
        expect("<");
    }

    std::string_view name()
    {
        const auto start = offset_;
        if (offset_ == input_.size() || !letter(input_[offset_])) {
            fail(UI_FLECK_SYNTAX, "Expected XML name");
        }
        while (offset_ < input_.size() && name_char(input_[offset_])) {
            ++offset_;
        }
        return input_.substr(start, offset_ - start);
    }

    std::string quoted()
    {
        if (offset_ == input_.size() || (input_[offset_] != '"' && input_[offset_] != '\'')) {
            fail(UI_FLECK_SYNTAX, "Expected quoted attribute");
        }
        const char quote = input_[offset_++];
        std::string result;
        while (offset_ < input_.size() && input_[offset_] != quote) {
            const char c = input_[offset_++];
            if (c == '<') {
                fail(UI_FLECK_SYNTAX, "Unescaped less-than in attribute");
            }
            if (c != '&') {
                if (c == '\r' && offset_ < input_.size() && input_[offset_] == '\n') {
                    ++offset_;
                }
                result.push_back(space(c) ? ' ' : c);
                continue;
            }
            const auto end = input_.find(';', offset_);
            if (end == std::string_view::npos || end - offset_ > 12) {
                fail(UI_FLECK_SYNTAX, "Invalid character reference");
            }
            const auto ref = input_.substr(offset_, end - offset_);
            if (ref == "amp") {
                result.push_back('&');
            } else if (ref == "lt") {
                result.push_back('<');
            } else if (ref == "gt") {
                result.push_back('>');
            } else if (ref == "quot") {
                result.push_back('"');
            } else if (ref == "apos") {
                result.push_back('\'');
            } else if (ref.starts_with("#")) {
                const bool hex = ref.starts_with("#x");
                const auto digits = ref.substr(hex ? 2 : 1);
                unsigned scalar = 0;
                const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(),
                                                    scalar, hex ? 16 : 10);
                if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
                    !xml_scalar(scalar)) {
                    fail(UI_FLECK_SYNTAX, "Invalid XML character reference");
                }
                append_scalar(result, scalar);
            } else {
                fail(UI_FLECK_FORBIDDEN, "Only predefined and numeric references are allowed");
            }
            offset_ = end + 1;
        }
        if (offset_ == input_.size()) {
            fail(UI_FLECK_SYNTAX, "Unclosed attribute");
        }
        ++offset_;
        return result;
    }

    static bool xml_scalar(unsigned value)
    {
        return value == 9 || value == 10 || value == 13 ||
            (value >= 32 && value <= 0xd7ff) ||
            (value >= 0xe000 && value <= 0xfffd) ||
            (value >= 0x10000 && value <= 0x10ffff);
    }

    static void append_scalar(std::string& out, unsigned value)
    {
        if (value < 0x80) {
            out.push_back(static_cast<char>(value));
            return;
        }
        const int count = value < 0x800 ? 1 : value < 0x10000 ? 2 : 3;
        constexpr unsigned prefixes[] = {0, 0xc0, 0xe0, 0xf0};
        out.push_back(static_cast<char>(prefixes[count] | (value >> (6 * count))));
        for (int i = count - 1; i >= 0; --i) {
            out.push_back(static_cast<char>(0x80 | ((value >> (6 * i)) & 0x3f)));
        }
    }

    std::vector<Attribute> attributes()
    {
        std::vector<Attribute> values;
        for (;;) {
            const auto previous = offset_;
            whitespace();
            if (offset_ == input_.size() || !letter(input_[offset_])) {
                return values;
            }
            if (offset_ == previous) {
                fail(UI_FLECK_SYNTAX, "Expected whitespace before attribute");
            }
            const auto start = offset_;
            const auto key = name();
            if (std::any_of(values.begin(), values.end(), [&](const auto& a) { return a.name == key; })) {
                throw Failure{start, UI_FLECK_ATTRIBUTE, "Duplicate attribute"};
            }
            if (values.size() == 16) {
                fail(UI_FLECK_LIMIT, "Too many attributes");
            }
            whitespace();
            expect("=");
            whitespace();
            const auto value_start = offset_ + 1;
            auto value = quoted();
            values.push_back({key, std::move(value), start, offset_ - start, value_start, offset_ - value_start - 1});
        }
    }

    static int number(const Attribute& a, int low, int high)
    {
        int value = 0;
        const auto* end = a.value.data() + a.value.size();
        const auto parsed = std::from_chars(a.value.data(), end, value);
        if (parsed.ec != std::errc{} || parsed.ptr != end || value < low || value > high) {
            throw Failure{a.offset, UI_FLECK_VALUE, "Integer attribute outside allowed range"};
        }
        return value;
    }

    static int number(const Attribute& a, std::span<const schema::Attribute> rules)
    {
        const auto* rule = schema::find(rules, a.name);
        if (!rule || rule->minimum < 0) { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown integer attribute"}; }
        return number(a, rule->minimum, rule->maximum);
    }

    void close_tag(std::string_view expected)
    {
        expect("</");
        if (name() != expected) {
            fail(UI_FLECK_SYNTAX, "Mismatched closing tag");
        }
        whitespace();
        expect(">");
    }

    docking::TemplateElement template_element(unsigned depth)
    {
        if (depth > 16 || ++nodes_ > 512) { fail(UI_FLECK_LIMIT, "Theme template limit exceeded"); }
        open_tag(); const auto tag = name(); docking::TemplateElement node;
        const auto found = std::find(std::begin(schema::template_names), std::end(schema::template_names), tag);
        if (found == std::end(schema::template_names)) { fail(UI_FLECK_ELEMENT, "Unknown template element"); }
        static_assert(docking::TemplateElement::Stack + 1 == std::size(schema::template_names));
        node.kind = static_cast<docking::TemplateElement::Kind>(found - std::begin(schema::template_names));
        for (const auto& a : attributes()) {
            if (a.name == "Background") { node.background = a.value; }
            else if (a.name == "Foreground") { node.foreground = a.value; }
            else if (a.name == "Text") { node.text = a.value; }
            else if (a.name == "Role") {
                if (std::find(std::begin(schema::roles), std::end(schema::roles), a.value) == std::end(schema::roles)) { throw Failure{a.offset, UI_FLECK_VALUE, "Unknown Dock template role"}; }
                node.role = a.value;
            } else if (a.name == "Orientation") {
                if (std::find(std::begin(schema::orientations), std::end(schema::orientations), a.value) == std::end(schema::orientations)) { throw Failure{a.offset, UI_FLECK_VALUE, "Invalid template orientation"}; }
                node.horizontal = a.value == "Horizontal";
            } else if (a.name == "Radius") { node.radius = number(a, schema::template_attributes); }
            else if (a.name == "Thickness") { node.thickness = number(a, schema::template_attributes); }
            else if (a.name == "Width") { node.width = number(a, schema::template_attributes); }
            else if (a.name == "Height") { node.height = number(a, schema::template_attributes); }
            else if (a.name == "Padding") { node.padding = number(a, schema::template_attributes); }
            else { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown template attribute"}; }
        }
        if (node.kind == docking::TemplateElement::Button && node.role != "Close" && node.role != "Pin" && node.role != "Float" && node.role != "Menu") { fail(UI_FLECK_ATTRIBUTE, "Template Button requires Close, Pin, Float or Menu Role"); }
        if (take("/>")) { return node; }
        expect(">"); trivia();
        while (!input_.substr(offset_).starts_with("</")) { node.children.push_back(template_element(depth + 1)); trivia(); }
        close_tag(tag); return node;
    }

    void drawing_object(drawing::Brush* brush,ui_effect_desc* effect) {
        open_tag();
        const auto tag=name();
        const auto attrs=attributes();
        ui_brush_desc desc{};
        desc.end={1,1};
        desc.stretch=UI_STRETCH_FILL;
        std::vector<uint8_t> image;
        std::vector<ui_gradient_stop> stops;
        if (effect) {
            if (tag=="BlurEffect") *effect={UI_EFFECT_BLUR,5,{},{}};
            else if (tag=="DropShadowEffect") *effect={UI_EFFECT_DROP_SHADOW,5,{3,3},{0xff000000}};
            else fail(UI_FLECK_ELEMENT,"Expected BlurEffect or DropShadowEffect");
        } else if (tag=="SolidColorBrush") desc.kind=UI_BRUSH_SOLID;
        else if (tag=="LinearGradientBrush") desc.kind=UI_BRUSH_LINEAR;
        else if (tag=="RadialGradientBrush") { desc.kind=UI_BRUSH_RADIAL; desc.start=desc.end={.5,.5}; }
        else if (tag=="ImageBrush") desc.kind=UI_BRUSH_IMAGE;
        else fail(UI_FLECK_ELEMENT,"Unknown brush type");
        const auto scalar=[](const Attribute& a) {
            double value;
            if (!numeric_literal::real_literal(a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Expected finite number"};
            return value;
        };
        const auto color=[](const Attribute& a) {
            BindingValue value;
            if (binding_parse_value(a.value,UI_VALUE_COLOR,value)!=UI_OK) throw Failure{a.offset,UI_FLECK_VALUE,"Expected color"};
            return ui_color{uint32_t(value.value.integer)};
        };
        const auto point=[](const Attribute& a) {
            BindingValue value;
            if (binding_parse_value(a.value,UI_VALUE_POINT,value)!=UI_OK) throw Failure{a.offset,UI_FLECK_VALUE,"Expected point"};
            return ui_point{value.geometry[0],value.geometry[1]};
        };
        for (const auto& a:attrs) {
            if (effect && a.name=="Radius") effect->radius=scalar(a);
            else if (tag=="DropShadowEffect" && a.name=="Offset") effect->offset=point(a);
            else if (tag=="DropShadowEffect" && a.name=="Color") effect->color=color(a);
            else if (tag=="SolidColorBrush" && a.name=="Color") desc.color=color(a);
            else if (tag=="LinearGradientBrush" && a.name=="StartPoint") desc.start=point(a);
            else if (tag=="LinearGradientBrush" && a.name=="EndPoint") desc.end=point(a);
            else if (tag=="RadialGradientBrush" && a.name=="Center") desc.start=point(a);
            else if (tag=="RadialGradientBrush" && a.name=="RadiusX") desc.end.x=scalar(a);
            else if (tag=="RadialGradientBrush" && a.name=="RadiusY") desc.end.y=scalar(a);
            else if (tag=="ImageBrush" && a.name=="Width") desc.width=number(a,1,4096);
            else if (tag=="ImageBrush" && a.name=="Height") desc.height=number(a,1,4096);
            else if (tag=="ImageBrush" && a.name=="Stretch" && visual_layout::parse_stretch(a.value,desc.stretch)) {}
            else if (tag=="ImageBrush" && a.name=="Rgba") {
                if (a.value.size()%8) throw Failure{a.offset,UI_FLECK_VALUE,"Expected packed RGBA8 hex pixels"};
                for(size_t i=0;i<a.value.size();i+=2) {
                    unsigned byte;
                    const char* at=a.value.data()+i;
                    auto r=std::from_chars(at,at+2,byte,16);
                    if (r.ec!=std::errc{} || r.ptr!=at+2) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid hex pixel"};
                    image.push_back(uint8_t(byte));
                }
            } else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Invalid drawing object attribute"};
        }
        if (!take("/>")) {
            expect(">"); trivia();
            while (!input_.substr(offset_).starts_with("</")) {
                if (effect || (desc.kind!=UI_BRUSH_LINEAR && desc.kind!=UI_BRUSH_RADIAL))
                    fail(UI_FLECK_ELEMENT,"This drawing object accepts no children");
                open_tag();
                if (name()!="GradientStop") fail(UI_FLECK_ELEMENT,"Expected GradientStop");
                ui_gradient_stop stop{};
                const auto values=attributes();
                if(values.size()!=2 || stops.size()>=256) fail(UI_FLECK_VALUE,"GradientStop requires Offset and Color (256 stops maximum)");
                for (const auto& a:values) {
                    if (a.name=="Offset") stop.offset=scalar(a);
                    else if (a.name=="Color") stop.color=color(a);
                    else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Unknown GradientStop attribute"};
                }
                if (!take("/>")) { expect(">"); trivia(); close_tag("GradientStop"); }
                stops.push_back(stop); trivia();
            }
            close_tag(tag);
        }
        if (effect) {
            if (!drawing::valid_effect(*effect)) fail(UI_FLECK_VALUE,"Invalid effect");
        } else {
            if (desc.kind==UI_BRUSH_IMAGE && image.size()!=size_t(desc.width)*desc.height*4)
                fail(UI_FLECK_VALUE,"ImageBrush pixel count differs from its dimensions");
            desc.rgba=image.data(); desc.stops=stops.data(); desc.stop_count=uint32_t(stops.size());
            auto result=drawing::Brush::copy(desc);
            if (!result) fail(UI_FLECK_VALUE,"Invalid brush");
            *brush=std::move(*result);
        }
    }

    std::string named_state_object(std::string_view tag) {
        open_tag();
        if (name() != tag) fail(UI_FLECK_ELEMENT,"Unexpected visual state element");
        const auto attrs = attributes();
        if (attrs.size() != 1 || attrs.front().name != "Name" || !valid_binding_name(attrs.front().value))
            fail(UI_FLECK_ATTRIBUTE,"Expected a visual state Name");
        return attrs.front().value;
    }

    void visual_state_groups(Element& node) {
        std::set<std::string> groups;
        whitespace(); expect(">"); trivia();
        while (!input_.substr(offset_).starts_with("</")) {
            auto group = named_state_object("VisualStateGroup");
            if (!groups.insert(group).second) fail(UI_FLECK_VALUE,"Duplicate VisualStateGroup");
            expect(">"); trivia();
            while (!input_.substr(offset_).starts_with("</")) {
                auto state = named_state_object("VisualState");
                std::string storyboard;
                if (!take("/>")) {
                    expect(">"); trivia();
                    if (!input_.substr(offset_).starts_with("</")) {
                        const auto begin = offset_;
                        std::vector<animation::Track> tracks;
                        storyboard_object(node.kind,tracks);
                        storyboard = input_.substr(begin,offset_-begin);
                        trivia();
                    }
                    close_tag("VisualState");
                }
                if (!node.properties.emplace(".State."+group+"/"+state,std::move(storyboard)).second)
                    fail(UI_FLECK_VALUE,"Duplicate VisualState");
                trivia();
            }
            close_tag("VisualStateGroup"); trivia();
        }
    }

    void storyboard_object(ui_node_kind kind,std::vector<animation::Track>& tracks) {
        open_tag();
        if (name() != "Storyboard") fail(UI_FLECK_ELEMENT,"Expected Storyboard");
        whitespace(); expect(">"); trivia();
        while (!input_.substr(offset_).starts_with("</")) {
            open_tag();
            if (name() != "Timeline") fail(UI_FLECK_ELEMENT,"Expected Timeline");
            animation::Track track;
            ui_property_descriptor descriptor{sizeof(descriptor),1};
            bool has_duration = false;
            for (const auto& a : attributes()) {
                if (a.name == "Property") {
                    if (!properties::find(a.value,track.property) ||
                        ui_property_describe(kind,track.property,&descriptor) != UI_OK ||
                        !(descriptor.flags & UI_PROPERTY_ANIMATABLE))
                        throw Failure{a.offset,UI_FLECK_VALUE,"Expected an animatable property of the owner"};
                } else if (a.name == "DurationMs") {
                    track.timeline.duration_ms = number(a,1,INT32_MAX);
                    has_duration = true;
                } else if (a.name == "DelayMs") track.timeline.delay_ms = number(a,0,INT32_MAX);
                else if (a.name == "RepeatCount") track.timeline.repeat = number(a,0,INT32_MAX);
                else if (a.name == "AutoReverse" || a.name == "HoldEnd") {
                    if (a.value != "true" && a.value != "false") throw Failure{a.offset,UI_FLECK_VALUE,"Expected boolean"};
                    (a.name == "HoldEnd" ? track.hold : track.timeline.auto_reverse) = a.value == "true";
                } else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Unknown Timeline attribute"};
            }
            if (!descriptor.flags || !has_duration) fail(UI_FLECK_ATTRIBUTE,"Timeline requires Property and DurationMs");
            if (std::any_of(tracks.begin(),tracks.end(),[&](const auto& other){ return other.property == track.property; }))
                fail(UI_FLECK_VALUE,"Duplicate Storyboard target");
            expect(">"); trivia();
            auto& frames = track.timeline.frames;
            while (!input_.substr(offset_).starts_with("</")) {
                open_tag();
                if (name() != "KeyFrame") fail(UI_FLECK_ELEMENT,"Expected KeyFrame");
                animation::KeyFrame frame{};
                frame.position = -1;
                bool has_value = false;
                for (const auto& a : attributes()) {
                    if (a.name == "Position") {
                        if (!numeric_literal::real_literal(a.value,frame.position) || frame.position < 0 || frame.position > 1)
                            throw Failure{a.offset,UI_FLECK_VALUE,"Expected position between zero and one"};
                    } else if (a.name == "Value") {
                        if (binding_parse_value(a.value,descriptor.kind,frame.value) != UI_OK ||
                            (descriptor.kind == UI_VALUE_LENGTH && frame.value.value.integer != UI_LENGTH_PIXELS))
                            throw Failure{a.offset,UI_FLECK_VALUE,"Invalid key frame value"};
                        has_value = true;
                    } else if (a.name == "Easing") {
                        if (a.value == "EaseIn") frame.easing = animation::Easing::ease_in;
                        else if (a.value == "EaseOut") frame.easing = animation::Easing::ease_out;
                        else if (a.value == "EaseInOut") frame.easing = animation::Easing::ease_in_out;
                        else if (a.value != "Linear") throw Failure{a.offset,UI_FLECK_VALUE,"Unknown easing"};
                    } else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Unknown KeyFrame attribute"};
                }
                if (!has_value || (frames.empty() ? frame.position != 0 : frame.position <= frames.back().position))
                    fail(UI_FLECK_VALUE,"Expected an ordered key frame with Position and Value");
                if (!take("/>")) { expect(">"); trivia(); close_tag("KeyFrame"); }
                frames.push_back(std::move(frame)); trivia();
            }
            if (frames.empty() || frames.back().position != 1) fail(UI_FLECK_VALUE,"Last key frame must be at one");
            close_tag("Timeline"); trivia(); tracks.push_back(std::move(track));
        }
        if (tracks.empty()) fail(UI_FLECK_ELEMENT,"Expected at least one Timeline");
        close_tag("Storyboard");
    }

    ui_matrix transform_object(unsigned depth)
    {
        if (depth > 64) fail(UI_FLECK_LIMIT,"Transform nesting too deep");
        open_tag();
        const auto tag = name();
        const auto attrs = attributes();
        auto matrix = affine::identity;
        if (tag == "TransformGroup") {
            if (!attrs.empty()) fail(UI_FLECK_ATTRIBUTE,"Unexpected TransformGroup attribute");
            if (take("/>")) return matrix;
            expect(">"); trivia();
            while (!input_.substr(offset_).starts_with("</")) {
                matrix = affine::compose(matrix,transform_object(depth+1));
                trivia();
            }
            close_tag(tag);
        } else {
            const bool translate = tag == "TranslateTransform", scale = tag == "ScaleTransform";
            const bool rotate = tag == "RotateTransform", skew = tag == "SkewTransform";
            if (!translate && !scale && !rotate && !skew && tag != "MatrixTransform")
                fail(UI_FLECK_ELEMENT,"Unknown transform object");
            double x=0,y=0,angle=0,center_x=0,center_y=0;
            constexpr double radians = 3.14159265358979323846 / 180;
            for (const auto& attr : attrs) {
                if (tag == "MatrixTransform" && attr.name == "Matrix") {
                    BindingValue parsed;
                    if (binding_parse_value(attr.value,UI_VALUE_MATRIX,parsed) != UI_OK)
                        throw Failure{attr.offset,UI_FLECK_VALUE,"Expected six matrix coefficients"};
                    const auto& v = parsed.geometry;
                    matrix = {v[0],v[1],v[2],v[3],v[4],v[5]};
                    continue;
                }
                double number;
                if (!numeric_literal::real_literal(attr.value,number))
                    throw Failure{attr.offset,UI_FLECK_VALUE,"Expected finite transform coefficient"};
                if (translate && attr.name == "X") matrix.offset_x = number;
                else if (translate && attr.name == "Y") matrix.offset_y = number;
                else if (scale && attr.name == "ScaleX") matrix.m11 = number;
                else if (scale && attr.name == "ScaleY") matrix.m22 = number;
                else if (rotate && attr.name == "Angle") angle = number*radians;
                else if (skew && attr.name == "AngleX") x = number*radians;
                else if (skew && attr.name == "AngleY") y = number*radians;
                else if ((scale || rotate || skew) && attr.name == "CenterX") center_x = number;
                else if ((scale || rotate || skew) && attr.name == "CenterY") center_y = number;
                else throw Failure{attr.offset,UI_FLECK_ATTRIBUTE,"Unknown transform attribute"};
            }
            if (rotate) matrix = {std::cos(angle),std::sin(angle),-std::sin(angle),std::cos(angle),0,0};
            if (skew) { matrix.m12 = std::tan(y); matrix.m21 = std::tan(x); }
            matrix = affine::around(matrix,center_x,center_y,1);
            if (!take("/>")) { expect(">"); trivia(); close_tag(tag); }
        }
        for (double v : {matrix.m11,matrix.m12,matrix.m21,matrix.m22,matrix.offset_x,matrix.offset_y})
            if (!std::isfinite(v)) fail(UI_FLECK_VALUE,"Transform exceeds finite matrix range");
        return matrix;
    }

    std::map<std::string,std::string> literals_;

    std::string static_literal(const Attribute& attribute) const
    {
        if (!attribute.value.starts_with("{StaticResource ")) return attribute.value;
        if (attribute.value.back() != '}') throw Failure{attribute.offset, UI_FLECK_VALUE, "Invalid StaticResource"};
        auto found = literals_.find(attribute.value.substr(16, attribute.value.size()-17));
        if (found == literals_.end()) throw Failure{attribute.offset, UI_FLECK_VALUE, "Unknown static value resource"};
        return found->second;
    }

    std::map<std::string,std::string>* declared_literals_ = nullptr;

    void resources_property(std::string_view root_tag, std::map<std::string,std::string>& declared)
    {
        trivia();
        const std::string resources_tag = std::string(root_tag) + ".Resources";
        if (input_.substr(offset_).starts_with(std::string("<") + resources_tag)) {
            declared_literals_ = &declared;
            open_tag();
            if (std::string(name()) != resources_tag || !attributes().empty()) fail(UI_FLECK_ATTRIBUTE, "Resources does not accept attributes");
            if (!take("/>")) {
                expect(">"); trivia();
                if (take("<ResourceDictionary")) {
                    resource_dictionary(1);
                    trivia();
                } else resources_object();
                close_tag(resources_tag);
            }
            declared_literals_ = nullptr;
            trivia();
        }
    }

    void resolve_styles(const std::set<std::string>& keys)
    {
        for (const auto& key : keys) {
            auto entry = styles_.find(key);
            if (entry == styles_.end()) continue;
            auto& style = entry->second;
            const Style* current = &style;
            std::set<const Style*> seen;
            while (!current->based_on.empty()) {
                if (!seen.insert(current).second) fail(UI_FLECK_VALUE, "Style BasedOn cycle");
                auto base = styles_.find(current->based_on);
                if (base == styles_.end() || base->second.target_type != style.target_type)
                    fail(UI_FLECK_VALUE, "BasedOn requires a style of the same TargetType");
                current = &base->second;
                if (current == &style) fail(UI_FLECK_VALUE, "Style BasedOn cycle");
                if (current->triggers.size()+style.triggers.size() > 512) fail(UI_FLECK_LIMIT,"Too many inherited triggers");
                style.triggers.insert(style.triggers.begin(),current->triggers.begin(),current->triggers.end());
                for (const auto& [property, value] : current->setters) style.setters.try_emplace(property, value);
            }
            style.based_on.clear();
        }
    }

    // Merge declarations precede local entries. Each dictionary has its own
    // duplicate-key check; later dictionaries and local entries may shadow keys.
    void dictionary_document(unsigned depth)
    {
        const auto attrs = attributes();
        for (const auto& attr : attrs) {
            if (attr.name == "xmlns" && attr.value == "urn:sunfleck:v1") namespaces_[""] = attr.value;
            else if (attr.name.starts_with("xmlns:")) namespaces_[std::string(attr.name.substr(6))] = attr.value;
            else throw Failure{attr.offset,UI_FLECK_ATTRIBUTE,"Unexpected dictionary root attribute"};
        }
        if (!namespaces_.contains("")) fail(UI_FLECK_ATTRIBUTE,"ResourceDictionary requires xmlns");
        dictionary_contents(depth);
        trivia();
        if (offset_ != input_.size()) fail(UI_FLECK_SYNTAX,"Unexpected content after ResourceDictionary");
    }

    void import_dictionary(const Attribute& reference, unsigned depth)
    {
        if (reference.value.empty() || !dictionaries) fail(UI_FLECK_VALUE,"Unknown dictionary Source");
        std::string path = reference.value;
        auto found = dictionaries->end();
        auto slash = source.find_last_of("/\\");
        if (slash != source.npos) found = dictionaries->find(std::string(source.substr(0,slash+1)) + path);
        if (found == dictionaries->end()) found = dictionaries->find(path);
        if (found == dictionaries->end() && source.empty()) {
            // A memory-backed page has no base URI. Accept a unique registered path suffix.
            const auto suffix = "/" + path;
            for (auto it = dictionaries->begin(); it != dictionaries->end(); ++it)
                if (it->first.ends_with(suffix)) {
                    if (found != dictionaries->end()) fail(UI_FLECK_VALUE,"Ambiguous dictionary Source");
                    found = it;
                }
        }
        if (found == dictionaries->end()) throw Failure{reference.offset,UI_FLECK_VALUE,"Unknown dictionary Source"};
        for (const Parser* p = this; p; p = p->importer)
            if (p->source == found->first) throw Failure{reference.offset,UI_FLECK_VALUE,"ResourceDictionary Source cycle"};
        if (found->second.size() > 1024*1024 || *imported_bytes + found->second.size() > 16*1024*1024)
            fail(UI_FLECK_LIMIT,"Imported dictionary size limit exceeded");
        *imported_bytes += found->second.size();
        Parser child(found->second);
        child.source = found->first; child.importer = this; child.dictionaries = dictionaries;
        child.imported_bytes = imported_bytes; child.declared_literals_ = declared_literals_;
        try {
            if (child.document_root() != "ResourceDictionary") child.fail(UI_FLECK_ELEMENT,"Source requires a ResourceDictionary");
            child.styles_.swap(styles_); child.literals_.swap(literals_);
            child.dictionary_document(depth);
            child.styles_.swap(styles_); child.literals_.swap(literals_);
        } catch (const Failure& error) {
            ui_fleck_diagnostic diagnostic{};
            report(found->second,error.offset,error.code,error.message,diagnostic);
            auto location = found->first + ":" + std::to_string(diagnostic.line) + ":" + std::to_string(diagnostic.column) + ": " + diagnostic.message;
            std::snprintf(diagnostic.message,sizeof(diagnostic.message),"%s",location.c_str());
            throw diagnostic;
        }
    }

    void resource_dictionary(unsigned depth)
    {
        const auto attrs = attributes();
        if (!attrs.empty()) {
            if (attrs.size()!=1 || attrs.front().name!="Source") fail(UI_FLECK_ATTRIBUTE,"Expected dictionary Source");
            expect("/>");
            if (depth > 64) fail(UI_FLECK_LIMIT,"Merged dictionary depth exceeded");
            import_dictionary(attrs.front(),depth);
            return;
        }
        dictionary_contents(depth);
    }

    void dictionary_contents(unsigned depth)
    {
        if (depth > 64) fail(UI_FLECK_LIMIT, "Merged dictionary depth exceeded");
        whitespace();
        if (take("/>")) return;
        expect(">"); trivia();
        constexpr std::string_view merged = "ResourceDictionary.MergedDictionaries";
        if (take("<ResourceDictionary.MergedDictionaries")) {
            whitespace();
            if (!take("/>")) {
                expect(">"); trivia();
                while (!input_.substr(offset_).starts_with("</")) {
                    open_tag();
                    if (name() != "ResourceDictionary") fail(UI_FLECK_ELEMENT, "Expected a merged ResourceDictionary");
                    resource_dictionary(depth + 1);
                    trivia();
                }
                close_tag(merged);
            }
        }
        resources_object();
        close_tag("ResourceDictionary");
    }

    void read_setter(ui_node_kind kind, std::map<std::string,std::string>& setters, bool trigger)
    {
        Attribute property{}, value{};
        bool has_property = false, has_value = false;
        for (const auto& a : attributes()) {
            if (a.name == "Property") { property = a; has_property = true; }
            else if (a.name == "Value") { value = a; has_value = true; }
            else fail(UI_FLECK_ATTRIBUTE, "Unknown Setter attribute");
        }
        if (!has_property || !has_value || property.value.empty()) fail(UI_FLECK_VALUE, "Setter requires Property and Value");
        ui_property target;
        BindingValue parsed;
        value.value = static_literal(value);
        const bool dynamic = value.value.starts_with("{DynamicResource ");
        if (dynamic ? (!value.value.ends_with('}') || !dynamic_style_target(kind, property.value,
                       std::string_view(value.value).substr(17,value.value.size()-18),target))
                    : !style_value(kind, property.value, value.value, target, parsed))
            throw Failure{value.offset, UI_FLECK_VALUE, "Invalid Style Setter property or value"};
        if (trigger && (dynamic || !dynamic_style_target(kind,property.value,"trigger",target)))
            throw Failure{property.offset, UI_FLECK_ATTRIBUTE, "Trigger Setter requires an appearance property and a literal"};
        if (!setters.emplace(property.value,value.value).second)
            throw Failure{property.offset, UI_FLECK_ATTRIBUTE, "Duplicate Style Setter"};
        expect("/>"); trivia();
    }

    void read_condition(ui_node_kind kind, Style::Trigger& trigger)
    {
        std::string condition, literal;
        for (const auto& a : attributes()) {
            if (a.name == "Property") condition = a.value;
            else if (a.name == "Value") literal = a.value;
            else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Unknown trigger condition attribute"};
        }
        ui_property property;
        BindingValue expected;
        if (!style_value(kind,condition,literal,property,expected) ||
            !visual_state::trigger_source(property) || expected.value.integer < 0 ||
            uint64_t(expected.value.integer) > UINT32_MAX)
            fail(UI_FLECK_VALUE,"Invalid trigger condition property or value");
        if (!trigger.conditions.emplace(condition,std::to_string(expected.value.integer)).second)
            fail(UI_FLECK_ATTRIBUTE,"Duplicate MultiTrigger condition");
    }

    void read_trigger(ui_node_kind kind, Style& style, std::string_view tag)
    {
        Style::Trigger trigger;
        if (tag == "DataTrigger") {
            bool has_value = false;
            for (const auto& a : attributes()) {
                if (a.name == "Binding") {
                    auto binding = parse_binding(a,UI_PROPERTY_TEXT);
                    if (binding.source != Binding::Context || !binding.format.empty() ||
                        binding.trigger != UI_UPDATE_PROPERTY_CHANGED ||
                        (binding.mode != UI_BIND_ONE_WAY && binding.mode != UI_BIND_DEFAULT))
                        throw Failure{a.offset,UI_FLECK_VALUE,"DataTrigger expects a one-way DataContext binding"};
                    trigger.data_path = binding.path;
                } else if (a.name == "Value") { trigger.data_value = static_literal(a); has_value = true; }
                else throw Failure{a.offset,UI_FLECK_ATTRIBUTE,"Unknown DataTrigger attribute"};
            }
            if (trigger.data_path.empty() || !has_value) fail(UI_FLECK_VALUE,"DataTrigger requires Binding and Value");
        } else if (tag == "Trigger") read_condition(kind,trigger);
        else if (tag != "MultiTrigger" || !attributes().empty()) fail(UI_FLECK_ELEMENT,"Expected Trigger or MultiTrigger");
        if (style.triggers.size() >= 512) fail(UI_FLECK_LIMIT,"Too many triggers");
        expect(">"); trivia();
        if (tag == "MultiTrigger") {
            open_tag();
            if (name() != "MultiTrigger.Conditions") fail(UI_FLECK_ELEMENT,"Expected MultiTrigger.Conditions");
            whitespace(); expect(">"); trivia();
            while (!input_.substr(offset_).starts_with("</")) {
                open_tag();
                if (name() != "Condition") fail(UI_FLECK_ELEMENT,"Expected Condition");
                read_condition(kind,trigger);
                expect("/>"); trivia();
            }
            if (trigger.conditions.empty()) fail(UI_FLECK_VALUE,"MultiTrigger requires conditions");
            close_tag("MultiTrigger.Conditions"); trivia();
        }
        while (!input_.substr(offset_).starts_with("</")) {
            open_tag();
            if (name() != "Setter") fail(UI_FLECK_ELEMENT,"Expected Trigger Setter");
            read_setter(kind,trigger.setters,true);
        }
        close_tag(tag); trivia();
        style.triggers.push_back(std::move(trigger));
    }

    void resources_object()
    {
        std::set<std::string> keys;
        while (true) {
            trivia();
            if (input_.substr(offset_).starts_with("</")) break;
            open_tag();
            const auto tag = name();
            if (tag == "Resource") {
                std::string key;
                std::optional<std::string> value;
                for (const auto& attribute : attributes()) {
                    if (attribute.name == "Key") key = attribute.value;
                    else if (attribute.name == "Value") value = static_literal(attribute);
                    else throw Failure{attribute.offset, UI_FLECK_ATTRIBUTE, "Unknown Resource attribute"};
                }
                if (!valid_binding_name(key) || !value || !keys.insert(key).second)
                    fail(UI_FLECK_VALUE, "Resource requires a unique Key and a Value");
                styles_.erase(key);
                (*declared_literals_)[key] = *value;
                literals_[key] = std::move(*value);
                expect("/>");
                continue;
            }
            if (tag != "Style") fail(UI_FLECK_ELEMENT, "Expected Resource or Style");
            if (styles_.size() >= 512) fail(UI_FLECK_LIMIT, "Too many styles");
            Style style;
            for (const auto& a : attributes()) {
                if (a.name == "Key" || a.name == "x:Key") style.key = a.value;
                else if (a.name == "TargetType") {
                    auto target = qualify(a.value);
                    if (target.first != "urn:sunfleck:v1") throw Failure{a.offset, UI_FLECK_VALUE, "Expected a built-in style target"};
                    style.target_type = target.second;
                }
                else if (a.name == "BasedOn") {
                    style.based_on = a.value.starts_with("{StaticResource ") && a.value.ends_with('}')
                        ? a.value.substr(16, a.value.size() - 17) : a.value;
                } else fail(UI_FLECK_ATTRIBUTE, "Unknown Style attribute");
            }
            int kind = UI_STACK_PANEL;
            while (kind <= UI_PATH && style.target_type != devtools::control_name(ui_node_kind(kind))) ++kind;
            if (kind > UI_PATH) fail(UI_FLECK_VALUE, "Style requires a built-in TargetType");
            const auto key = style.key.empty() ? "." + style.target_type : style.key;
            if (!style.key.empty() && !valid_binding_name(style.key)) fail(UI_FLECK_VALUE, "Invalid Style Key");
            if (!keys.insert(key).second) fail(UI_FLECK_VALUE, "Duplicate Style");
            styles_.erase(key);
            literals_.erase(key);
            declared_literals_->erase(key);
            if (take("/>")) { styles_.emplace(key, std::move(style)); continue; }
            expect(">"); trivia();
            while (!input_.substr(offset_).starts_with("</")) {
                open_tag();
                const auto setter_tag = name();
                if (setter_tag == "Trigger" || setter_tag == "MultiTrigger" || setter_tag == "DataTrigger") { read_trigger(ui_node_kind(kind),style,setter_tag); continue; }
                if (setter_tag == "Style.Triggers") {
                    whitespace(); expect(">"); trivia();
                    while (!input_.substr(offset_).starts_with("</")) {
                        open_tag(); read_trigger(ui_node_kind(kind),style,name());
                    }
                    close_tag(setter_tag); trivia(); continue;
                }
                if (setter_tag != "Setter") fail(UI_FLECK_ELEMENT, "Expected Setter or Trigger");
                read_setter(ui_node_kind(kind),style.setters,false);
            }
            close_tag(tag); styles_.emplace(key, std::move(style));
        }
        resolve_styles(keys);
    }

    std::string template_xml(unsigned depth) {
        const size_t begin = offset_;
        (void)element(depth+1);
        std::string xml = "<Window";
        for (const auto& [prefix,uri] : namespaces_) {
            xml += prefix.empty() ? " xmlns='" : " xmlns:"+prefix+"='";
            for (char c : uri) {
                if (c == '&') xml += "&amp;";
                else if (c == 39) xml += "&apos;";
                else if (c == '<') xml += "&lt;";
                else xml += c;
            }
            xml += "'";
        }
        xml += ">";
        xml.append(input_.substr(begin,offset_-begin));
        xml += "</Window>";
        return xml;
    }

    Element element(unsigned depth)
    {
        if (depth > 64) {
            fail(UI_FLECK_LIMIT, "Fleck element depth exceeded");
        }
        Element node;
        node.offset = offset_;
        open_tag();
        const auto tag = name();
        auto saved_namespaces = namespaces_;
        auto attrs = attributes();
        for (const auto& a : attrs) {
            if (a.name.starts_with("xmlns:")) { namespaces_[std::string(a.name.substr(6))] = a.value; }
            else if (a.name == "xmlns") { namespaces_[""] = a.value; }
        }
        auto qualified = qualify(tag);
        const bool custom = qualified.first != "urn:sunfleck:v1";
        int found = UI_STACK_PANEL;
        while (found <= UI_PATH && qualified.second != devtools::control_name(static_cast<ui_node_kind>(found))) { ++found; }
        if ((found > UI_PATH || found == UI_ICON || found == UI_ICON_BUTTON) && !custom) {
            throw Failure{node.offset, UI_FLECK_ELEMENT, "Unknown Fleck element"};
        }
        if (custom) {
            node.type_namespace = qualified.first; node.type_name = qualified.second;
        }
        node.kind = custom ? UI_PANEL : static_cast<ui_node_kind>(found);
        const bool container = is_container(node.kind) || inline_container(node.kind);
        const bool text = devtools::text_target(node.kind);
        const bool empty_element = take("/>");
        decltype(styles_) outer_styles;
        decltype(literals_) outer_literals;
        bool has_resources = false;
        if (!empty_element) {
            expect(">"); trivia();
            has_resources = input_.substr(offset_).starts_with("<" + std::string(tag) + ".Resources");
            if (has_resources) {
                outer_styles = styles_;
                outer_literals = literals_;
                std::map<std::string,std::string> declared;
                resources_property(tag,declared);
                for (const auto& [key,value] : declared) node.properties.emplace(".Resource."+key,value);
            }
        }
        std::string style_key;
        bool has_command_mode = false;
        std::set<ui_property> targets;
        for (auto a : attrs) {
            if (a.name == "xmlns" || a.name.starts_with("xmlns:")) { continue; }
            node.location.attributes.push_back({std::string(a.name), source_index_.span(a.offset, a.length), source_index_.span(a.value_offset, a.value_length)});
            if ((custom || fleck::supports_template(node.kind)) && a.name == "Template") {
                auto qualified = qualify(a.value);
                node.template_namespace = qualified.first; node.template_name = qualified.second; continue;
            }
            if (a.name == "Style") {
                style_key = a.value.starts_with("{StaticResource ") && a.value.ends_with('}')
                    ? a.value.substr(16, a.value.size() - 17) : a.value;
                if (!valid_binding_name(style_key)) throw Failure{a.offset, UI_FLECK_VALUE, "Invalid style key"};
                continue;
            }
            if (a.value.starts_with("{DynamicResource ")) {
                ui_property property;
                const auto key = a.value.substr(17,a.value.size()-18);
                if (!a.value.ends_with('}') || !dynamic_style_target(node.kind,a.name,key,property))
                    throw Failure{a.offset,UI_FLECK_VALUE,"Invalid DynamicResource key or property"};
                node.properties.emplace(".DynamicLocal." + std::string(a.name),key);
                continue;
            }
            a.value = static_literal(a);
            if (custom && !visual_layout::canvas_coordinate(a.name) && a.name != "Panel.ZIndex" && !grid_layout::attribute(a.name) && a.name != "Id" && a.name != "Anchor" && a.name != "IsEnabled" && a.name != "Visibility" && a.name != "LayoutTransform" && a.name != "RenderTransform" && a.name != "RenderTransformOrigin" && a.name != "Opacity" && a.name != "IsFocusScope" && a.name != "IsTabStop" && a.name != "TabIndex" && a.name != "ClipToBounds" && a.name != "IsHitTestVisible" && a.name != "FontFamily" && a.name != "FontSize" && a.name != "FontWeight" && a.name != "FontStyle" && a.name != "FontStretch" && a.name != "Foreground" && a.name != "TextAlignment" && a.name != "TextWrapping" && a.name != "TextTrimming" && a.name != "LineHeight" && a.name != "TextDecorations" && a.name != "FlowDirection" && a.name != "Height" && !visual_layout::property(visual_layout::find(a.name)) && a.name != "Weight" && a.name != "Dock") {
                if (a.value.starts_with("{") && !a.value.starts_with("{}")) {
                    auto binding = parse_binding(a, UI_PROPERTY_TEXT);
                    binding.target = a.name;
                    node.bindings.push_back(std::move(binding));
                } else { node.properties[std::string(a.name)] = a.value.starts_with("{}") ? a.value.substr(2) : a.value; }
                continue;
            }
            if (!devtools::attribute_allowed(node.kind, a.name)) { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown attribute for this element"}; }
            if (a.name == "Text" || a.name == "Content" || a.name == "Value" || a.name == "IsEnabled" || a.name == "Command") {
                const auto target = a.name == "Text" || a.name == "Content" ? UI_PROPERTY_TEXT : a.name == "Value" ? UI_PROPERTY_VALUE : a.name == "IsEnabled" ? UI_PROPERTY_IS_ENABLED : UI_PROPERTY_COMMAND;
                if (!targets.insert(target).second) { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Duplicate target property"}; }
            }
            if (a.name == "Command" && a.value.starts_with("{RoutedCommand ") && a.value.ends_with('}')) {
                auto key = a.value.substr(15,a.value.size()-16);
                if (!valid_binding_name(key)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid routed command name"};
                node.properties.emplace(".RoutedCommand",std::move(key));
                continue;
            }
            const bool literal = a.value.starts_with("{}");
            if (literal) { a.value.erase(0, 2); }
            if (!literal && a.value.starts_with("{")) {
                ui_property property;
                if (a.name == "Text") { property = UI_PROPERTY_TEXT; }
                else if (a.name == "Content") { property = UI_PROPERTY_CONTENT; }
                else if (a.name == "IsEnabled") { property = UI_PROPERTY_IS_ENABLED; }
                else if (a.name == "Value") { property = UI_PROPERTY_VALUE; }
                else if (a.name == "Height") { property = UI_PROPERTY_HEIGHT; }
                else if (visual_layout::property(visual_layout::find(a.name))) { property = visual_layout::find(a.name); }
                else if (a.name == "Command") { property = UI_PROPERTY_COMMAND; }
                else if (a.name == "ItemsSource") { property = UI_PROPERTY_ITEMS_SOURCE; }
                else if (a.name == "Source") { property = UI_PROPERTY_SOURCE; }
                else if (a.name == "DataContext") { property = UI_PROPERTY_DATA_CONTEXT; }
                else if (properties::find(a.name, property) && (property == UI_PROPERTY_LAYOUT_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN || property == UI_PROPERTY_OPACITY || properties::metric(property) || border::property(property) || visual_state::property(property) || text_format::property(property))) { }
                else { throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Property does not support Binding"}; }
                auto binding = parse_binding(a, property, &node);
                ui_node probe{};
                probe.kind = node.kind;
                const ui_binding_options options{sizeof(options), 1, binding.property, binding.path.c_str(), binding.mode, binding.trigger, binding.format.c_str()};
                const char* message = nullptr;
                if (binding_validate(probe, options, &message) != UI_OK) { throw Failure{a.offset, UI_FLECK_VALUE, message}; }
                node.bindings.push_back(std::move(binding));
            } else if (a.name == "Source") {
                if (a.value.size() > 1024) throw Failure{a.offset, UI_FLECK_VALUE, "Image source key exceeds 1024 bytes"};
                node.properties["Source"] = a.value;
            } else if (ui_property font; properties::find(a.name,font) && text_format::property(font)) {
                ui_binding_value value{};
                if (!text_format::parse(font,a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid font property"};
                node.properties[std::string(a.name)] = a.value;
            } else if (ui_property state; properties::find(a.name,state) && visual_state::property(state)) {
                ui_binding_value value{};
                if (!visual_state::parse(state,a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid visual state"};
                node.properties[std::string(a.name)] = a.value;
            } else if (ui_property appearance; properties::find(a.name, appearance) && border::property(appearance)) {
                BindingValue parsed;
                if (node.kind != UI_BORDER || binding_parse_value(a.value, border::kind(appearance), parsed) != UI_OK ||
                    !border::valid(appearance, parsed.value)) throw Failure{a.offset, UI_FLECK_VALUE, "Invalid Border property"};
                node.properties[std::string(a.name)] = a.value;
            } else if (drawing::shape_kind(node.kind) && (a.name=="Fill" || a.name=="Stroke" || a.name=="StrokeThickness" || a.name=="Data")) {
                BindingValue color;
                double width;
                bool valid=a.name=="Data" ? node.kind==UI_PATH && drawing::parse_path(a.value).has_value() :
                    a.name=="StrokeThickness" ? numeric_literal::real_literal(a.value,width) && width>=0 && width<=4096 :
                    binding_parse_value(a.value,UI_VALUE_COLOR,color)==UI_OK;
                if (!valid) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid shape property"};
                node.properties[std::string(a.name)]=a.value;
            } else if (a.name == "LayoutTransform" || a.name == "RenderTransform" || a.name == "RenderTransformOrigin") {
                BindingValue value;
                if (binding_parse_value(a.value,a.name != "RenderTransformOrigin" ? UI_VALUE_MATRIX : UI_VALUE_POINT,value) != UI_OK)
                    throw Failure{a.offset,UI_FLECK_VALUE,"Invalid transform geometry"};
                node.properties[std::string(a.name)] = a.value;
            } else if (a.name == "Stretch") {
                ui_stretch value;
                if (!visual_layout::parse_stretch(a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid Stretch"};
                node.properties["Stretch"] = a.value;
            } else if (a.name == "DragText" || a.name == "AllowDrop") {
                if (a.name=="AllowDrop" && a.value!="true" && a.value!="false")
                    throw Failure{a.offset,UI_FLECK_VALUE,"AllowDrop requires true or false"};
                node.properties[std::string(a.name)]=a.value;
            } else if (a.name == "Opacity") {
                ui_binding_value parsed{};
                if (ui_value_parse(UI_VALUE_DOUBLE,a.value.c_str(),&parsed,nullptr,0) != UI_OK || parsed.number < 0 || parsed.number > 1)
                    throw Failure{a.offset,UI_FLECK_VALUE,"Opacity must be between zero and one"};
                node.properties["Opacity"] = a.value;
            } else if (visual_layout::scroll_policy_name(a.name)) {
                ui_scroll_policy policy;
                if (node.kind != UI_SCROLL_VIEW || !visual_layout::parse_scroll_policy(a.value,policy)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid scroll policy"};
                node.properties[std::string(a.name)] = a.value;
            } else if (visual_layout::canvas_coordinate(a.name)) {
                double value;
                if (!visual_layout::parse_coordinate(a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Invalid Canvas coordinate"};
                node.properties[std::string(a.name)] = a.value;
            } else if (a.name == "Panel.ZIndex") {
                int32_t value;
                if (!grid_layout::parse_z_index(a.value,value)) throw Failure{a.offset,UI_FLECK_VALUE,"Panel.ZIndex requires a signed 32-bit integer"};
                node.properties["Panel.ZIndex"] = a.value;
            } else if (grid_layout::attribute(a.name)) {
                grid_layout::Settings parsed;
                if (!grid_layout::assign(parsed,node.kind == UI_GRID,a.name,a.value))
                    throw Failure{a.offset,UI_FLECK_VALUE,"Invalid grid track or cell"};
                node.properties[std::string(a.name)] = a.value;
            } else if (a.name == "DataContext") {
                if (a.value != "null") throw Failure{a.offset, UI_FLECK_VALUE, "DataContext requires a model binding or null"};
                node.properties["DataContext"] = "null";
            } else if (a.name == "IsEnabled") {
                if (a.value != "true" && a.value != "false") { throw Failure{a.offset, UI_FLECK_VALUE, "IsEnabled requires true or false"}; }
                node.enabled = a.value == "true";
            } else if ((node.kind == UI_DOCK_DOCUMENT || node.kind == UI_DOCK_TOOL) && a.name == "Title") {
                if (a.value.size() > 4096) { throw Failure{a.offset, UI_FLECK_LIMIT, "Dock title is too long"}; }
                node.text = a.value;
            } else if ((node.kind == UI_DOCK_DOCUMENT || node.kind == UI_DOCK_TOOL) && a.name == "Icon") {
                if (a.value.size() > 64) { throw Failure{a.offset, UI_FLECK_LIMIT, "Dock icon is too long"}; }
                node.dock_icon = a.value;
            } else if ((node.kind == UI_DOCK_DOCUMENT || node.kind == UI_DOCK_TOOL) &&
                       (a.name == "CanClose" || a.name == "CanFloat" || a.name == "CanAutoHide")) {
                if (a.value != "true" && a.value != "false") { throw Failure{a.offset, UI_FLECK_VALUE, "Expected true or false"}; }
                const unsigned flag = a.name == "CanClose" ? UI_DOCK_CAN_CLOSE : a.name == "CanFloat" ? UI_DOCK_CAN_FLOAT : UI_DOCK_CAN_AUTO_HIDE;
                if (a.value == "true") { node.dock_capabilities |= flag; } else { node.dock_capabilities &= ~flag; }
            } else if (visual_layout::property(visual_layout::find(a.name))) {
                if (!visual_layout::literal(node.layout, visual_layout::find(a.name), a.value)) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Invalid layout property value"};
                }
                if (node.kind == UI_DOCK_DOCUMENT || node.kind == UI_DOCK_TOOL) {
                    if (a.name == "MinWidth" || a.name == "MinHeight") node.properties[std::string(a.name)] = a.value;
                }
            } else if (a.name == "Orientation" && node.kind != UI_DOCK_SPLIT) {
                if (a.value != "Horizontal" && a.value != "Vertical") throw Failure{a.offset,UI_FLECK_VALUE,"Invalid orientation"};
                node.properties["Orientation"] = a.value;
            } else if (node.kind == UI_DOCK_SPLIT && a.name == "Orientation") {
                if (a.value != "Horizontal" && a.value != "Vertical") { throw Failure{a.offset, UI_FLECK_VALUE, "Invalid Dock split orientation"}; }
                node.dock = a.value == "Vertical" ? UI_DOCK_TOP : UI_DOCK_LEFT;
            } else if (node.kind == UI_DOCK_SPLIT && a.name == "Ratio") {
                node.value = number(a, 1, 99); node.has_value = true;
            } else if (container && (a.name == "Spacing" || a.name == "Padding")) {
                const bool spacing=a.name=="Spacing";
                ui_binding_value value{};
                if (spacing && a.value=="Theme") { value.kind=UI_VALUE_DOUBLE; value.number=-1; }
                else if (ui_value_parse(UI_VALUE_DOUBLE,a.value.c_str(),&value,nullptr,0)!=UI_OK || value.number<0)
                    throw Failure{a.offset,UI_FLECK_VALUE,"Expected a finite metric in 0..64 (Spacing also accepts Theme)"};
                if (!visual_layout::valid_metric(spacing?UI_PROPERTY_SPACING:UI_PROPERTY_PADDING,value.number))
                    throw Failure{a.offset,UI_FLECK_VALUE,"Layout metric is outside 0..64"};
                (spacing?node.spacing:node.padding)=value.number;
            } else if ((text && a.name == "Text") ||
                       (has_content(node.kind) && a.name == "Content")) {
                if (a.value.size() > 16384) {
                    throw Failure{a.offset, UI_FLECK_LIMIT, "Text exceeds the Fleck limit"};
                }
                node.text = a.value;
            } else if (a.name == "Id" || a.name == "Anchor") {
                if (a.value.empty() || a.value.size() > 64 || !letter(a.value.front()) ||
                    !std::all_of(a.value.begin(), a.value.end(), name_char)) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Invalid node identifier"};
                }
                (a.name == "Id" ? node.id : node.anchor) = a.value;
            } else if (a.name == "CommandMode") {
                if (a.value != "Async" && a.value != "Sync") {
                    throw Failure{a.offset, UI_FLECK_VALUE, "CommandMode must be Sync or Async"};
                }
                node.async_command = a.value == "Async";
                has_command_mode = true;
            } else if (a.name == "Value") {
                const auto parsed = std::from_chars(a.value.data(), a.value.data() + a.value.size(), node.value);
                if (parsed.ec != std::errc{} || parsed.ptr != a.value.data() + a.value.size()) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Expected an unsigned integer"};
                }
                node.has_value = true;
            } else if (a.name == "Height") {
                ui_binding_value height{};
                if (ui_value_parse(UI_VALUE_LENGTH,a.value.c_str(),&height,nullptr,0)!=UI_OK || !visual_layout::decode(UI_PROPERTY_HEIGHT,height,node.height))
                    throw Failure{a.offset,UI_FLECK_VALUE,"Height requires a nonnegative finite length or Auto"};

            } else if (a.name == "Weight") {
                node.weight = number(a, 0, 1024);
            } else if (a.name == "Dock") {
                constexpr std::string_view edges[] = {"Left", "Top", "Right", "Bottom"};
                const auto edge = std::find(std::begin(edges), std::end(edges), a.value);
                if (edge == std::end(edges)) {
                    throw Failure{a.offset, UI_FLECK_VALUE, "Invalid dock edge"};
                }
                node.dock = static_cast<ui_dock_edge>(edge - std::begin(edges));
            } else {
                throw Failure{a.offset, UI_FLECK_ATTRIBUTE, "Unknown attribute for this element"};
            }
        }
        auto selected = styles_.find(style_key.empty() ? "." + qualified.second : style_key);
        if (!style_key.empty() && (selected == styles_.end() || custom || selected->second.target_type != qualified.second))
            throw Failure{node.offset, UI_FLECK_VALUE, "Unknown Style or incompatible TargetType"};
        if (!style_key.empty()) node.properties[".StyleKey"] = style_key;
        if (!custom && selected != styles_.end()) {
            unsigned order = 0;
            for (const auto& trigger : selected->second.triggers) {
                const auto index = std::to_string(order++);
                auto prefix = ".Trigger." + index + ".";
                if (!trigger.data_path.empty()) {
                    node.properties.emplace(".DataPath."+index,trigger.data_path);
                    node.properties.emplace(".DataValue."+index,trigger.data_value);
                    prefix += "Data.";
                }
                for (const auto& [condition,expected] : trigger.conditions) prefix += condition+"."+expected+".";
                for (const auto& [property,value] : trigger.setters) node.properties.emplace(prefix+property,value);
            }
            for (const auto& [property, value] : selected->second.setters) {
                if (value.starts_with("{DynamicResource ")) {
                    auto key = value.substr(17,value.size()-18);
                    node.properties.emplace(".DynamicStyle." + property, key);
                } else node.properties.emplace(".Style." + property, value);
            }
        }
        const bool new_command = std::any_of(node.bindings.begin(), node.bindings.end(), [](const auto& b) { return b.property == UI_PROPERTY_COMMAND; });
        if (!visual_layout::valid(node.layout)) { throw Failure{node.offset, UI_FLECK_VALUE, "Minimum size cannot exceed maximum size"}; }
        if (has_command_mode && !new_command && !node.properties.contains(".RoutedCommand")) { fail(UI_FLECK_ATTRIBUTE, "CommandMode requires a Command Binding"); }
        if (empty_element) {
            node.location.element = source_index_.span(node.offset, offset_ - node.offset);
            namespaces_ = std::move(saved_namespaces);
            return node;
        }
        bool has_state_groups = false;
        while (!input_.substr(offset_).starts_with("</")) {
            const std::string owner = std::string(tag)+".";
            if (input_.substr(offset_+1).starts_with(owner)) {
                const auto start=offset_;
                open_tag(); const auto full=name();
                const auto key=full.substr(owner.size());
                ui_property bound_property;
                if (properties::find(key,bound_property) && devtools::attribute_allowed(node.kind,key)) {
                    const auto before = offset_;
                    whitespace(); expect(">"); trivia();
                    if (input_.substr(offset_).starts_with("<MultiBinding")) {
                        if (std::any_of(node.location.attributes.begin(),node.location.attributes.end(),[&](const auto& attr){return attr.name==key;}))
                            fail(UI_FLECK_ATTRIBUTE,"Duplicate bound property");
                        open_tag(); if (name()!="MultiBinding") fail(UI_FLECK_ELEMENT,"Expected MultiBinding");
                        const auto attrs=attributes();
                        if (attrs.size()!=1 || attrs[0].name!="Converter" || !binding_path(attrs[0].value))
                            fail(UI_FLECK_ATTRIBUTE,"MultiBinding requires a converter name");
                        std::string encoded=attrs[0].value;
                        expect(">"); trivia();
                        unsigned count=0;
                        while (!input_.substr(offset_).starts_with("</")) {
                            open_tag(); if (name()!="Binding") fail(UI_FLECK_ELEMENT,"Expected Binding input");
                            const auto input=attributes();
                            if (input.size()!=1 || input[0].name!="Path" || !binding_path(input[0].value) || ++count>64)
                                fail(UI_FLECK_VALUE,"Expected up to 64 model paths");
                            encoded+=" "+input[0].value;
                            expect("/>"); trivia();
                        }
                        if (!count) fail(UI_FLECK_VALUE,"MultiBinding needs at least one input");
                        close_tag("MultiBinding"); trivia(); close_tag(full);
                        Binding binding;
                        binding.property=bound_property;
                        binding.path=conversion_path(encoded);
                        binding.mode=UI_BIND_ONE_WAY;
                        node.properties[".Conversion."+binding.path]=std::move(encoded);
                        node.bindings.push_back(std::move(binding));
                        node.location.attributes.push_back({std::string(key),source_index_.span(start,offset_-start),source_index_.span(before,offset_-before)});
                        trivia(); continue;
                    }
                    offset_=before;
                }
                if (key == "Columns" && node.kind == UI_DATA_GRID) {
                    if (node.properties.contains(".ColumnPath.0")) fail(UI_FLECK_ATTRIBUTE,"Duplicate DataGrid.Columns");
                    whitespace(); expect(">"); trivia();
                    unsigned index = 0;
                    while (!input_.substr(offset_).starts_with("</")) {
                        open_tag(); if (name() != "DataGridTextColumn" || index>=512) fail(UI_FLECK_ELEMENT,"Invalid DataGrid column");
                        const auto suffix = "."+std::to_string(index++);
                        for (const auto& attr : attributes()) {
                            if (attr.name == "Binding") {
                                const auto binding = parse_binding(attr,UI_PROPERTY_TEXT);
                                if (binding.source!=Binding::Context || !binding.format.empty()) fail(UI_FLECK_VALUE,"Expected column model path");
                                node.properties[".ColumnPath"+suffix] = binding.path;
                                if (binding.mode == UI_BIND_ONE_WAY) node.properties[".ColumnReadOnly"+suffix] = "true";
                            } else if (attr.name == "Width") {
                                double width;
                                if (!numeric_literal::real_literal(attr.value,width) || width<16 || width>4096) fail(UI_FLECK_VALUE,"Invalid column width");
                                node.properties[".ColumnWidth"+suffix] = attr.value;
                            } else if (attr.name == "Header") node.properties[".ColumnHeader"+suffix] = attr.value;
                            else throw Failure{attr.offset,UI_FLECK_ATTRIBUTE,"Unknown DataGridTextColumn attribute"};
                        }
                        if (!node.properties.contains(".ColumnPath"+suffix)) fail(UI_FLECK_ATTRIBUTE,"Column Binding is required");
                        expect("/>"); trivia();
                    }
                    close_tag(full); trivia(); continue;
                }
                if (key == "ItemTemplateSelector") {
                    if (node.kind != UI_LIST_VIEW && node.kind != UI_ITEMS_CONTROL)
                        fail(UI_FLECK_ATTRIBUTE,"ItemTemplateSelector requires ListView or ItemsControl");
                    whitespace(); expect(">"); trivia();
                    unsigned count = 0;
                    while (!input_.substr(offset_).starts_with("</")) {
                        open_tag();
                        if (name() != "DataTemplate") fail(UI_FLECK_ELEMENT,"Expected DataTemplate");
                        const auto attrs = attributes();
                        if (attrs.size() != 1 || attrs.front().name != "Key" || attrs.front().value.empty())
                            fail(UI_FLECK_ATTRIBUTE,"Selector templates require an item label Key");
                        expect(">"); trivia();
                        if (!node.properties.emplace(".ItemTemplateKey."+attrs.front().value,template_xml(depth)).second)
                            fail(UI_FLECK_ATTRIBUTE,"Duplicate template selector key");
                        trivia(); close_tag("DataTemplate"); trivia();
                        ++count;
                    }
                    if (!count) fail(UI_FLECK_ELEMENT,"Expected a keyed DataTemplate");
                    close_tag(full); trivia(); continue;
                }
                if (key == "ItemTemplate") {
                    if (!devtools::attribute_allowed(node.kind,"ItemsSource") || (node.properties.contains(".ItemTemplate") || node.properties.contains(".ChildrenPath")))
                        fail(UI_FLECK_ATTRIBUTE,"Invalid or duplicate ItemTemplate");
                    whitespace(); expect(">"); trivia();
                    open_tag();
                    const auto template_type = name();
                    if (template_type == "HierarchicalDataTemplate") {
                        if (node.kind != UI_TREE_VIEW) fail(UI_FLECK_ELEMENT,"HierarchicalDataTemplate requires TreeView");
                        const auto members = attributes();
                        if (members.size()!=1 || members.front().name!="ItemsSource") fail(UI_FLECK_ATTRIBUTE,"Expected children ItemsSource binding");
                        const auto children = parse_binding(members.front(),UI_PROPERTY_ITEMS_SOURCE);
                        if (children.source != Binding::Context || !children.format.empty() || children.mode == UI_BIND_TWO_WAY)
                            fail(UI_FLECK_VALUE,"Expected one-way child collection binding");
                        node.properties[".ChildrenPath"] = children.path;
                        expect(">"); trivia(); open_tag();
                        if (name() != "TextBlock") fail(UI_FLECK_ELEMENT,"Expected TextBlock tree header");
                        const auto header = attributes();
                        if (header.size()!=1 || header.front().name!="Text") fail(UI_FLECK_ATTRIBUTE,"Expected tree header Text binding");
                        const auto label = parse_binding(header.front(),UI_PROPERTY_TEXT);
                        if (label.source != Binding::Context || !label.format.empty() || label.mode == UI_BIND_TWO_WAY)
                            fail(UI_FLECK_VALUE,"Expected one-way tree header binding");
                        node.properties[".TreeLabelPath"] = label.path;
                        expect("/>"); trivia(); close_tag(template_type); trivia(); close_tag(full); trivia(); continue;
                    }
                    if (template_type != "DataTemplate") fail(UI_FLECK_ELEMENT,"Expected DataTemplate");
                    whitespace(); expect(">"); trivia();
                    node.properties.emplace(".ItemTemplate",template_xml(depth));
                    trivia(); close_tag("DataTemplate"); trivia(); close_tag(full); trivia(); continue;
                }
                if (key == "CommandBindings") {
                    whitespace(); expect(">"); trivia();
                    while (!input_.substr(offset_).starts_with("</")) {
                        open_tag();
                        if (name() != "CommandBinding") fail(UI_FLECK_ELEMENT,"Expected CommandBinding");
                        std::string command, path;
                        for (const auto& attr : attributes()) {
                            if (attr.name == "Command" && valid_binding_name(attr.value)) command = attr.value;
                            else if (attr.name == "Executed") {
                                auto binding = parse_binding(attr,UI_PROPERTY_COMMAND);
                                if (binding.source != Binding::Context || !binding.format.empty() ||
                                    (binding.mode != UI_BIND_DEFAULT && binding.mode != UI_BIND_ONE_WAY))
                                    throw Failure{attr.offset,UI_FLECK_VALUE,"Expected DataContext command binding"};
                                path = binding.path;
                            } else throw Failure{attr.offset,UI_FLECK_ATTRIBUTE,"Invalid CommandBinding attribute"};
                        }
                        if (command.empty() || path.empty()) fail(UI_FLECK_ATTRIBUTE,"CommandBinding requires Command and Executed");
                        if (!node.properties.emplace(".CommandBinding."+command,path).second)
                            fail(UI_FLECK_VALUE,"Duplicate command binding");
                        expect("/>"); trivia();
                    }
                    close_tag(full); trivia(); continue;
                }
                if (key == "InputBindings") {
                    whitespace(); expect(">"); trivia();
                    while (!input_.substr(offset_).starts_with("</")) {
                        open_tag();
                        const auto tag = name();
                        if (tag == "PointerBinding") {
                            std::string action, phase = "Bubble", command;
                            for (const auto& attr : attributes()) {
                                if (attr.name == "Action") action = attr.value;
                                else if (attr.name == "Phase") phase = attr.value;
                                else if (attr.name == "Command" && valid_binding_name(attr.value)) command = attr.value;
                                else throw Failure{attr.offset,UI_FLECK_ATTRIBUTE,"Invalid PointerBinding attribute"};
                            }
                            if (command.empty() || !pointer_binding(action,phase)) fail(UI_FLECK_VALUE,"Invalid pointer action, phase or command");
                            if (!node.properties.emplace(".PointerBinding."+action+"."+phase,command).second)
                                fail(UI_FLECK_VALUE,"Duplicate pointer binding");
                            expect("/>"); trivia(); continue;
                        }
                        if (tag != "KeyBinding") fail(UI_FLECK_ELEMENT,"Expected KeyBinding or PointerBinding");
                        unsigned code = 0, modifiers = 0;
                        ui_event_phase phase = UI_EVENT_BUBBLE;
                        std::string command;
                        for (const auto& attr : attributes()) {
                            if (attr.name == "Gesture" && key_gesture(attr.value,code,modifiers)) continue;
                            if (attr.name == "Phase" && attr.value == "Preview") phase = UI_EVENT_PREVIEW;
                            else if (attr.name == "Phase" && attr.value == "Direct") phase = UI_EVENT_DIRECT;
                            else if (attr.name == "Phase" && attr.value == "Bubble") phase = UI_EVENT_BUBBLE;
                            else if (attr.name == "Command" && valid_binding_name(attr.value)) command = attr.value;
                            else throw Failure{attr.offset,UI_FLECK_VALUE,"Invalid KeyBinding Gesture, Phase or Command"};
                        }
                        if (!code || command.empty()) fail(UI_FLECK_ATTRIBUTE,"KeyBinding requires Gesture and Command");
                        auto property = ".KeyBinding."+std::to_string(gesture_code(code,modifiers,phase));
                        if (!node.properties.emplace(std::move(property),std::move(command)).second)
                            fail(UI_FLECK_VALUE,"Duplicate key gesture");
                        expect("/>"); trivia();
                    }
                    close_tag(full); trivia(); continue;
                }
                if (key == "VisualStateGroups") {
                    if (has_state_groups) fail(UI_FLECK_ATTRIBUTE,"Duplicate VisualStateGroups");
                    has_state_groups = true;
                    visual_state_groups(node); close_tag(full); trivia(); continue;
                }
                if (key == "Storyboard") {
                    if (node.properties.contains(".Storyboard")) fail(UI_FLECK_ATTRIBUTE,"Duplicate Storyboard");
                    whitespace(); expect(">"); trivia();
                    const auto begin = offset_;
                    std::vector<animation::Track> tracks;
                    storyboard_object(node.kind,tracks);
                    node.properties[".Storyboard"] = input_.substr(begin,offset_-begin);
                    trivia(); close_tag(full); trivia(); continue;
                }
                const bool effect=key=="Effect", mask=key=="OpacityMask";
                if (effect || mask || (drawing::shape_kind(node.kind) && (key=="Fill" || key=="Stroke")) || (node.kind==UI_BORDER && (key=="Background" || key=="BorderBrush"))) {
                    if (std::any_of(node.location.attributes.begin(),node.location.attributes.end(),[&](const auto& a){ return a.name==key; }))
                        fail(UI_FLECK_ATTRIBUTE,"Duplicate drawing property");
                    whitespace(); expect(">"); trivia();
                    const auto value_start=offset_;
                    drawing::Brush brush; ui_effect_desc filter{};
                    drawing_object(effect ? nullptr : &brush,effect ? &filter : nullptr);
                    const auto value_end=offset_;
                    trivia(); close_tag(full);
                    node.properties["."+std::string(key)]=input_.substr(value_start,value_end-value_start);
                    node.location.attributes.push_back({std::string(key),source_index_.span(start,offset_-start),
                                                        source_index_.span(value_start,value_end-value_start)});
                    trivia(); continue;
                }
                offset_=start;
            }
            const auto property = input_.substr(offset_+1).starts_with(owner+"LayoutTransform") ? "LayoutTransform" : "RenderTransform";
            const auto property_tag = owner+property;
            if (input_.substr(offset_+1).starts_with(property_tag)) {
                const auto start = offset_;
                open_tag();
                if (name() != property_tag) fail(UI_FLECK_ELEMENT,"Unknown transform property element");
                if (std::any_of(node.location.attributes.begin(),node.location.attributes.end(),
                                [&](const auto& attr) { return attr.name == property; }))
                    fail(UI_FLECK_ATTRIBUTE,"Duplicate transform assignment");
                whitespace(); expect(">"); trivia();
                const auto value_start = offset_;
                const auto matrix = transform_object(depth+1);
                const auto value_end = offset_;
                trivia(); close_tag(property_tag);
                std::ostringstream value;
                value.imbue(std::locale::classic());
                value << std::setprecision(17) << matrix.m11 << ',' << matrix.m12 << ',' << matrix.m21 << ','
                      << matrix.m22 << ',' << matrix.offset_x << ',' << matrix.offset_y;
                node.properties[property] = value.str();
                node.location.attributes.push_back({property,source_index_.span(start,offset_-start),
                                                     source_index_.span(value_start,value_end-value_start)});
                trivia();
                continue;
            }
            if ((!container && !custom) || (custom && !node.children.empty())) {
                fail(UI_FLECK_ELEMENT, "Invalid child: leaf elements accept none and component instances accept one");
            }
            if (node.kind == UI_BORDER && !node.children.empty()) fail(UI_FLECK_ELEMENT, "Border accepts at most one child");
            auto child=element(depth + 1);
            if (!devtools::accepts_child(node.kind,node.children.size(),child.kind)) fail(UI_FLECK_ELEMENT,"Invalid child for this element");
            node.children.push_back(std::move(child));
            trivia();
        }
        close_tag(tag);
        if (has_resources) { styles_.swap(outer_styles); literals_.swap(outer_literals); }
        node.location.element = source_index_.span(node.offset, offset_ - node.offset);
        namespaces_ = std::move(saved_namespaces);
        return node;
    }
};
}

bool trigger_setter(ui_node_kind kind, std::string_view key, const std::string& literal, unsigned& order,
                    std::map<ui_property, unsigned>& conditions, ui_property& property, BindingValue& value)
{
    auto field = [&]() {
        auto dot = key.find('.');
        auto part = key.substr(0,dot);
        key = dot == key.npos ? std::string_view{} : key.substr(dot+1);
        return part;
    };
    auto integer = [](std::string_view text,auto& number) {
        auto result = std::from_chars(text.data(),text.data()+text.size(),number);
        return result.ec == std::errc{} && result.ptr == text.data()+text.size();
    };
    if (!integer(field(),order) || order >= 512) return false;
    const bool data = key.starts_with("Data.");
    if (data) key.remove_prefix(5);
    while (key.find('.') != key.npos) {
        ui_property condition;
        unsigned expected;
        if (!properties::find(field(),condition) || !integer(field(),expected)) return false;
        ui_property_descriptor descriptor{sizeof(descriptor),1};
        if (ui_property_describe(kind,condition,&descriptor) != UI_OK) return false;
        if (data || !visual_state::trigger_source(condition)) return false;
        if (condition == UI_PROPERTY_IS_ENABLED && expected > 1) return false;
        if (visual_state::property(condition)) {
            ui_binding_value v{};
            v.kind = descriptor.kind;
            v.integer = expected;
            if (!visual_state::valid(condition,v)) return false;
        }
        if (!conditions.emplace(condition,expected).second) return false;
    }
    return (!conditions.empty() || data) && dynamic_style_target(kind,key,"trigger",property) && style_value(kind,key,literal,property,value);
}

bool dynamic_style_target(ui_node_kind kind, std::string_view name, std::string_view key, ui_property& property) {
    ui_property_descriptor descriptor{sizeof(descriptor),1};
    return valid_binding_name(key) && properties::find(name,property) &&
           ui_property_describe(kind,property,&descriptor) == UI_OK &&
           (text_format::property(property) || border::property(property) || property == UI_PROPERTY_OPACITY ||
            property == UI_PROPERTY_RENDER_TRANSFORM || property == UI_PROPERTY_RENDER_TRANSFORM_ORIGIN ||
            property == UI_PROPERTY_LAYOUT_TRANSFORM);
}

bool style_value(ui_node_kind kind, std::string_view name, const std::string& literal, ui_property& property, BindingValue& value) {
    ui_property_descriptor descriptor{sizeof(descriptor), 1};
    if (!properties::find(name, property) || ui_property_describe(kind, property, &descriptor) != UI_OK) return false;
    if (text_format::property(property) || visual_state::property(property)) {
        ui_binding_value parsed{};
        bool valid = text_format::property(property) ? text_format::parse(property,literal,parsed)
                                                      : visual_state::parse(property,literal,parsed);
        return valid && binding_copy_value(parsed,value,descriptor.kind) == UI_OK;
    }
    return binding_parse_value(literal,descriptor.kind,value) == UI_OK;
}

bool read_storyboard(std::string_view xml,ui_node_kind kind,std::vector<animation::Track>& tracks) {
    try { Parser(xml).storyboard_fragment(kind,tracks); return true; }
    catch (const Failure&) { return false; }
}

bool read_drawing(std::string_view xml,drawing::Brush* brush,ui_effect_desc* effect) {
    try { Parser(xml).drawing_fragment(brush,effect); return true; }
    catch (const Failure&) { return false; }
}

void report(std::string_view input, std::size_t offset, ui_fleck_error code,
            const char* message, ui_fleck_diagnostic& diagnostic)
{
    diagnostic = {};
    diagnostic.code = code;
    diagnostic.line = 1;
    diagnostic.column = 1;
    for (std::size_t i = 0; i < std::min(offset, input.size()); ++i) {
        if (input[i] == '\r' || (input[i] == '\n' && (i == 0 || input[i - 1] != '\r'))) {
            ++diagnostic.line;
            diagnostic.column = 1;
        } else if (input[i] != '\n' && (static_cast<unsigned char>(input[i]) & 0xc0) != 0x80) {
            ++diagnostic.column;
        }
    }
    std::snprintf(diagnostic.message, sizeof(diagnostic.message), "%s", message);
}

std::optional<PointerBindingKey> pointer_binding(std::string_view action,std::string_view phase)
{
    PointerBindingKey key;
    if (action == "Down") key.first = UI_POINTER_DOWN;
    else if (action == "Up") key.first = UI_POINTER_UP;
    else if (action == "Move") key.first = UI_POINTER_MOVE;
    else if (action == "Cancel") key.first = UI_POINTER_CANCEL;
    else return {};
    if (phase == "Direct") key.second = UI_EVENT_DIRECT;
    else if (phase == "Preview") key.second = UI_EVENT_PREVIEW;
    else if (phase == "Bubble") key.second = UI_EVENT_BUBBLE;
    else return {};
    return key;
}

std::optional<PointerBindingKey> pointer_binding(std::string_view property)
{
    constexpr std::string_view prefix = ".PointerBinding.";
    if (!property.starts_with(prefix)) return {};
    property.remove_prefix(prefix.size());
    auto split = property.find('.');
    if (split == property.npos) return {};
    return pointer_binding(property.substr(0,split),property.substr(split+1));
}

std::optional<unsigned> key_binding_code(std::string_view property)
{
    if (!property.starts_with(".KeyBinding.")) return {};
    property.remove_prefix(12);
    unsigned code = 0;
    auto parsed = std::from_chars(property.data(),property.data()+property.size(),code);
    if (parsed.ec != std::errc{} || parsed.ptr != property.data()+property.size() || (code >> 11) > 2 || !valid_gesture(code & 255, (code >> 8) & 7)) return {};
    return code;
}

bool valid_gesture(unsigned key,unsigned modifiers)
{
    if (modifiers >= 8) return false;
    if (key >= UI_KEY_F1 && key <= UI_KEY_F24) return true;
    return ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) && (modifiers & (UI_KEY_CTRL | UI_KEY_ALT));
}

bool key_gesture(std::string_view text,unsigned& key,unsigned& modifiers)
{
    modifiers = 0;
    while (text.size() > 1) {
        auto split = text.find('+');
        if (split == text.npos) break;
        auto name = text.substr(0,split);
        unsigned flag = name == "Ctrl" ? UI_KEY_CTRL : name == "Shift" ? UI_KEY_SHIFT : name == "Alt" ? UI_KEY_ALT : 0;
        if (!flag || (modifiers & flag)) return false;
        modifiers |= flag;
        text.remove_prefix(split+1);
    }
    if (text.size() == 1) key = static_cast<unsigned char>(text.front());
    else {
        if (!text.starts_with('F') || text[1] == '0') return false;
        unsigned number = 0;
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data()+1,end,number);
        if (parsed.ec != std::errc{} || parsed.ptr != end || number < 1 || number > 24) return false;
        key = UI_KEY_F1 + number - 1;
    }
    return valid_gesture(key, modifiers);
}

bool is_dictionary(std::string_view input)
{
    if (input.size() > 1024*1024) return false;
    try { return Parser(input).document_root() == "ResourceDictionary"; }
    catch (const Failure&) { return false; }
}
bool parse(std::string_view input, Page& page, ui_fleck_diagnostic& diagnostic,
           const DictionarySources& dictionaries, std::string_view source)
{
    try {
        if (input.size() > 1024 * 1024) {
            throw Failure{0, UI_FLECK_LIMIT, "Fleck file exceeds 1 MiB"};
        }
        for (std::size_t i = 0; i < input.size(); ++i) {
            const auto byte = static_cast<unsigned char>(input[i]);
            if (byte < 32 && !space(input[i])) {
                throw Failure{i, UI_FLECK_SYNTAX, "Invalid control character"};
            }
        }
        try {
            (void)utf8_text::decode(input);
        } catch (const std::range_error&) {
            throw Failure{0, UI_FLECK_SYNTAX, "Invalid UTF-8"};
        }
        Parser parser(input);
        size_t imported = 0;
        parser.dictionaries = &dictionaries;
        parser.imported_bytes = &imported;
        parser.source = source;
        page = parser.document();
        diagnostic = {};
        return true;
    } catch (const ui_fleck_diagnostic& external) {
        diagnostic = external;
        return false;
    } catch (const Failure& error) {
        report(input, error.offset, error.code, error.message, diagnostic);
        return false;
    }
}
}


bool fleck::valid_binding_name(std::string_view name)
{
    return !name.empty() && name.size() <= 64 && letter(name.front()) &&
        std::all_of(name.begin(), name.end(), name_char);
}

docking::Theme docking::parse_theme(std::string_view text, ui_fleck_diagnostic& diagnostic)
{
    diagnostic = {};
    try {
        if (text.size() > 1024 * 1024) { throw fleck::Failure{0, UI_FLECK_LIMIT, "Theme exceeds 1 MiB"}; }
        (void)utf8_text::decode(text);
        for (const unsigned char ch : text) { if (ch < 32 && ch != '\t' && ch != '\r' && ch != '\n') { throw fleck::Failure{0, UI_FLECK_SYNTAX, "Invalid theme character"}; } }
        return fleck::Parser(text).theme_document();
    } catch (const fleck::Failure& error) {
        fleck::report(text, error.offset, error.code, error.message, diagnostic);
        throw std::range_error(error.message);
    }
}

