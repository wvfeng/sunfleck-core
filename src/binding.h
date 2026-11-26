#pragma once
#include <sunfleck/ui.h>
#include "property_value.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <functional>

struct PropertyBinding {
    ui_model* model = nullptr;
    ui_property property = UI_PROPERTY_TEXT;
    std::string path, format;
    ui_binding_mode mode = UI_BIND_ONE_WAY;
    ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED;
    bool pending = false;
    uint64_t model_id = 0;
    bool uses_data_context = false;
    bool invalid = false;
    std::shared_ptr<ui_model> element_source;
};
struct ElementPropertySource {
    ui_node* node = nullptr;
    std::weak_ptr<ui_model> model;
};
void binding_element_changed(ui_node& node, ui_property property);
struct ModelObserver { std::function<ui_result(const std::string&, bool)> notify; };
struct CollectionItem { uint64_t identity; std::string text; ui_model* model = nullptr; };
struct TextCollection {
    std::vector<CollectionItem> items;
    ui_model* source = nullptr;
    std::shared_ptr<ModelObserver> subscription;
    ui_collection_sort sort = UI_COLLECTION_SOURCE_ORDER;
    std::string filter;
    bool read_only = false;
    bool group_by_label = false;
    struct Group { uint32_t first, count; };
    std::vector<Group> groups;
    uint64_t next_identity = 1;
    int64_t revision = 0;
};
struct ui_model {
    std::weak_ptr<ElementPropertySource> element_owner;
    std::unique_ptr<TextCollection> collection;
    std::vector<std::weak_ptr<ModelObserver>> observers;
    uint64_t debug_id = 0;
    struct Property {
        ui_value_kind kind; ui_property_get get; ui_property_set set; void* context;
        std::string source_uri;
        uint32_t source_line = 0, source_column = 0;
        uint32_t command_modes = 0;
    };
    ui_app* owner;
    std::map<std::string, Property> properties;
    std::set<std::string> pending;
    unsigned batch = 0, callbacks = 0;
    bool notifying = false;
    bool has_context_targets = false; // Conservative hint; avoids a second node scan for scalar-only models.
    struct Adapter { ui_text_source* source = nullptr; ui_command* command = nullptr; std::string text; };
    std::vector<std::unique_ptr<Adapter>> adapters;
    struct Converter { ui_value_kind output; ui_binding_convert convert; void* context; };
    std::map<std::string,Converter,std::less<>> converters;
    std::map<std::string,std::vector<std::string>,std::less<>> conversion_inputs;
    std::vector<std::shared_ptr<void>> derived_properties;
    std::map<std::string,std::vector<std::string>,std::less<>> dependents;
};
ui_result binding_app_check(ui_app* app);
std::vector<std::unique_ptr<ui_node>>& binding_app_nodes(ui_app* app);
void binding_app_dirty(ui_app* app);
ui_result binding_apply_text(ui_node& node, const std::wstring& text);
void binding_app_error(ui_app* app, ui_result status);
bool binding_node_focused(ui_node* node);
void binding_user_changed(ui_node& node, ui_property property);
bool binding_allows_line_breaks(const ui_node& node);
void binding_flush_focus(ui_app* app);
void binding_detach(ui_node& node, ui_property property);
void binding_text_source_changed(ui_text_source& source);
ui_result binding_validate(ui_node& node, const ui_binding_options& options, const char** diagnostic = nullptr);
ui_result binding_read_value(ui_model& model, const std::string& path, BindingValue& result);
bool binding_path(std::string_view path);
ui_result binding_copy_value(const ui_binding_value& input, BindingValue& output, ui_value_kind expected);

ui_result binding_parse_value(std::string_view text, ui_value_kind kind, BindingValue& output);
ui_result binding_read_command(ui_node& node,const std::string& path,ui_command*& command);
ui_model* binding_data_context(const ui_node& node);
ui_result binding_initialize_data_triggers(ui_node& node);
ui_result binding_refresh_data_context(ui_node& root);
ui_result binding_release_data_context(ui_model& model);
bool binding_releasing_model();
ui_result binding_set_data_context(ui_node* node, ui_model* model, bool has_value);
