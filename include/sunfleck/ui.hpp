#pragma once

#include <sunfleck/ui.h>

#include <concepts>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <functional>
#include <exception>
#include <future>

namespace sunfleck {

class Error : public std::runtime_error {
public:
    explicit Error(ui_result result)
        : std::runtime_error(result == UI_ERROR_VERSION_MISMATCH
              ? "Sunfleck SDK/runtime version mismatch; rebuild the application and Fleck resources with the same SDK"
              : "Sunfleck error " + std::to_string(static_cast<int>(result))),
          result_(result) {}

    ui_result result() const noexcept { return result_; }

private:
    ui_result result_;
};

namespace detail {
inline void check(ui_result result);
template<class Read>
std::string json(Read read)
{
    size_t size = 0;
    check(read(nullptr, &size));
    std::string value(size, '\0');
    check(read(value.data(), &size));
    value.resize(size - 1);
    return value;
}
inline void check(ui_result result)
{
    if (result != UI_OK) {
        throw Error(result);
    }
}
}

inline ui_property_descriptor describeProperty(ui_node_kind kind, ui_property property) {
    ui_property_descriptor descriptor{};
    descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
    detail::check(ui_property_describe(kind, property, &descriptor));
    return descriptor;
}

// Borrowed handle. Use only while its owning App is alive, on the app thread.
enum class ActionConcurrency {
    SingleFlight = UI_ACTION_SINGLE_FLIGHT,
    AllowOverlap = UI_ACTION_ALLOW_OVERLAP
};

// Borrowed for the duration of an async command's worker callback.
class CancellationToken {
    const ui_cancellation_token* token_ = nullptr;
public:
    CancellationToken() = default;
    explicit CancellationToken(const ui_cancellation_token* token) : token_(token) {}
    const ui_cancellation_token* nativeHandle() const noexcept { return token_; }
    bool cancelled() const noexcept { return ui_cancellation_requested(token_) != 0; }
    void throwIfCancelled() const { if (cancelled()) { throw Error(UI_CANCELLED); } }
};

struct RegisteredProperty {
    ui_property_id id = 0;
    bool operator==(const RegisteredProperty&) const = default;
};

class Node {
public:
    using Action = void (*)(ui_node*, void*) noexcept;

    ui_node* nativeHandle() const noexcept { return node_; }
    struct Validation { ui_result status; std::string code, message; };
    Validation validation(ui_property property) const {
        ui_validation_result result{};
        detail::check(ui_node_get_validation(node_,property,&result));
        return {result.status,result.code,result.message};
    }
    void bindElement(ui_property target,Node source,const char* path,ui_binding_mode mode=UI_BIND_DEFAULT) const {
        const ui_binding_options options{sizeof(options),1,target,path,mode,UI_UPDATE_PROPERTY_CHANGED,nullptr};
        detail::check(ui_node_bind_element_property(node_,source.node_,&options));
    }
    uint64_t debugId() const { uint64_t id = 0; detail::check(ui_node_get_debug_id(node_, &id)); return id; }
    void setLength(ui_property property,ui_length value) const {
        ui_binding_value input{}; input.kind=UI_VALUE_LENGTH; input.integer=value.unit; input.number=value.value;
        detail::check(ui_node_set_property(node_,property,&input));
    }
    ui_length lengthValue(ui_property property) const {
        ui_binding_value value{}; detail::check(ui_node_get_property(node_,property,&value));
        if (value.kind!=UI_VALUE_LENGTH) throw Error(UI_ERROR_INVALID_ARGUMENT);
        return {value.number,ui_length_unit(value.integer)};
    }
    double lengthPixels(ui_property property) const {
        const auto value=lengthValue(property);
        return value.unit==UI_LENGTH_AUTO?-1:value.unit==UI_LENGTH_INFINITY?std::numeric_limits<double>::infinity():value.value;
    }
    void setGridTracks(const std::vector<ui_grid_track>& rows, const std::vector<ui_grid_track>& columns) const {
        if (rows.size()>256 || columns.size()>256) throw Error(UI_ERROR_INVALID_ARGUMENT);
        detail::check(ui_grid_set_tracks(nativeHandle(),rows.data(),uint32_t(rows.size()),columns.data(),uint32_t(columns.size())));
    }
    void focus() const { detail::check(ui_node_focus(node_)); }
    Node logicalFocus() const {
        ui_node* target = nullptr;
        detail::check(ui_node_get_logical_focus(node_, &target));
        return Node(target);
    }
    Node parent(ui_tree_kind tree) const {
        ui_node* result = nullptr;
        detail::check(ui_node_get_parent(node_, tree, &result));
        return Node(result);
    }
    uint32_t childCount(ui_tree_kind tree) const {
        uint32_t result = 0;
        detail::check(ui_node_get_child_count(node_, tree, &result));
        return result;
    }
    Node child(ui_tree_kind tree, uint32_t index) const {
        ui_node* result = nullptr;
        detail::check(ui_node_get_child(node_, tree, index, &result));
        return Node(result);
    }
    void setResourceReference(ui_property property, const char* key, ui_value_precedence layer = UI_PRECEDENCE_LOCAL) {
        detail::check(ui_node_set_resource_reference(node_,property,layer,key));
    }
    void setResource(const char* key, const ui_binding_value& value) { detail::check(ui_node_set_resource(node_,key,&value)); }
    void clearResource(const char* key) { detail::check(ui_node_set_resource(node_,key,nullptr)); }
    void setPanelCallbacks(const ui_panel_callbacks* callbacks) const { detail::check(ui_panel_set_callbacks(node_,callbacks)); }
    void setScrollPolicies(ui_scroll_policy horizontal, ui_scroll_policy vertical) const { detail::check(ui_scroll_set_policies(node_,horizontal,vertical)); }
    void scrollTo(double left, double top) const { detail::check(ui_scroll_to(node_,left,top)); }
    void bringIntoView() const { detail::check(ui_node_bring_into_view(node_)); }
    void setSharedSizeGroup(ui_orientation axis, uint32_t index, const char* group) const {
        detail::check(ui_grid_set_shared_size_group(node_, axis, index, group));
    }
    void setOrientation(ui_orientation value) const { detail::check(ui_node_set_orientation(nativeHandle(),value)); }
    void setCanvasPosition(double left, double top) const { detail::check(ui_node_set_canvas_position(nativeHandle(), left, top)); }
    void setZIndex(int32_t value) const { detail::check(ui_node_set_z_index(nativeHandle(),value)); }
    void setGridCell(unsigned row, unsigned column, unsigned rowSpan=1, unsigned columnSpan=1) const {
        detail::check(ui_node_set_grid_cell(nativeHandle(),row,column,rowSpan,columnSpan));
    }
    void setHeight(double value) const { setLayoutValue(UI_PROPERTY_HEIGHT,value); }
    void setHeight(ui_length value) const { setLength(UI_PROPERTY_HEIGHT,value); }
    double height() const { return lengthPixels(UI_PROPERTY_HEIGHT); }

    void setLayoutValue(ui_property property, double value) const {
        ui_binding_value input{}; input.kind = UI_VALUE_LENGTH;
        input.integer = value==-1?UI_LENGTH_AUTO:value==std::numeric_limits<double>::infinity()?UI_LENGTH_INFINITY:UI_LENGTH_PIXELS;
        input.number = input.integer==UI_LENGTH_PIXELS?value:0;
        detail::check(ui_node_set_layout_value(node_, property, &input));
    }
    void setLayoutValue(ui_property property, const char* value) const {
        ui_binding_value input{}; input.kind = UI_VALUE_STRING; input.string = value;
        detail::check(ui_node_set_layout_value(node_, property, &input));
    }
    ui_binding_value layoutValue(ui_property property) const {
        ui_binding_value value{}; detail::check(ui_node_get_layout_value(node_, property, &value)); return value;
    }
    void setWidth(double value) const { setLayoutValue(UI_PROPERTY_WIDTH, value); }
    double width() const { return lengthPixels(UI_PROPERTY_WIDTH); }
    void setWidth(ui_length value) const { setLength(UI_PROPERTY_WIDTH,value); }
    void setMinWidth(double value) const { setLayoutValue(UI_PROPERTY_MIN_WIDTH, value); }
    double minWidth() const { return lengthPixels(UI_PROPERTY_MIN_WIDTH); }
    void setMinWidth(ui_length value) const { setLength(UI_PROPERTY_MIN_WIDTH,value); }
    void setMaxWidth(double value) const { setLayoutValue(UI_PROPERTY_MAX_WIDTH, value); }
    double maxWidth() const { return lengthPixels(UI_PROPERTY_MAX_WIDTH); }
    void setMaxWidth(ui_length value) const { setLength(UI_PROPERTY_MAX_WIDTH,value); }
    void setMinHeight(double value) const { setLayoutValue(UI_PROPERTY_MIN_HEIGHT, value); }
    double minHeight() const { return lengthPixels(UI_PROPERTY_MIN_HEIGHT); }
    void setMinHeight(ui_length value) const { setLength(UI_PROPERTY_MIN_HEIGHT,value); }
    void setMaxHeight(double value) const { setLayoutValue(UI_PROPERTY_MAX_HEIGHT, value); }
    double maxHeight() const { return lengthPixels(UI_PROPERTY_MAX_HEIGHT); }
    void setMaxHeight(ui_length value) const { setLength(UI_PROPERTY_MAX_HEIGHT,value); }
    void setMargin(ui_thickness value) const {
        const double sides[]={value.left,value.top,value.right,value.bottom};
        ui_binding_value input{}; input.kind=UI_VALUE_THICKNESS; input.geometry=sides;
        detail::check(ui_node_set_layout_value(node_,UI_PROPERTY_MARGIN,&input));
    }
    ui_thickness margin() const {
        const auto* sides=layoutValue(UI_PROPERTY_MARGIN).geometry;
        return {sides[0],sides[1],sides[2],sides[3]};
    }
    void setHorizontalAlignment(const char* value) const { setLayoutValue(UI_PROPERTY_HORIZONTAL_ALIGNMENT, value); }
    std::string horizontalAlignment() const { return std::string(layoutValue(UI_PROPERTY_HORIZONTAL_ALIGNMENT).string); }
    void setVerticalAlignment(const char* value) const { setLayoutValue(UI_PROPERTY_VERTICAL_ALIGNMENT, value); }
    std::string verticalAlignment() const { return std::string(layoutValue(UI_PROPERTY_VERTICAL_ALIGNMENT).string); }
    void setLayout(double spacing, double padding) const
    {
        detail::check(ui_node_set_layout(node_, spacing, padding));
    }

    void loadDocument(std::string_view json) const { detail::check(ui_document_load(node_,json.data(),json.size())); }
    void scrollToHeading(const char* id) const { detail::check(ui_document_scroll_to(node_,id)); }
    void setDocumentScroll(int32_t offset) const { detail::check(ui_document_set_scroll(node_,offset)); }
    int32_t documentScroll() const { int32_t value=0;detail::check(ui_document_get_scroll(node_,&value));return value; }
    void scrollDocumentHorizontal(int32_t x,int32_t y,int32_t delta) const { detail::check(ui_document_scroll_horizontal(node_,x,y,delta)); }
    void selectDocument() const { detail::check(ui_document_select_all(node_)); }
    std::string documentSelection() const { return detail::json([&](char* out,size_t* size){return ui_document_selection(node_,out,size);}); }
    std::pair<ui_document_event,std::string> takeDocumentEvent() const {
        ui_document_event event=UI_DOCUMENT_NONE;
        auto value=detail::json([&](char* out,size_t* size){return ui_document_take_event(node_,&event,out,size);});return {event,std::move(value)};
    }

    void setText(const char* text) const { detail::check(ui_node_set_text(node_, text)); }
    void setDataContext(ui_model* model) const { detail::check(ui_node_set_data_context(node_, model)); }
    void clearDataContext() const { detail::check(ui_node_clear_data_context(node_)); }
    ui_model* dataContext() const {
        ui_model* model = nullptr; detail::check(ui_node_get_data_context(node_, &model)); return model;
    }
    ui_property_source dataContextSource() const {
        ui_property_source source{}; detail::check(ui_node_get_data_context_source(node_, &source)); return source;
    }
    void bindContext(ui_property property, const char* path, ui_binding_mode mode = UI_BIND_DEFAULT,
                     ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED, const char* format = nullptr) const {
        const ui_binding_options options{sizeof(options), 1, property, path, mode, trigger, format};
        detail::check(ui_node_bind_context_property(node_, &options));
    }

    // Strings in the returned native value are borrowed; copy them before the next read/update.
    RegisteredProperty resolveProperty(const char* name) const {
        RegisteredProperty property; detail::check(ui_node_resolve_property(node_, name, &property.id)); return property;
    }
    ui_registered_property_descriptor describeProperty(RegisteredProperty property) const {
        ui_registered_property_descriptor descriptor{sizeof(descriptor), 1};
        detail::check(ui_node_describe_registered_property(node_, property.id, &descriptor)); return descriptor;
    }
    ui_binding_value propertyValue(RegisteredProperty property) const {
        ui_binding_value value{}; detail::check(ui_node_get_registered_property(node_, property.id, &value)); return value;
    }
    ui_binding_value basePropertyValue(RegisteredProperty property) const {
        ui_binding_value value{}; detail::check(ui_node_get_registered_base_property(node_, property.id, &value)); return value;
    }
    void setPropertyValue(RegisteredProperty property, const ui_binding_value& value) const {
        setPropertyLayer(property, UI_PRECEDENCE_LOCAL, value);
    }
    void setPropertyLayer(RegisteredProperty property, ui_value_precedence precedence, const ui_binding_value& value) const {
        detail::check(ui_node_set_registered_property(node_, property.id, precedence, &value));
    }
    void clearPropertyLayer(RegisteredProperty property, ui_value_precedence precedence) const {
        detail::check(ui_node_clear_registered_property(node_, property.id, precedence));
    }
    void clearProperty(RegisteredProperty property) const { clearPropertyLayer(property, UI_PRECEDENCE_LOCAL); }
    ui_property_source propertySource(RegisteredProperty property) const {
        ui_property_source source{}; detail::check(ui_node_get_registered_property_source(node_, property.id, &source)); return source;
    }
    void bindProperty(RegisteredProperty property, ui_model* model, const char* path, ui_binding_mode mode = UI_BIND_DEFAULT,
                      ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED, const char* format = nullptr) const {
        const ui_registered_binding_options options{sizeof(options), 1, property.id, path, mode, trigger, format};
        detail::check(ui_node_bind_registered_property(node_, model, &options));
    }
    void bindContext(RegisteredProperty property, const char* path, ui_binding_mode mode = UI_BIND_DEFAULT,
                     ui_update_source_trigger trigger = UI_UPDATE_PROPERTY_CHANGED, const char* format = nullptr) const {
        const ui_registered_binding_options options{sizeof(options), 1, property.id, path, mode, trigger, format};
        detail::check(ui_node_bind_registered_context_property(node_, &options));
    }
    ui_binding_value propertyValue(ui_property property) const {
        ui_binding_value value{}; detail::check(ui_node_get_property(node_, property, &value)); return value;
    }
    void setPropertyValue(ui_property property, const ui_binding_value& value) const {
        detail::check(ui_node_set_property(node_, property, &value));
    }
    ui_binding_value basePropertyValue(ui_property property) const {
        ui_binding_value value{}; detail::check(ui_node_get_base_property(node_, property, &value)); return value;
    }
    void setPropertyLayer(ui_property property, ui_value_precedence precedence, const ui_binding_value& value) const {
        detail::check(ui_node_set_property_layer(node_, property, precedence, &value));
    }
    void clearPropertyLayer(ui_property property, ui_value_precedence precedence) const {
        detail::check(ui_node_clear_property_layer(node_, property, precedence));
    }
    void clearProperty(ui_property property) const { detail::check(ui_node_clear_property(node_, property)); }
    ui_property_source propertySource(ui_property property) const {
        ui_property_source source{}; detail::check(ui_node_get_property_source(node_, property, &source)); return source;
    }

    std::string text() const
    {
        uint32_t size = 0;
        detail::check(ui_node_get_text(node_, nullptr, &size));
        std::string result(size, '\0');
        detail::check(ui_node_get_text(node_, result.data(), &size));
        result.resize(size - 1);
        return result;
    }

    uint32_t value() const
    {
        uint32_t result = 0;
        detail::check(ui_node_get_value(node_, &result));
        return result;
    }

    void setValue(uint32_t value) const { detail::check(ui_node_set_value(node_, value)); }
    void setEnabled(bool enabled) const { detail::check(ui_node_set_enabled(node_, enabled)); }
    bool enabled() const { uint32_t value = 0; detail::check(ui_node_get_enabled(node_, &value)); return value != 0; }
    void clearBinding(ui_property property) const { detail::check(ui_node_clear_binding(node_, property)); }
    void updateSource(ui_property property) const { detail::check(ui_node_update_source(node_, property)); }

    void setGridColumns(const ui_data_grid_column* columns,uint32_t count) const { detail::check(ui_data_grid_set_columns(node_,columns,count)); }
    void setColumnWidth(uint32_t column,double width) const { detail::check(ui_data_grid_set_column_width(node_,column,width)); }
    void sortColumn(uint32_t column,bool descending=false) const { detail::check(ui_data_grid_sort_column(node_,column,descending)); }
    void finishCellEdit(bool commit) const { detail::check(ui_data_grid_finish_edit(node_,commit)); }
    void setItemsSource(ui_model* source) const { detail::check(ui_node_set_items_source(node_, source)); }
    void setItemTemplate(const char* label, std::string_view xml) const {
        detail::check(ui_node_set_item_template(node_, label, xml.data(), xml.size()));
    }
    void clearItemTemplate(const char* label = nullptr) const {
        detail::check(ui_node_set_item_template(node_, label, nullptr, 0));
    }
    uint64_t itemIdentity() const {
        uint64_t identity{};
        detail::check(ui_node_get_item_identity(node_, &identity));
        return identity;
    }
    void setItems(const char* const* items, uint32_t count) const
    {
        detail::check(ui_node_set_items(node_, items, count));
    }

    void setPathData(const char* data) const { detail::check(ui_path_set_data(node_,data)); }
    void setStrokeThickness(double width) const { detail::check(ui_shape_set_stroke_thickness(node_,width)); }
    void setBrush(ui_brush_target target,const ui_brush_desc* brush) const { detail::check(ui_node_set_brush(node_,target,brush)); }
    void setEffect(const ui_effect_desc* effect) const { detail::check(ui_node_set_effect(node_,effect)); }
    void setRenderTransform(ui_matrix matrix, ui_point origin = {}) const { detail::check(ui_node_set_render_transform(node_,matrix,origin)); }
    void startAnimation(ui_property property, const ui_timeline& timeline) const {
        detail::check(ui_node_start_animation(node_,property,&timeline));
    }
    void setInputBinding(unsigned key, unsigned modifiers, const char* commandName) const {
        detail::check(ui_node_set_input_binding(node_,key,modifiers,commandName));
    }
    void setInputBinding(unsigned key, unsigned modifiers, ui_event_phase phase, const char* commandName) const {
        detail::check(ui_node_set_key_binding(node_,key,modifiers,phase,commandName));
    }
    void setRoutedCommand(const char* name) const { detail::check(ui_node_set_routed_command(node_,name)); }
    void setCommandBinding(const char* name, ui_command* command) const {
        detail::check(ui_node_set_command_binding(node_,name,command));
    }
    bool startDrag(const ui_drop_data& data) const {
        const auto status = ui_node_start_drag(node_, &data);
        if (status == UI_CANCELLED) return false;
        detail::check(status);
        return true;
    }
    void setDropHandler(uint32_t kinds, ui_drop_handler handler, void* context = nullptr) const {
        detail::check(ui_node_set_drop_handler(node_, kinds, handler, context));
    }
    void setPointerHandler(ui_pointer_action action, ui_event_phase phase, ui_pointer_handler handler, void* context = nullptr) const {
        detail::check(ui_node_set_pointer_handler(node_,action,phase,handler,context));
    }
    void setActivationHandler(ui_event_phase phase, ui_activation_handler handler, void* context = nullptr) const {
        detail::check(ui_node_set_activation_handler(node_,phase,handler,context));
    }
    void beginStoryboard() const { detail::check(ui_node_begin_storyboard(node_)); }
    void registerVisualState(const char* group, const char* name, const char* storyboardXml) const {
        detail::check(ui_node_register_visual_state(node_, group, name, storyboardXml));
    }
    void goToVisualState(const char* group, const char* name) const {
        detail::check(ui_node_go_to_visual_state(node_, group, name));
    }
    void controlAnimation(ui_property property, ui_animation_action action, uint32_t positionMs = 0) const {
        detail::check(ui_node_control_animation(node_,property,action,positionMs));
    }
    ui_point toWindow(ui_point local) const { ui_point result{}; detail::check(ui_node_to_window(node_,local,&result)); return result; }
    ui_point fromWindow(ui_point window) const { ui_point result{}; detail::check(ui_node_from_window(node_,window,&result)); return result; }
    void setStretch(ui_stretch stretch) const { detail::check(ui_node_set_stretch(node_,stretch)); }
    void setImageRgba(const uint8_t* rgba, uint32_t width, uint32_t height) const { detail::check(ui_node_set_image_rgba(node_,rgba,width,height)); }
    void setImage(const uint8_t* rgb, uint32_t width, uint32_t height) const
    {
        detail::check(ui_node_set_image(node_, rgb, width, height));
    }

    void append(Node child) const { detail::check(ui_node_append(node_, child.node_)); }

    void setAction(Action action, void* context = nullptr,
                   ActionConcurrency concurrency = ActionConcurrency::SingleFlight) const
    {
        detail::check(ui_node_set_action_with_concurrency(
            node_, action, context, static_cast<ui_action_concurrency>(concurrency)));
    }

    void deferAction() const { detail::check(ui_node_defer_action(node_)); }
    void completeAction() const { detail::check(ui_node_complete_action(node_)); }
    void setCommandMode(ui_command_mode mode) const { detail::check(ui_node_set_command_mode(node_, mode)); }

private:
    friend class App;
    friend class UserControl;
    friend class FrameworkElement;
    friend class Control;
    friend class DataContext;
    explicit Node(ui_node* node) noexcept : node_(node) {}

    ui_node* node_;
};

inline void setDocumentFont(uint32_t role, const void* bytes, size_t size) { detail::check(ui_document_set_font(role,bytes,size)); }

class TextSource {
public:
    ui_text_source* nativeHandle() const noexcept { return source_.get(); }
    void set(const char* value) const { detail::check(ui_text_source_set(source_.get(), value)); }

private:
    friend class App;
    friend class DataContext;
    explicit TextSource(ui_text_source* source) noexcept : source_(source, ui_text_source_destroy) {}

    std::unique_ptr<ui_text_source, decltype(&ui_text_source_destroy)> source_;
};

using FleckLoader = ui_result (*)(ui_app*, const ui_fleck_data_context*, ui_fleck_diagnostic*);
using FleckModelLoader = ui_result (*)(ui_app*, ui_model*, const ui_fleck_data_context*, ui_fleck_diagnostic*);

// Keep the returned context alive while using the page.
// Destroy it on the UI thread before destroying the App or moving/destroying the model.
class DataContext {
public:
    void text(const char* name, const TextSource& source)
    {
        texts_.push_back({name, source.source_.get()});
    }

    template<auto Method, class Model>
        requires requires(Model& model, Node node) { { (model.*Method)(node) } noexcept -> std::same_as<void>; }
    ui_command* command(const char* name, Model& model)
    {
        ui_command* command = nullptr;
        detail::check(ui_command_create(app_, [](ui_node* node, void* context) noexcept {
            (static_cast<Model*>(context)->*Method)(Node(node));
        }, &model, &command));
        Command owned(command, ui_command_destroy);
        commands_.push_back({name, std::move(owned)});
        return command;
    }

    DataContext(DataContext&&) noexcept = default;
    ui_command* asyncCommand(const char* name, const ui_async_command_options& options)
    {
        ui_command* command = nullptr;
        detail::check(ui_command_create_async(app_, &options, &command));
        Command owned(command, ui_command_destroy);
        commands_.push_back({name, std::move(owned)});
        return command;
    }
    DataContext& operator=(DataContext&&) = delete;

private:
    friend class App;
    explicit DataContext(ui_app* app) noexcept : app_(app) {}
    template<class> friend class FleckPage;

    using Command = std::unique_ptr<ui_command, decltype(&ui_command_destroy)>;
    struct TextExport { std::string name; ui_text_source* source; };
    struct CommandExport { std::string name; Command command; };

    void load(FleckLoader loader, FleckModelLoader modelLoader = nullptr, ui_model* model = nullptr)
    {
        if (!loader && !modelLoader) {
            throw Error(UI_ERROR_INVALID_ARGUMENT);
        }
        std::vector<ui_fleck_text_export> texts;
        std::vector<ui_fleck_command_export> commands;
        texts.reserve(texts_.size());
        commands.reserve(commands_.size());
        for (const auto& entry : texts_) {
            texts.push_back({entry.name.c_str(), entry.source});
        }
        for (const auto& entry : commands_) {
            commands.push_back({entry.name.c_str(), entry.command.get()});
        }
        const ui_fleck_data_context context{texts.data(), texts.size(), commands.data(), commands.size()};
        detail::check(modelLoader ? modelLoader(app_, model, &context, nullptr) : loader(app_, &context, nullptr));
    }

    ui_app* app_;
    std::vector<TextExport> texts_;
    std::vector<CommandExport> commands_;
};

template<class Model>
concept FleckModel = requires(Model& model, DataContext& context) {
    { model.exportBindings(context) } -> std::same_as<void>;
};

// Create, use and destroy on the same thread.
class Window {
    ui_window* window_;
public:
    explicit Window(ui_window* window) noexcept : window_(window) {}
    ui_window* nativeHandle() const noexcept { return window_; }
    uint32_t id() const { uint32_t value = 0; detail::check(ui_window_get_id(window_, &value)); return value; }
    // Existing App APIs act on this window for the duration of the callback.
    void withContext(const std::function<void()>& work) const
    {
        struct State { const std::function<void()>* work; std::exception_ptr error; } state{&work, {}};
        const auto result = ui_window_invoke(window_, [](ui_app*, void* raw) -> ui_result {
            auto& state = *static_cast<State*>(raw);
            try { (*state.work)(); return UI_OK; }
            catch (...) { state.error = std::current_exception(); return UI_ERROR_INVALID_STATE; }
        }, &state);
        if (state.error) { std::rethrow_exception(state.error); }
        detail::check(result);
    }
    void open() const { detail::check(ui_window_invoke(window_, [](ui_app* app, void*) { return ui_app_open_window(app); }, nullptr)); }
    void requestClose() const { detail::check(ui_window_request_close(window_)); }
};

class App {
public:
    explicit App(ui_renderer renderer, bool headless = false)
        : App(ui_app_options{sizeof(ui_app_options), UI_API_VERSION, static_cast<uint32_t>(renderer), headless ? uint32_t{UI_HEADLESS} : 0u}) {}
    explicit App(const ui_app_options& options)
    {
        ui_app* app = nullptr;
        detail::check(ui_app_create(&options, &app));
        app_.reset(app);
    }

    App(App&&) noexcept = default;
    App& operator=(App&&) noexcept = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    ui_app* nativeHandle() const noexcept { return app_.get(); }
    void advanceAnimationClock(uint32_t elapsedMs) { detail::check(ui_app_advance_animation_clock(app_.get(),elapsedMs)); }
    void setReducedMotion(bool enabled) { detail::check(ui_app_set_reduced_motion(app_.get(),enabled)); }
    void setAnimationCompleted(ui_animation_completed callback,void* context = nullptr) {
        detail::check(ui_app_set_animation_completed(app_.get(),callback,context));
    }
    ui_font_id registerFont(const void* bytes, size_t size) {
        ui_font_id id=0; detail::check(ui_app_register_font(app_.get(),bytes,size,&id)); return id;
    }
    void unregisterFont(ui_font_id id) { detail::check(ui_app_unregister_font(app_.get(),id)); }
    std::vector<std::string> fontFamilies(ui_font_id id) const {
        uint32_t count=0; detail::check(ui_app_font_family_count(app_.get(),id,&count));
        std::vector<std::string> names;
        for (uint32_t i=0;i<count;++i) names.push_back(detail::json([&](char* text,size_t* size){return ui_app_font_family(app_.get(),id,i,text,size);}));
        return names;
    }
    Window createWindow()
    {
        ui_window* window = nullptr;
        detail::check(ui_app_create_window(app_.get(), &window));
        return Window(window);
    }
    void setResource(const char* key, const ui_binding_value& value) { detail::check(ui_app_set_resource(app_.get(),key,&value)); }
    void registerImageRgba(const char* key, const uint8_t* rgba, uint32_t width, uint32_t height) {
        detail::check(ui_app_register_image_rgba(app_.get(), key, rgba, width, height));
    }
    void clearResource(const char* key) { detail::check(ui_app_set_resource(app_.get(),key,nullptr)); }
    void setThemeResource(const char* key, const ui_binding_value& value) { detail::check(ui_app_set_theme_resource(app_.get(),key,&value)); }
    void clearThemeResource(const char* key) { detail::check(ui_app_set_theme_resource(app_.get(),key,nullptr)); }
    static std::string fleckSchema() { return detail::json(ui_fleck_get_schema); }
    std::string debugSnapshot() const
    {
        return detail::json([&](char* value, size_t* size) { return ui_app_get_debug_snapshot(app_.get(), value, size); });
    }
    void setDebugPicking(bool enabled) const
    {
        detail::check(ui_app_set_debug_picking(app_.get(), enabled ? 1u : 0u));
    }
    void setDebugHighlight(uint64_t objectId, uint32_t leaseMilliseconds = 1000) const
    {
        detail::check(ui_app_set_debug_highlight(app_.get(), objectId, leaseMilliseconds));
    }
    void enableDebugServer(const char* pipeName) const
    {
        detail::check(ui_app_enable_debug_server(app_.get(), pipeName));
    }
    std::string bindingDiagnostics() const
    {
        return detail::json([&](char* value, size_t* size) { return ui_app_get_binding_diagnostics(app_.get(), value, size); });
    }
    void setDiagnosticCallback(ui_diagnostic_callback callback, void* context = nullptr)
    {
        detail::check(ui_app_set_diagnostic_callback(app_.get(), callback, context));
    }
    void dispatchDiagnostics() { detail::check(ui_app_dispatch_diagnostics(app_.get())); }
    void setDiagnosticLogCallback(ui_diagnostic_callback callback, void* context = nullptr)
    {
        detail::check(ui_app_set_diagnostic_log_callback(app_.get(), callback, context));
    }
    void setFleckSourceUri(const char* uri) { detail::check(ui_app_set_fleck_source_uri(app_.get(), uri)); }
    void attachFleckSourceMap(std::string_view map)
    {
        detail::check(ui_app_attach_fleck_source_map(app_.get(), map.data(), map.size(), nullptr));
    }
    Node findDebugNode(uint64_t id) const
    {
        ui_node* node = nullptr;
        detail::check(ui_app_find_debug_node(app_.get(), id, &node));
        return Node(node);
    }

    void setWindowOptions(const char* title, uint32_t width, uint32_t height)
    {
        detail::check(ui_app_set_window_options(app_.get(), title, width, height));
    }

    Node createNode(ui_node_kind kind)
    {
        ui_node* node = nullptr;
        detail::check(ui_node_create(app_.get(), kind, &node));
        return Node(node);
    }

    TextSource createTextSource(const char* value)
    {
        ui_text_source* source = nullptr;
        detail::check(ui_text_source_create(app_.get(), value, &source));
        return TextSource(source);
    }

    template<FleckModel Model>
    [[nodiscard]] DataContext loadFleck(FleckLoader loader, Model& model)
    {
        DataContext context(app_.get());
        model.exportBindings(context);
        context.load(loader);
        return context;
    }

    template<FleckModel Model>
    [[nodiscard]] DataContext loadFleck(FleckModelLoader loader, Model& model, ui_model* properties)
    {
        DataContext context(app_.get());
        model.exportBindings(context);
        context.load(nullptr, loader, properties);
        return context;
    }

    Node findFleckNode(const char* id) const
    {
        ui_node* node = nullptr;
        detail::check(ui_app_find_fleck_node(app_.get(), id, &node));
        return Node(node);
    }
    void loadFleck(std::string_view xml)
    {
        detail::check(ui_app_load_fleck(app_.get(), xml.data(), xml.size(), nullptr));
    }
    void refreshFleck(std::string_view xml, ui_model* model = nullptr)
    {
        detail::check(ui_app_refresh_fleck(app_.get(), xml.data(), xml.size(), model, nullptr));
    }
    void queueFleckRefresh(std::string_view xml, ui_model* model = nullptr)
    {
        detail::check(ui_app_queue_fleck_refresh(app_.get(), xml.empty() ? "" : xml.data(), xml.size(), model));
    }
    void cancelFleckRefresh() { detail::check(ui_app_queue_fleck_refresh(app_.get(), nullptr, 0, nullptr)); }
    void setFleckRefreshStateCallback(ui_fleck_refresh_state_callback callback, void* context = nullptr)
    {
        detail::check(ui_app_set_fleck_refresh_state_callback(app_.get(), callback, context));
    }
    ui_fleck_refresh_status fleckRefreshStatus() const
    {
        ui_fleck_refresh_status status{sizeof(status), 1};
        detail::check(ui_app_get_fleck_refresh_status(app_.get(), &status));
        return status;
    }

    void setTheme(ui_builtin_theme theme, bool dark = false, bool compact = false)
    {
        detail::check(ui_app_set_builtin_theme(app_.get(), theme, dark, compact));
    }
    void setTheme(const ui_theme& theme) { detail::check(ui_app_set_theme(app_.get(), &theme)); }
    void setRoot(Node root) { detail::check(ui_app_set_root(app_.get(), root.nativeHandle())); }
    [[nodiscard]] Node fleckRoot() const
    {
        ui_node* root = nullptr;
        detail::check(ui_app_get_fleck_root(app_.get(), &root));
        return Node(root);
    }
    void discardFleckPage(Node root) { detail::check(ui_app_discard_fleck_page(app_.get(), root.nativeHandle())); }
    void openWindow() { detail::check(ui_app_open_window(app_.get())); }
    void activateButton() { detail::check(ui_app_activate_button(app_.get())); }
    void requestClose() { detail::check(ui_app_request_close(app_.get())); }
    // May be called by workers while App remains alive; captures run/release on UI thread.
    void post(std::function<void()> work, CancellationToken cancellation = {})
    {
        if (!work) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        auto task = std::make_unique<std::function<void()>>([work = std::move(work), error = posted_error_] {
            try { work(); } catch (...) { if (!*error) { *error = std::current_exception(); } }
        });
        const auto callback = [](void* p) { (*static_cast<std::function<void()>*>(p))(); };
        const auto release = [](void* p) { delete static_cast<std::function<void()>*>(p); };
        detail::check(cancellation.nativeHandle()
            ? ui_app_post_cancellable(app_.get(), cancellation.nativeHandle(), callback, task.get(), release)
            : ui_app_post(app_.get(), callback, task.get(), release));
        task.release();
    }

    // Queue on UI; do not wait/get on the UI thread. Cancellation resolves the
    // future with Error(UI_CANCELLED), including during command/App destruction.
    std::future<void> invokeAsync(std::function<void()> work, CancellationToken cancellation = {})
    {
        if (!work) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        struct Invocation { std::function<void()> work; std::promise<void> done; bool invoked = false; };
        auto task = std::make_unique<Invocation>();
        task->work = std::move(work);
        auto future = task->done.get_future();
        const auto callback = [](void* raw) {
            auto& task = *static_cast<Invocation*>(raw);
            task.invoked = true;
            try { task.work(); task.done.set_value(); }
            catch (...) { task.done.set_exception(std::current_exception()); }
        };
        const auto release = [](void* raw) {
            std::unique_ptr<Invocation> task(static_cast<Invocation*>(raw));
            if (!task->invoked) { task->done.set_exception(std::make_exception_ptr(Error(UI_CANCELLED))); }
        };
        const auto result = cancellation.nativeHandle()
            ? ui_app_post_cancellable(app_.get(), cancellation.nativeHandle(), callback, task.get(), release)
            : ui_app_post(app_.get(), callback, task.get(), release);
        if (result == UI_OK) { task.release(); }
        else { task->done.set_exception(std::make_exception_ptr(Error(result))); }
        return future;
    }

    ui_result poll(uint32_t timeoutMs)
    {
        const auto result = ui_app_poll(app_.get(), timeoutMs);
        if (*posted_error_) { std::rethrow_exception(std::exchange(*posted_error_, {})); }
        if (result != UI_TIMEOUT && result != UI_EXITED) {
            detail::check(result);
        }
        return result;
    }

    ui_result run()
    {
        while (poll(1000) != UI_EXITED) {}
        return UI_EXITED;
    }

private:
    std::shared_ptr<std::exception_ptr> posted_error_ = std::make_shared<std::exception_ptr>();
    std::unique_ptr<ui_app, decltype(&ui_app_destroy)> app_{nullptr, ui_app_destroy};
};

} // namespace sunfleck
