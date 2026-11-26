#pragma once

#include "ui_tree.h"
#include <functional>
#include <map>
#include <optional>
#include <set>

namespace docking {
struct Region {
    unsigned id = 0;
    unsigned axis = 0; // 0: tabs, 1: horizontal, 2: vertical
    unsigned first = 0, second = 0, ratio = 5000;
    ui_dock_kind kind = UI_DOCK_DOCUMENT_KIND;
    std::vector<std::string> tabs;
    std::string active;
};
struct Floating {
    unsigned id, root;
    ui_rect bounds;
};
struct Placement {
    ui_dock_state state = UI_DOCK_DOCKED;
    unsigned group = 1, index = 0;
    ui_dock_edge edge = UI_DOCK_LEFT;
    struct ReturnSplit {
        unsigned sibling = 0;
        std::string anchor;
        ui_dock_position position = UI_DOCK_CENTER;
        unsigned ratio = 5000;
    };
    std::vector<ReturnSplit> path;
    unsigned window = 0;
    ui_rect bounds{120, 120, 480, 320};
    ui_dock_state resume = UI_DOCK_DOCKED;
};
struct Layout {
    unsigned root = 1, next = 2, next_window = 1;
    std::map<unsigned, Region> regions{{1, Region{1}}};
    std::vector<Floating> floating;
    std::map<std::string, Placement> placements;
    std::string expanded;
};
struct Hit {
    ui_dock_part part;
    layout::Rect bounds;
    unsigned group;
    std::string item;
    unsigned action = 0;
};
struct DropTarget {
    layout::Rect bounds;
    ui_dock_position position;
};
struct View {
    layout::Rect area{}, popup{};
    unsigned dpi = 96;
    std::vector<Hit> hits;
    std::map<unsigned, layout::Rect> groups, splits;
    std::vector<DropTarget> targets;
    layout::Rect menu{};
};
struct Drag {
    bool prepared = false, active = false, whole_group = false;
    bool allowed = true;
    bool window_move = false;
    unsigned window = 0, target_window = 0, group = 0;
    int start_x = 0, start_y = 0, x = 0, y = 0;
    Hit source{};
    std::string target;
    ui_dock_position position = UI_DOCK_CENTER;
    unsigned target_index = UINT32_MAX;
    layout::Rect preview{};
    layout::Rect target_area{};
    unsigned original_ratio = 5000;
    std::string original_active;
};
struct TemplateElement {
    enum Kind { Fill, Border, Text, Button, Stack } kind = Fill;
    std::string background, foreground, text, role;
    unsigned radius = 0, thickness = 1, width = 0, height = 0;
    int padding = 0;
    bool horizontal = true;
    std::vector<TemplateElement> children;
};
struct Theme {
    ui_theme palette = builtin_palette(UI_BUILTIN_SUNFLECK_LITE, false, false);
    ThemeShape shape = builtin_themes[UI_BUILTIN_SUNFLECK_LITE].shape;
    std::map<std::string, uint32_t> colors;
    std::map<std::string, unsigned> metrics;
    std::map<std::pair<ui_dock_part, unsigned>, TemplateElement> templates;
    std::string skin;
};
struct Skin { ui_dock_painter painter; void* context; };
struct WindowFrame {
    layout::Rect title{}, minimize{}, maximize{}, close{};
    int height = 0, resize = 0;
};
WindowFrame window_frame(const Theme& theme, int width, unsigned dpi);
void paint_window_frame(ui_dock& dock, int width, int height, unsigned dpi, const char* title,
                        bool active, bool maximized, ui_dock_part hovered, ui_dock_part pressed,
                        std::vector<FillRectCommand>& commands);

ui_result check(ui_dock* dock, bool mutation = false);
ui_result check(ui_dock_item* item, bool mutation = false);
ui_dock_item* find(ui_dock& dock, const std::string& id);
unsigned group_of(const Layout& layout, const std::string& id);
unsigned window_of(const Layout& layout, unsigned group);
void remove(Layout& layout, const std::string& id);
void normalize(Layout& layout);
void place(Layout& layout, const std::string& id, ui_dock_kind kind, const std::string& target,
           ui_dock_position position, unsigned index);
void return_to(Layout& layout, const ui_dock_item& item);
std::string save(const Layout& layout);
Layout restore(std::string_view input, const ui_dock& dock);
void changed(ui_dock& dock, ui_dock_item* item = nullptr, ui_dock_event_kind event = UI_DOCK_CHANGED);
void paint(ui_dock& dock, unsigned window, layout::Rect area, layout::Rect clip,
           GalleryStyle style, ThemeShape shape, unsigned dpi,
           std::vector<FillRectCommand>& commands,
           const std::function<void(ui_node&, layout::Rect, layout::Rect)>& content);
bool pointer(ui_dock& dock, unsigned window, ui_pointer_action action, int x, int y);
bool consumes(const ui_dock& dock, unsigned window, int x, int y);
bool close_window(ui_dock& dock, unsigned window);
void cancel(ui_dock& dock);
bool begin_window_drag(ui_dock& dock, unsigned window, int x, int y);
void paint_drag_overlay(ui_dock& dock, unsigned window, std::vector<FillRectCommand>& commands);
std::unique_ptr<ui_dock> from_fleck(ui_app* app, ui_node& root);
Theme parse_theme(std::string_view text, ui_fleck_diagnostic& diagnostic);
}

struct ui_dock_item {
    ui_dock* dock;
    std::string id, title;
    ui_node* content;
    ui_dock_kind kind;
    unsigned capabilities;
    double min_width, min_height;
    std::string icon;
};

struct ui_dock {
    ui_app* owner = nullptr;
    ui_node* host = nullptr;
    std::vector<std::unique_ptr<ui_dock_item>> items;
    docking::Layout layout;
    std::optional<docking::Layout> initial;
    std::map<std::string, docking::Placement> defaults;
    std::map<unsigned, docking::View> views;
    docking::Drag drag;
    std::optional<docking::Hit> hovered, pressed;
    ui_dock_event callback = nullptr;
    void* callback_context = nullptr;
    bool notifying = false, painting = false;
    bool external_overlay = false;
    struct Menu { unsigned window, group; std::string item; layout::Rect anchor; };
    std::optional<Menu> menu;
};

/* Narrow services provided by app.cpp; the model never owns application content. */
ui_result dock_app_check(ui_app* app);
ui_result dock_app_check_workspace(ui_dock* dock);
ui_result dock_app_pointer(ui_dock* dock, unsigned window, ui_pointer_action action, int x, int y);
const uint8_t* dock_workspace_pixels(ui_dock* dock, unsigned window, uint32_t* width, uint32_t* height);
uint32_t dock_app_callback(ui_dock& dock, ui_dock_item* item, ui_dock_event_kind event);
ui_dock* dock_app_workspace(ui_app* app);
void dock_app_set_workspace(ui_app* app, std::unique_ptr<ui_dock> dock);
void dock_app_dirty(ui_app* app);
docking::Theme& dock_app_theme(ui_app* app);
std::map<std::string, docking::Skin>& dock_app_skins(ui_app* app);
void dock_app_apply_theme(ui_app* app, docking::Theme theme);
ui_node* dock_app_root(ui_dock* dock);
const uint8_t* dock_app_pixels(ui_app* app, unsigned window, uint32_t* width, uint32_t* height);
bool dock_app_locate(ui_app* app, unsigned source_window, int x, int y, unsigned& target_window, int& tx, int& ty);
ui_rect dock_app_screen(ui_app* app, unsigned window, int x, int y);
