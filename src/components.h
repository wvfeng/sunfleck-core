#pragma once
#include "binding.h"
#include "property.h"
#include "fleck.h"
#include <map>
#include <memory>
#include <stdexcept>

namespace components {
using Key = std::pair<std::string, std::string>;
struct Property : BindingValue {
    ui_property_id id = 0;
    std::string name;
    bool read_only = false;
    ui_binding_mode mode = UI_BIND_ONE_WAY;
    ui_component_property_metadata metadata{};
    Property() = default;
    Property(std::string property_name, const BindingValue& data, bool read, ui_binding_mode binding_mode)
        : BindingValue(data), name(std::move(property_name)), read_only(read), mode(binding_mode) {}
};
struct Type {
    Key key, default_template;
    std::vector<Property> properties;
    std::vector<std::string> events;
    struct Part {
        std::string name;
        ui_node_kind kind;
        bool required;
    };
    std::vector<Part> parts;
    ui_component_lifecycle lifecycle = nullptr;
    ui_component_changed changed = nullptr;
    void *context = nullptr;
};
struct Registry {
    std::map<Key, std::shared_ptr<Type>> types;
    std::map<Key, fleck::Page> documents;
    fleck::DictionarySources dictionaries;
};
struct Error : std::range_error {
    uint32_t line, column;
    Error(const fleck::Element &element, const std::string &message);
};
void report(const fleck::Element &element, const char *message,
            ui_fleck_diagnostic &diagnostic);
ui_result validate_binding(ui_node &node, const fleck::Binding &binding);
Registry &registry(ui_app *app);
void expand(Registry &registry, fleck::Element &element);
void prepare(ui_node &node, const fleck::Element &element, ui_model *inherited);
ui_result bind(ui_node &node, const fleck::Element &element,
               ui_model *inherited);
void validate_parts(ui_node &node);
void mount(ui_node &node);
void shutdown(ui_node &node);
void model_destroyed(ui_model &model);
void clear_reference(ui_app *app, ui_value_kind kind, const void *reference);
ui_result resolve_retained_values(ui_component& component);
} // namespace components

struct ui_component : std::enable_shared_from_this<ui_component> {
    ui_node *host = nullptr;
    void *state = nullptr;
    void (*release_state)(void *) = nullptr;
    std::shared_ptr<components::Type> type;
    std::unique_ptr<ui_model> properties;
    struct Value : components::Property {
        properties::Slot layers;
        ui_component *component = nullptr;
    };
    std::map<std::string, Value> values;
    bool initialized = false, mounted = false, destroyed = false,
         changing_property = false;
    bool is_control = false;
    components::Key selected_template;
    fleck::Element declaration;
    struct Connection {
        ui_model *model;
        std::string path;
        ui_binding_mode mode;
        std::shared_ptr<ModelObserver> observer;
        bool uses_data_context = false;
    };
    std::map<std::string, Connection> connections;
    struct Event {
        uint64_t token;
        std::string name;
        ui_component_event_handler handler;
        void *context;
    };
    std::vector<Event> events;
    uint64_t next_token = 1;
    ~ui_component();
};

namespace components {
struct ContextBindingUpdate {
    ui_component* component = nullptr;
    std::string name;
    ui_component::Connection connection{};
    properties::Slot layers;
    BindingValue effective;
    bool reconnect = false, disconnect = false;
};
const ContextBindingUpdate* pending_property(const std::vector<ContextBindingUpdate>& updates, const ui_component& component, const std::string& name);
void record_property_update(std::vector<ContextBindingUpdate>& updates, ContextBindingUpdate update);
ui_result prepare_inherited_values(ui_node& node, std::vector<ContextBindingUpdate>& updates);
ui_result prepare_inherited_subtree(ui_node& node, std::vector<ContextBindingUpdate>& updates);
ui_result bind_context_property(ui_component& component, const char* name, const char* path, ui_binding_mode mode);
ui_result prepare_context_bindings(ui_component& component, ui_model* model, std::vector<ContextBindingUpdate>& updates);
const BindingValue* prepared_binding_value(const std::vector<ContextBindingUpdate>& updates, ui_model* model, const std::string& path);
ui_result commit_context_bindings(std::vector<ContextBindingUpdate>& updates);
}
