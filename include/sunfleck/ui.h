#ifndef SUNFLECK_UI_H
#define SUNFLECK_UI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_app ui_app;
typedef struct ui_window ui_window;
typedef struct ui_node ui_node;
typedef struct ui_text_source ui_text_source;
typedef struct ui_command ui_command;
typedef struct ui_cancellation_token ui_cancellation_token;
typedef struct ui_model ui_model;
typedef enum ui_fleck_error {
    UI_FLECK_NONE, UI_FLECK_SYNTAX, UI_FLECK_LIMIT, UI_FLECK_FORBIDDEN,
    UI_FLECK_ELEMENT, UI_FLECK_ATTRIBUTE, UI_FLECK_VALUE, UI_FLECK_RUNTIME
} ui_fleck_error;
typedef struct ui_fleck_diagnostic {
    ui_fleck_error code;
    uint32_t line, column;
    char message[128];
} ui_fleck_diagnostic;
typedef enum ui_result {
    UI_OK, UI_ERROR_INVALID_ARGUMENT, UI_ERROR_OUT_OF_MEMORY,
    UI_ERROR_PLATFORM, UI_ERROR_UNSUPPORTED, UI_ERROR_WRONG_THREAD,
    UI_ERROR_INVALID_STATE, UI_TIMEOUT, UI_EXITED, UI_CANCELLED, UI_ERROR_VERSION_MISMATCH
} ui_result;

typedef enum ui_renderer { UI_AUTO, UI_GLES3, UI_SOFTWARE, UI_D3D11, UI_METAL, UI_ANGLE } ui_renderer;
/* App-owned independent window session. Shares models/commands with the App;
   owns its page, Dock, input and refresh queue. Handles live until App destruction.
   Invoke selects this session for existing App APIs on the UI thread. The callback
   may load/configure/open/refresh/close it, but may not Poll/Run/destroy App or
   create/switch to another session. Exceptions must not cross the C callback. */
typedef ui_result (*ui_window_callback)(ui_app* app, void* context);
ui_result ui_app_create_window(ui_app* app, ui_window** out_window);
ui_result ui_app_get_current_window(ui_app* app, ui_window** out_window);
ui_result ui_window_invoke(ui_window* window, ui_window_callback callback, void* context);
ui_result ui_window_request_close(ui_window* window);
ui_result ui_window_get_id(ui_window* window, uint32_t* id);
typedef enum ui_node_kind {
    UI_STACK_PANEL, UI_FLEX, UI_GRID, UI_SCROLL_VIEW, UI_PANEL, UI_CARD,
    UI_BUTTON, UI_TEXT_BLOCK, UI_IMAGE, UI_CHECK_BOX, UI_RADIO_BUTTON,
    UI_SWITCH, UI_SLIDER, UI_SEPARATOR, UI_PROGRESS_BAR,
    UI_POPUP, UI_COMBO_BOX, UI_TEXT_BOX, UI_ITEMS_CONTROL, UI_LIST_VIEW,
    UI_LIST_ITEM, UI_TAB_VIEW, UI_TAB_ITEM,
    UI_OVERLAY, UI_WRAP_PANEL, UI_DOCK_PANEL, UI_SPLIT_VIEW,
    UI_LABEL, UI_ICON, UI_BADGE, UI_ICON_BUTTON, UI_TOGGLE_BUTTON, UI_HYPERLINK,
    UI_SPIN_BOX, UI_PASSWORD_BOX, UI_TEXT_AREA, UI_SEARCH_BOX,
    UI_TREE_VIEW, UI_DATA_GRID, UI_MENU_BAR, UI_TOOL_BAR, UI_NAVIGATION_VIEW,
    UI_DIALOG, UI_TOOLTIP, UI_CONTEXT_MENU, UI_SPINNER, UI_INFO_BAR,
    UI_TREE_ITEM,
    UI_DOCK_HOST, UI_DOCK_SPLIT, UI_DOCK_GROUP, UI_DOCK_DOCUMENT, UI_DOCK_TOOL,
    UI_DOCUMENT_VIEW, UI_BORDER, UI_RUN, UI_SPAN, UI_CANVAS, UI_CONTENT_CONTROL, UI_CONTENT_PRESENTER, UI_VIEWBOX, UI_RECTANGLE, UI_ELLIPSE, UI_LINE, UI_PATH
} ui_node_kind;
/* Document data v1 is copied transactionally. All offsets are device pixels.
   Events are consumed by take_event; strings use the two-call size protocol.
   Handles follow the same owning-App and UI-thread rules as other nodes. */
typedef enum ui_document_event { UI_DOCUMENT_NONE, UI_DOCUMENT_LINK, UI_DOCUMENT_COPY } ui_document_event;
ui_result ui_document_load(ui_node* node, const char* json, size_t size);
ui_result ui_document_scroll_to(ui_node* node, const char* heading);
ui_result ui_document_set_scroll(ui_node* node, int32_t offset);
ui_result ui_document_get_scroll(ui_node* node, int32_t* offset);
ui_result ui_document_selection(ui_node* node, char* text, size_t* size);
ui_result ui_document_select_all(ui_node* node);
ui_result ui_document_take_event(ui_node* node, ui_document_event* event, char* text, size_t* size);
ui_result ui_document_scroll_horizontal(ui_node* node, int32_t x, int32_t y, int32_t delta);
/* Font bytes are copied; set before creating apps. 0=body, 1=code, 2=symbol fallback. */
ui_result ui_document_set_font(uint32_t role, const void* bytes, size_t size);

/* App-scoped fonts. Bytes are copied; IDs remain valid until unregister or App
   destruction. Calls require the owning UI thread. Failed registration leaves
   the previous collection and output ID unchanged. Windows supports TTF/OTF/TTC;
   other backends currently return UI_ERROR_UNSUPPORTED. Family names use the
   two-call UTF-8 buffer protocol, including the terminating zero in size. */
typedef uint64_t ui_font_id;
ui_result ui_app_register_font(ui_app* app, const void* bytes, size_t size, ui_font_id* id);
ui_result ui_app_unregister_font(ui_app* app, ui_font_id id);
ui_result ui_app_font_family_count(ui_app* app, ui_font_id id, uint32_t* count);
ui_result ui_app_font_family(ui_app* app, ui_font_id id, uint32_t index, char* text, size_t* size);

typedef enum ui_dock_edge { UI_DOCK_LEFT, UI_DOCK_TOP, UI_DOCK_RIGHT, UI_DOCK_BOTTOM } ui_dock_edge;
typedef enum ui_builtin_theme {
    UI_BUILTIN_FLUENT_2 = 0,
    UI_BUILTIN_MATERIAL_3 = 1,
    UI_BUILTIN_RADIX = 2,
    UI_BUILTIN_LIQUID_GLASS = 3,
    UI_BUILTIN_SUNFLECK_LITE = 4,
    UI_BUILTIN_THEME_COUNT = 5
} ui_builtin_theme;

typedef enum ui_theme_color {
    UI_THEME_BACKGROUND, UI_THEME_PANEL, UI_THEME_CARD, UI_THEME_TEXT,
    UI_THEME_PRIMARY, UI_THEME_ON_PRIMARY, UI_THEME_SELECTED, UI_THEME_CONTROL,
    UI_THEME_FOCUS, UI_THEME_DIVIDER, UI_THEME_SCRIM, UI_THEME_COLOR_COUNT
} ui_theme_color;
typedef struct ui_theme {
    uint32_t struct_size;
    uint32_t colors[UI_THEME_COLOR_COUNT]; /* 0xRRGGBB */
    uint32_t gap, control_height, window_padding, gallery_gap; /* logical pixels */
} ui_theme;
typedef enum ui_event_phase { UI_EVENT_PREVIEW, UI_EVENT_DIRECT, UI_EVENT_BUBBLE } ui_event_phase;
/* Activation travels through logical ancestors, the source, then its ancestors in reverse.
   Return nonzero to mark handled and suppress subsequent handlers and default activation. */
typedef int (*ui_activation_handler)(ui_node* sender, ui_node* source, ui_event_phase phase, void* context);
/* Assign a named command source, or register a handler on a logical ancestor.
   A null name on the source clears routing. A null command removes a binding. */
/* Letters and digits require Ctrl or Alt. Function keys allow any modifier combination. */
typedef enum ui_key_modifiers { UI_KEY_CTRL = 1, UI_KEY_SHIFT = 2, UI_KEY_ALT = 4 } ui_key_modifiers;
typedef enum ui_function_key {
    UI_KEY_F1 = 0xA0, UI_KEY_F2, UI_KEY_F3, UI_KEY_F4, UI_KEY_F5, UI_KEY_F6,
    UI_KEY_F7, UI_KEY_F8, UI_KEY_F9, UI_KEY_F10, UI_KEY_F11, UI_KEY_F12,
    UI_KEY_F13, UI_KEY_F14, UI_KEY_F15, UI_KEY_F16, UI_KEY_F17, UI_KEY_F18,
    UI_KEY_F19, UI_KEY_F20, UI_KEY_F21, UI_KEY_F22, UI_KEY_F23, UI_KEY_F24
} ui_function_key;
ui_result ui_node_set_input_binding(ui_node* node, unsigned key, unsigned modifiers, const char* command_name);
ui_result ui_node_set_key_binding(ui_node* node, unsigned key, unsigned modifiers, ui_event_phase phase, const char* command_name);
ui_result ui_host_key_gesture(ui_app* app, unsigned key, unsigned modifiers, uint32_t* handled);
ui_result ui_node_set_routed_command(ui_node* node, const char* name);
ui_result ui_node_set_command_binding(ui_node* node, const char* name, ui_command* command);
ui_result ui_node_set_activation_handler(ui_node* node, ui_event_phase phase, ui_activation_handler handler, void* context);

typedef void (*ui_node_action)(ui_node* node, void* context);
typedef enum ui_command_mode { UI_COMMAND_DEFAULT, UI_COMMAND_SYNC, UI_COMMAND_ASYNC } ui_command_mode;
/* UI_DEBUG_PIPE opts into SUNFLECK_DEBUG_PIPE at creation; unset means no service. */
enum { UI_API_VERSION = 5, UI_FLECK_RESOURCE_VERSION = 16, UI_HEADLESS = 1, UI_SHOW_RENDERER = 2, UI_DEBUG_PIPE = 4 };
uint32_t ui_api_version(void);
typedef struct ui_app_options {
    uint32_t struct_size, api_version, renderer, flags;
} ui_app_options;

/* Calls and destruction belong to the creating thread. */
ui_result ui_app_create(const ui_app_options* options, ui_app** out_app);
/* Configure the title and logical size before opening the window. */
ui_result ui_app_set_window_options(ui_app* app, const char* title, uint32_t width, uint32_t height);
ui_result ui_app_open_window(ui_app* app);
/* A page's exports. Arrays and names are borrowed only during loading.
   Exported sources and commands must be live objects owned by the receiving app. */
typedef struct ui_fleck_text_export {
    const char* name;
    ui_text_source* source;
} ui_fleck_text_export;

typedef struct ui_fleck_command_export {
    const char* name;
    ui_command* command;
} ui_fleck_command_export;

typedef struct ui_fleck_data_context {
    const ui_fleck_text_export* text_sources;
    size_t text_source_count;
    const ui_fleck_command_export* commands;
    size_t command_count;
} ui_fleck_data_context;

/* Load the static Fleck subset before opening the window. On failure the app is unchanged. */
ui_result ui_app_load_fleck(ui_app* app, const char* utf8, size_t length, ui_fleck_diagnostic* diagnostic);
/* A supplied context explicitly exports text sources and commands as model properties.
   NULL provides no bindings; there is no global name lookup. */
ui_result ui_app_load_fleck_with_context(ui_app* app, const char* utf8, size_t length,
                                        const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic);
/* Load before opening the window. The builder is valid only during the synchronous callback.
   Nodes are copied in insertion order. Add the root first, then refer to parents by zero-based index.
   Failed loads leave the app unchanged. */
typedef struct ui_fleck_builder ui_fleck_builder;
typedef struct ui_fleck_node_spec {
    ui_node_kind kind;
    const char* text;
    const char* id;
    const char* anchor;
    uint32_t value, has_value, weight;
    double spacing, padding;
    ui_dock_edge dock;
} ui_fleck_node_spec;
#define UI_FLECK_NO_PARENT UINT32_MAX
typedef ui_result (*ui_fleck_build_fn)(ui_fleck_builder* builder);
/* Generated/manual builders must call this before adding nodes. Regenerate old
   compiled C/C++ pages with the SDK's fleckc; a missing handshake is rejected. */
ui_result ui_fleck_require_version(ui_fleck_builder* builder, uint32_t api_version);
ui_result ui_fleck_add_node(ui_fleck_builder* builder, uint32_t parent, const ui_fleck_node_spec* spec);
/* Optional metadata for a previously added DockDocument/DockTool node. */
ui_result ui_fleck_set_dock_icon(ui_fleck_builder* builder, uint32_t node_index, const char* icon);
ui_result ui_fleck_set_dock_options(ui_fleck_builder* builder, uint32_t node_index,
                                   uint32_t capabilities, uint32_t min_width, uint32_t min_height);
ui_result ui_app_build_fleck(ui_app* app, const char* title, uint32_t width, uint32_t height,
                             ui_fleck_build_fn build, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_build_fleck_with_context(ui_app* app, const char* title, uint32_t width, uint32_t height,
                                         ui_fleck_build_fn build, const ui_fleck_data_context* context,
                                         ui_fleck_diagnostic* diagnostic);
/* Load a versioned fleckc binary resource from memory. No XML is parsed. */
ui_result ui_app_load_fleck_resource(ui_app* app, const void* bytes, size_t size,
                                     ui_fleck_diagnostic* diagnostic);
ui_result ui_app_load_fleck_resource_with_context(ui_app* app, const void* bytes, size_t size,
                                                 const ui_fleck_data_context* context,
                                                 ui_fleck_diagnostic* diagnostic);
/* Look up an Id in the most recently loaded Fleck page. */
ui_result ui_app_find_fleck_node(ui_app* app, const char* id, ui_node** out_node);
/* Discard an inactive Fleck page obtained through ui_app_get_fleck_root. */
ui_result ui_app_discard_fleck_page(ui_app* app, ui_node* root);
ui_result ui_app_get_fleck_root(ui_app* app, ui_node** out_root);
/* Nodes and borrowed node handles live until app destruction. All calls use the UI thread. */
ui_result ui_node_create(ui_app* app, ui_node_kind kind, ui_node** out_node);
ui_result ui_node_append(ui_node* parent, ui_node* child);
typedef enum ui_tree_kind { UI_LOGICAL_TREE, UI_VISUAL_TREE } ui_tree_kind;
/* Focus a visible interactive node, or restore the remembered node of a focus scope. */
ui_result ui_node_focus(ui_node* node);
/* Returns NULL when the scope has no available remembered target. */
ui_result ui_node_get_logical_focus(ui_node* scope, ui_node** target);
ui_result ui_node_get_parent(ui_node* node, ui_tree_kind tree, ui_node** parent);
ui_result ui_node_get_child_count(ui_node* node, ui_tree_kind tree, uint32_t* count);
ui_result ui_node_get_child(ui_node* node, ui_tree_kind tree, uint32_t index, ui_node** child);
/* Explicit Width fixes the track. Auto tracks use Weight; Flex grows from MinWidth. */
/* Height is in logical pixels (0..4096); -1 restores automatic measurement. */
ui_result ui_node_set_height(ui_node* node, int32_t height);
ui_result ui_node_get_height(const ui_node* node, int32_t* height);
typedef enum ui_grid_unit { UI_GRID_PIXELS, UI_GRID_AUTO, UI_GRID_STAR } ui_grid_unit;
typedef struct ui_grid_track { ui_grid_unit unit; double value; } ui_grid_track;
typedef enum ui_orientation { UI_HORIZONTAL, UI_VERTICAL } ui_orientation;
ui_result ui_grid_set_shared_size_group(ui_node* grid, ui_orientation axis, uint32_t index, const char* group);
typedef enum ui_scroll_policy { UI_SCROLL_DISABLED, UI_SCROLL_AUTO, UI_SCROLL_HIDDEN, UI_SCROLL_VISIBLE } ui_scroll_policy;
ui_result ui_scroll_set_policies(ui_node* node, ui_scroll_policy horizontal, ui_scroll_policy vertical);
/* Scroll offsets are in logical pixels. */
ui_result ui_scroll_get_offset(const ui_node* node, double* left, double* top);
ui_result ui_scroll_to(ui_node* node, double left, double top);
ui_result ui_node_bring_into_view(ui_node* node);
ui_result ui_node_set_orientation(ui_node* node, ui_orientation orientation);
ui_result ui_node_set_canvas_position(ui_node* node, double left, double top);
ui_result ui_node_set_z_index(ui_node* node, int32_t z_index);
ui_result ui_grid_set_tracks(ui_node* grid, const ui_grid_track* rows, uint32_t row_count,
                             const ui_grid_track* columns, uint32_t column_count);
ui_result ui_node_set_grid_cell(ui_node* node, uint32_t row, uint32_t column, uint32_t row_span, uint32_t column_span);
ui_result ui_node_set_weight(ui_node* node, uint32_t weight);
/* Set container spacing and padding in logical pixels. -1 selects theme spacing. */
ui_result ui_node_set_layout(ui_node* node, double spacing, double padding);
ui_result ui_node_set_dock(ui_node* node, ui_dock_edge edge);
/* Associate a tooltip or context menu with a visible node in the same app. */
ui_result ui_node_set_anchor(ui_node* popup, ui_node* anchor);
/* Populate an empty ItemsControl, ListView, or ComboBox with copied labels. */
ui_result ui_node_set_items(ui_node* node, const char* const* labels, uint32_t count);
/* Attach a text collection to an empty ItemsControl, ListView or ComboBox.
   Passing NULL detaches the collection and removes its generated children. */
ui_result ui_node_set_items_source(ui_node* node, ui_model* source);

/* Set a ListView/ItemsControl template from a Fleck Window containing one visual.
   NULL label sets the fallback. NULL xml removes the selected template. */
ui_result ui_node_set_item_template(ui_node* node, const char* label, const char* xml, size_t size);
/* Zero for a static node. Generated container handles expire when their item
   is removed or their source is replaced. Moves preserve the handle. */
typedef struct ui_data_grid_column {
    const char* path;
    const char* header;
    double width;
    uint32_t editable;
} ui_data_grid_column;
/* An empty definition enables automatic columns from the first object item. */
ui_result ui_data_grid_set_columns(ui_node* grid, const ui_data_grid_column* columns, uint32_t count);
ui_result ui_data_grid_sort_column(ui_node* grid, uint32_t column, uint32_t descending);
ui_result ui_data_grid_finish_edit(ui_node* cell, uint32_t commit);
ui_result ui_data_grid_set_column_width(ui_node* grid, uint32_t column, double width);
ui_result ui_node_get_item_identity(ui_node* node, uint64_t* identity);
ui_result ui_app_set_root(ui_app* app, ui_node* root);
ui_result ui_node_set_text(ui_node* node, const char* utf8);
/* Size includes the terminating NUL. Pass NULL to query the required size. */
ui_result ui_node_get_text(ui_node* node, char* utf8, uint32_t* size);
typedef enum ui_stretch { UI_STRETCH_NONE, UI_STRETCH_FILL, UI_STRETCH_UNIFORM, UI_STRETCH_UNIFORM_TO_FILL } ui_stretch;
ui_result ui_node_set_stretch(ui_node* node, ui_stretch stretch);
/* Packed, row-major images accept dimensions from 1 to 4096 pixels per axis. */
ui_result ui_node_set_image_rgba(ui_node* node, const uint8_t* rgba, uint32_t width, uint32_t height);
/* Copy decoded RGBA pixels into the app's named image resources. Reusing a key
   updates all Images whose Source names it. Unresolved keys display no image. */
ui_result ui_app_register_image_rgba(ui_app* app, const char* key, const uint8_t* rgba, uint32_t width, uint32_t height);
ui_result ui_node_set_image(ui_node* node, const uint8_t* rgb, uint32_t width, uint32_t height);
/* Selection is 0/1; slider and progress values are 0–100.
   Radio buttons with the same parent are mutually exclusive. */
ui_result ui_node_set_value(ui_node* node, uint32_t value);
ui_result ui_node_get_value(ui_node* node, uint32_t* value);
/* User input invokes the callback on the UI thread; programmatic updates do not.
   Callbacks may update properties or request close. */
typedef enum ui_action_concurrency {
    UI_ACTION_SINGLE_FLIGHT, UI_ACTION_ALLOW_OVERLAP
} ui_action_concurrency;
/* By default, an activation is dropped while this node's action is in flight. */
ui_result ui_node_set_action(ui_node* node, ui_node_action action, void* context);
ui_result ui_node_set_action_with_concurrency(ui_node* node, ui_node_action action, void* context,
                                             ui_action_concurrency concurrency);
/* Defer once from the active action or command callback. Complete once per successful defer,
   on the UI thread, after the asynchronous work finishes or is cancelled. */
ui_result ui_node_defer_action(ui_node* node);
ui_result ui_node_complete_action(ui_node* node);
/* Sources and commands are UI-thread objects owned by the app. Destroying either unbinds its nodes. */
ui_result ui_text_source_create(ui_app* app, const char* utf8, ui_text_source** out_source);
ui_result ui_text_source_set(ui_text_source* source, const char* utf8);
ui_result ui_text_source_destroy(ui_text_source* source);
typedef enum ui_binding_mode {
    UI_BIND_ONE_WAY, UI_BIND_ONE_TIME, UI_BIND_TWO_WAY, UI_BIND_DEFAULT
} ui_binding_mode;

/* Property bindings v1.
   Models are app-owned UI-thread objects. Getters return borrowed UTF-8, copied before
   returning to the caller; callbacks must not throw or destroy/reconfigure a model.
   Notify after changing business data; NULL/empty path refreshes all properties. */
typedef enum ui_value_kind { UI_VALUE_STRING, UI_VALUE_BOOL, UI_VALUE_INT, UI_VALUE_DOUBLE, UI_VALUE_COMMAND, UI_VALUE_MODEL,
    UI_VALUE_COLOR, UI_VALUE_THICKNESS, UI_VALUE_CORNER_RADIUS, UI_VALUE_POINT, UI_VALUE_SIZE, UI_VALUE_MATRIX,
    UI_VALUE_LENGTH, UI_VALUE_ENUM } ui_value_kind;
typedef struct ui_thickness { double left, top, right, bottom; } ui_thickness;
typedef struct ui_corner_radius { double top_left, top_right, bottom_right, bottom_left; } ui_corner_radius;
typedef struct ui_point { double x, y; } ui_point;
typedef struct ui_size { double width, height; } ui_size;
typedef struct ui_layout_rect { double x, y, width, height; } ui_layout_rect;
typedef ui_result (*ui_panel_measure_fn)(void* context, ui_size available, const ui_size* children, uint32_t count, ui_size* desired);
typedef ui_result (*ui_panel_arrange_fn)(void* context, ui_size final_size, const ui_size* children, uint32_t count, ui_layout_rect* slots);
typedef struct ui_panel_callbacks {
    uint32_t struct_size, version;
    ui_panel_measure_fn measure;
    ui_panel_arrange_fn arrange;
    void* context;
    void (*release)(void* context);
} ui_panel_callbacks;
ui_result ui_panel_set_callbacks(ui_node* panel, const ui_panel_callbacks* callbacks);
typedef struct ui_matrix { double m11, m12, m21, m22, offset_x, offset_y; } ui_matrix;
ui_result ui_node_set_render_transform(ui_node* node, ui_matrix matrix, ui_point relative_origin);
ui_result ui_node_to_window(ui_node* node, ui_point local, ui_point* window);
ui_result ui_node_from_window(ui_node* node, ui_point window, ui_point* local);
typedef enum ui_length_unit { UI_LENGTH_PIXELS, UI_LENGTH_AUTO, UI_LENGTH_INFINITY } ui_length_unit;
typedef struct ui_length { double value; ui_length_unit unit; } ui_length;
typedef struct ui_color { uint32_t argb; } ui_color;
typedef enum ui_brush_kind { UI_BRUSH_SOLID, UI_BRUSH_LINEAR, UI_BRUSH_RADIAL, UI_BRUSH_IMAGE } ui_brush_kind;
typedef enum ui_brush_target { UI_BRUSH_BACKGROUND, UI_BRUSH_OUTLINE, UI_BRUSH_OPACITY_MASK } ui_brush_target;
typedef struct ui_gradient_stop { double offset; ui_color color; } ui_gradient_stop;
typedef struct ui_brush_desc {
    ui_brush_kind kind;
    ui_color color;
    ui_point start, end; /* Relative coordinates. Radial brushes use center and radii. */
    const ui_gradient_stop* stops;
    uint32_t stop_count;
    const uint8_t* rgba;
    uint32_t width, height;
    ui_stretch stretch;
} ui_brush_desc;
typedef enum ui_effect_kind { UI_EFFECT_NONE, UI_EFFECT_BLUR, UI_EFFECT_DROP_SHADOW } ui_effect_kind;
typedef struct ui_effect_desc { ui_effect_kind kind; double radius; ui_point offset; ui_color color; } ui_effect_desc;
/* Brush inputs are copied. A null descriptor clears the brush override. */
ui_result ui_node_set_brush(ui_node* node, ui_brush_target target, const ui_brush_desc* brush);
ui_result ui_path_set_data(ui_node* node,const char* data);
ui_result ui_shape_set_stroke_thickness(ui_node* node,double thickness);
ui_result ui_node_set_effect(ui_node* node, const ui_effect_desc* effect);
typedef struct ui_binding_value {
    ui_value_kind kind;
    int64_t integer;
    double number;
    const char* string;
    /* Geometry points to 2/4/6 borrowed doubles matching the structures above.
       Setters/registrations copy it synchronously. Color uses integer (ARGB),
       Length uses number + integer unit, Enum uses integer. */
    union { ui_command* command; ui_model* model; const double* geometry; };
} ui_binding_value;
/* Parse an invariant literal. Geometry is copied into caller-owned storage (2/4/6 doubles).
   String results borrow utf8. On failure neither output nor storage is changed.
   Model/Command require object handles and cannot be parsed from text. */
ui_result ui_value_parse(ui_value_kind kind, const char* utf8, ui_binding_value* value, double* geometry, uint32_t geometry_count);
typedef ui_result (*ui_property_get)(void* context, ui_binding_value* value);
typedef ui_result (*ui_property_set)(void* context, const ui_binding_value* value);
typedef struct ui_model_property {
    uint32_t struct_size, version;
    const char* path;
    ui_value_kind kind;
    ui_property_get get;
    ui_property_set set;
    void* context;
} ui_model_property;
typedef enum ui_font_weight { UI_FONT_THIN=100, UI_FONT_EXTRA_LIGHT=200, UI_FONT_LIGHT=300, UI_FONT_SEMI_LIGHT=350, UI_FONT_NORMAL_WEIGHT=400, UI_FONT_MEDIUM=500, UI_FONT_SEMI_BOLD=600, UI_FONT_BOLD=700, UI_FONT_EXTRA_BOLD=800, UI_FONT_BLACK=900, UI_FONT_EXTRA_BLACK=950 } ui_font_weight;
typedef enum ui_font_style { UI_FONT_NORMAL_STYLE, UI_FONT_OBLIQUE, UI_FONT_ITALIC } ui_font_style;
typedef enum ui_font_stretch { UI_FONT_ULTRA_CONDENSED=1, UI_FONT_EXTRA_CONDENSED, UI_FONT_CONDENSED, UI_FONT_SEMI_CONDENSED, UI_FONT_NORMAL_STRETCH, UI_FONT_SEMI_EXPANDED, UI_FONT_EXPANDED, UI_FONT_EXTRA_EXPANDED, UI_FONT_ULTRA_EXPANDED } ui_font_stretch;
typedef enum ui_visibility { UI_VISIBLE, UI_HIDDEN, UI_COLLAPSED } ui_visibility;

typedef enum ui_text_alignment { UI_TEXT_ALIGN_LEFT, UI_TEXT_ALIGN_CENTER, UI_TEXT_ALIGN_RIGHT, UI_TEXT_ALIGN_JUSTIFY } ui_text_alignment;
typedef enum ui_text_wrapping { UI_TEXT_NO_WRAP, UI_TEXT_WRAP } ui_text_wrapping;
typedef enum ui_text_trimming { UI_TEXT_TRIM_NONE, UI_TEXT_TRIM_CHARACTER_ELLIPSIS, UI_TEXT_TRIM_WORD_ELLIPSIS } ui_text_trimming;
typedef enum ui_text_decorations { UI_TEXT_DECORATIONS_NONE, UI_TEXT_UNDERLINE, UI_TEXT_STRIKETHROUGH, UI_TEXT_UNDERLINE_STRIKETHROUGH } ui_text_decorations;
typedef enum ui_flow_direction { UI_FLOW_LEFT_TO_RIGHT, UI_FLOW_RIGHT_TO_LEFT } ui_flow_direction;

typedef enum ui_property { UI_PROPERTY_TEXT, UI_PROPERTY_CONTENT, UI_PROPERTY_IS_ENABLED, UI_PROPERTY_VALUE, UI_PROPERTY_COMMAND, UI_PROPERTY_HEIGHT, UI_PROPERTY_WIDTH, UI_PROPERTY_MIN_WIDTH, UI_PROPERTY_MAX_WIDTH, UI_PROPERTY_MIN_HEIGHT, UI_PROPERTY_MAX_HEIGHT, UI_PROPERTY_MARGIN, UI_PROPERTY_HORIZONTAL_ALIGNMENT, UI_PROPERTY_VERTICAL_ALIGNMENT, UI_PROPERTY_SPACING, UI_PROPERTY_PADDING, UI_PROPERTY_WEIGHT, UI_PROPERTY_DATA_CONTEXT, UI_PROPERTY_BACKGROUND, UI_PROPERTY_BORDER_BRUSH, UI_PROPERTY_BORDER_THICKNESS, UI_PROPERTY_CORNER_RADIUS, UI_PROPERTY_VISIBILITY, UI_PROPERTY_IS_HIT_TEST_VISIBLE, UI_PROPERTY_FONT_FAMILY, UI_PROPERTY_FONT_SIZE, UI_PROPERTY_FONT_WEIGHT, UI_PROPERTY_FONT_STYLE, UI_PROPERTY_FONT_STRETCH, UI_PROPERTY_FOREGROUND, UI_PROPERTY_TEXT_ALIGNMENT, UI_PROPERTY_TEXT_WRAPPING, UI_PROPERTY_TEXT_TRIMMING, UI_PROPERTY_LINE_HEIGHT, UI_PROPERTY_TEXT_DECORATIONS, UI_PROPERTY_FLOW_DIRECTION, UI_PROPERTY_CLIP_TO_BOUNDS, UI_PROPERTY_OPACITY, UI_PROPERTY_RENDER_TRANSFORM, UI_PROPERTY_RENDER_TRANSFORM_ORIGIN, UI_PROPERTY_LAYOUT_TRANSFORM, UI_PROPERTY_IS_TAB_STOP, UI_PROPERTY_TAB_INDEX, UI_PROPERTY_IS_FOCUS_SCOPE, UI_PROPERTY_ITEMS_SOURCE, UI_PROPERTY_SOURCE } ui_property;
/* Property descriptor v1 covers existing built-in mutable properties. Querying
   an unsupported kind/property pair fails. String defaults have static lifetime.
   Effects describe dependencies; they do not promise incremental rendering. */
typedef enum ui_property_flags {
    UI_PROPERTY_BINDABLE = 1, UI_PROPERTY_TWO_WAY = 2,
    UI_PROPERTY_AFFECTS_MEASURE = 4, UI_PROPERTY_AFFECTS_ARRANGE = 8,
    UI_PROPERTY_AFFECTS_RENDER = 16, UI_PROPERTY_AFFECTS_INPUT = 32,
    UI_PROPERTY_ANIMATABLE = 64,
    /* Custom values propagate to descendants using the same registered type,
       through intervening ordinary nodes. Inherited layers are framework-owned. */
    UI_PROPERTY_INHERITS = 128
} ui_property_flags;
typedef enum ui_property_source {
    UI_PROPERTY_SOURCE_DEFAULT, UI_PROPERTY_SOURCE_LOCAL, UI_PROPERTY_SOURCE_BINDING,
    UI_PROPERTY_SOURCE_INHERITED, UI_PROPERTY_SOURCE_THEME, UI_PROPERTY_SOURCE_STYLE,
    UI_PROPERTY_SOURCE_TRIGGER, UI_PROPERTY_SOURCE_TEMPLATE, UI_PROPERTY_SOURCE_ANIMATION
} ui_property_source;
typedef enum ui_value_precedence {
    UI_PRECEDENCE_INHERITED, UI_PRECEDENCE_THEME, UI_PRECEDENCE_STYLE,
    UI_PRECEDENCE_TRIGGER, UI_PRECEDENCE_TEMPLATE, UI_PRECEDENCE_LOCAL, UI_PRECEDENCE_ANIMATION
} ui_value_precedence;
typedef struct ui_property_descriptor {
    uint32_t struct_size, version;
    ui_property property;
    const char* name;
    ui_value_kind kind;
    uint32_t flags;
    ui_binding_value default_value;
} ui_property_descriptor;
ui_result ui_property_describe(ui_node_kind kind, ui_property property, ui_property_descriptor* descriptor);
/* Generic access shares validation/side effects with the existing setters.
   Text/Content require STRING; Value requires INT; commands are borrowed.
   Returned strings are borrowed until the next property read/update on this node.
   Clear removes the binding/local value and restores the metadata default.
   IsEnabled reads the effective ancestor-constrained state; its source reports
   the node's own value slot, as does the existing local property inspector. */
ui_result ui_node_set_property(ui_node* node, ui_property property, const ui_binding_value* value);
ui_result ui_node_get_property(ui_node* node, ui_property property, ui_binding_value* value);
ui_result ui_node_clear_property(ui_node* node, ui_property property);
ui_result ui_node_get_property_source(const ui_node* node, ui_property property, ui_property_source* source);
/* Layer values are copied and resolved from highest precedence to lowest.
   Animation never changes the underlying binding; base reads omit animation.
   ClearProperty removes LOCAL, revealing a lower layer or metadata default. */
ui_result ui_node_set_property_layer(ui_node* node, ui_property property, ui_value_precedence precedence, const ui_binding_value* value);
ui_result ui_node_clear_property_layer(ui_node* node, ui_property property, ui_value_precedence precedence);
typedef enum ui_easing { UI_EASE_LINEAR, UI_EASE_IN, UI_EASE_OUT, UI_EASE_IN_OUT } ui_easing;
typedef struct ui_key_frame {
    double position; /* Fraction of the forward duration. First=0, last=1. */
    ui_binding_value value;
    ui_easing easing; /* Interpolation approaching this frame. */
} ui_key_frame;
typedef struct ui_timeline {
    const ui_key_frame* frames;
    uint32_t frame_count, duration_ms, delay_ms, repeat_count;
    uint32_t auto_reverse, hold_end;
} ui_timeline;
typedef enum ui_animation_action {
    UI_ANIMATION_PAUSE, UI_ANIMATION_RESUME, UI_ANIMATION_SEEK, UI_ANIMATION_STOP
} ui_animation_action;
ui_result ui_node_start_animation(ui_node* node, ui_property property, const ui_timeline* timeline);
ui_result ui_node_begin_storyboard(ui_node* node);
/* Register and activate a named visual state. The storyboard fragment uses the
   same Timeline syntax as a node Storyboard. NULL defines an empty state. */
ui_result ui_node_register_visual_state(ui_node* node, const char* group, const char* name, const char* storyboard_xml);
ui_result ui_node_go_to_visual_state(ui_node* node, const char* group, const char* name);
/* Select a manual monotonic clock for this App. Call with zero before starting
   test animations, advance by elapsed milliseconds, then poll to paint. */
ui_result ui_app_advance_animation_clock(ui_app* app,uint32_t elapsed_ms);
typedef void (*ui_animation_completed)(ui_node* node, ui_property property, void* context);
ui_result ui_app_set_animation_completed(ui_app* app,ui_animation_completed callback,void* context);
/* A null value removes the named entry. Application resources override theme resources. */
ui_result ui_node_set_resource_reference(ui_node* node, ui_property property, ui_value_precedence layer, const char* key);
ui_result ui_node_set_resource(ui_node* node, const char* key, const ui_binding_value* value);
ui_result ui_app_set_resource(ui_app* app, const char* key, const ui_binding_value* value);
ui_result ui_app_set_theme_resource(ui_app* app, const char* key, const ui_binding_value* value);
ui_result ui_app_set_reduced_motion(ui_app* app,uint32_t enabled);
ui_result ui_node_control_animation(ui_node* node, ui_property property, ui_animation_action action, uint32_t position_ms);
ui_result ui_node_get_base_property(ui_node* node, ui_property property, ui_binding_value* value);
/* Common layout values: integer Width/min/max sizes, string Margin/alignment.
   Returned strings are borrowed until the next layout update on the node. */
ui_result ui_node_set_layout_value(ui_node* node, ui_property property, const ui_binding_value* value);
ui_result ui_node_get_layout_value(const ui_node* node, ui_property property, ui_binding_value* value);
typedef enum ui_update_source_trigger { UI_UPDATE_PROPERTY_CHANGED, UI_UPDATE_LOST_FOCUS } ui_update_source_trigger;
typedef struct ui_binding_options {
    uint32_t struct_size, version;
    ui_property property;
    const char* path;
    ui_binding_mode mode;
    ui_update_source_trigger trigger;
    const char* string_format; /* Empty or one {0}; belongs to this target binding. */
} ui_binding_options;
/* Bind to an effective built-in property on another node in the same App.
   options.path names the source property, options.property names the target. */
ui_result ui_node_bind_element_property(ui_node* target, ui_node* source, const ui_binding_options* options);
/* Runtime property identities. Zero is invalid. Built-in identities may be used
   across Apps; component identities belong to one registered type in one App.
   They survive page/template replacement but must not be serialized. Resolve
   names again in a new App. Component declarations shadow built-in names.
   Descriptor strings/default payloads are borrowed for the App lifetime.
   Registered writes enforce read_only; component-owner setters remain available
   to publish read-only state. Unsupported binding options fail without mutation. */
typedef uint64_t ui_property_id;
typedef struct ui_registered_property_descriptor {
    uint32_t struct_size, version;
    ui_property_id id;
    const char* name;
    const char* owner_namespace;
    const char* owner_name;
    ui_value_kind kind;
    uint32_t flags, read_only;
    ui_binding_value default_value;
} ui_registered_property_descriptor;
typedef struct ui_registered_binding_options {
    uint32_t struct_size, version;
    ui_property_id property;
    const char* path;
    ui_binding_mode mode;
    ui_update_source_trigger trigger;
    const char* string_format;
} ui_registered_binding_options;
ui_result ui_node_resolve_property(ui_node* node, const char* name, ui_property_id* property);
ui_result ui_node_describe_registered_property(ui_node* node, ui_property_id property, ui_registered_property_descriptor* descriptor);
ui_result ui_node_get_registered_property(ui_node* node, ui_property_id property, ui_binding_value* value);
ui_result ui_node_get_registered_base_property(ui_node* node, ui_property_id property, ui_binding_value* value);
ui_result ui_node_get_registered_property_source(ui_node* node, ui_property_id property, ui_property_source* source);
ui_result ui_node_set_registered_property(ui_node* node, ui_property_id property, ui_value_precedence precedence, const ui_binding_value* value);
ui_result ui_node_clear_registered_property(ui_node* node, ui_property_id property, ui_value_precedence precedence);
ui_result ui_node_bind_registered_property(ui_node* node, ui_model* model, const ui_registered_binding_options* options);
ui_result ui_node_bind_registered_context_property(ui_node* node, const ui_registered_binding_options* options);
/* Reusable Fleck components. Descriptors are copied; callback contexts remain borrowed.
   All component operations are UI-thread operations. Event payloads are borrowed. */
typedef struct ui_component ui_component;
typedef struct ui_component_subscription ui_component_subscription;
typedef enum ui_component_phase {
    UI_COMPONENT_INITIALIZE, UI_COMPONENT_MOUNT, UI_COMPONENT_UNMOUNT,
    UI_COMPONENT_DESTROY, UI_COMPONENT_TEMPLATE_APPLIED
} ui_component_phase;
typedef ui_result (*ui_component_lifecycle)(ui_component*, ui_component_phase, void*);
typedef void (*ui_component_changed)(ui_component*, const char*, void*);
typedef void (*ui_component_event_handler)(ui_component*, const ui_binding_value*, void*);
/* Typed C instance bases: embed as the first member of application types.
   Handles are borrowed; the App owns the component lifetime. */
typedef struct ui_framework_element { ui_component* component; } ui_framework_element;
typedef struct ui_control { ui_framework_element base; } ui_control;
typedef struct ui_user_control { ui_control base; } ui_user_control;
/* Callbacks run on the UI thread before publishing an effective value. They
   must not mutate this component. The coerced output is copied before return
   from the property operation; callback-owned pointers must remain valid until
   then. Callback contexts remain borrowed for the registered type's lifetime.
   Only the final selected value is coerced/validated, including defaults.
   Returning an error leaves all value layers and subscriptions unchanged. */
typedef ui_result (*ui_component_property_validate)(ui_component*, const ui_binding_value*, void*);
typedef ui_result (*ui_component_property_coerce)(ui_component*, const ui_binding_value*, ui_binding_value*, void*);
typedef struct ui_component_property_metadata {
    uint32_t struct_size, version, flags;
    ui_component_property_validate validate;
    ui_component_property_coerce coerce;
    void* context;
} ui_component_property_metadata;
typedef struct ui_component_property {
    const char* name;
    ui_binding_value default_value;
    uint32_t read_only;
    ui_binding_mode default_mode;
    /* NULL retains the inferred flags used by SDK 0.1. Metadata is copied. */
    const ui_component_property_metadata* metadata;
} ui_component_property;
typedef struct ui_template_part {
    const char* name;
    ui_node_kind kind;
    uint32_t required;
} ui_template_part;
enum { UI_COMPONENT_TYPE_VERSION = 2 };
typedef struct ui_component_type {
    uint32_t struct_size, version;
    const char* namespace_uri;
    const char* name;
    const char* template_namespace;
    const char* template_name;
    const ui_component_property* properties;
    size_t property_count;
    const char* const* events;
    size_t event_count;
    const ui_template_part* parts;
    size_t part_count;
    ui_component_lifecycle lifecycle;
    ui_component_changed changed;
    void* context;
} ui_component_type;
typedef struct ui_fleck_resource_entry {
    const char* uri;
    const char* utf8;
    size_t length;
} ui_fleck_resource_entry;
ui_result ui_app_register_component_type(ui_app* app, const ui_component_type* type);
ui_result ui_app_register_component_manifest(ui_app* app, const char* json, size_t length, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_validate_fleck(ui_app* app, const char* utf8, size_t length, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_register_fleck_resources(ui_app* app, const ui_fleck_resource_entry* entries, size_t count, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_register_component_resource(ui_app* app, const void* bytes, size_t size, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_load_fleck_with_resources(ui_app* app, const char* utf8, size_t length, const ui_fleck_resource_entry* entries, size_t count, ui_model* model, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_queue_fleck_refresh_with_resources(ui_app* app, const char* utf8, size_t length, const ui_fleck_resource_entry* entries, size_t count, ui_model* model);
ui_result ui_app_has_component_type(ui_app* app, const char* namespace_uri, const char* name, uint32_t* registered);
ui_result ui_component_create(ui_app* app, const char* namespace_uri, const char* name, ui_model* model, ui_component** component, ui_fleck_diagnostic* diagnostic);
ui_result ui_node_get_component(ui_node* node, ui_component** component);
ui_result ui_component_get_node(ui_component* component, ui_node** node);
ui_result ui_component_set_state(ui_component* component, void* state, void (*release)(void*));
ui_result ui_component_get_state(ui_component* component, void** state);
ui_result ui_component_find_node(ui_component* component, const char* id, ui_node** node);
ui_result ui_component_find_template_part(ui_component* component, const char* id, ui_node** node);
ui_result ui_component_get_property(ui_component* component, const char* name, ui_binding_value* value);
typedef struct ui_component_property_descriptor {
    uint32_t struct_size, version;
    const char* name;
    ui_value_kind kind;
    uint32_t flags, read_only;
    ui_binding_value default_value;
} ui_component_property_descriptor;
ui_result ui_component_describe_property(ui_component* component, const char* name, ui_component_property_descriptor* descriptor);
ui_result ui_component_set_property_layer(ui_component* component, const char* name, ui_value_precedence precedence, const ui_binding_value* value);
ui_result ui_component_clear_property_layer(ui_component* component, const char* name, ui_value_precedence precedence);
ui_result ui_component_get_base_property(ui_component* component, const char* name, ui_binding_value* value);
ui_result ui_component_get_property_source(ui_component* component, const char* name, ui_property_source* source);
ui_result ui_component_set_property(ui_component* component, const char* name, const ui_binding_value* value);
ui_result ui_component_bind_property(ui_component* component, const char* name, ui_model* model, const char* path, ui_binding_mode mode);
ui_result ui_component_set_data_context(ui_component* component, ui_model* model);
ui_result ui_component_clear_data_context(ui_component* component);
ui_result ui_component_get_data_context(ui_component* component, ui_model** model);
ui_result ui_component_apply_template(ui_component* component, const char* namespace_uri, const char* name, ui_fleck_diagnostic* diagnostic);
ui_result ui_component_subscribe(ui_component* component, const char* event_name, ui_component_event_handler handler, void* context, uint64_t* subscription);
ui_result ui_component_unsubscribe(ui_component* component, uint64_t subscription);
/* Owned subscription handle. Destruction is safe after its component/page/App is gone.
   While the component is alive, destroy on the owning UI thread. */
ui_result ui_component_subscribe_scoped(ui_component* component, const char* event_name, ui_component_event_handler handler, void* context, ui_component_subscription** subscription);
ui_result ui_component_subscription_destroy(ui_component_subscription* subscription);
ui_result ui_component_raise(ui_component* component, const char* event_name, const ui_binding_value* value);

/* A model may expose a UI-thread text collection. Enabling adds the read-only
   integer binding paths Count and Revision. Each insertion receives a stable ID.
   item text is borrowed until the next edit or model destruction. A splice is
   committed before model notifications; a binding error does not undo the edit. */
typedef enum ui_collection_sort {
    UI_COLLECTION_SOURCE_ORDER, UI_COLLECTION_ASCENDING, UI_COLLECTION_DESCENDING
} ui_collection_sort;
/* Configure a read-only view on a separate empty model, or reconfigure that view.
   Labels use ordinal UTF-8 order and case-sensitive substring filtering. */
ui_result ui_collection_set_view(ui_model* view, ui_model* source, ui_collection_sort sort, const char* filter);
/* Group a view by exact UTF-8 item label, keeping the first occurrence order of groups.
   The returned key is borrowed until the next collection change. */
ui_result ui_collection_set_group_by_label(ui_model* view, uint32_t enabled);
ui_result ui_collection_group_count(ui_model* view, uint32_t* count);
ui_result ui_collection_group(ui_model* view, uint32_t index, const char** key, uint32_t* first, uint32_t* count);
ui_result ui_model_enable_text_collection(ui_model* model);
ui_result ui_collection_size(ui_model* model, uint32_t* count);
ui_result ui_collection_item(ui_model* model, uint32_t index, uint64_t* identity, const char** text);
ui_result ui_collection_splice(ui_model* model, uint32_t index, uint32_t removed,
                               const char* const* inserted, uint32_t count);
/* Optional per-item DataContext. The referenced model belongs to the same App.
   Destroying it clears the context while preserving the item and its label. */
ui_result ui_collection_splice_models(ui_model* model, uint32_t index, uint32_t removed,
    const char* const* labels, ui_model* const* contexts, uint32_t count);
ui_result ui_collection_item_model(ui_model* model, uint32_t index, ui_model** context);
/* Destination is the starting index in the final sequence. */
ui_result ui_collection_move(ui_model* model, uint32_t index, uint32_t count, uint32_t destination);
ui_result ui_model_create(ui_app* app, ui_model** out_model);
ui_result ui_model_destroy(ui_model* model);
ui_result ui_model_add_property(ui_model* model, const ui_model_property* property);
typedef ui_result (*ui_binding_convert)(void* context,const ui_binding_value* inputs,uint32_t count,ui_binding_value* result);
ui_result ui_model_add_converter(ui_model* model,const char* name,ui_value_kind output,ui_binding_convert convert,void* context);
/* Creates a read-only model property and subscribes it to the ordered inputs. */
ui_result ui_model_add_multi_binding(ui_model* model,const char* path,const char* converter,const char* const* inputs,uint32_t count);
/* Convenience adapters for existing UI-thread sources and commands. */
ui_result ui_model_add_text_source(ui_model* model, const char* path, ui_text_source* source);
ui_result ui_model_add_command(ui_model* model, const char* path, ui_command* command);
/* UI-thread notification. Workers must post model changes and notifications
   to the UI thread; other threads return WRONG_THREAD. */
ui_result ui_model_notify_property_changed(ui_model* model, const char* path);
ui_result ui_model_begin_update(ui_model* model);
ui_result ui_model_end_update(ui_model* model);
/* Thread-safe enqueue while app is alive; execution/release occur on the UI thread.
   On success ownership of context passes to the queue. release also runs for work
   cancelled by app destruction. Callers must finish posting before destroying app. */
typedef void (*ui_post_callback)(void* context);
ui_result ui_app_post(ui_app* app, ui_post_callback work, void* context, ui_post_callback release);
/* Token is borrowed during execute. Queued items retain cancellation state.
   Cancelled work is skipped; release still runs on the UI thread. */
ui_result ui_app_post_cancellable(ui_app* app, const ui_cancellation_token* token,
    ui_post_callback work, void* context, ui_post_callback release);
ui_result ui_node_bind_property(ui_node* node, ui_model* model, const ui_binding_options* options);
/* DataContext is inherited through ordinary parents. Setting nullptr is a local
   empty value; clear restores inheritance. Invalid context/binding changes keep
   existing values and subscriptions. A context binding may be attached before
   a model exists and resumes when a nonempty context becomes available. */
ui_result ui_node_set_data_context(ui_node* node, ui_model* model);
ui_result ui_node_clear_data_context(ui_node* node);
ui_result ui_node_get_data_context(const ui_node* node, ui_model** model);
ui_result ui_node_get_data_context_source(const ui_node* node, ui_property_source* source);
ui_result ui_node_bind_context_property(ui_node* node, const ui_binding_options* options);
ui_result ui_node_clear_binding(ui_node* node, ui_property property);
ui_result ui_node_update_source(ui_node* node, ui_property property);
ui_result ui_node_set_enabled(ui_node* node, uint32_t enabled);
ui_result ui_node_get_enabled(ui_node* node, uint32_t* enabled);
/* Explicit model is exclusive for new {Binding} expressions; legacy context is optional. */
ui_result ui_app_load_fleck_with_model(ui_app* app, const char* utf8, size_t length,
    ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic);
/* Refresh the active Windows, X11 or headless Fleck page. Handles to removed or
   replaced nodes expire on success; resolve saved IDs with ui_app_find_debug_node.
   Pass the current model explicitly when the new page declares bindings. */
ui_result ui_app_refresh_fleck(ui_app* app, const char* utf8, size_t length,
                              ui_model* model, ui_fleck_diagnostic* diagnostic);
typedef struct ui_fleck_refresh_status {
    uint32_t struct_size, version;
    uint64_t sequence;
    ui_result result;
    uint32_t pending;
    int64_t elapsed_microseconds;
    ui_fleck_diagnostic diagnostic;
} ui_fleck_refresh_status;
/* Copy the latest XML request for the active page; apply at a later Poll safe
   point. Busy input/callbacks/commands and uncommitted drafts defer it. A new
   request replaces the previous one. NULL/zero cancels. Switching pages or
   destroying the explicit model rejects the request without touching the UI. */
ui_result ui_app_queue_fleck_refresh(ui_app* app, const char* utf8, size_t length, ui_model* model);
ui_result ui_app_get_fleck_refresh_status(ui_app* app, ui_fleck_refresh_status* status);
typedef enum ui_fleck_refresh_phase { UI_FLECK_REFRESH_CAPTURE, UI_FLECK_REFRESH_COMPLETE } ui_fleck_refresh_phase;
/* Application-owned state hook. CAPTURE runs after parsing, before staging; a
   non-OK result aborts the attempt. Once capture succeeds, COMPLETE runs once,
   including rollback/busy attempts, with the actual refresh result. Resolve new
   nodes by Fleck/debug ID on success. Callbacks run on the UI thread and may
   read/set content, but cannot Poll, load/switch/destroy pages, replace the hook,
   or recursively refresh. COMPLETE errors use the callback error channel and
   never undo a committed page. Context/state ownership remains with the caller. */
typedef ui_result (*ui_fleck_refresh_state_callback)(ui_app* app, ui_fleck_refresh_phase phase, ui_result result, void* context);
ui_result ui_app_set_fleck_refresh_state_callback(ui_app* app, ui_fleck_refresh_state_callback callback, void* context);
ui_result ui_app_load_fleck_resource_with_model(ui_app* app, const void* bytes, size_t size,
    ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic);
ui_result ui_app_build_fleck_with_model(ui_app* app, const char* title, uint32_t width, uint32_t height,
    ui_fleck_build_fn build, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic);
ui_result ui_fleck_add_binding(ui_fleck_builder* builder, uint32_t node_index, const ui_binding_options* options);
ui_result ui_fleck_set_enabled(ui_fleck_builder* builder, uint32_t node_index, uint32_t enabled);
/* Development metadata v1. JSON buffers include the trailing NUL; NULL queries size.
   Model metadata never calls getters. Snapshots belong to the UI thread. */
ui_result ui_fleck_get_schema(char* utf8, size_t* size);
/* Validate page/theme syntax and static structure without executing bindings. */
ui_result ui_fleck_validate(const char* utf8, size_t size, ui_fleck_diagnostic* diagnostic);
ui_result ui_model_describe(ui_model* model, const char* name, char* utf8, size_t* size);
/* Register a declarative property table atomically; uses the same metadata as Describe. */
ui_result ui_model_add_properties(ui_model* model, const ui_model_property* properties, size_t count);
ui_result ui_model_set_property_source(ui_model* model, const char* path, const char* uri, uint32_t line, uint32_t column);
typedef enum ui_command_execution_modes {
    UI_COMMAND_SUPPORTS_SYNC = 1, UI_COMMAND_SUPPORTS_ASYNC = 2
} ui_command_execution_modes;
/* Static metadata only; never evaluates a getter. Zero removes the declaration.
   ui_model_add_command infers these flags from the registered command. */
ui_result ui_model_set_command_modes(ui_model* model, const char* path, uint32_t modes);
ui_result ui_app_get_debug_snapshot(ui_app* app, char* utf8, size_t* size);
/* On Windows, intercept the next client-area click to select a rendered node. */
ui_result ui_app_set_debug_picking(ui_app* app, uint32_t enabled);
/* Transient inspector overlay; 0 clears. A lease (1..5000 ms) prevents stale highlights. */
ui_result ui_app_set_debug_highlight(ui_app* app, uint64_t object_id, uint32_t lease_ms);
/* Windows local editor service, progressed by Poll/Run on the owner thread.
   NULL disables it. Reusing the same name is a no-op; a failed replacement keeps
   the existing service. Name conflicts return UI_ERROR_PLATFORM synchronously. */
ui_result ui_app_enable_debug_server(ui_app* app, const char* pipe_name);
ui_result ui_app_get_binding_diagnostics(ui_app* app, char* utf8, size_t* size);
/* Current binding validation for a single node/property. Strings are borrowed until
   the next UI operation. A valid binding returns UI_OK in status and empty strings. */
typedef struct ui_validation_result { ui_result status; const char* code; const char* message; } ui_validation_result;
ui_result ui_node_get_validation(ui_node* node, ui_property property, ui_validation_result* result);
/* Delivered on the UI thread at Poll return, or by explicit dispatch. The JSON
   snapshot is borrowed for the callback. Callbacks cannot reenter Poll/destroy App. */
typedef void (*ui_diagnostic_callback)(const char* utf8, void* context);
ui_result ui_app_set_diagnostic_callback(ui_app* app, ui_diagnostic_callback callback, void* context);
/* Independent structured-log adapter. Same versioned JSON envelope, containing
   only records changed since this sink's last dispatch (coalesced between Polls).
   First dispatch includes retained history. NULL detaches; no file I/O is owned
   by the library. The borrowed JSON and UI-thread/reentrancy rules above apply. */
ui_result ui_app_set_diagnostic_log_callback(ui_app* app, ui_diagnostic_callback callback, void* context);
ui_result ui_app_dispatch_diagnostics(ui_app* app);
ui_result ui_node_get_debug_id(ui_node* node, uint64_t* id);
ui_result ui_app_find_debug_node(ui_app* app, uint64_t id, ui_node** out_node);
/* Attach URI/ranges to the latest loaded page. Source maps are optional, versioned
   fleckc sidecars and are verified against the page's content fingerprint. */
ui_result ui_app_set_fleck_source_uri(ui_app* app, const char* uri);
ui_result ui_app_attach_fleck_source_map(ui_app* app, const char* utf8, size_t size, ui_fleck_diagnostic* diagnostic);
/* OneWay tracks a source on TextBlock. OneTime copies the current value. TwoWay updates editors and their source on each edit.
   NULL detaches a TwoWay editor and keeps its text. Setting literal text also detaches it.
   A source connected to a single-line editor rejects line breaks. */
ui_result ui_node_bind_text(ui_node* node, ui_text_source* source, ui_binding_mode mode);
ui_result ui_command_create(ui_app* app, ui_node_action action, void* context, ui_command** out_command);
/* Version 2: execute runs on a worker with the original business context.
   Post UI/model changes to the UI thread. Single-flight is per command.
   cancel and completed run on the UI thread. Destroy requests cancellation,
   cancels queued work for this command, then joins execute. Business work
   must cooperate and keep its context alive. Async commands reject Sync mode. */
typedef struct ui_async_command_options {
    uint32_t struct_size, version;
    ui_result (*execute)(void* context, const ui_cancellation_token* token);
    void (*cancel)(void* context);
    void (*completed)(void* context, ui_result result);
    void* context;
} ui_async_command_options;
ui_result ui_command_create_async(ui_app* app, const ui_async_command_options* options, ui_command** out_command);
/* Completion-driven execution on the UI thread, without a native worker.
   begin receives one owned completion reference: release it exactly once, even
   after App/command destruction. complete is UI-thread-only and queues finishing
   for Poll; duplicate completion returns INVALID_STATE. A detached completion
   returns CANCELLED without touching its former App. cancel only requests
   cancellation; completed runs from Poll, never from complete. No callbacks
   run after destruction. Existing worker-based v2 options keep their contract. */
typedef struct ui_completion ui_completion;
typedef struct ui_external_command_options {
    uint32_t struct_size, version; /* version 1 */
    void (*begin)(void* context, ui_completion* completion);
    void (*cancel)(void* context);
    void (*completed)(void* context, ui_result result);
    void* context;
} ui_external_command_options;
ui_result ui_command_create_external(ui_app* app, const ui_external_command_options* options, ui_command** out_command);
ui_result ui_completion_complete(ui_completion* completion, ui_result result);
void ui_completion_release(ui_completion* completion);
/* Safe from the worker; token expires when execute returns. */
uint32_t ui_cancellation_requested(const ui_cancellation_token* token);
ui_result ui_command_cancel(ui_command* command);
ui_result ui_app_cancel_commands(ui_app* app);
ui_result ui_command_is_running(ui_command* command, uint32_t* running);
ui_result ui_model_is_running(ui_model* model, uint32_t* running);
ui_result ui_node_set_command_mode(ui_node* node, ui_command_mode mode);
ui_result ui_fleck_set_command_mode(ui_fleck_builder* builder, uint32_t index, ui_command_mode mode);
ui_result ui_command_destroy(ui_command* command);
ui_result ui_node_bind_command(ui_node* node, ui_command* command);
ui_result ui_app_set_style(ui_app* app, uint32_t dark, uint32_t compact);
/* Returns a static UTF-8 name, or NULL for an invalid ID. */
const char* ui_builtin_theme_name(ui_builtin_theme theme);
/* Apply a built-in theme. dark and compact each accept 0 or 1. */
ui_result ui_app_set_builtin_theme(ui_app* app, ui_builtin_theme theme, uint32_t dark, uint32_t compact);
/* Initialize a customizable palette from Sunfleck Lite. */
ui_result ui_theme_init(ui_theme* theme, uint32_t dark, uint32_t compact);
/* Apply newline-separated name=value overrides to an initialized theme atomically. */
ui_result ui_theme_apply_overrides(ui_theme* theme, const char* resource);
ui_result ui_app_set_theme(ui_app* app, const ui_theme* theme);
/* Override one color for this node. UI_THEME_COLOR_COUNT is not a color. */
ui_result ui_node_set_theme_color(ui_node* node, ui_theme_color token, uint32_t rgb);

/* Dock handles are borrowed from their app and remain valid until app destruction.
   All operations, callbacks, and drawing run on the creating UI thread. */
typedef struct ui_dock ui_dock;
typedef struct ui_dock_item ui_dock_item;
typedef struct ui_draw_context ui_draw_context;
typedef enum ui_dock_kind { UI_DOCK_DOCUMENT_KIND, UI_DOCK_TOOL_KIND } ui_dock_kind;
typedef enum ui_dock_position {
    UI_DOCK_CENTER, UI_DOCK_POSITION_LEFT, UI_DOCK_POSITION_TOP,
    UI_DOCK_POSITION_RIGHT, UI_DOCK_POSITION_BOTTOM
} ui_dock_position;
typedef enum ui_dock_state { UI_DOCK_DOCKED, UI_DOCK_FLOATING, UI_DOCK_AUTO_HIDDEN, UI_DOCK_HIDDEN, UI_DOCK_CLOSED } ui_dock_state;
enum { UI_DOCK_CAN_CLOSE = 1, UI_DOCK_CAN_FLOAT = 2, UI_DOCK_CAN_AUTO_HIDE = 4,
       UI_DOCK_DEFAULT_CAPABILITIES = 7 };
typedef struct ui_rect { int32_t x, y, width, height; } ui_rect;
typedef struct ui_dock_item_options {
    uint32_t struct_size, version;
    const char* id;
    const char* title;
    ui_node* content;
    ui_dock_kind kind;
    uint32_t capabilities, min_width, min_height;
} ui_dock_item_options;
typedef enum ui_dock_event_kind { UI_DOCK_CLOSING, UI_DOCK_CHANGED, UI_DOCK_ACTIVATED } ui_dock_event_kind;
/* Return nonzero to cancel UI_DOCK_CLOSING; the return value is ignored for notifications.
   Layout mutation during callbacks is rejected; content properties may be updated. */
typedef uint32_t (*ui_dock_event)(ui_dock* dock, ui_dock_item* item, ui_dock_event_kind event, void* context);
ui_result ui_dock_create(ui_node* host, ui_dock** out_dock);
ui_result ui_dock_register(ui_dock* dock, const ui_dock_item_options* options, ui_dock_item** out_item);
ui_result ui_dock_find(ui_dock* dock, const char* id, ui_dock_item** out_item);
ui_result ui_dock_item_get_debug_id(ui_dock_item* item, uint64_t* id);
ui_result ui_dock_set_event(ui_dock* dock, ui_dock_event callback, void* context);
ui_result ui_dock_move(ui_dock_item* item, ui_dock_item* target, ui_dock_position position, uint32_t tab_index);
ui_result ui_dock_move_group(ui_dock_item* item, ui_dock_item* target, ui_dock_position position);
ui_result ui_dock_activate(ui_dock_item* item);
ui_result ui_dock_hide(ui_dock_item* item);
ui_result ui_dock_show(ui_dock_item* item);
ui_result ui_dock_close(ui_dock_item* item);
ui_result ui_dock_float(ui_dock_item* item, const ui_rect* bounds, uint32_t whole_group);
ui_result ui_dock_auto_hide(ui_dock_item* item, ui_dock_edge edge);
ui_result ui_dock_pin(ui_dock_item* item);
ui_result ui_dock_get_state(ui_dock_item* item, ui_dock_state* state);
ui_result ui_dock_get_id(ui_dock_item* item, char* utf8, uint32_t* size);
ui_result ui_dock_set_title(ui_dock_item* item, const char* title);
/* Optional UTF-8 icon glyph/name for the default painter and skin data context. */
ui_result ui_dock_set_icon(ui_dock_item* item, const char* icon);
/* Reset captures the first opened layout, or the most recent explicit capture. */
ui_result ui_dock_capture_default(ui_dock* dock);
ui_result ui_dock_reset(ui_dock* dock);
/* Size includes NUL. NULL queries the required buffer size. Restore is atomic. */
ui_result ui_dock_save(ui_dock* dock, char* utf8, uint32_t* size);
ui_result ui_dock_restore(ui_dock* dock, const char* utf8, size_t size, ui_fleck_diagnostic* diagnostic);
/* Headless floating surfaces and input use the same layout/input paths as desktop windows.
   Window 0 is the main DockHost; floating IDs are returned by get_windows. */
ui_result ui_dock_get_windows(ui_dock* dock, uint32_t* ids, uint32_t* count);
typedef enum ui_pointer_action { UI_POINTER_DOWN, UI_POINTER_MOVE, UI_POINTER_UP, UI_POINTER_CANCEL } ui_pointer_action;
/* Primary pointer events use window client pixels. A nonzero return handles the event. */
typedef int (*ui_pointer_handler)(ui_node* sender, ui_node* source, ui_pointer_action action, ui_event_phase phase, int32_t x, int32_t y, void* context);
/* Native Windows copy drag. Strings are UTF-8. Paths and data are borrowed
   during the callback. Object tokens are meaningful only in the originating app. */
typedef enum ui_drop_kind { UI_DROP_TEXT=1, UI_DROP_FILES=2, UI_DROP_OBJECT=4 } ui_drop_kind;
typedef struct ui_drop_data {
    ui_drop_kind kind;
    const char* text;
    const char* const* paths;
    uint32_t count;
    ui_app* owner;
    uint64_t object;
} ui_drop_data;
typedef int (*ui_drop_handler)(ui_node* target, const ui_drop_data* data, void* context);
ui_result ui_node_set_drop_handler(ui_node* node, uint32_t kinds, ui_drop_handler handler, void* context);
ui_result ui_node_start_drag(ui_node* node, const ui_drop_data* data);
ui_result ui_node_set_pointer_handler(ui_node* node, ui_pointer_action action, ui_event_phase phase, ui_pointer_handler handler, void* context);
ui_result ui_dock_pointer(ui_dock* dock, uint32_t window_id, ui_pointer_action action, int32_t x, int32_t y);
/* Hosted surfaces: main Headless view and in-page Dock views.
   The host owns its event loop and calls Poll(0), then reads pixels or commands.
   Resize dimensions and pointer coordinates are device pixels; dpi uses 96 as
   1x. Input is rejected before opening/after closing or during callbacks.
   A completed down/up gesture activates at most once; cancel never activates. */
ui_result ui_host_resize(ui_app* app, uint32_t width, uint32_t height, uint32_t dpi);
ui_result ui_host_pointer(ui_app* app, ui_pointer_action action, int32_t x, int32_t y);
/* While debug picking is active, inspect the rendered view without invoking controls.
 * MOVE previews a device-pixel location, UP selects it and CANCEL ends picking. */
ui_result ui_host_debug_pointer(ui_app* app, uint32_t view, ui_pointer_action action, int32_t x, int32_t y);
/* Vertical scroll delta in device pixels; bubbles from inner to outer scroll containers. */
ui_result ui_host_scroll(ui_app* app, int32_t x, int32_t y, int32_t dy);
ui_result ui_host_context_menu(ui_app* app, int32_t x, int32_t y);
/* Enable read-only pointer selection for a TextBlock in a hosted view. */
ui_result ui_host_set_text_selectable(ui_node* node, uint32_t enabled);
/* Copy the focused TextBlock's selection as UTF-8, including a NUL terminator.
   Pass NULL for output to query the required size. */
ui_result ui_host_selected_text(ui_app* app, char* output, size_t* size);
enum { UI_HOST_CAP_TEXT = 1, UI_HOST_CAP_SCROLL = 2, UI_HOST_CAP_DOCK_VIEWS = 4,
       UI_HOST_CAP_SEMANTICS = 8, UI_HOST_CAP_EXTERNAL_COMMANDS = 16, UI_HOST_CAP_DRAW_COMMANDS = 32 };
/* Native independent windows, native dialogs and blocking loops are not hosted capabilities. */
uint32_t ui_host_capabilities(void);
/* Select main view (0) or a live in-page Dock view for subsequent host calls.
   Poll always runs the whole App. IDs are supplied by the hosted snapshot.
   Snapshot is UTF-8 JSON v1; size includes NUL, NULL queries buffer size.
   It exposes visible semantic nodes and view bounds, never password values. */
ui_result ui_host_select_view(ui_app* app, uint32_t view);
const uint8_t* ui_host_pixels(ui_app* app, uint32_t* width, uint32_t* height);
/* App-wide mode: 0 (default) rasterizes pixels, 1 retains ordered rectangles
   for external GPU rendering, including future Dock views. Call after open.
   ui_host_pixels/ui_dock_pixels can still lazily rasterize in mode 1.
   ui_app_pixels is the legacy raster-mode API. Changing mode invalidates views. */
ui_result ui_host_set_render_mode(ui_app* app, uint32_t mode);
/* Command wire format v1: each 20-byte record contains four little-endian int32
   bounds (left, top, right, bottom; exclusive right/bottom), then RGBA8 straight
   color. Ordered source-over rectangles, already clipped by the core; viewport
   clipping is still required. Borrowed until next Poll/mode change/destruction.
   Read after a successful Poll in mode 1; size is bytes, not rectangle count. */
ui_result ui_host_draw_commands(ui_app* app, const uint8_t** data, uint32_t* size);
ui_result ui_host_snapshot(ui_app* app, char* output, size_t* size);
/* Focus (0) or activate (1) a visible semantic node in the selected view. */
ui_result ui_host_semantic_action(ui_app* app, uint64_t node, uint32_t action);
typedef enum ui_host_key { UI_HOST_TAB, UI_HOST_ACTIVATE, UI_HOST_PREVIOUS, UI_HOST_NEXT, UI_HOST_ESCAPE, UI_HOST_LEFT, UI_HOST_RIGHT, UI_HOST_UP, UI_HOST_DOWN } ui_host_key;
ui_result ui_host_key_event(ui_app* app, ui_host_key key, uint32_t shift);
ui_result ui_host_focus(ui_app* app, uint32_t focused);
/* Hosted DOM editing. Read after Poll/input; active=0 means no editable focus.
   text is borrowed until the next read/update or App destruction (UTF-8, no NUL).
   Selection offsets are UTF-16 units, never inside a surrogate pair. The host
   owns IME events; preview is transient and only updates publish bound text. Stale session/revision
   updates return INVALID_STATE; reread to reconcile external/model/page changes.
   Version 1 supports at most 1 MiB UTF-8 per update. Geometry is device pixels. */
typedef struct ui_host_text_state {
    uint32_t struct_size, version;
    uint32_t active, session, revision, multiline, password;
    int32_t left, top, right, bottom;
    int32_t clip_left, clip_top, clip_right, clip_bottom;
    uint32_t selection_start, selection_end, selection_backward;
    uint32_t foreground, background, font_size; /* colors 0xRRGGBB */
    int32_t text_left, text_top, text_right, text_bottom;
    uint32_t line_height; /* renderer metrics, in device pixels */
    int32_t caret_x, caret_y, scroll_x, scroll_y;
    const char* text;
    uint32_t text_size;
} ui_host_text_state;
ui_result ui_host_text_read(ui_app* app, ui_host_text_state* state);
ui_result ui_host_text_update(ui_app* app, uint32_t session, uint32_t revision,
    const char* utf8, uint32_t size, uint32_t selection_start, uint32_t selection_end, uint32_t backward);
/* Display an IME draft without changing the bound value or committed revision.
   All ranges are UTF-16 offsets. Update commits/clears the draft; external model
   changes, focus retirement and destruction discard it. */
typedef struct ui_host_composition_range {
    uint32_t struct_size, version;
    uint32_t selection_start, selection_end, backward, composition_start, composition_end;
} ui_host_composition_range;
ui_result ui_host_text_preview(ui_app* app, uint32_t session, uint32_t revision,
    const char* utf8, uint32_t size, const ui_host_composition_range* range);
/* Hit testing and scrolling use the renderer's displayed text, including drafts. */
ui_result ui_host_text_hit_test(ui_app* app, uint32_t session, uint32_t revision,
    int32_t x, int32_t y, uint32_t* offset);
ui_result ui_host_text_scroll(ui_app* app, uint32_t session, uint32_t revision, int32_t dx, int32_t dy);
/* Register a copied font for this thread's FreeType text backend before creating
   hosted Apps. Other text backends return UNSUPPORTED. TTF/OTF/TTC (face 0), up to 64 MiB. */
ui_result ui_host_set_font(const void* bytes, size_t size);
const uint8_t* ui_dock_pixels(ui_dock* dock, uint32_t window_id, uint32_t* width, uint32_t* height);

typedef enum ui_dock_part {
    UI_DOCK_PART_TITLE, UI_DOCK_PART_TAB, UI_DOCK_PART_CLOSE, UI_DOCK_PART_PIN,
    UI_DOCK_PART_FLOAT, UI_DOCK_PART_SPLITTER, UI_DOCK_PART_INDICATOR,
    UI_DOCK_PART_PREVIEW, UI_DOCK_PART_AUTO_HIDE,
    UI_DOCK_PART_MENU, UI_DOCK_PART_MENU_ITEM,
    UI_DOCK_PART_WINDOW_TITLE, UI_DOCK_PART_WINDOW_MINIMIZE,
    UI_DOCK_PART_WINDOW_MAXIMIZE, UI_DOCK_PART_WINDOW_CLOSE, UI_DOCK_PART_COUNT
} ui_dock_part;
enum { UI_PART_ACTIVE = 1, UI_PART_HOVER = 2, UI_PART_PRESSED = 4, UI_PART_DISABLED = 8 };
typedef struct ui_dock_paint {
    uint32_t struct_size, version;
    ui_dock_part part;
    uint32_t state, capabilities, dpi;
    ui_rect bounds;
    const char* title;
    const char* item_id;
    uint32_t background, foreground, border; /* RGB */
    const char* icon;
} ui_dock_paint;
/* Return 1 if handled, 0 to use the registered template or built-in painter.
   Drawing contexts are valid only during the synchronous callback. */
typedef uint32_t (*ui_dock_painter)(ui_draw_context* draw, const ui_dock_paint* part, void* context);
ui_result ui_app_register_dock_skin(ui_app* app, const char* name, ui_dock_painter painter, void* context);
/* Theme root: <Theme Base="Fluent2" Dark="false" Compact="false" Skin="name">.
   Resource keys and template syntax are documented in docs/themes.md. */
ui_result ui_app_load_theme(ui_app* app, const char* utf8, size_t size, ui_fleck_diagnostic* diagnostic);
/* Version 1. resources contains UTF-8 Resource/DockTemplate elements (no Theme root).
   All strings are copied synchronously. Unspecified resources inherit base shapes and colors. */
typedef struct ui_theme_descriptor {
    uint32_t struct_size, version;
    ui_builtin_theme base;
    uint32_t dark, compact;
    const char* skin;
    const char* resources;
    size_t resources_size;
} ui_theme_descriptor;
ui_result ui_app_apply_theme(ui_app* app, const ui_theme_descriptor* theme, ui_fleck_diagnostic* diagnostic);
ui_result ui_draw_fill(ui_draw_context* draw, ui_rect bounds, uint32_t rgb, uint32_t opacity, uint32_t radius);
ui_result ui_draw_stroke(ui_draw_context* draw, ui_rect bounds, uint32_t rgb, uint32_t width, uint32_t radius);
ui_result ui_draw_text(ui_draw_context* draw, ui_rect bounds, const char* text, uint32_t rgb);
ui_result ui_draw_image(ui_draw_context* draw, ui_rect bounds, const uint8_t* rgba, uint32_t width, uint32_t height);
ui_result ui_draw_push_clip(ui_draw_context* draw, ui_rect bounds);
ui_result ui_draw_pop_clip(ui_draw_context* draw);
/* Enable the preview gallery. Pass 0 or 1 for each style setting. */
ui_result ui_app_set_gallery(ui_app* app, uint32_t dark, uint32_t compact);
/* Copy a tightly packed RGB8 bitmap (1–128 pixels per side) into the gallery. */
ui_result ui_app_set_gallery_image(ui_app* app, const uint8_t* rgb, uint32_t width, uint32_t height);
ui_result ui_app_poll(ui_app* app, uint32_t timeout_ms);
ui_result ui_app_run(ui_app* app);
ui_result ui_app_request_close(ui_app* app);
void ui_app_destroy(ui_app* app);

/* Headless test surface. The returned RGBA buffer is owned by the app. */
ui_result ui_app_activate_button(ui_app* app);
const uint8_t* ui_app_pixels(const ui_app* app, uint32_t* width, uint32_t* height);

#ifdef __cplusplus
}
#endif
#endif
