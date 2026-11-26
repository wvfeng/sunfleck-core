#pragma once
#include <sunfleck/ui.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace devtools {
struct Span {
    size_t offset = 0, length = 0;
    uint32_t line = 0, column = 0, end_line = 0, end_column = 0;
};
struct Attribute {
    std::string name;
    Span range, value;
};
struct Source {
    std::string uri, model, fingerprint;
    uint64_t page_id = 0, model_id = 0;
};
struct Location {
    std::string source_uri;
    Span element;
    std::vector<Attribute> attributes;
};
struct Diagnostic {
    uint64_t sequence = 0, object = 0, model = 0, occurrences = 0;
    std::string fleck_id, path, code, phase, message, expected_type, actual_type;
    ui_property property = UI_PROPERTY_TEXT;
    ui_result status = UI_OK;
    bool active = false;
    std::shared_ptr<Source> source;
    Location location;
};
void report(ui_node& node, ui_model* model, const ui_binding_options& options, ui_result status,
            const char* code, const char* phase, const char* message, const char* actual_type = nullptr) noexcept;
const char* value_type(ui_value_kind kind);
void recover(ui_node& node, ui_property property, std::string_view path) noexcept;
std::string diagnostics_json(ui_app* app, uint64_t after_sequence = 0);
void dispatch_diagnostics(ui_app* app) noexcept;
class SourceIndex {
    std::vector<std::pair<uint32_t, uint32_t>> positions_;
public:
    explicit SourceIndex(std::string_view source);
    Span span(size_t offset, size_t length) const;
};
std::string span_json(const Span& span);
std::string location_json(const Source* source, const Location& location);
ui_result copy_json(const std::string& json, char* output, size_t* size);
const char* control_name(ui_node_kind kind);
bool text_target(ui_node_kind kind);
bool numeric_target(ui_node_kind kind);
bool boolean_target(ui_node_kind kind);
bool indexed(ui_node_kind kind);
bool numeric(ui_node_kind kind);
bool has_value(ui_node_kind kind);
bool accepts_child(ui_node_kind parent, size_t count, ui_node_kind child);
bool attribute_allowed(ui_node_kind kind, std::string_view name);
std::string schema_json();
std::string node_properties_json(const ui_node& node);
std::string model_json(ui_model* model, const char* name);
uint64_t next_id(ui_app* app);
}
