#pragma once
#include <functional>
#include "visual_layout.h"
#include "timeline.h"
#include "affine.h"
#include "layout_cache.h"
#include "grid_layout.h"
#include "border.h"
#include "geometry.h"
#include "drawing_commands.h"
#include "visual_state.h"
#include "text_format.h"
#include <sunfleck/ui.h>
#include "gallery.h"
#include "document.h"
#include "binding.h"
#include "async_command.h"
#include "devtools.h"
#include "property.h"

#include <memory>
#include <optional>
#include <string>

struct ui_text_source {
    ui_app* owner;
    std::wstring value;
    std::vector<std::pair<ui_model*, std::string>> property_observers;
};

struct ui_command {
    ui_app* owner;
    ui_node_action action;
    void* context;
    ui_async_command_options async{};
    ui_external_command_options external{};
    std::shared_ptr<AsyncCommandRun> run;
    bool asynchronous() const { return async.execute || external.begin; }
};

namespace fleck { struct Element; }
struct DataGridColumn { std::string path; std::wstring header; double width = 120; bool editable = true; };
struct DataGridState {
    std::vector<DataGridColumn> columns;
    bool automatic = true;
    uint32_t first = 0, visible = 0;
    layout::Rect header{};
    double left = 0;
    std::string sort_path;
    bool descending = false;
    int resizing_column = -1;
    double resize_origin = 0, resize_width = 0;
    int64_t sorted_revision = -1;
    std::vector<uint32_t> order;
    std::set<uint64_t> sorted_models;
};
struct ui_component;
struct ui_node {
    ui_node() = default;
    // Keep the small stack-node construction form used by render/layout tests
    // and embedders while the runtime also supports staged component nodes.
    ui_node(ui_app* owner_value, ui_node_kind kind_value) : owner(owner_value), kind(kind_value) {}
    ui_node(ui_node&&) noexcept = default;
    ui_node& operator=(ui_node&&) noexcept = default;
    ~ui_node();
    std::shared_ptr<ui_component> component;
    std::shared_ptr<char> animation_lifetime;
    std::vector<animation::Track> storyboard;
    using StateTracks = std::map<std::string, std::vector<animation::Track>, std::less<>>;
    std::map<std::string, StateTracks, std::less<>> visual_groups;
    std::string_view common_input_state, check_state, focus_state, selection_state, validation_state;
    uint32_t first_realized = 0, realized_limit = 12;
    uint64_t selected_identity = 0, realized_source_id = 0;
    size_t item_count() const { return items_source ? items_source->collection->items.size() : children.size(); }
    bool is_selected_item() const {
        if (collection_item_id && parent) return parent->selected_identity == collection_item_id;
        return parent && parent->value < parent->children.size() && parent->children[parent->value] == this;
    }
    struct ActivationHandler { ui_activation_handler callback = nullptr; void* context = nullptr; };
    std::array<ActivationHandler,3> activation_handlers{};
    std::map<std::pair<ui_pointer_action,ui_event_phase>,std::string> pointer_commands;
    std::string drag_text;
    bool allow_text_drop = false;
    uint32_t drop_kinds = 0;
    ui_drop_handler drop_handler = nullptr;
    void* drop_context = nullptr;
    struct PointerHandler { ui_pointer_handler callback; void* context; };
    std::map<std::pair<ui_pointer_action,ui_event_phase>,PointerHandler> pointer_handlers;
    std::shared_ptr<ui_component> component_scope;
    std::string scope_path;
    std::string source_uri;
    ui_app* owner;
    std::shared_ptr<document::State> document;
    ui_node_kind kind;
    ui_node* parent = nullptr;
    std::shared_ptr<ui_panel_callbacks> panel_callbacks;
    mutable ui_size panel_desired{};
    mutable std::vector<ui_layout_rect> panel_slots;
    ui_node* presented_button = nullptr;
    ui_node* content_owner = nullptr;
    ui_node* inheritance_parent() const { return content_owner ? content_owner : parent; }
    ui_node* anchor = nullptr;
    std::vector<ui_node*> children;
    ui_model* items_source = nullptr;
    std::shared_ptr<const fleck::Element> item_template;
    std::map<std::wstring,std::shared_ptr<const fleck::Element>> item_templates_by_label;
    std::map<std::string,std::vector<std::string>> binding_conversions;
    std::shared_ptr<ElementPropertySource> element_property_source;
    std::string tree_children_path, tree_label_path;
    std::unique_ptr<DataGridState> table;
    bool items_refresh_pending = false;
    uint64_t collection_item_id = 0;
    bool grid_cell_editor = false;
    std::wstring text;
    std::string fleck_id, style_key;
    struct Resources {
        std::map<std::string,std::string> declarations;
        std::map<std::string,BindingValue> overrides;
    };
    std::unique_ptr<Resources> resources;
    struct ResourceReference {
        ui_property property;
        ui_value_precedence layer;
        std::string key;
    };
    std::vector<ResourceReference> resource_references;
    bool applying_resource = false;
    struct TriggerSetter {
        unsigned order;
        std::map<ui_property, unsigned> conditions;
        std::string data_path, data_value;
        bool data_matches = false;
        ui_property property;
        BindingValue value;
    };
    std::vector<TriggerSetter> triggers;
    ui_text_source* text_source = nullptr;
    std::map<std::string,ui_command*,std::less<>> command_bindings;
    std::map<std::string,std::string,std::less<>> command_paths;
    struct InputBinding { std::string command; bool local = false; };
    std::map<unsigned,InputBinding> input_bindings;
    std::string routed_command;
    bool local_routed_command = false;
    ui_command* command = nullptr;
    ui_command_mode command_mode = UI_COMMAND_DEFAULT;
    std::vector<PropertyBinding> bindings;
    std::vector<PropertyBinding> declared_bindings;
    // A local nullptr intentionally stops inheritance; absence restores the parent.
    ui_model* local_data_context = nullptr;
    bool has_local_data_context = false;
    bool data_context_edited = false;
    std::string binding_message;
    uint64_t local_properties = 0;
    std::map<ui_property, properties::Slot> property_slots;
    bool applying_property = false;
    std::array<double,6> property_geometry{};
    std::string property_text; // UTF-8 storage for the generic borrowed-value API.
    bool enabled = true, applying_binding = false;
    double spacing = -1;
    double padding = 0;
    GalleryImage image;
    std::string image_source;
    std::array<std::uint32_t, UI_THEME_COLOR_COUNT> theme_overrides{};
    unsigned theme_override_mask = 0;
    unsigned value = 0;
    unsigned fleck_value = 0;
    double height = -1; // Logical pixels; -1 retains automatic measurement.
    std::optional<ui_orientation> orientation;
    bool vertical() const { return orientation.value_or(kind==UI_STACK_PANEL ? UI_VERTICAL : UI_HORIZONTAL)==UI_VERTICAL; }
    double canvas_left = 0, canvas_top = 0;
    int32_t z_index = 0;
    VisualLayout layout;
    grid_layout::Settings grid;
    BorderStyle border;
    std::array<std::shared_ptr<const drawing::Brush>,3> brushes;
    ui_effect_desc effect{};
    std::shared_ptr<const drawing::Geometry> geometry;
    double stroke_thickness=1;
    TextFormat font, resolved_font;
    std::vector<StyledRun> resolved_runs;
    mutable std::wstring inline_content;
    ui_visibility visibility = UI_VISIBLE;
    bool hit_test_visible = true;
    std::optional<ui_stretch> stretch;
    ui_matrix render_transform = affine::identity;
    ui_point transform_origin{};
    ui_matrix layout_transform = affine::identity;
    ui_matrix world_transform = affine::identity;
    layout::Rect local_hit{};
    double opacity = 1;
    bool is_focus_scope = false;
    uint64_t logical_focus_id = 0;
    ui_node* focus_scope() {
        for (auto* n = this; n; n = n->inheritance_parent())
            if (n->is_focus_scope) return n;
        return nullptr;
    }
    bool is_tab_stop = true;
    int tab_index = INT32_MAX;
    bool clip_to_bounds = true;
    std::vector<ui_node*> layout_children;
    LayoutCache layout_cache;
    unsigned weight = 1;
    ui_dock_edge dock = UI_DOCK_LEFT;
    ui_dock* workspace = nullptr;
    unsigned dock_window = 0;
    std::string dock_title;
    std::string dock_icon;
    unsigned dock_capabilities = UI_DOCK_DEFAULT_CAPABILITIES;
    double dock_min_width = 96, dock_min_height = 64;
    // Device-space layout distances remain fractional until painting/input boundaries.
    ui_scroll_policy horizontal_scroll = UI_SCROLL_DISABLED, vertical_scroll = UI_SCROLL_AUTO;
    double horizontal_offset = 0, horizontal_limit = 0;
    layout::Rect horizontal_bar{}, horizontal_thumb{}, viewport{};
    bool dragging_horizontal = false;
    double offset = 0;
    double scroll_limit = 0;
    layout::Rect scrollbar{};
    layout::Rect scrollbar_thumb{};
    bool dragging = false;
    bool expanded = false;
    bool tree_selected = false;
    bool select_all = false;
    bool text_selectable = false;
    std::size_t caret = 0;
    std::optional<std::size_t> selection_anchor;
    bool anchor_trailing = false;
    bool caret_trailing = false;
    std::optional<double> caret_column; // Preferred logical content X during vertical navigation.
    std::pair<size_t,size_t> selection(size_t length) const {
        if (select_all) return {0,length};
        const auto anchor=std::min(selection_anchor.value_or(caret),length);
        return std::minmax(anchor,std::min(caret,length));
    }
    std::pair<size_t,size_t> selection() const { return selection(text.size()); }
    void clear_selection() { select_all=false; selection_anchor.reset(); caret_column.reset(); }
    std::vector<TextStop> text_stops;
    layout::Rect text_bounds{}; // Actual renderer text origin and wrapping width.
    HostedEditor hosted_editor;
    wchar_t pending_surrogate = 0;
    layout::Rect bounds{};
    layout::Rect visible{};
    ui_action_concurrency action_concurrency = UI_ACTION_SINGLE_FLIGHT;
    unsigned running_actions = 0;
    unsigned deferred_actions = 0;
    ui_node_action action = nullptr;
    void* context = nullptr;
    uint64_t debug_id = 0;
    std::shared_ptr<devtools::Source> source;
    devtools::Location location;
    ui_window* window_owner = nullptr;

    bool action_busy() const
    {
        return (command || action || !routed_command.empty()) && action_concurrency == UI_ACTION_SINGLE_FLIGHT &&
            (running_actions || deferred_actions);
    }

    bool is_visible() const {
        for (auto* n = this; n; n = n->parent) if (n->visibility != UI_VISIBLE) return false;
        return true;
    }
    bool accepts_input() const {
        for (auto* n = this; n; n = n->parent) if (!n->enabled || !n->hit_test_visible || n->visibility != UI_VISIBLE) return false;
        return true;
    }

    bool is_enabled() const
    {
        for (auto* node = this; node; node = node->parent) { if (!node->enabled) { return false; } }
        return true;
    }

    const std::wstring& display_text() const
    {
        if (presented_button && children.empty()) return presented_button->display_text();
        if (inline_container(kind) && !children.empty()) {
            inline_content=text_source ? text_source->value : text;
            for (const auto* child:children) if (child->visibility!=UI_COLLAPSED) inline_content+=child->display_text();
            return inline_content;
        }
        return text_source ? text_source->value : text;
    }
};

struct UiOverlay {
    ui_node* owner;
    layout::Rect bounds;
};

void queue_items_refresh(ui_node& node);
const GalleryImage& resolved_image(const ui_node& node);
void prepare_grid_columns(ui_node& host, uint32_t first, uint32_t count);
ui_result realize_list_range(ui_node& host, uint32_t first, uint32_t count);
bool panel_drawing(ui_app* app, bool active);
bool is_container(ui_node_kind kind);
inline bool content_host(ui_node_kind kind) { return kind == UI_CONTENT_CONTROL || kind == UI_CONTENT_PRESENTER; }
inline bool has_content(ui_node_kind kind) { return kind == UI_BUTTON || content_host(kind); }
bool is_interactive(ui_node_kind kind);
bool is_editor(ui_node_kind kind);
bool is_popup(ui_node_kind kind);
bool is_selector(ui_node_kind kind);
// Internal measure pass in scaled layout pixels (logical pixels * DPI / 96), without
// arranging or painting. available_width is nonnegative or positive infinity.
double measure_tree_height(ui_node& root, double available_width, GalleryStyle style, ThemeShape shape, unsigned dpi);
void render_tree(ui_node& root, layout::Rect area, GalleryStyle style, ThemeShape shape, unsigned dpi,
                 const ui_node* focus, bool keyboard_focus, bool caret_visible,
                 const ui_node* hovered, const ui_node* pressed,
                 std::vector<FillRectCommand>& commands,
                 std::vector<ui_node*>& interactive, std::vector<ui_node*>& scrollable,
                 std::vector<UiOverlay>* overlays = nullptr);
void render_tree_with_pick_order(ui_node& root, layout::Rect area, GalleryStyle style, ThemeShape shape, unsigned dpi,
                 const ui_node* focus, bool keyboard_focus, bool caret_visible,
                 const ui_node* hovered, const ui_node* pressed,
                 std::vector<FillRectCommand>& commands,
                 std::vector<ui_node*>& interactive, std::vector<ui_node*>& scrollable,
                 std::vector<UiOverlay>* overlays, std::vector<ui_node*>* paint_order, drawing::CommandList* list=nullptr,
                 const std::function<void(ui_node&)>& prepare_dock_content = {});
