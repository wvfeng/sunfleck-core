#include "components.h"
#include <sunfleck/ui.h>
#include "raster.h"
#include "gallery.h"
#include "dwrite_text.h"
#include "ui_tree.h"
#include "utf8_text.h"
#include "input/hosted_text.h"
#include "input/hosted_window.h"
#include "input/text_navigation.h"
#include "fleck.h"
#include "dock.h"
#include "debug_pipe.h"
#include "timeline.h"

#include <algorithm>
#include <bit>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <mutex>
#include <sstream>
#include <set>
#include "dock_json.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <imm.h>
#include <dwmapi.h>
#include "windows_accessibility.h"
#include "windows_drop.h"
#elif defined(SUNFLECK_X11)
#include "x11_drop.h"
#ifdef SUNFLECK_ATSPI
#include "x11_accessibility.h"
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include <poll.h>
#include <cstdlib>
#include <cerrno>
#elif defined(SUNFLECK_MACOS)
#include "macos_window.h"
#endif

#include "gpu_surface.h"
#ifdef SUNFLECK_SKIA_D3D12
#include "skia_d3d12_surface.h"
#endif
#include <cstdio>
#include <cstdlib>
#ifdef SUNFLECK_D3D11
#include "d3d11_surface.h"
#endif
#ifdef SUNFLECK_GLES3
#include "gles_surface.h"
#endif

static void update_host_state(ui_node& node, const char* group, const char* state,
                              std::string_view& current, const char* fallback = nullptr);

/* One context per native/headless window. App owns content; contexts only borrow it.
   The inherited context is the currently dispatched window. WindowScope exchanges
   contexts so existing input/editor/rendering paths stay identical for all windows. */
struct ui_window_context {
    ui_app* application = nullptr;
    ui_window* session = nullptr;
    unsigned debug_window_id = 0;
    unsigned dock_window = 0;
    bool opened = false, closing = false, dirty = true, render_failed = false;
    bool active = false, focused = false, keyboard_focus = false;
    bool host_pointer_down = false, pointer_down_handled = false;
    int pointer_x = 0, pointer_y = 0;
    bool hosted_input = false;
    hosted_window::State hosted_window;
    hosted_text::Session host_text;
    std::chrono::steady_clock::time_point hosted_caret_tick = std::chrono::steady_clock::now();
    bool ime_composing = false;
    ui_node* hovered_control = nullptr;
    ui_node* pressed_control = nullptr;
    std::wstring window_title = L"Sunfleck";
    std::string window_title_utf8 = "Sunfleck";
    bool configured_window = false;
    ui_node* root = nullptr;
    std::unique_ptr<ui_node> dock_root;
    ui_node* focus = nullptr;
    bool caret_visible = true;
    ui_node* dragged_control = nullptr;
    int scrollbar_grab_offset = 0;
    std::vector<ui_node*> interactive, scrollable;
    std::vector<UiOverlay> overlays;
    std::vector<ui_node*> debug_paint_order;
    uint64_t debug_hover = 0;
    layout::Rect debug_hover_bounds{};
    std::unique_ptr<GpuSurface> gpu;
    int width = 320, height = 200;
    unsigned dpi = 96;
    bool maximized = false, custom_frame = false;
    bool native_dock_move = false;
    ui_dock_part chrome_hover = UI_DOCK_PART_COUNT, chrome_pressed = UI_DOCK_PART_COUNT;
    std::vector<std::uint8_t> pixels;
    struct HostedRect { int32_t left,top,right,bottom; std::array<uint8_t,4> color; };
    std::vector<HostedRect> hosted_wire_commands;
    std::vector<FillRectCommand> hosted_commands;
#ifdef _WIN32
    HWND window = nullptr;
    accessibility::Window accessibility_window;
    bool drop_registered = false;
    POINT drag_origin{};
    bool native_caret = false, animated_spinner = false;
    POINT ime_point{-1, -1};
    int ime_line_height = 0;
    std::vector<std::uint8_t> dib;
    POINT native_grab{};
    RECT native_move_origin{};
#elif defined(SUNFLECK_X11)
    Display* display = nullptr;
    XIM input_method = nullptr;
    XIC input_context = nullptr;
    XFontSet xim_fontset = nullptr;
    bool xim_focused = false, xim_positioned = false;
    XPoint xim_spot{-1, -1};
    Atom clipboard = 0, utf8 = 0, targets = 0, paste_property = 0;
    x11_drop::Target drop_target;
#ifdef SUNFLECK_ATSPI
    x11_accessibility::Window* accessibility_window = nullptr;
#endif
    std::string clipboard_text;
    ui_node* paste_target = nullptr;
    std::chrono::steady_clock::time_point animation_tick{};
    long long last_animation_tick = -1;
    Window window = 0;
    Atom wm_protocols = 0, wm_delete = 0;
    bool mapped = false, pending_native_focus = false;
    bool native_position_known = false, native_move_cancelled = false;
    int native_x = 0, native_y = 0, native_origin_x = 0, native_origin_y = 0;
    bool chrome_manual = false;
    int chrome_resize = -1, chrome_grab_x = 0, chrome_grab_y = 0, chrome_origin_width = 0, chrome_origin_height = 0;
    ui_rect chrome_restore{};
    Time chrome_click_time = 0;
#elif defined(SUNFLECK_MACOS)
    std::unique_ptr<macos::Window> window;
#endif
};

struct ui_window_session {
    bool ended = false;
    std::unique_ptr<ui_window_context> main_state = std::make_unique<ui_window_context>();
    ui_window_context* current = main_state.get();
    std::map<unsigned, std::unique_ptr<ui_window_context>> dock_windows;
    std::unique_ptr<ui_dock> workspace;
#ifdef _WIN32
    HWND dock_overlay = nullptr;
#elif defined(SUNFLECK_X11)
    Display* dock_overlay_display = nullptr;
    Window dock_overlay = 0;
    std::vector<FillRectCommand> dock_overlay_commands;
    int dock_overlay_width = 0, dock_overlay_height = 0;
#endif
    struct FleckPageSession {
        std::vector<ui_node*> named;
        std::vector<ui_node*> nodes;
        std::shared_ptr<devtools::Source> source;
        ui_model* exported_model = nullptr;
        ui_node* root = nullptr;
        std::unique_ptr<ui_dock> workspace;
    } fleck_page;
    std::vector<FleckPageSession> inactive_fleck_pages;
    struct FleckRefreshReport {
        uint64_t sequence = 0;
        ui_result result = UI_OK;
        int64_t elapsed_us = 0;
        ui_fleck_diagnostic diagnostic{};
    } fleck_refresh_report;
    struct QueuedFleckRefresh {
        std::string text; uint64_t page, model;
        struct Resource { std::string uri, text; bool removed = false; };
        std::vector<Resource> resources;
    };
    std::optional<QueuedFleckRefresh> queued_fleck_refresh;
    bool applying_queued_fleck_refresh = false;
    uint64_t fleck_queue_generation = 0;
    ui_fleck_refresh_state_callback fleck_state_callback = nullptr;
    void* fleck_state_context = nullptr;
};

struct ui_window : ui_window_session {
    ui_app* owner = nullptr;
    unsigned id = 0;
};

struct ui_app : ui_window_context, ui_window_session {
    components::Registry component_registry;
    windows_text::FontResources font_resources;
    ui_font_id next_font_id=1;
    unsigned hosted_view = 0;
    bool hosted_command_rendering = false;
    std::string hosted_text_snapshot;
    ui_window primary_window;
    ui_window* active_session = &primary_window;
    std::vector<std::unique_ptr<ui_window>> windows;
    unsigned next_window_id = 1;
    unsigned window_callback_depth = 0;
    bool polling = false;
    bool has_pending_items = false;
    docking::Theme theme_resource;
    std::map<std::string,BindingValue> application_resources, theme_values;
    std::map<std::string, docking::Skin> dock_skins;
    bool drawing = false;
    ui_app()
    {
        primary_window.owner = this;
        application = this;
        session = &primary_window;
        main_state->application = this;
        main_state->session = &primary_window;
    }
    std::thread::id owner = std::this_thread::get_id();
    bool headless = false;
    bool show_renderer = false;
    bool gallery = false;
    GalleryStyle gallery_style = builtin_palette(UI_BUILTIN_SUNFLECK_LITE, false, false);
#ifdef _WIN32
    std::optional<std::array<uint32_t, UI_THEME_COLOR_COUNT>> contrast_colors;

    void refresh_system_colors()
    {
        HIGHCONTRASTW setting{sizeof(setting)};
        if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(setting), &setting, 0)) return;
        if (!(setting.dwFlags & HCF_HIGHCONTRASTON)) {
            contrast_colors.reset();
            return;
        }
        constexpr int roles[] = {COLOR_WINDOW, COLOR_BTNFACE, COLOR_WINDOW, COLOR_WINDOWTEXT,
            COLOR_HIGHLIGHT, COLOR_HIGHLIGHTTEXT, COLOR_BTNFACE, COLOR_BTNFACE,
            COLOR_WINDOWTEXT, COLOR_WINDOWTEXT, COLOR_WINDOW};
        static_assert(std::size(roles) == UI_THEME_COLOR_COUNT);
        auto& colors = contrast_colors.emplace();
        for (size_t i = 0; i < colors.size(); ++i) {
            const auto rgb = GetSysColor(roles[i]);
            colors[i] = (GetRValue(rgb) << 16) | (GetGValue(rgb) << 8) | GetBValue(rgb);
        }
    }
#endif
    GalleryStyle effective_palette() const
    {
        auto palette = gallery_style;
#ifdef _WIN32
        if (contrast_colors) std::copy(contrast_colors->begin(), contrast_colors->end(), palette.colors);
#endif
        return palette;
    }
    ThemeShape theme_shape = builtin_themes[UI_BUILTIN_SUNFLECK_LITE].shape;
    GalleryImage gallery_image;
    std::map<std::string, GalleryImage> image_resources;
    int gallery_scroll = 0;
    std::vector<std::unique_ptr<ui_node>> nodes;
    struct Playback {
        ui_node* node;
        std::weak_ptr<char> lifetime;
        ui_property property;
        animation::Timeline timeline;
        animation::Clock::time_point started;
        double offset_ms = 0;
        bool paused = false, hold = false, ended = false;
        std::string state_group;
        double position(animation::Clock::time_point now) const {
            return offset_ms + (paused ? 0 : animation::Milliseconds(now-started).count());
        }
    };
    std::vector<Playback> animations;
    std::optional<animation::Clock::time_point> manual_frame_time;
    ui_animation_completed animation_completed = nullptr;
    void* animation_context = nullptr;
    bool reduced_motion = false;
    bool track_drop_targets = false;
    struct Completion { ui_node* node; std::weak_ptr<char> lifetime; ui_property property; };
    std::vector<Completion> completions;
    animation::Clock::time_point frame_time() const {
        return manual_frame_time ? *manual_frame_time : animation::Clock::now();
    }
    uint64_t next_debug_id = 1;
    uint64_t diagnostic_sequence = 0, delivered_diagnostic_sequence = 0;
    std::vector<devtools::Diagnostic> diagnostics;
    bool debug_picking = false;
    std::unique_ptr<devtools::DebugPipe> debug_pipe;
    uint64_t debug_selection = 0;
    uint64_t debug_highlight = 0;
    std::chrono::steady_clock::time_point debug_highlight_until{};
    ui_diagnostic_callback diagnostic_callback = nullptr;
    void* diagnostic_context = nullptr;
    ui_diagnostic_callback diagnostic_log_callback = nullptr;
    void* diagnostic_log_context = nullptr;
    uint64_t logged_diagnostic_sequence = 0;
    std::vector<std::unique_ptr<ui_text_source>> sources;
    std::vector<std::unique_ptr<ui_command>> commands;
    std::vector<std::unique_ptr<ui_model>> models;
    ui_result binding_error = UI_OK;
    struct Posted { ui_post_callback work, release; void* context; std::shared_ptr<std::atomic_bool> cancellation; };
    bool stopping = false; // Protected by post_mutex for cross-thread enqueue.
    std::mutex post_mutex;
    std::vector<Posted> posted;
    bool dispatching = false;
    struct ActionDispatch {
        ui_node* node;
        bool deferred = false;
    };
    ActionDispatch* current_action = nullptr;
    ui_renderer renderer = UI_AUTO;
#if defined(SUNFLECK_X11)
    static unsigned long x11_channel(unsigned value, unsigned long mask)
    {
        const auto low = mask & (0ul - mask);
        return low ? ((value * (mask / low) + 127) / 255) * low : 0ul;
    }

    unsigned long x11_background() const
    {
        const auto* visual = DefaultVisual(display, DefaultScreen(display));
        const auto color = theme_rgb(gallery_style.colors[UI_THEME_BACKGROUND]);
        return x11_channel(color[0], visual->red_mask) | x11_channel(color[1], visual->green_mask) |
            x11_channel(color[2], visual->blue_mask);
    }

    void close_x11()
    {
        if (display) {
#ifdef SUNFLECK_ATSPI
            x11_accessibility::close(std::exchange(accessibility_window, nullptr));
#endif
            if (input_context) {
                XDestroyIC(input_context);
                input_context = nullptr;
            }
            xim_positioned = false;
            xim_focused = false;
            xim_spot = {-1, -1};
            if (xim_fontset) {
                XFreeFontSet(display, xim_fontset);
                xim_fontset = nullptr;
            }
            if (input_method) {
                XCloseIM(input_method);
                input_method = nullptr;
            }
            if (window) {
                x11_drop::cancel(drop_target, display, window);
                XDestroyWindow(display, window);
            }
            XCloseDisplay(display);
            display = nullptr;
            window = 0;
        }
    }
#endif

    FillRectCommand button() const
    {
        const auto palette = effective_palette();
        if (gallery) {
            return gallery_button(palette, active, dpi);
        }
        FillRectCommand rect{24, 24, 144, 64,
            theme_rgb(palette.colors[active ? UI_THEME_SELECTED : UI_THEME_PRIMARY])};
        const auto scale = [this](int value) { return (value * dpi + 48) / 96; };
        rect.left = scale(rect.left);
        rect.top = scale(rect.top);
        rect.right = scale(rect.right);
        rect.bottom = scale(rect.bottom);
        return rect;
    }

    void activate()
    {
        active = !active;
        dirty = true;
    }

    ui_command* find_routed_command(ui_node& source,const std::string& name,ui_model*& model)
    {
        for (auto* owner = &source; owner; owner = owner->inheritance_parent()) {
            auto binding = owner->command_bindings.find(name);
            if (binding != owner->command_bindings.end()) return binding->second;
            auto path = owner->command_paths.find(name);
            if (path != owner->command_paths.end()) {
                ui_command* result = nullptr;
                model = binding_data_context(*owner);
                binding_app_error(this,binding_read_command(*owner,path->second,result));
                return result;
            }
        }
        return nullptr;
    }

    void invoke(ui_node& node, unsigned value)
    {
        if (!node.is_enabled() || node.action_busy()) {
            return;
        }
        ui_model* model = nullptr;
        auto* command = node.command;
        if (!node.routed_command.empty()) {
            command = find_routed_command(node,node.routed_command,model);
            if (!command) return;
        }
        if (command && command->run) return;
        if (node.kind == UI_RADIO_BUTTON && value && node.parent) {
            for (auto* sibling : node.parent->children) {
                if (sibling->kind == UI_RADIO_BUTTON) {
                    sibling->value = 0;
                }
            }
        }
        node.value = value;
        if (node.items_source && devtools::indexed(node.kind) && value < node.item_count()) {
            node.selected_identity = node.items_source->collection->items[value].identity;
        }
        binding_user_changed(node, UI_PROPERTY_VALUE);
        if (node.component && fleck::range_template(node.kind))
            binding_app_error(this, ui_model_notify_property_changed(node.component->properties.get(), "Value"));
        dirty = true;
        execute_command(node,command,model);
    }

    void execute_command(ui_node& node, ui_command* command,ui_model* model)
    {
        if (command && command->asynchronous()) {
            const bool previous = std::exchange(dispatching, true);
            binding_app_error(this, async_command_start(node,*command,model));
            dispatching = previous;
            return;
        }
        const auto action = command ? command->action : node.action;
        void* context = command ? command->context : node.context;
        if (action) {
            const bool previous_dispatching = std::exchange(dispatching, true);
            ActionDispatch dispatch{&node};
            auto* previous_action = std::exchange(current_action, &dispatch);
            ++node.running_actions;
            try {
                action(&node, context);
            } catch (...) {
                closing = true;
            }
            const bool was_busy = node.action_busy();
            --node.running_actions;
            dirty |= was_busy != node.action_busy();
            current_action = previous_action;
            dispatching = previous_dispatching;
        }
    }

    bool key_gesture(unsigned key, unsigned modifiers)
    {
        auto* source = focus ? focus : root;
        if (!source || !source->is_enabled() || !source->is_visible()) return false;
        bool matched = false;
        route_event(*source,[&](ui_node* scope, ui_event_phase phase) {
            auto gesture = scope->input_bindings.find(fleck::gesture_code(key, modifiers, phase));
            if (gesture == scope->input_bindings.end()) return false;
            ui_model* model = nullptr;
            auto* command = find_routed_command(*source, gesture->second.command, model);
            if (command) {
                matched = true;
                if (!command->run && !source->action_busy()) execute_command(*source,command,model);
            }
            return true;
        });
        return matched;
    }

    template<class Handler> bool route_event(ui_node& source, Handler handled)
    {
        std::vector<ui_node*> route;
        for (auto* current = &source; current; current = current->inheritance_parent())
            route.push_back(current);
        struct DispatchGuard {
            bool& flag;
            bool previous;
            ~DispatchGuard() { flag = previous; }
        } guard{dispatching,std::exchange(dispatching,true)};
        try {
            for (auto it = route.rbegin(); it != route.rend(); ++it)
                if (handled(*it,UI_EVENT_PREVIEW)) return true;
            if (handled(&source,UI_EVENT_DIRECT)) return true;
            for (auto* sender : route)
                if (handled(sender,UI_EVENT_BUBBLE)) return true;
        } catch (...) { closing = true; return true; }
        return false;
    }

    bool activation_handled(ui_node& source) {
        return route_event(source,[&](ui_node* sender, ui_event_phase phase) {
            auto handler = sender->activation_handlers[phase];
            return handler.callback && handler.callback(sender,&source,phase,handler.context);
        });
    }

    bool pointer_handled(ui_pointer_action action, int x, int y) {
        pointer_x = x;
        pointer_y = y;
        auto* source = dragged_control;
        if (action == UI_POINTER_CANCEL && !source) source = pressed_control;
        if (action == UI_POINTER_CANCEL && !source) return false;
        if (!source) {
            const auto hit = std::find_if(interactive.rbegin(),interactive.rend(),[&](auto* node) {
                return hit_allowed(node,x,y) && contains(node->visible,x,y);
            });
            if (hit != interactive.rend()) source = *hit;
        }
        if (!source) return false;
        auto* ancestor = source;
        while (ancestor && ancestor->pointer_handlers.empty() && ancestor->pointer_commands.empty()) ancestor = ancestor->inheritance_parent();
        if (!ancestor) return false;
        return route_event(*source,[&](ui_node* sender, ui_event_phase phase) {
            auto handler = sender->pointer_handlers.find({action,phase});
            if (handler != sender->pointer_handlers.end()) {
                auto [callback,context] = handler->second;
                if (callback(sender,source,action,phase,x,y,context)) return true;
            }
            const auto binding = sender->pointer_commands.find({action,phase});
            if (binding == sender->pointer_commands.end()) return false;
            ui_model* model = nullptr;
            auto* command = find_routed_command(*sender,binding->second,model);
            if (!command) return false;
            if (!command->run && !source->action_busy()) execute_command(*source,command,model);
            return true;
        });
    }

    bool end_pointer_event(int x,int y) {
        const bool suppressed = std::exchange(pointer_down_handled,false);
        if (!pointer_handled(UI_POINTER_UP,x,y) && !suppressed) return false;
        pressed_control = nullptr;
        cancel_drag();
        dirty = true;
        return true;
    }

    void assign_focus(ui_node* target) {
        const bool changed=focus!=target;
        focus = target;
#ifdef _WIN32
        if (changed && focused) accessibility::focus_changed(accessibility_window,target ? target->debug_id : 0);
#endif
        if (target)
            if (auto* scope = target->focus_scope()) scope->logical_focus_id = target->debug_id;
    }

    void activate_node(ui_node& node)
    {
        if (!node.is_enabled() || !node.is_visible() || node.action_busy() || activation_handled(node)) { return; }
        if (!node.is_enabled() || !node.is_visible() || closing) return;
        if (node.kind == UI_COMBO_BOX) {
            node.expanded = !node.expanded;
            dirty = true;
        } else if (node.kind == UI_LIST_ITEM || node.kind == UI_TAB_ITEM) {
            if (node.parent) {
                auto& parent = *node.parent;
                const auto index = std::find(parent.children.begin(), parent.children.end(), &node);
                parent.expanded = false;
                if (parent.kind == UI_COMBO_BOX) {
                    assign_focus(&parent);
                }
                invoke(parent, parent.first_realized + static_cast<unsigned>(index - parent.children.begin()));
            }
        } else if (node.kind == UI_TREE_ITEM) {
            auto* tree = &node;
            while (tree->parent && tree->kind != UI_TREE_VIEW) {
                tree = tree->parent;
            }
            const auto clear_selection = [](auto&& self, ui_node& item) -> void {
                item.tree_selected = false;
                for (auto* child : item.children) {
                    self(self, *child);
                }
            };
            clear_selection(clear_selection, *tree);
            node.tree_selected = true;
            node.expanded = (node.items_source ? node.item_count() > 0 : !node.children.empty()) && !node.expanded;
            if (node.items_source) queue_items_refresh(node);
            invoke(node, node.expanded);
        } else if (node.kind == UI_SPIN_BOX) {
            invoke(node, std::min(100u, node.value + 1));
        } else if (!is_editor(node.kind) && node.kind != UI_SPLIT_VIEW) {
            invoke(node, node.kind == UI_RADIO_BUTTON ? 1 : !node.value);
        }
    }

    static bool contains(layout::Rect area, int x, int y)
    {
        return x >= area.left && x < area.right && y >= area.top && y < area.bottom;
    }

    void reset_caret()
    {
        caret_visible = true;
        if (focus) focus->hosted_editor.reveal=true;
        dirty = true;
        hosted_caret_tick = std::chrono::steady_clock::now();
#if defined(SUNFLECK_X11)
        animation_tick = std::chrono::steady_clock::now();
        last_animation_tick = -1;
#endif
    }

    static bool low_surrogate(wchar_t character)
    {
        return character >= 0xdc00 && character <= 0xdfff;
    }

    static std::size_t next_character(const ui_node& node, std::size_t position)
    {
#ifdef _WIN32
        const auto font = text_format::effective(node);
        TextFormatScope font_scope(font);
        const auto stops = windows_text::clusters(node.text);
        const auto next = std::upper_bound(stops.begin(),stops.end(),position);
        return next == stops.end() ? node.text.size() : *next;
#else
        if (position < node.text.size()) {
            ++position;
            if (position < node.text.size() && low_surrogate(node.text[position])) {
                ++position;
            }
        }
        return position;
#endif
    }

    static std::size_t previous_character(const ui_node& node, std::size_t position)
    {
#ifdef _WIN32
        const auto font = text_format::effective(node);
        TextFormatScope font_scope(font);
        const auto stops = windows_text::clusters(node.text);
        const auto previous = std::lower_bound(stops.begin(),stops.end(),position);
        return previous == stops.begin() ? 0 : *std::prev(previous);
#else
        if (position && low_surrogate(node.text[--position]) && position) {
            --position;
        }
        return position;
#endif
    }

    ui_node* overlay_at(int x, int y) const
    {
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (it->owner->accepts_input() && contains(it->bounds, x, y)) { return it->owner; }
        }
        return nullptr;
    }

    uint64_t debug_hit(int x, int y, layout::Rect* bounds = nullptr) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height) { return 0; }
        const auto* overlay = overlay_at(x, y);
        if (!overlay && workspace) {
            const auto view = workspace->views.find(dock_window);
            if (view != workspace->views.end()) {
                for (auto hit = view->second.hits.rbegin(); hit != view->second.hits.rend(); ++hit) {
                    if (!contains(hit->bounds, x, y)) { continue; }
                    if (hit->part == UI_DOCK_PART_SPLITTER && workspace->host) {
                        if (bounds) { *bounds = hit->bounds; }
                        return workspace->host->debug_id;
                    }
                    if (hit->item.empty()) { continue; }
                    const auto item = std::find_if(workspace->items.begin(), workspace->items.end(),
                        [&](const auto& entry) { return entry->id == hit->item; });
                    if (item != workspace->items.end()) {
                        if (bounds) { *bounds = hit->bounds; }
                        return (*item)->content->debug_id;
                    }
                }
            }
        }
        for (auto it = debug_paint_order.rbegin(); it != debug_paint_order.rend(); ++it) {
            const auto* node = *it;
            if (!node->debug_id || !contains(node->visible, x, y) || !geometry_hit(node,x,y)) { continue; }
            if (!overlay) {
                if (bounds) { *bounds = node->visible; }
                return node->debug_id;
            }
            for (auto* ancestor = node; ancestor; ancestor = ancestor->parent) {
                if (ancestor == overlay) {
                    if (bounds) { *bounds = node->visible; }
                    return node->debug_id;
                }
            }
        }
        if (!overlay) { return 0; }
        if (bounds) { *bounds = overlay->visible; }
        return overlay->debug_id;
    }

    void update_debug_hover(int x, int y)
    {
        layout::Rect bounds{};
        const auto hit = debug_hit(x, y, &bounds);
        if (std::exchange(debug_hover, hit) != hit || bounds.left != debug_hover_bounds.left ||
            bounds.top != debug_hover_bounds.top || bounds.right != debug_hover_bounds.right ||
            bounds.bottom != debug_hover_bounds.bottom) { dirty = true; }
        debug_hover_bounds = bounds;
    }

    static ui_point layout_point(const ui_node& node,int x,int y)
    {
        const auto inverse = affine::inverse(node.world_transform);
        return inverse ? affine::map(*inverse,x,y) : ui_point{};
    }

    static bool geometry_hit(const ui_node* node,int x,int y)
    {
        for (auto* ancestor = node; ancestor; ancestor = ancestor->parent) {
            if (affine::trivial(ancestor->world_transform) || (ancestor != node && !ancestor->clip_to_bounds)) continue;
            auto inverse = affine::inverse(ancestor->world_transform);
            if (!inverse) return false;
            auto p = affine::map(*inverse,x+0.5,y+0.5);
            const auto& b = ancestor->local_hit;
            if (p.x < b.left || p.y < b.top || p.x >= b.right || p.y >= b.bottom) return false;
        }
        return true;
    }

    bool hit_allowed(const ui_node* node, int x, int y) const
    {
        if (!node->accepts_input()) { return false; }
        if (!geometry_hit(node,x,y)) return false;
        const auto* overlay = overlay_at(x, y);
        if (!overlay) { return true; }
        for (auto* ancestor = node; ancestor; ancestor = ancestor->parent) {
            if (ancestor == overlay) { return true; }
        }
        return false;
    }

    bool owns_input(const ui_node* node) const
    {
        if (node->window_owner && node->window_owner != active_session) { return false; }
        for (auto* ancestor = node; ancestor; ancestor = ancestor->parent) {
            if (workspace) {
                for (const auto& item : workspace->items) {
                    if (item->content != ancestor) { continue; }
                    const auto& place = workspace->layout.placements.at(item->id);
                    return place.state == UI_DOCK_FLOATING
                        ? docking::window_of(workspace->layout, place.group) == dock_window
                        : dock_window == 0;
                }
            }
            if (ancestor == root) { return true; }
        }
        return false;
    }

    void click(int x, int y)
    {
        if (workspace && (workspace->drag.prepared || (!overlay_at(x, y) && docking::consumes(*workspace, dock_window, x, y)))) {
            for (const auto& node : nodes) {
                if (!owns_input(node.get())) { continue; }
                if (node->kind == UI_COMBO_BOX && node->expanded) { node->expanded = false; dirty = true; }
            }
            docking::pointer(*workspace, dock_window, UI_POINTER_UP, x, y); return;
        }
        if (std::exchange(keyboard_focus, false)) {
            dirty = true;
        }
        if (!root) {
            const auto rect = button();
            if (contains({rect.left, rect.top, rect.right, rect.bottom}, x, y)) {
                activate();
            }
            return;
        }
        for (auto it = scrollable.rbegin(); it != scrollable.rend(); ++it) {
            auto& node = **it;
            const bool horizontal = contains(node.horizontal_bar, x, y);
            if (hit_allowed(&node, x, y) && contains(node.visible, x, y) && (horizontal || contains(node.scrollbar, x, y))) {
                if (horizontal) {
                    const auto bar = node.horizontal_bar;
                    node.horizontal_offset = (x-bar.left)*node.horizontal_limit/std::max(1,bar.right-bar.left-1);
                } else {
                    const auto bar = node.scrollbar;
                    node.offset = (y-bar.top)*node.scroll_limit/std::max(1,bar.bottom-bar.top-1);
                }
                dirty = true;
                return;
            }
        }
        ui_node* target = nullptr;
        for (auto it = interactive.rbegin(); it != interactive.rend(); ++it) {
            if (hit_allowed(*it, x, y) && contains((*it)->visible, x, y)) {
                target = *it;
                break;
            }
        }
        for (const auto& node : nodes) {
            if (!owns_input(node.get())) { continue; }
            if (node->kind == UI_COMBO_BOX && node->expanded &&
                target != node.get() && (!target || target->parent != node.get())) {
                node->expanded = false;
                dirty = true;
            }
        }
        if (target) {
            auto& node = *target;
            if (node.kind == UI_LABEL && node.anchor) {
                if (!node.anchor->is_visible() || !node.anchor->is_enabled()) return;
                assign_focus(node.anchor);
                reset_caret();
                return;
            }
            assign_focus(&node);
            reset_caret();
            if (is_editor(node.kind) || node.text_selectable) {
                const auto font = text_format::effective(node);
                TextFormatScope font_scope(font);
                const auto point = layout_point(node,x,y);
                const auto* nearest=text_navigation::nearest(node.text_stops,int(std::round(point.x)),int(std::round(point.y)),text_line_height(dpi));
                node.caret = nearest ? nearest->index : node.display_text().size();
                node.caret_trailing=nearest && nearest->trailing;
                node.clear_selection();
                node.pending_surrogate = 0;
            } else if (node.kind == UI_SPIN_BOX) {
                const int delta = x >= (node.bounds.left + node.bounds.right) / 2 ? 1 : -1;
                invoke(node, std::clamp(static_cast<int>(node.value) + delta, 0, 100));
            } else if (node.kind == UI_SLIDER) {
                update_drag_value(node, x, y);
            } else {
                activate_node(node);
            }
        }
    }

    void update_drag_value(ui_node& node, int x, int y)
    {
        if (node.table && node.table->resizing_column>=0) {
            auto& table=*node.table;
            auto width=std::clamp(table.resize_width+(x-table.resize_origin)*96.0/dpi,16.0,4096.0);
            (void)ui_data_grid_set_column_width(&node,static_cast<uint32_t>(table.resizing_column),width);
            return;
        }
        if (is_editor(node.kind) || node.text_selectable) {
            TextFormatScope font_scope(text_format::effective(node));
            const auto point = layout_point(node,x,y);
            if (const auto* stop=text_navigation::nearest(node.text_stops,int(std::round(point.x)),int(std::round(point.y)),text_line_height(dpi))) {
                node.caret=stop->index; node.caret_trailing=stop->trailing; node.select_all=false; node.caret_column.reset();
                reset_caret();
            }
            return;
        }

        if (node.kind == UI_DOCUMENT_VIEW && node.document && node.document->selecting) {
            document::pointer(node, 1, x, y); dirty = true; return;
        }
        if (node.kind == UI_SCROLL_VIEW || node.kind == UI_LIST_VIEW ||
            node.kind == UI_TREE_VIEW || node.kind == UI_DATA_GRID || node.kind == UI_DOCUMENT_VIEW) {
            const bool horizontal = node.dragging_horizontal;
            const auto bar = horizontal ? node.horizontal_bar : node.scrollbar;
            const auto thumb = horizontal ? node.horizontal_thumb : node.scrollbar_thumb;
            const int start = horizontal ? bar.left : bar.top;
            const int end = horizontal ? bar.right : bar.bottom;
            const int thumb_start = horizontal ? thumb.left : thumb.top;
            const int thumb_end = horizontal ? thumb.right : thumb.bottom;
            const int travel = end-start-(thumb_end-thumb_start);
            if (travel > 0) {
                const int position = std::clamp((horizontal ? x : y)-start-scrollbar_grab_offset,0,travel);
                if (position == thumb_start-start && position != 0 && position != travel) return;
                const double next = position*(horizontal ? node.horizontal_limit : node.scroll_limit)/travel;
                double& offset = horizontal ? node.horizontal_offset : node.offset;
                if (offset != next) { offset = next; dirty = true; }
            }
            return;
        }
        const int padding = node.kind == UI_SLIDER ? (gallery_style.gap * dpi + 48) / 96 : 0;
        const int left = node.bounds.left + padding;
        const int extent = std::max(1, node.bounds.right - padding - left);
        const auto value = static_cast<unsigned>(std::clamp((x - left) * 100 / extent, 0, 100));
        if (value != node.value) {
            invoke(node, value);
        }
    }

    void begin_drag(int x, int y)
    {
        pointer_down_handled = pointer_handled(UI_POINTER_DOWN,x,y);
        if (pointer_down_handled) { pressed_control = nullptr; dirty = true; return; }
        if (workspace && !overlay_at(x, y) && docking::pointer(*workspace, dock_window, UI_POINTER_DOWN, x, y)) { return; }
        if (std::exchange(keyboard_focus, false)) {
            dirty = true;
        }
        cancel_drag();
        for (auto it = scrollable.rbegin(); it != scrollable.rend(); ++it) {
            auto& node = **it;
            if (node.table && contains(node.table->header,x,y) && hit_allowed(&node,x,y)) {
                auto& table=*node.table;
                double right=table.left;
                for (uint32_t i=table.first;i<table.first+table.visible;++i) {
                    right+=table.columns[i].width*dpi/96.0;
                    if (std::abs(right-x)<=4*dpi/96.0) {
                        table.resizing_column=static_cast<int>(i);
                        table.resize_origin=x;
                        table.resize_width=table.columns[i].width;
                        dragged_control=&node;
                        node.dragging=true;
                        return;
                    }
                    if (x<right) {
                        binding_app_error(this,ui_data_grid_sort_column(&node,i,table.sort_path==table.columns[i].path && !table.descending));
                        return;
                    }
                }
            }
            const bool horizontal = contains(node.horizontal_bar, x, y);
            if (hit_allowed(&node, x, y) && contains(node.visible, x, y) && (horizontal || contains(node.scrollbar, x, y))) {
                const auto thumb = horizontal ? node.horizontal_thumb : node.scrollbar_thumb;
                if (contains(thumb, x, y)) {
                    node.dragging_horizontal = horizontal;
                    dragged_control = &node;
                    node.dragging = true;
                    scrollbar_grab_offset = horizontal ? x-thumb.left : y-thumb.top;
                    dirty = true;
                }
                return;
            }
        }
        const auto hit = std::find_if(interactive.rbegin(), interactive.rend(), [&](const ui_node* node) {
            return hit_allowed(node, x, y) && contains(node->visible, x, y);
        });
        if (hit != interactive.rend() && (is_editor((*hit)->kind) || (*hit)->text_selectable)) {
            auto* node=*hit;
            const auto anchor=node->selection_anchor.value_or(node->caret);
            const bool anchor_trailing=node->selection_anchor ? node->anchor_trailing : node->caret_trailing;
            bool extend=false;
#ifdef _WIN32
            extend=GetKeyState(VK_SHIFT)<0;
#endif
            click(x,y); node->selection_anchor=extend?anchor:node->caret;
            node->anchor_trailing=extend?anchor_trailing:node->caret_trailing;
            dragged_control=node; node->dragging=true; return;
        }
        if (hit != interactive.rend() && (*hit)->kind == UI_DOCUMENT_VIEW) {
            dragged_control = *hit; assign_focus(dragged_control); dragged_control->dragging = true;
            document::pointer(**hit, 0, x, y); dirty = true; return;
        }
        if (hit == interactive.rend() || ((*hit)->kind != UI_SLIDER && (*hit)->kind != UI_SPLIT_VIEW)) {
            return;
        }
        dragged_control = *hit;
        dragged_control->dragging = true;
        dirty = true;
        if (dragged_control->kind == UI_SLIDER) {
            click(x, y);
        } else {
            assign_focus(dragged_control);
        }
    }

    void cancel_drag()
    {
        if (workspace && (!workspace->drag.window_move || workspace->drag.window == dock_window)) {
            docking::cancel(*workspace);
        }
        if (auto* node = std::exchange(dragged_control, nullptr)) {
            node->dragging = false;
            if (node->table) node->table->resizing_column=-1;
            if(node->kind == UI_DOCUMENT_VIEW) document::pointer(*node,3,0,0);
            dirty = true;
        }
    }

    void cancel_pointer(int x, int y)
    {
        if (pressed_control || dragged_control) {
            pointer_handled(UI_POINTER_CANCEL, x, y);
            pointer_down_handled = true;
            dirty = true;
        }
        host_pointer_down = false;
        pressed_control = nullptr;
        cancel_drag();
    }

    void finish_drag(int x, int y)
    {
        if (auto* node = dragged_control) {
            if(node->kind == UI_DOCUMENT_VIEW && node->document && node->document->selecting) {
                document::pointer(*node,2,x,y); cancel_drag(); return;
            }
            cancel_drag();
            update_drag_value(*node, x, y);
        }
    }

    void move_directional_focus(int step, bool vertical)
    {
        const auto origin = focus->visible;
        const double cx = (origin.left + double(origin.right)) / 2, cy = (origin.top + double(origin.bottom)) / 2;
        ui_node* nearest = nullptr;
        double best = std::numeric_limits<double>::infinity();
        for (auto* candidate : interactive) {
            const auto rect = candidate->visible;
            if (candidate == focus || candidate->text_selectable || !candidate->is_tab_stop || !candidate->is_enabled() ||
                !candidate->is_visible() || rect.left >= rect.right || rect.top >= rect.bottom) continue;
            const double dx = (rect.left + double(rect.right)) / 2 - cx, dy = (rect.top + double(rect.bottom)) / 2 - cy;
            const double forward = vertical ? dy : dx, side = vertical ? dx : dy;
            if (step * forward <= 0) continue;
            const double distance = forward * forward + 4 * side * side;
            if (distance < best) { nearest = candidate; best = distance; }
        }
        if (nearest) {
            assign_focus(nearest);
            keyboard_focus = true;
            reset_caret();
        }
    }

    void key(bool tab, bool reverse, int adjustment, bool directional = false, bool vertical = false)
    {
        if (!root) {
            if (!tab && !adjustment) {
                activate();
            }
            return;
        }
        if (tab && !interactive.empty()) {
            auto stops = interactive;
            std::erase_if(stops, [](const ui_node* node) { return node->text_selectable || !node->is_tab_stop || !node->is_enabled() || !node->is_visible(); });
            if (stops.empty()) return;
            std::stable_sort(stops.begin(), stops.end(), [](const ui_node* a, const ui_node* b) { return a->tab_index < b->tab_index; });
            keyboard_focus = true;
            auto next = std::find(stops.begin(), stops.end(), focus);
            if (reverse) {
                assign_focus(next == stops.begin() || next == stops.end()
                    ? stops.back() : *std::prev(next));
            } else {
                assign_focus(next == stops.end() || ++next == stops.end()
                    ? stops.front() : *next);
            }
            reset_caret();
        } else if (!tab && focus &&
                   std::find(interactive.begin(), interactive.end(), focus) != interactive.end()) {
            if (focus->kind == UI_DOCUMENT_VIEW && adjustment) {
                focus->offset = std::clamp(focus->offset + adjustment * 12, 0.0, focus->scroll_limit); dirty = true;
            } else if ((focus->kind == UI_SLIDER || focus->kind == UI_SPIN_BOX ||
                 focus->kind == UI_SPLIT_VIEW) && adjustment) {
                const int delta = focus->kind == UI_SPIN_BOX ? adjustment / 5 : adjustment;
                invoke(*focus, std::clamp(static_cast<int>(focus->value) + delta, 0, 100));
            } else if (adjustment && (focus->kind == UI_COMBO_BOX || focus->kind == UI_TAB_ITEM ||
                                      focus->kind == UI_LIST_ITEM)) {
                auto* selection = focus->kind == UI_COMBO_BOX ? focus : focus->parent;
                if (selection && !selection->children.empty()) {
                    const int step = adjustment > 0 ? 1 : -1;
                    int next = static_cast<int>(selection->value) + step;
                    while (next >= 0 && next < static_cast<int>(selection->children.size()) &&
                        (!selection->children[next]->is_visible() || !selection->children[next]->is_enabled())) next += step;
                    if (next >= 0 && next < static_cast<int>(selection->children.size())) {
                        if (focus != selection) assign_focus(selection->children[next]);
                        invoke(*selection, next);
                    }
                }
            } else if (directional && !is_editor(focus->kind)) {
                move_directional_focus(adjustment, vertical);
            } else if (!adjustment) {
                activate_node(*focus);
            }
        }
    }

    void hover(int x, int y)
    {
        if (workspace) {
            const bool overlay = overlay_at(x, y) && !workspace->drag.prepared;
            if (docking::pointer(*workspace, dock_window, UI_POINTER_MOVE, overlay ? -1 : x, overlay ? -1 : y) && !overlay) { return; }
        }
        const auto hit = std::find_if(interactive.rbegin(), interactive.rend(), [&](const ui_node* node) {
            return hit_allowed(node, x, y) && contains(node->visible, x, y);
        });
        auto* next = hit == interactive.rend() ? nullptr : *hit;
        dirty = dirty || hovered_control != next;
        hovered_control = next;
        for (const auto& node : nodes) {
            if (!owns_input(node.get())) { continue; }
            if (node->kind == UI_TOOLTIP && node->anchor) {
                const unsigned visible = contains(node->anchor->visible, x, y);
                dirty = dirty || node->value != visible;
                node->value = visible;
            }
        }
    }

    void context_menu(int x, int y)
    {
        for (const auto& node : nodes) {
            if (!owns_input(node.get())) { continue; }
            if (node->kind == UI_CONTEXT_MENU && node->anchor) {
                const unsigned visible = contains(node->anchor->visible, x, y);
                dirty = dirty || node->value != visible;
                node->value = visible;
            }
        }
    }

    bool allows_line_breaks(const ui_node& editor) const
    {
        return editor.kind == UI_TEXT_AREA && binding_allows_line_breaks(editor) && (!editor.text_source ||
            std::none_of(nodes.begin(), nodes.end(), [&](const auto& node) {
                return node->text_source == editor.text_source &&
                    is_editor(node->kind) && node->kind != UI_TEXT_AREA;
            }));
    }

    void replace_node_text(ui_node& node, const std::wstring& value)
    {
        node.text.assign(value);
        node.text_stops.clear();
        node.caret_trailing=false;
        node.caret = node.text.size();
        node.clear_selection();
        node.pending_surrogate = 0;
        if (focus == &node) {
            reset_caret();
        } else {
            dirty = true;
        }
        binding_app_dirty(this);
    }

    void update_text_source(ui_text_source& source, const std::wstring& value, ui_node* origin = nullptr)
    {
        if (source.value == value) {
            return;
        }
        source.value.assign(value);
        binding_text_source_changed(source);
        bool affected = false;
        for (const auto& node : nodes) {
            if (node->text_source == &source) {
                affected = true;
                properties::capture(*node, UI_PROPERTY_TEXT);
                if (node.get() != origin && is_editor(node->kind)) {
                    replace_node_text(*node, source.value);
                }
                dirty = true;
            }
        }
        if (affected) { binding_app_dirty(this); }
    }

    void publish_editor_text(ui_node& node)
    {
        node.caret_column.reset();
        binding_user_changed(node, UI_PROPERTY_TEXT);
        if (node.text_source) {
            update_text_source(*node.text_source, node.text, &node);
        }
    }

    enum class EditKey { left, right, up, down, home, end, remove };

    static bool erase_selection(ui_node& node) {
        const auto [first,last]=node.selection();
        if (first==last) return false;
        node.text.erase(first,last-first); node.caret=first; node.caret_trailing=false;
        node.clear_selection(); return true;
    }
    bool editor_key(EditKey key, bool extend=false, bool document_edge=false)
    {
        if (!focus || !is_editor(focus->kind) || !focus->is_enabled() ||
            std::find(interactive.begin(),interactive.end(),focus)==interactive.end()) return false;
        auto& node=*focus;
#ifdef _WIN32
        if (ime_composing) return false; // Candidate navigation belongs to the native IME.
#endif
        if (node.hosted_editor.composing) return true;
        const auto [first,last]=node.selection();
        const auto original=node.caret;
        const bool vertical=key==EditKey::up || key==EditKey::down;
        if (!vertical) node.caret_column.reset();
        if (key==EditKey::remove) {
            if (!erase_selection(node)) node.text.erase(node.caret,next_character(node,node.caret)-node.caret);
            node.clear_selection(); node.caret_trailing=false; publish_editor_text(node);
        } else {
            const bool left=key==EditKey::left, right=key==EditKey::right;
            if (extend && !node.selection_anchor) {
                node.selection_anchor=node.select_all ? (node.caret==first?last:first) : original;
                node.anchor_trailing=node.select_all ? *node.selection_anchor==last : node.caret_trailing;
            }
            if (!extend && first!=last && (left || right)) {
                const size_t anchor=node.selection_anchor.value_or(node.caret==first?last:first);
                const bool anchor_trailing=node.selection_anchor ? node.anchor_trailing : anchor==last;
                const auto* anchored=text_navigation::locate(node.text_stops,{anchor,anchor_trailing});
                const auto* active=text_navigation::locate(node.text_stops,{node.caret,node.caret_trailing});
                const bool anchor_on_left=anchored && active
                    ? std::pair{anchored->y,anchored->x}<=std::pair{active->y,active->x}
                    : anchor<=node.caret;
                if (left==anchor_on_left) {
                    node.caret=anchor;
                    node.caret_trailing=anchor_trailing;
                }
            } else if (document_edge && (key==EditKey::home || key==EditKey::end)) {
                node.caret=key==EditKey::home?0:node.text.size(); node.caret_trailing=key==EditKey::end;
            } else {
                const int dx=(left || key==EditKey::home)?-1:1;
                const int dy=key==EditKey::up?-1:key==EditKey::down?1:0;
                std::optional<int> preferred_x;
                if (dy) {
                    if (!node.caret_column) {
                        if (const auto* current=text_navigation::locate(node.text_stops,{node.caret,node.caret_trailing}))
                            node.caret_column=(current->x-node.text_bounds.left+node.hosted_editor.scroll_x)*96.0/dpi;
                    }
                    if (node.caret_column) preferred_x=int(std::lround(*node.caret_column*dpi/96.0+
                        node.text_bounds.left-node.hosted_editor.scroll_x));
                }
                const auto next=text_navigation::move(node.text_stops,{node.caret,node.caret_trailing},dx,dy,
                    node.resolved_font.flow==UI_FLOW_RIGHT_TO_LEFT,key==EditKey::home || key==EditKey::end,preferred_x);
                if (next) { node.caret=next->index; node.caret_trailing=next->trailing; }
                else if (left || right) { node.caret=left?previous_character(node,node.caret):next_character(node,node.caret); node.caret_trailing=false; }
            }
            node.select_all=false;
            if (!extend) node.selection_anchor.reset();
        }
        node.pending_surrogate=0; reset_caret(); return true;
    }

#ifdef _WIN32
    void place_ime(bool force = false)
    {
        if (!window || !focused || !focus || !is_editor(focus->kind) || !focus->is_enabled() ||
            std::find(interactive.begin(), interactive.end(), focus) == interactive.end()) {
            if (native_caret) {
                DestroyCaret();
                native_caret = false;
                ime_point = {-1, -1};
            }
            return;
        }
        const auto& node = *focus;
        const auto* stop=text_navigation::locate(node.text_stops,{node.caret,node.caret_trailing});
        const auto position = stop ? affine::map(node.world_transform,stop->x,stop->y)
                                   : ui_point{double(node.visible.left),double(node.visible.top)};
        const POINT point{LONG(std::round(position.x)),LONG(std::round(position.y))};
        const auto font = text_format::effective(node);
        TextFormatScope font_scope(font);
        const int line_height = text_line_height(dpi);
        if (native_caret && ime_line_height != line_height) { DestroyCaret(); native_caret = false; }
        if (!native_caret) {
            ime_line_height = line_height;
            native_caret = CreateCaret(window, nullptr, 1, std::max(1, line_height)) != 0;
            force = true;
        }
        if (!force && point.x == ime_point.x && point.y == ime_point.y) {
            return;
        }
        ime_point = point;
        if (native_caret) {
            SetCaretPos(point.x, point.y);
        }
        if (const auto context = ImmGetContext(window)) {
            COMPOSITIONFORM composition{};
            composition.dwStyle = CFS_POINT;
            composition.ptCurrentPos = point;
            ImmSetCompositionWindow(context, &composition);
            CANDIDATEFORM candidates{};
            candidates.dwStyle = CFS_EXCLUDE;
            const auto bottom = point.y + static_cast<LONG>(line_height);
            candidates.ptCurrentPos = {point.x, bottom};
            candidates.rcArea = {point.x, point.y, point.x + 2, bottom};
            ImmSetCandidateWindow(context, &candidates);
            ImmReleaseContext(window, context);
        }
    }

    void edit(HWND hwnd, wchar_t character)
    {
        if (!focus || !is_editor(focus->kind) || !focus->is_enabled() ||
            std::find(interactive.begin(), interactive.end(), focus) == interactive.end()) {
            return;
        }
        auto& node = *focus;
        try {
            const auto replace_selection = [&] {
                erase_selection(node);
            };
            if (character == 1) {
                node.select_all = true; node.selection_anchor=0; node.anchor_trailing=false; node.caret=node.text.size(); node.caret_trailing=true; node.caret_column.reset();
                dirty = true;
                return;
            }
            if (character == 3 || character == 24) {
                if (node.kind != UI_PASSWORD_BOX && node.selection().first!=node.selection().second && OpenClipboard(hwnd)) {
                    const auto [first,last]=node.selection();
                    const auto selected=node.text.substr(first,last-first);
                    const auto bytes = (selected.size() + 1) * sizeof(wchar_t);
                    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (memory) {
                        if (auto* target = GlobalLock(memory)) {
                            std::memcpy(target, selected.c_str(), bytes);
                            GlobalUnlock(memory);
                            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory)) {
                                memory = nullptr;
                                if (character == 24) {
                                    erase_selection(node);
                                    publish_editor_text(node);
                                    reset_caret();
                                }
                            }
                        }
                        if (memory) {
                            GlobalFree(memory);
                        }
                    }
                    CloseClipboard();
                }
                return;
            }
            if (character == 13) {
                if (node.kind != UI_TEXT_AREA || GetKeyState(VK_CONTROL) < 0) {
                    invoke(node, 0);
                } else {
                    if (!allows_line_breaks(node)) {
                        return;
                    }
                    replace_selection();
                    if (node.text.size() < 4096) {
                        node.text.insert(node.caret++, 1, L'\n');
                    }
                    node.clear_selection();
                    node.caret_trailing=false;
                    publish_editor_text(node);
                    reset_caret();
                }
                return;
            }
            if (character == 22 && OpenClipboard(hwnd)) {
                if (auto memory = GetClipboardData(CF_UNICODETEXT)) {
                    if (auto* text = static_cast<const wchar_t*>(GlobalLock(memory))) {
                        wchar_t buffer[4096];
                        const auto capacity = std::min<std::size_t>(GlobalSize(memory) / sizeof(wchar_t), 4096);
                        std::size_t length = 0;
                        const bool multiline = allows_line_breaks(node);
                        while (length < capacity && (text[length] >= L' ' ||
                            (multiline && (text[length] == L'\r' || text[length] == L'\n')))) {
                            buffer[length] = text[length];
                            ++length;
                        }
                        GlobalUnlock(memory);
                        CloseClipboard();
                        replace_selection();
                        length = std::min(length, 4096 - node.text.size());
                        if (length && buffer[length - 1] >= 0xd800 && buffer[length - 1] <= 0xdbff) {
                            --length;
                        }
                        node.text.insert(node.caret, buffer, length);
                        node.caret += length;
                        node.clear_selection();
                        node.caret_trailing=false;
                        publish_editor_text(node);
                        reset_caret();
                        return;
                    }
                }
                CloseClipboard();
                return;
            }
            if (character == 8) {
                if (node.selection().first!=node.selection().second) {
                    replace_selection();
                } else if (node.caret) {
                    const auto previous = previous_character(node, node.caret);
                    node.text.erase(previous, node.caret - previous);
                    node.caret = previous;
                }
            } else if (character >= 32 && character != 127) {
                if (character >= 0xd800 && character <= 0xdbff) {
                    node.pending_surrogate = character;
                    return;
                }
                replace_selection();
                if (character >= 0xdc00 && character <= 0xdfff) {
                    if (node.pending_surrogate && node.text.size() <= 4094) {
                        const wchar_t pair[] = {node.pending_surrogate, character};
                        node.text.insert(node.caret, pair, 2);
                        node.caret += 2;
                    }
                } else if (node.text.size() < 4096) {
                    node.text.insert(node.caret++, 1, character);
                }
            } else {
                return;
            }
            node.pending_surrogate = 0;
            node.caret_trailing=false;
            node.clear_selection();
            publish_editor_text(node);
            reset_caret();
        } catch (const std::bad_alloc&) {
            closing = true;
        }
    }
#endif

#if defined(SUNFLECK_X11)
    void place_xim()
    {
        if (!input_context || !xim_positioned || !focus || !is_editor(focus->kind)) {
            return;
        }
        const auto& node = *focus;
        const auto* stop=text_navigation::locate(node.text_stops,{node.caret,node.caret_trailing});
        const auto position = stop ? affine::map(node.world_transform,stop->x,stop->y)
                                   : ui_point{double(node.visible.left),double(node.visible.top)};
        const int x = int(std::round(position.x)), y = int(std::round(position.y));
        XPoint spot{static_cast<short>(std::clamp(x, 0, 32767)),
                    static_cast<short>(std::clamp(y + static_cast<int>((16 * dpi + 48) / 96), 0, 32767))};
        if (spot.x == xim_spot.x && spot.y == xim_spot.y) {
            return;
        }
        if (auto attributes = XVaCreateNestedList(0, XNSpotLocation, &spot, nullptr)) {
            if (!XSetICValues(input_context, XNPreeditAttributes, attributes, nullptr)) {
                xim_spot = spot;
                XFlush(display);
            }
            XFree(attributes);
        }
    }

    void sync_xim_focus()
    {
        const bool editing = focused && focus && focus->is_enabled() && is_editor(focus->kind) &&
            std::find(interactive.begin(), interactive.end(), focus) != interactive.end();
        if (editing) {
            place_xim();
        }
        if (input_context && editing != xim_focused) {
            if (editing) {
                XSetICFocus(input_context);
            } else {
                XUnsetICFocus(input_context);
            }
            xim_focused = editing;
        }
    }

    void selection_request(const XSelectionRequestEvent& request)
    {
        XEvent event{};
        auto& reply = event.xselection;
        reply.type = SelectionNotify;
        reply.display = display;
        reply.requestor = request.requestor;
        reply.selection = request.selection;
        reply.target = request.target;
        reply.time = request.time;
        const Atom property = request.property ? request.property : request.target;
        if (request.target == targets) {
            const Atom formats[] = {targets, utf8};
            XChangeProperty(display, request.requestor, property, XA_ATOM, 32, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(formats), 2);
            reply.property = property;
        } else if (request.target == utf8) {
            XChangeProperty(display, request.requestor, property, utf8, 8, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(clipboard_text.data()),
                            static_cast<int>(clipboard_text.size()));
            reply.property = property;
        }
        XSendEvent(display, request.requestor, False, NoEventMask, &event);
        XFlush(display);
    }

    void selection_notify(const XSelectionEvent& event)
    {
        auto* node = std::exchange(paste_target, nullptr);
        if (!node || node != focus || event.property != paste_property) {
            return;
        }
        Atom type = 0;
        int format = 0;
        unsigned long size = 0, remaining = 0;
        unsigned char* bytes = nullptr;
        const int status = XGetWindowProperty(display, window, paste_property, 0, 4096, True,
            utf8, &type, &format, &size, &remaining, &bytes);
        const auto release = [](unsigned char* data) { XFree(data); };
        std::unique_ptr<unsigned char, decltype(release)> storage(bytes, release);
        if (status != Success || type != utf8 || format != 8 || remaining || !bytes) {
            XDeleteProperty(display, window, paste_property);
            return;
        }
        try {
            const auto* start = reinterpret_cast<const char*>(bytes);
            auto value = utf8_text::decode({start, size});
            const bool multiline = allows_line_breaks(*node);
            const auto end = std::find_if(value.begin(), value.end(), [&](wchar_t c) {
                return c < L' ' && (!multiline || (c != L'\r' && c != L'\n'));
            });
            value.erase(end, value.end());
            erase_selection(*node);
            value.resize(std::min(value.size(), 4096 - node->text.size()));
            node->text.insert(node->caret, value);
            node->caret += value.size();
            node->clear_selection();
            publish_editor_text(*node);
            reset_caret();
        } catch (const std::range_error&) {
        } catch (const std::bad_alloc&) {
            closing = true;
        }
    }

    void edit_x11(XKeyEvent& event)
    {
        if (!focus || !is_editor(focus->kind) || !focus->is_enabled() ||
            std::find(interactive.begin(), interactive.end(), focus) == interactive.end()) {
            return;
        }
        auto& node = *focus;
        char buffer[4096];
        KeySym symbol{};
        Status lookup = XLookupNone;
        const int size = input_context
            ? Xutf8LookupString(input_context, &event, buffer, sizeof(buffer), &symbol, &lookup)
            : XLookupString(&event, buffer, sizeof(buffer), &symbol, nullptr);
        if (lookup == XBufferOverflow || size > static_cast<int>(sizeof(buffer))) {
            return;
        }
        const auto key = event.keycode ? XLookupKeysym(&event, 0) : NoSymbol;
        // XIM may return a committed string alongside the candidate-selection keysym.
        const auto first = size > 0 ? static_cast<unsigned char>(buffer[0]) : 0;
        const bool commit = first >= ' ' && first != 127 &&
            (lookup == XLookupChars || (lookup == XLookupBoth &&
                (!event.keycode || first >= 0x80 || size > 1)));
        if (!commit) {
            switch (key) {
            case XK_Left: editor_key(EditKey::left,(event.state & ShiftMask)!=0,(event.state & ControlMask)!=0); return;
            case XK_Right: editor_key(EditKey::right,(event.state & ShiftMask)!=0,(event.state & ControlMask)!=0); return;
            case XK_Home: editor_key(EditKey::home,(event.state & ShiftMask)!=0,(event.state & ControlMask)!=0); return;
            case XK_End: editor_key(EditKey::end,(event.state & ShiftMask)!=0,(event.state & ControlMask)!=0); return;
            case XK_Delete: editor_key(EditKey::remove,(event.state & ShiftMask)!=0,(event.state & ControlMask)!=0); return;
            }
        }
        try {
            if (!commit && event.state & ControlMask) {
                if (key == XK_a) {
                    node.select_all = true;
                    reset_caret();
                } else if (key == XK_Return) {
                    invoke(node, 0);
                } else if ((key == XK_c || key == XK_x) && node.selection().first!=node.selection().second &&
                           node.kind != UI_PASSWORD_BOX) {
                    const auto [first,last]=node.selection();
                    clipboard_text = utf8_text::encode(node.text.substr(first,last-first));
                    XSetSelectionOwner(display, clipboard, window, event.time);
                    if (key == XK_x) {
                        erase_selection(node);
                        publish_editor_text(node);
                        reset_caret();
                    }
                } else if (key == XK_v) {
                    paste_target = &node;
                    XConvertSelection(display, clipboard, utf8, paste_property, window, event.time);
                }
                return;
            }
            if (!commit && key == XK_BackSpace) {
                if (erase_selection(node)) {} else if (node.caret) {
                    node.text.erase(--node.caret, 1);
                }
            } else {
                std::wstring text;
                if (!commit && (key == XK_Return || key == XK_KP_Enter)) {
                    if (node.kind != UI_TEXT_AREA) {
                        invoke(node, 0);
                        return;
                    }
                    if (!allows_line_breaks(node)) {
                        return;
                    }
                    text = L"\n";
                } else {
                    if (size <= 0) {
                        return;
                    }
                    text = utf8_text::decode({buffer, static_cast<std::size_t>(size)});
                    if (text.empty() || text.front() < L' ' ||
                        (!allows_line_breaks(node) && text.find_first_of(L"\r\n") != std::wstring::npos)) {
                        return;
                    }
                }
                erase_selection(node);
                text.resize(std::min(text.size(), 4096 - node.text.size()));
                node.text.insert(node.caret, text);
                node.caret += text.size();
            }
            node.clear_selection();
            publish_editor_text(node);
            reset_caret();
        } catch (const std::range_error&) {
        } catch (const std::bad_alloc&) {
            closing = true;
        }
    }
#endif

    void scroll_gallery(double steps, int x, int y, bool horizontal = false)
    {
        scroll_pixels(steps * double(dpi) / 4, x, y, horizontal);
    }

    void scroll_nodes(double delta, int x, int y, bool horizontal)
    {
        for (auto it = scrollable.rbegin(); it != scrollable.rend(); ++it) {
            auto& node = **it;
            if (hit_allowed(&node, x, y) && contains(node.visible, x, y)) {
                double& offset = horizontal ? node.horizontal_offset : node.offset;
                const double limit = horizontal ? node.horizontal_limit : node.scroll_limit;
                const double next = std::clamp(offset + delta, 0.0, limit);
                if (next != offset) {
                    offset = next;
                    dirty = true;
                    return;
                }
            }
        }
    }

    void scroll_pixels(double delta, int x, int y, bool horizontal)
    {
        if (root) {
            scroll_nodes(delta, x, y, horizontal);
            return;
        }
        if (gallery) {
            const int limit = gallery_scroll_limit(gallery_style, height, dpi);
            const int offset = std::clamp(gallery_scroll + static_cast<int>(delta), 0, limit);
            if (offset != gallery_scroll) {
                gallery_scroll = offset;
                dirty = true;
            }
        }
    }

    void sync_window_frame(bool initial = false)
    {
        const bool wanted = workspace != nullptr;
        if ((!initial && custom_frame == wanted) || headless) { return; }
#if defined(_WIN32) || defined(SUNFLECK_X11)
        if (!window) { return; }
#endif
        custom_frame = wanted;
#ifdef _WIN32
        // Match the rounded client border and avoid a second, differently colored
        // DWM border at the corners. Older Windows versions ignore these hints.
        const DWORD corner = wanted ? 2 : 0; // DWMWCP_ROUND / DWMWCP_DEFAULT
        const COLORREF border = wanted ? 0xfffffffe : 0xffffffff; // NONE / DEFAULT
        (void)DwmSetWindowAttribute(window, 33, &corner, sizeof(corner));
        (void)DwmSetWindowAttribute(window, 34, &border, sizeof(border));
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#elif defined(SUNFLECK_X11)
        unsigned long hints[] = {2, 0, wanted ? 0ul : 1ul, 0, 0};
        const auto atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
        XChangeProperty(display, window, atom, atom, 32, PropModeReplace, reinterpret_cast<unsigned char*>(hints), 5); XFlush(display);
#endif
    }
    docking::WindowFrame window_frame() const { return docking::window_frame(theme_resource, width, dpi); }
    ui_dock_part caption_part(int x, int y) const
    {
        if (!workspace || (hosted_input && !dock_window)) { return UI_DOCK_PART_COUNT; }
        const auto frame = window_frame();
        if (contains(frame.close, x, y)) { return UI_DOCK_PART_WINDOW_CLOSE; }
        if (contains(frame.maximize, x, y)) { return UI_DOCK_PART_WINDOW_MAXIMIZE; }
        if (contains(frame.minimize, x, y)) { return UI_DOCK_PART_WINDOW_MINIMIZE; }
        return contains(frame.title, x, y) ? UI_DOCK_PART_WINDOW_TITLE : UI_DOCK_PART_COUNT;
    }
    void caption_hover(int x, int y)
    {
        const auto next = caption_part(x, y);
        if (chrome_hover != next) { chrome_hover = next; dirty = true; }
    }
    void caption_action(ui_dock_part part)
    {
        if (part == UI_DOCK_PART_WINDOW_CLOSE) {
            if (!dock_window || !workspace || docking::close_window(*workspace, dock_window)) { closing = true; }
        }
#ifdef _WIN32
        else if (part == UI_DOCK_PART_WINDOW_MINIMIZE) { ShowWindow(window, SW_MINIMIZE); }
        else if (part == UI_DOCK_PART_WINDOW_MAXIMIZE) { ShowWindow(window, IsZoomed(window) ? SW_RESTORE : SW_MAXIMIZE); }
#elif defined(SUNFLECK_X11)
        else if (part == UI_DOCK_PART_WINDOW_MINIMIZE) { XIconifyWindow(display, window, DefaultScreen(display)); XFlush(display); }
        else if (part == UI_DOCK_PART_WINDOW_MAXIMIZE) {
            const auto state = XInternAtom(display, "_NET_WM_STATE", False);
            XEvent event{}; event.xclient.type = ClientMessage; event.xclient.window = window; event.xclient.message_type = state; event.xclient.format = 32;
            event.xclient.data.l[0] = 2;
            event.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
            event.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False); event.xclient.data.l[3] = 1;
            Atom actual = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* data = nullptr;
            const auto wm = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
            const bool managed = XGetWindowProperty(display, DefaultRootWindow(display), wm, 0, 1, False, XA_WINDOW, &actual, &format, &count, &remaining, &data) == Success && count;
            if (data) { XFree(data); }
            if (managed) { XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event); }
            else {
                if (!maximized) {
                    Window child = 0; int x = 0, y = 0;
                    XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child);
                    chrome_restore = {x, y, width, height}; XMoveResizeWindow(display, window, 0, 0, DisplayWidth(display, DefaultScreen(display)), DisplayHeight(display, DefaultScreen(display)));
                }
                else { XMoveResizeWindow(display, window, chrome_restore.x, chrome_restore.y, chrome_restore.width, chrome_restore.height); }
                maximized = !maximized;
            }
            dirty = true; XFlush(display);
        }
#endif
    }
#if defined(SUNFLECK_X11)
    bool begin_frame_drag(int x, int y, Time time)
    {
        if (!workspace) { return false; }
        const auto frame = window_frame();
        int direction = -1;
        if (!maximized) {
            const bool left = x < frame.resize, right = x >= width - frame.resize;
            const bool top = y < frame.resize, bottom = y >= height - frame.resize;
            direction = top ? (left ? 0 : right ? 2 : 1) : bottom ? (left ? 6 : right ? 4 : 5) : left ? 7 : right ? 3 : -1;
        }
        if (direction < 0 && caption_part(x, y) != UI_DOCK_PART_WINDOW_TITLE) { return false; }
        if (direction < 0) {
            if (time && chrome_click_time && time - chrome_click_time < 400) { chrome_click_time = 0; caption_action(UI_DOCK_PART_WINDOW_MAXIMIZE); return true; }
            chrome_click_time = time; direction = 8;
        }
        if (maximized) { caption_action(UI_DOCK_PART_WINDOW_MAXIMIZE); XSync(display, False); }
        Window child = 0;
        XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &native_origin_x, &native_origin_y, &child);
        chrome_grab_x = native_origin_x + x; chrome_grab_y = native_origin_y + y;
        chrome_origin_width = width; chrome_origin_height = height; chrome_resize = direction;
        if (direction == 8 && dock_window && docking::begin_window_drag(*workspace, dock_window, x, y)) { native_dock_move = true; }
        XSetInputFocus(display, window, RevertToParent, CurrentTime);
        const auto atom = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
        Atom actual = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* data = nullptr;
        const auto supported = XInternAtom(display, "_NET_SUPPORTED", False);
        bool managed = false;
        if (XGetWindowProperty(display, DefaultRootWindow(display), supported, 0, 4096, False, XA_ATOM, &actual, &format, &count, &remaining, &data) == Success && format == 32 && count) {
            const auto* atoms = reinterpret_cast<const Atom*>(data); managed = std::find(atoms, atoms + count, atom) != atoms + count;
        }
        if (data) { XFree(data); }
        // A floating Dock caption needs pointer positions over other windows to
        // update the preview. The compositor's move grab can hide them from X11.
        if (managed && !native_dock_move) {
            XEvent event{}; event.xclient.type = ClientMessage; event.xclient.window = window; event.xclient.message_type = atom; event.xclient.format = 32;
            event.xclient.data.l[0] = chrome_grab_x; event.xclient.data.l[1] = chrome_grab_y;
            event.xclient.data.l[2] = direction; event.xclient.data.l[3] = Button1; event.xclient.data.l[4] = 1;
            XUngrabPointer(display, CurrentTime);
            XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        } else {
            chrome_manual = XGrabPointer(display, window, False, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime) == GrabSuccess;
            if (!chrome_manual) { native_dock_move = false; docking::cancel(*workspace); }
        }
        XFlush(display); return true;
    }
    void move_frame(int x, int y)
    {
        Window child = 0; int sx = 0, sy = 0;
        XTranslateCoordinates(display, window, DefaultRootWindow(display), x, y, &sx, &sy, &child);
        const int dx = sx - chrome_grab_x, dy = sy - chrome_grab_y;
        int left = native_origin_x, top = native_origin_y, right = left + chrome_origin_width, bottom = top + chrome_origin_height;
        if (chrome_resize == 8) { left += dx; right += dx; top += dy; bottom += dy; }
        else {
            const int min_width = 200 * static_cast<int>(dpi) / 96, min_height = 120 * static_cast<int>(dpi) / 96;
            if (chrome_resize == 0 || chrome_resize == 6 || chrome_resize == 7) { left = std::min(right - min_width, left + dx); }
            if (chrome_resize == 2 || chrome_resize == 3 || chrome_resize == 4) { right = std::max(left + min_width, right + dx); }
            if (chrome_resize == 0 || chrome_resize == 1 || chrome_resize == 2) { top = std::min(bottom - min_height, top + dy); }
            if (chrome_resize == 4 || chrome_resize == 5 || chrome_resize == 6) { bottom = std::max(top + min_height, bottom + dy); }
        }
        XMoveResizeWindow(display, window, left, top, right - left, bottom - top); XSync(display, False);
        if (native_dock_move && workspace && (dx || dy)) {
            workspace->drag.active = true;
            XTranslateCoordinates(display, DefaultRootWindow(display), window, sx, sy, &x, &y, &child);
            (void)docking::pointer(*workspace, dock_window, UI_POINTER_MOVE, x, y);
        }
    }
    void cancel_frame_drag()
    {
        if (!native_dock_move && !chrome_manual) { return; }
        native_dock_move = false; native_move_cancelled = true;
        if (chrome_manual) { XUngrabPointer(display, CurrentTime); }
        chrome_manual = false; chrome_resize = -1;
        if (workspace) { docking::cancel(*workspace); }
        XMoveResizeWindow(display, window, native_origin_x, native_origin_y,
            chrome_origin_width ? chrome_origin_width : width, chrome_origin_height ? chrome_origin_height : height); XFlush(display);
    }
#endif

    void update_renderer_title()
    {
        if (!show_renderer) {
            return;
        }
#ifdef _WIN32
        const auto title = window_title + (gpu ? L" [GPU]" : L" [CPU]");
        SetWindowTextW(window, title.c_str());
#elif defined(SUNFLECK_X11)
        const auto title = window_title_utf8 + (gpu ? " [GPU]" : " [CPU]");
        XStoreName(display, window, title.c_str());
        const Atom property = XInternAtom(display, "_NET_WM_NAME", False);
        XChangeProperty(display, window, property, utf8, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title.data()),
                        static_cast<int>(title.size()));
#elif defined(SUNFLECK_MACOS)
        const auto title = window_title_utf8 + (gpu ? " [GPU]" : " [CPU]");
        window->title(title.c_str());
#endif
    }

    bool render([[maybe_unused]] bool notify_window = true)
    {
        binding_flush_focus(this);
        if (hosted_input) { host_text.observe(focused && focus && is_editor(focus->kind) && focus->is_enabled() ? focus : nullptr); }
        const auto palette = effective_palette();
        const auto background = theme_rgb(palette.colors[UI_THEME_BACKGROUND]);
        std::vector<FillRectCommand> commands{{0, 0, width, height, background}};
        drawing::CommandList display_list;
        auto* recording = gpu && gpu->records_commands() ? &display_list : nullptr;
        const bool paint_caption = workspace && (!hosted_input || dock_window)
#ifdef SUNFLECK_MACOS
            && !window
#endif
            ;
        if (root) {
            const auto floating_padding = theme_resource.metrics.find("dock.floating_padding");
            const unsigned logical_padding = dock_window
                ? (floating_padding == theme_resource.metrics.end() ? 4u : floating_padding->second)
                : gallery_style.window_padding;
            const int padding = (logical_padding * dpi + 48) / 96;
            const int caption = paint_caption ? window_frame().height : 0;
            const auto clear = [&](const auto& self, ui_node& node) -> void {
                // Content in another Dock window has its own visible geometry.
                if (!node.visual_groups.empty()) {
                    auto* control = &node;
                    if (node.component_scope && fleck::native_template(node.component_scope->declaration))
                        control = node.component_scope->host;
                    const char* input_state = !control->is_enabled() ? "Disabled" : hovered_control != control ? "Normal" :
                        pressed_control == control ? "Pressed" : "MouseOver";
                    update_host_state(node, "CommonStates", input_state, node.common_input_state, "Normal");
                    if (devtools::boolean_target(control->kind))
                        update_host_state(node, "CheckStates", control->value ? "Checked" : "Unchecked", node.check_state);
                    bool focus_within = focus == control;
                    if (!focus_within && control->component && fleck::native_template(control->component->declaration)) {
                        for (auto* part = focus; part; part = part->inheritance_parent()) {
                            if (part == control) { focus_within = true; break; }
                        }
                    }
                    focus_within = focus_within && (focused || (headless && !hosted_input));
                    const bool has_focus = keyboard_focus && focus_within;
                    update_host_state(node, "FocusStates", has_focus ? "Focused" : "Unfocused", node.focus_state);
                    if (node.visual_groups.contains("ValidationStates")) {
                        const bool invalid = std::any_of(control->bindings.begin(), control->bindings.end(),
                            [](const auto& binding) { return binding.invalid; });
                        const char* state = !invalid ? "Valid" : focus_within ?
                            "InvalidFocused" : "InvalidUnfocused";
                        update_host_state(node, "ValidationStates", state, node.validation_state);
                    }
                    if (control->kind == UI_LIST_ITEM)
                        update_host_state(node, "SelectionStates", control->is_selected_item() ? "Selected" : "Unselected", node.selection_state);
                }
                if (node.kind == UI_DOCK_HOST) { node.visible = {}; return; }
                node.visible = {};
                for (auto* child : node.children) { self(self, *child); }
            };
            clear(clear, *root);
            render_tree_with_pick_order(*root, {padding, padding + caption, width - padding, height - padding},
                        palette, theme_shape, dpi, focused ? focus : nullptr, keyboard_focus, caret_visible,
                        hovered_control, pressed_control == hovered_control ? pressed_control : nullptr,
                        commands, interactive, scrollable, &overlays, (debug_picking || debug_highlight || track_drop_targets) ? &debug_paint_order : nullptr,recording,
                        [&](ui_node& content) { clear(clear, content); });
        } else {
            commands.push_back(button());
        }
        if (paint_caption) {
            const auto title = window_title_utf8 + (show_renderer ? (gpu ? " [GPU]" : " [CPU]") : "");
            docking::paint_window_frame(*workspace, width, height, dpi, title.c_str(), focused, maximized, chrome_hover, chrome_pressed, commands);
        }
        const auto highlight_node = [&](const ui_node& node, bool fill) {
            const auto first = commands.size();
            const bool transformed = !affine::trivial(node.world_transform);
            const layout::Rect box = transformed ? layout::Rect(node.layout_cache.arranged) : node.bounds;
            const auto clip = transformed ? node.local_hit : node.visible;
            if (fill) drawing::rounded(commands, box, clip, {32, 133, 250, 45}, 0);
            drawing::rounded(commands, box, clip, {32, 133, 250, 255}, 0,
                             static_cast<int>(std::max(1u, (dpi + 48) / 96) * 2));
            if (transformed) drawing::transform(commands, first, node.world_transform, node.visible);
        };
        if (debug_highlight && std::chrono::steady_clock::now() < debug_highlight_until) {
            for (const auto* node : debug_paint_order) {
                if (node->debug_id != debug_highlight) { continue; }
                const auto clip = node->visible;
                if (clip.right > clip.left && clip.bottom > clip.top) {
                    highlight_node(*node, true);
                }
                break;
            }
        }
        if (debug_picking && debug_hover) {
            const auto thickness = std::max(1u, (dpi + 48) / 96) * 2;
            const auto found = std::find_if(debug_paint_order.begin(), debug_paint_order.end(),
                [&](const auto* node) { return node->debug_id == debug_hover && !affine::trivial(node->world_transform); });
            if (found != debug_paint_order.end()) highlight_node(**found, false);
            else drawing::rounded(commands, debug_hover_bounds, debug_hover_bounds, {32, 133, 250, 255}, 0, static_cast<int>(thickness));
        }
#ifdef _WIN32
        if (window) {
            place_ime();
            animated_spinner = std::any_of(nodes.begin(),nodes.end(),[&](const auto& n){
                return n->kind == UI_SPINNER && owns_input(n.get()) && n->visible.right > n->visible.left && n->visible.bottom > n->visible.top;
            });
        }
#endif
#if defined(SUNFLECK_X11)
        sync_xim_focus();
#endif
        if (gallery && !root) {
            gallery_scroll = std::min(gallery_scroll, gallery_scroll_limit(gallery_style, height, dpi));
            append_gallery(commands, palette, width, height, dpi, gallery_scroll, gallery_image);
        }
        if (headless && hosted_command_rendering) {
            hosted_wire_commands.clear();
            hosted_wire_commands.reserve(commands.size());
            for (const auto& c : commands)
                hosted_wire_commands.push_back({c.left,c.top,c.right,c.bottom,c.color});
            hosted_commands = std::move(commands);
            pixels.clear(); // CPU fallback is generated only when explicitly read.
            dirty = false;
            return true;
        }
        hosted_commands.clear();
        if (gpu) {
            dirty = false;
            if (recording) recording->flush(commands);
            if (gpu->present_frame(width,height,commands,display_list)) {
                return true;
            }
            gpu.reset();
            if (renderer != UI_AUTO && renderer != UI_ANGLE) {
                return false;
            }
            update_renderer_title();
            std::fprintf(stderr, "Sunfleck: switching to CPU after GPU presentation failure\n");
        }
        bool rasterized = false;
#ifdef _WIN32
        if (window) {
            rasterize(dib, width, height, commands, PixelOrder::Bgra);
            rasterized = true;
            if (notify_window) {
                // WM_PAINT should present this DIB, not render the same frame again.
                dirty = false;
                InvalidateRect(window, nullptr, FALSE);
                UpdateWindow(window);
            }
        }
#endif
#if defined(SUNFLECK_X11)
        if (display) {
            auto* image = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)),
                DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, nullptr,
                width, height, 32, 0);
            if (!image) {
                throw std::bad_alloc{};
            }
            const bool native_bgra = image->depth == 24 && image->bits_per_pixel == 32 &&
                image->byte_order == LSBFirst &&
                image->bytes_per_line == width * 4 && image->red_mask == 0xff0000 &&
                image->green_mask == 0xff00 && image->blue_mask == 0xff;
            const auto destroy = [native_bgra](XImage* value) {
                if (native_bgra) {
                    value->data = nullptr;
                }
                XDestroyImage(value);
            };
            std::unique_ptr<XImage, decltype(destroy)> guard(image, destroy);
            if (native_bgra) {
                rasterize(pixels, width, height, commands, PixelOrder::Bgra);
                image->data = reinterpret_cast<char*>(pixels.data());
            } else {
                rasterize(pixels, width, height, commands);
                image->data = static_cast<char*>(std::calloc(image->bytes_per_line, height));
                if (!image->data) {
                    throw std::bad_alloc{};
                }
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const auto* pixel = pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
                        XPutPixel(image, x, y, x11_channel(pixel[0], image->red_mask) |
                            x11_channel(pixel[1], image->green_mask) | x11_channel(pixel[2], image->blue_mask));
                    }
                }
            }
            rasterized = true;
            XPutImage(display, window, DefaultGC(display, DefaultScreen(display)), image,
                      0, 0, 0, 0, width, height);
            XSync(display, False);
        }
#endif
        if (!rasterized) {
            rasterize(pixels, width, height, commands);
        }
#ifdef SUNFLECK_MACOS
        if (window && notify_window) {
            window->present(width, height, pixels);
        }
#endif
        dirty = false;
        return true;
    }

    void close_window()
    {
        gpu.reset();
#ifdef _WIN32
        if (window) {
            DestroyWindow(window);
            window = nullptr;
        }
#elif defined(SUNFLECK_X11)
        close_x11();
#elif defined(SUNFLECK_MACOS)
        window.reset();
#endif
    }
};

namespace {
// Session slots own stable context addresses used by native window callbacks.
// Park the currently dispatched context before exchanging session ownership.
class SessionScope {
    ui_app& app;
    ui_window* previous;
    bool exchanged;
    void activate(ui_window* target)
    {
        std::swap(static_cast<ui_window_context&>(app), *app.current);
        std::swap(static_cast<ui_window_session&>(app), static_cast<ui_window_session&>(*app.active_session));
        std::swap(static_cast<ui_window_session&>(app), static_cast<ui_window_session&>(*target));
        app.active_session = target;
        std::swap(static_cast<ui_window_context&>(app), *app.current);
        app.application = &app;
        app.session = target;
        app.current->application = &app;
        app.current->session = target;
    }
public:
    SessionScope(ui_app& owner, ui_window* target) : app(owner), previous(owner.active_session), exchanged(target && target != previous)
    {
        if (exchanged) { activate(target); }
    }
    ~SessionScope() { if (exchanged) { activate(previous); } }
};

template<class F> void for_each_session(ui_app& app, F operation)
{
    { SessionScope scope(app, &app.primary_window); operation(); }
    for (const auto& window : app.windows) { SessionScope scope(app, window.get()); operation(); }
}

class WindowScope {
    ui_app& app;
    SessionScope session_scope;
    ui_window_context* previous;
    bool exchanged;
public:
    WindowScope(ui_app& owner, ui_window_context* target) : app(owner), session_scope(owner, target->session), previous(owner.current), exchanged(target != previous)
    {
        if (exchanged) {
            std::swap(static_cast<ui_window_context&>(app), *previous);
            std::swap(static_cast<ui_window_context&>(app), *target);
            app.current = target;
        }
    }
    ~WindowScope()
    {
        if (exchanged) {
            std::swap(static_cast<ui_window_context&>(app), *app.current);
            std::swap(static_cast<ui_window_context&>(app), *previous);
            app.current = previous;
        }
    }
};
ui_window_context& main_window(ui_app& app)
{
    return app.current == app.main_state.get() ? static_cast<ui_window_context&>(app) : *app.main_state;
}

#ifdef SUNFLECK_MACOS
bool macos_event(void* user, const macos::Event& event, std::string* output)
{
    auto* context = static_cast<ui_window_context*>(user);
    auto& app = *context->application;
    WindowScope scope(app, context);
    struct CallbackScope {
        ui_app& app;
        ~CallbackScope() { --app.window_callback_depth; }
    } callback_scope{app};
    ++app.window_callback_depth;
    if (app.closing || app.render_failed) {
        return false;
    }
    using macos::Event;
    try {
        switch (event.kind) {
        case Event::resize:
            app.width = event.x;
            app.height = event.y;
            app.dpi = static_cast<unsigned>(event.value);
            app.dirty = true;
            return true;
        case Event::close:
            app.caption_action(UI_DOCK_PART_WINDOW_CLOSE);
            return true;
        case Event::focus:
            app.focused = event.value != 0;
            if (!app.focused && app.host_pointer_down) {
                app.cancel_pointer(app.pointer_x, app.pointer_y);
            }
            app.reset_caret();
            return true;
        default:
            break;
        }
        if (!app.opened || app.width <= 0 || app.height <= 0) {
            return false;
        }
        if (app.dirty && !app.render()) {
            app.render_failed = true;
            return false;
        }
        const auto x = event.x, y = event.y;
        if (event.kind == Event::pointer) {
            app.hover(x, y);
            if (event.value == UI_POINTER_DOWN) {
                app.host_pointer_down = true;
                app.pressed_control = app.hovered_control;
                app.begin_drag(x, y);
                app.dirty = true;
            } else if (event.value == UI_POINTER_MOVE) {
                if (!app.pointer_handled(UI_POINTER_MOVE, x, y) && app.dragged_control) {
                    app.update_drag_value(*app.dragged_control, x, y);
                }
            } else if (std::exchange(app.host_pointer_down, false)) {
                auto* pressed = std::exchange(app.pressed_control, nullptr);
                app.dirty = true;
                if (!app.end_pointer_event(x, y)) {
                    if (app.dragged_control) {
                        app.finish_drag(x, y);
                    } else if ((app.workspace && app.workspace->drag.prepared) ||
                               (pressed && pressed == app.hovered_control) || !app.root) {
                        app.click(x, y);
                    }
                }
            }
            return true;
        }
        if (event.kind == Event::context_menu) {
            app.context_menu(x, y);
            return true;
        }
        if (event.kind == Event::scroll) {
            app.scroll_pixels(event.value, x, y, (event.modifiers & UI_KEY_SHIFT) != 0);
            return true;
        }

        auto* editor = app.focused && app.focus && is_editor(app.focus->kind) &&
                       app.focus->is_enabled() ? app.focus : nullptr;
        if (event.kind == Event::copy || event.kind == Event::cut) {
            if (!editor || editor->kind == UI_PASSWORD_BOX) {
                return false;
            }
            const auto [first, last] = editor->selection();
            if (first == last) {
                return false;
            }
            *output = utf8_text::encode(std::wstring_view(editor->text).substr(first, last - first));
            if (event.kind == Event::cut) {
                app.erase_selection(*editor);
                app.publish_editor_text(*editor);
                app.reset_caret();
            }
            return true;
        }
        const auto key = event.value;
        if (event.kind == Event::key) {
            const auto gesture = key >= 'a' && key <= 'z' ? key - 'a' + 'A' : key;
            if (fleck::valid_gesture(gesture, event.modifiers) && app.key_gesture(gesture, event.modifiers)) {
                return true;
            }
            const bool shift = (event.modifiers & UI_KEY_SHIFT) != 0;
            const bool control = (event.modifiers & UI_KEY_CTRL) != 0;
            if (editor && gesture == 'A' && control) {
                editor->select_all = true;
                app.reset_caret();
                return true;
            }
            if (key == '\t') {
                app.key(true, shift, 0);
                return true;
            }
            if (editor && editor->kind == UI_TEXT_BOX && editor->collection_item_id &&
                (key == '\r' || key == 27)) {
                binding_app_error(&app, ui_data_grid_finish_edit(editor, key == '\r'));
                return true;
            }
            if (key == 27) {
                app.cancel_pointer(app.pointer_x, app.pointer_y);
                return true;
            }
            if (key >= macos::left && key <= macos::forward_delete) {
                const auto edit_key = static_cast<ui_app::EditKey>(key - macos::left);
                if (!app.editor_key(edit_key, shift, control) && key <= macos::down) {
                    app.key(false, false, key == macos::left || key == macos::up ? -5 : 5, true, key >= macos::up);
                }
                return true;
            }
            if (!editor && (key == ' ' || key == '\r')) {
                app.key(false, false, 0);
                return true;
            }
            if (key != '\r' && key != 127 && key != '\b') {
                return false;
            }
        }
        if (!editor) {
            return false;
        }
        if (event.kind == Event::key && key != '\r') {
            if (!app.erase_selection(*editor) && editor->caret) {
                const auto previous = app.previous_character(*editor, editor->caret);
                editor->text.erase(previous, editor->caret - previous);
                editor->caret = previous;
            }
        } else {
            auto value = event.kind == Event::text ? utf8_text::decode(event.characters) : std::wstring(1, L'\n');
            if (!app.allows_line_breaks(*editor)) {
                value.erase(std::find_if(value.begin(), value.end(), [](wchar_t ch) { return ch < L' '; }), value.end());
            }
            if (value.empty()) {
                return true;
            }
            app.erase_selection(*editor);
            value.resize(std::min(value.size(), size_t(4096) - std::min(size_t(4096), editor->text.size())));
            editor->text.insert(editor->caret, value);
            editor->caret += value.size();
        }
        editor->clear_selection();
        editor->caret_trailing = false;
        app.publish_editor_text(*editor);
        app.reset_caret();
        return true;
    } catch (...) {
        app.render_failed = true;
        return false;
    }
}
#endif
}

namespace {
ui_result check(const ui_app* app)
{
    if (!app) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->owner != std::this_thread::get_id()) { return UI_ERROR_WRONG_THREAD; }
    return app->drawing || (app->workspace && app->workspace->painting) ? UI_ERROR_INVALID_STATE : UI_OK;
}

std::wstring decode_ui_text(const char* utf8)
{
#ifdef _WIN32
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (size <= 0 || size > 4097) {
        throw std::range_error("Invalid UTF-8 or text too long");
    }
    std::wstring text(size, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, text.data(), size)) {
        throw std::range_error("Invalid UTF-8");
    }
    text.pop_back();
    return text;
#else
    auto text = utf8_text::decode(utf8);
    if (text.size() > 4096) {
        throw std::range_error("Text too long");
    }
    return text;
#endif
}

bool indexed(ui_node_kind kind);
bool numeric(ui_node_kind kind);
bool has_value(ui_node_kind kind);

bool accepts_child(const ui_node& parent, ui_node_kind child)
{
    return devtools::accepts_child(parent.kind, parent.children.size(), child);
}

struct FleckAnchor {
    ui_node* node;
    const std::string* id;
    std::size_t offset;
};

namespace {
using ResourceValues = std::map<std::string,BindingValue>;

// During an update, substitute the proposed dictionary without publishing it.
struct ResourceChange {
    const ResourceValues* current = nullptr;
    const ResourceValues* proposed = nullptr;
    const ui_node* attaching = nullptr;
    ui_node* parent = nullptr;
    const BindingValue* find(const ResourceValues& dictionary, const std::string& key) const {
        const auto& values = current == &dictionary ? *proposed : dictionary;
        auto found = values.find(key);
        return found == values.end() ? nullptr : &found->second;
    }
};
ui_result apply_resource_value(ui_node& node, ui_property property, ui_value_precedence layer, const std::optional<BindingValue>& value)
{
    struct Guard {
        ui_node& node;
        bool previous;
        ~Guard() { node.applying_resource = previous; }
    } guard{node,std::exchange(node.applying_resource,true)};
    return value ? ui_node_set_property_layer(&node,property,layer,&value->value)
                 : ui_node_clear_property_layer(&node,property,layer);
}
ui_result resolve_style_resource(ui_node& node, ui_property property, const std::string& key,
                                 const ResourceChange& change, std::optional<BindingValue>& result)
{
    for (auto* owner = &node; owner; owner = owner == change.attaching && !owner->content_owner ? change.parent : owner->inheritance_parent()) {
        if (!owner->resources) continue;
        if (auto value = change.find(owner->resources->overrides,key)) { result = *value; return UI_OK; }
        auto found = owner->resources->declarations.find(key);
        if (found != owner->resources->declarations.end()) {
            BindingValue parsed;
            if (!fleck::style_value(node.kind,properties::name(property),found->second,property,parsed))
                return UI_ERROR_INVALID_ARGUMENT;
            result = std::move(parsed);
            return UI_OK;
        }
    }
    auto value = change.find(node.owner->application_resources,key);
    if (!value) value = change.find(node.owner->theme_values,key);
    if (value) result = *value;
    return UI_OK;
}
struct ResourceUpdate {
    ui_node* node;
    ui_property property;
    ui_value_precedence layer;
    std::optional<BindingValue> previous, selected;
};
ui_result prepare_resource_updates(ui_node& node, const ResourceChange& change, std::vector<ResourceUpdate>& changes,
                                   const std::string* changed_key = nullptr)
{
    for (const auto& [property,layer,key] : node.resource_references) {
        if (changed_key && *changed_key != key) continue;
        ResourceUpdate update{&node,property,layer};
        if (auto status = resolve_style_resource(node,property,key,{},update.previous); status != UI_OK) return status;
        if (auto status = resolve_style_resource(node,property,key,change,update.selected); status != UI_OK) return status;
        if (!update.previous && !update.selected) continue;
        if (update.previous && update.selected && update.previous->equals(*update.selected)) continue;
        if (update.selected) {
            if (auto status = properties::validate(node,property,update.selected->value); status != UI_OK) return status;
        }
        changes.push_back(std::move(update));
    }
    return UI_OK;
}
void undo_resource_updates(const std::vector<ResourceUpdate>& changes, size_t applied)
{
    while (applied > 0) {
        const auto& update = changes[--applied];
        apply_resource_value(*update.node,update.property,update.layer,update.previous);
    }
}
ui_result apply_resource_updates(const std::vector<ResourceUpdate>& changes)
{
    for (size_t i = 0; i < changes.size(); ++i) {
        const auto& update = changes[i];
        if (auto status = apply_resource_value(*update.node,update.property,update.layer,update.selected); status != UI_OK) {
            undo_resource_updates(changes,i);
            return status;
        }
    }
    return UI_OK;
}

}
ui_node* stage_fleck_node(ui_app* app, const fleck::Element& element, ui_node* parent,
                          std::vector<std::unique_ptr<ui_node>>& staged, std::vector<ui_node*>& named,
                          std::vector<FleckAnchor>& anchors, std::size_t& offset, ui_model* model = nullptr,
                          std::shared_ptr<ui_component> scope = {})
{
    offset = element.offset;
    auto node = std::make_unique<ui_node>();
    node->debug_id = devtools::next_id(app);
    node->location = element.location;
    node->location.source_uri = element.source_uri;
    node->owner = app;
    node->window_owner = app->active_session;
    node->parent = parent;
    if (element.projected && scope) {
        node->content_owner = scope->host;
        scope = scope->host->component_scope;
    }
    node->component_scope = scope;
    if (element.kind == UI_CONTENT_PRESENTER && scope && fleck::native_template(scope->declaration))
        node->presented_button = scope->host;
    node->source_uri = element.source_uri;
    node->scope_path = scope ? scope->host->scope_path + "/" + (scope->host->fleck_id.empty() ? "@" + std::to_string(scope->host->debug_id) : scope->host->fleck_id) + ":" + scope->type->key.first + ":" + scope->type->key.second : "";
    node->kind = element.kind;
    node->enabled = element.enabled;
    const bool template_root = scope && scope->host == parent && fleck::native_template(scope->declaration);
    if (parent && !(template_root ? devtools::accepts_child(UI_PANEL, 0, node->kind) : accepts_child(*parent, node->kind))) {
        throw std::range_error("Invalid child for parent");
    }
    if (element.has_value && (!has_value(node->kind) || (indexed(node->kind)
        ? element.value >= element.children.size()
        : element.value > (numeric(node->kind) ? 100u : 1u)))) {
        throw std::range_error("Invalid initial value for component");
    }
    node->text = decode_ui_text(element.text.c_str());
    if (is_editor(node->kind) && node->kind != UI_TEXT_AREA &&
        node->text.find_first_of(L"\r\n") != std::wstring::npos) {
        throw std::range_error("Single-line editor cannot contain a line break");
    }
    node->caret = node->text.size();
    node->value = element.has_value ? element.value : node->kind == UI_SPLIT_VIEW || node->kind == UI_DOCK_SPLIT ? 50 : 0;
    node->fleck_value = node->value;
    node->dock_title = element.text;
    node->dock_capabilities = element.dock_capabilities;
    node->dock_icon = element.dock_icon;
    (void)utf8_text::decode(node->dock_icon);
    node->dock_min_width = element.properties.contains("MinWidth") || element.layout.min_width > 0 ? std::max(32.0,element.layout.min_width) : element.dock_min_width;
    node->dock_min_height = element.properties.contains("MinHeight") || element.layout.min_height > 0 ? std::max(32.0,element.layout.min_height) : element.dock_min_height;
    node->expanded = node->kind == UI_TREE_ITEM && node->value != 0;
    node->height = element.height;
    node->layout = element.layout;
    node->weight = element.weight;
    node->dock = element.dock;
    node->spacing = element.spacing;
    node->padding = element.padding;
    if (element.kind==UI_DATA_GRID) {
        node->table = std::make_unique<DataGridState>();
        for (unsigned i=0; i<512; ++i) {
            const auto suffix = "."+std::to_string(i);
            auto path = element.properties.find(".ColumnPath"+suffix);
            if (path==element.properties.end()) break;
            DataGridColumn column;
            column.path = path->second;
            auto header = element.properties.find(".ColumnHeader"+suffix);
            column.header = utf8_text::decode(header==element.properties.end() ? column.path : header->second);
            if (auto width=element.properties.find(".ColumnWidth"+suffix);width!=element.properties.end())
                if (!numeric_literal::real_literal(width->second,column.width)) throw std::range_error("Invalid column width");
            column.editable = !element.properties.contains(".ColumnReadOnly"+suffix);
            node->table->columns.push_back(std::move(column));
        }
        node->table->automatic = node->table->columns.empty();
    }
    if (element.properties.contains("DataContext")) node->has_local_data_context = true;
    for (const auto& [name,text] : element.properties) {
        if (drawing::shape_kind(node->kind) && (name=="Fill" || name=="Stroke")) {
            BindingValue value;
            if (binding_parse_value(text,UI_VALUE_COLOR,value)!=UI_OK) throw std::range_error("Invalid shape color");
            auto brush=std::make_shared<drawing::Brush>(); brush->color=uint32_t(value.value.integer);
            node->brushes[name=="Fill" ? UI_BRUSH_BACKGROUND : UI_BRUSH_OUTLINE]=std::move(brush);
            continue;
        }
        if (drawing::shape_kind(node->kind) && name=="StrokeThickness") {
            if (!numeric_literal::real_literal(text,node->stroke_thickness) || node->stroke_thickness<0 || node->stroke_thickness>4096)
                throw std::range_error("Invalid stroke thickness");
            continue;
        }
        if (node->kind==UI_PATH && name=="Data") {
            auto path=drawing::parse_path(text);
            if (!path) throw std::range_error("Invalid path");
            node->geometry=std::make_shared<drawing::Geometry>(std::move(*path)); continue;
        }
        if (name.starts_with(".Trigger.")) {
            ui_node::TriggerSetter trigger{};
            if (!fleck::trigger_setter(node->kind,std::string_view(name).substr(9),text,
                trigger.order,trigger.conditions,trigger.property,trigger.value) ||
                properties::validate(*node,trigger.property,trigger.value.value) != UI_OK)
                throw std::range_error("Invalid Trigger Setter");
            if (trigger.conditions.empty()) {
                const auto index = std::to_string(trigger.order);
                auto path = element.properties.find(".DataPath."+index);
                auto value = element.properties.find(".DataValue."+index);
                if (path == element.properties.end() || !binding_path(path->second) || value == element.properties.end())
                    throw std::range_error("Invalid DataTrigger condition");
                trigger.data_path = path->second;
                trigger.data_value = value->second;
            }
            node->triggers.push_back(std::move(trigger));
            continue;
        }
        if (name.starts_with(".Resource.")) {
            if (!node->resources) node->resources = std::make_unique<ui_node::Resources>();
            node->resources->declarations.emplace(name.substr(10),text);
            continue;
        }
        if (name == "DragText") { node->drag_text = text; continue; }
        if (name == "AllowDrop") { node->allow_text_drop = text=="true"; continue; }
        if (name == ".StyleKey") { node->style_key = text; continue; }
        if (auto key = fleck::pointer_binding(name)) { node->pointer_commands.emplace(*key,text); continue; }
        if (auto key = fleck::key_binding_code(name)) { node->input_bindings[*key] = {text, false}; continue; }
        if (name.starts_with(".CommandBinding.")) { node->command_paths.emplace(name.substr(16),text); continue; }
        if (name == ".RoutedCommand") { node->routed_command = text; continue; }
        if (name.starts_with(".State.")) {
            const auto split = name.find('/',7);
            if (split == name.npos || ui_node_register_visual_state(node.get(), name.substr(7,split-7).c_str(),
                name.substr(split+1).c_str(), text.empty() ? nullptr : text.c_str()) != UI_OK)
                throw std::range_error("Invalid VisualState declaration");
            continue;
        }
        if (name.starts_with(".Conversion.")) {
            fleck::Conversion conversion;
            auto path=name.substr(12);
            if (path!=fleck::conversion_path(text) || !fleck::parse_conversion(text,conversion))
                throw std::range_error("Invalid binding converter or input paths");
            conversion.inputs.insert(conversion.inputs.begin(),std::move(conversion.converter));
            node->binding_conversions.emplace(path,std::move(conversion.inputs));
            continue;
        }
        if (name==".ChildrenPath") { node->tree_children_path=text; continue; }
        if (name==".TreeLabelPath") { node->tree_label_path=text; continue; }
        if (name==".ItemTemplate" || name.starts_with(".ItemTemplateKey.")) {
            fleck::Page template_page;
            ui_fleck_diagnostic diagnostic{};
            if (!fleck::parse(text,template_page,diagnostic) || template_page.document_kind != "Window")
                throw std::range_error("Invalid DataTemplate visual tree");
            components::expand(app->component_registry,template_page.root);
            auto visual = std::make_shared<fleck::Element>(std::move(template_page.root));
            if (name==".ItemTemplate") node->item_template = std::move(visual);
            else node->item_templates_by_label.emplace(utf8_text::decode(name.substr(17)),std::move(visual));
            continue;
        }
        if (name==".Storyboard") {
            if (!fleck::read_storyboard(text,node->kind,node->storyboard)) throw std::range_error("Invalid Storyboard");
            continue;
        }
        if (name==".Effect") {
            if (!fleck::read_drawing(text,nullptr,&node->effect)) throw std::range_error("Invalid effect");
            continue;
        }
        if (name==".OpacityMask" || name==".Background" || name==".BorderBrush" || name==".Fill" || name==".Stroke") {
            auto brush=std::make_shared<drawing::Brush>();
            if (!fleck::read_drawing(text,brush.get(),nullptr)) throw std::range_error("Invalid brush");
            node->brushes[name==".OpacityMask" ? UI_BRUSH_OPACITY_MASK : (name==".Background" || name==".Fill") ? UI_BRUSH_BACKGROUND : UI_BRUSH_OUTLINE]=std::move(brush);
            continue;
        }
        if (name == "Orientation" && (node->kind == UI_STACK_PANEL || node->kind == UI_WRAP_PANEL || node->kind == UI_SEPARATOR)) {
            node->orientation = text=="Vertical" ? UI_VERTICAL : UI_HORIZONTAL;
            continue;
        }
        if (name == "LayoutTransform" || name == "RenderTransform" || name == "RenderTransformOrigin") {
            BindingValue parsed;
            const bool matrix = name != "RenderTransformOrigin";
            if (binding_parse_value(text,matrix ? UI_VALUE_MATRIX : UI_VALUE_POINT,parsed) != UI_OK)
                throw std::range_error("Invalid transform");
            const auto& v = parsed.geometry;
            if (name == "LayoutTransform") node->layout_transform = {v[0],v[1],v[2],v[3],v[4],v[5]};
            else if (matrix) node->render_transform = {v[0],v[1],v[2],v[3],v[4],v[5]};
            else node->transform_origin = {v[0],v[1]};
            properties::written(*node,name == "LayoutTransform" ? UI_PROPERTY_LAYOUT_TRANSFORM : matrix ? UI_PROPERTY_RENDER_TRANSFORM : UI_PROPERTY_RENDER_TRANSFORM_ORIGIN);
            continue;
        }
        if (name == "Stretch") {
            ui_stretch value{};
            if (!visual_layout::parse_stretch(text,value)) throw std::range_error("Invalid stretch");
            node->stretch = value; continue;
        }
        if (name == "Opacity") {
            ui_binding_value parsed{};
            if (ui_value_parse(UI_VALUE_DOUBLE,text.c_str(),&parsed,nullptr,0) != UI_OK || parsed.number < 0 || parsed.number > 1)
                throw std::range_error("Invalid opacity");
            node->opacity = parsed.number; properties::written(*node,UI_PROPERTY_OPACITY); continue;
        }
        if (visual_layout::scroll_policy_name(name)) {
            auto& axis = name == "HorizontalScrollBarVisibility" ? node->horizontal_scroll : node->vertical_scroll;
            if (!visual_layout::parse_scroll_policy(text,axis)) throw std::range_error("Invalid scroll policy");
            continue;
        }
        if (visual_layout::canvas_coordinate(name)) {
            double& coordinate = name == "Canvas.Left" ? node->canvas_left : node->canvas_top;
            if (!visual_layout::parse_coordinate(text,coordinate)) throw std::range_error("Invalid Canvas coordinate");
            continue;
        }
        if (name == "Panel.ZIndex") {
            if (!grid_layout::parse_z_index(text,node->z_index)) throw std::range_error("Invalid Panel.ZIndex");
            continue;
        }
        if (grid_layout::attribute(name)) {
            if (!grid_layout::assign(node->grid,node->kind==UI_GRID,name,text)) throw std::range_error("Invalid grid layout");
            continue;
        }
        ui_property property; ui_binding_value value{};
        if (!properties::find(name,property)) continue;
        if (property == UI_PROPERTY_SOURCE) {
            (void)utf8_text::decode(text);
            node->image_source = text;
            properties::written(*node, property);
            continue;
        }
        if (text_format::property(property)) {
            if (!text_format::parse(property,text,value)) throw std::range_error("Invalid font property");
            text_format::set(node->font,property,value); properties::written(*node,property); continue;
        }
        if (!visual_state::property(property)) continue;
        if (!visual_state::parse(property,text,value)) throw std::range_error("Invalid visual state");
        if (property == UI_PROPERTY_VISIBILITY) node->visibility = ui_visibility(value.integer);
        else if (property == UI_PROPERTY_IS_FOCUS_SCOPE) { node->is_focus_scope = value.integer != 0; node->logical_focus_id = 0; }
        else if (property == UI_PROPERTY_IS_TAB_STOP) node->is_tab_stop = value.integer != 0;
        else if (property == UI_PROPERTY_TAB_INDEX) node->tab_index = static_cast<int>(value.integer);
        else if (property == UI_PROPERTY_CLIP_TO_BOUNDS) node->clip_to_bounds = value.integer != 0;
        else node->hit_test_visible = value.integer != 0;
        properties::written(*node,property);
    }
    if (node->kind == UI_BORDER) for (const auto& [name, text] : element.properties) {
        ui_property property;
        if (!properties::find(name, property) || !border::property(property)) continue;
        BindingValue value;
        if (binding_parse_value(text, border::kind(property), value) != UI_OK || !border::valid(property, value.value))
            throw std::range_error("Invalid Border property");
        border::set(node->border, property, value.value);
        properties::written(*node, property);
    }
    if (!parent && model && !node->has_local_data_context) {
        node->has_local_data_context = true;
        node->local_data_context = model;
    }
    properties::initialize(*node);
    const auto apply_setter = [&](ui_node& target, std::string_view name, const std::string& literal) {
        ui_property property;
        BindingValue value;
        if (!fleck::style_value(target.kind, name, literal, property, value) ||
            ui_node_set_property_layer(&target, property, UI_PRECEDENCE_STYLE, &value.value) != UI_OK)
            throw std::range_error("Invalid Style Setter for target");
    };
    // A resource expression occupies the local layer only while its key resolves.
    for (const auto& [name,key] : element.properties) {
        bool local = name.starts_with(".DynamicLocal.");
        if (!local && !name.starts_with(".DynamicStyle.")) continue;
        ui_property property;
        if (!fleck::dynamic_style_target(node->kind,std::string_view(name).substr(14),key,property))
            throw std::range_error("Invalid dynamic resource target");
        if (local) node->local_properties &= ~(uint64_t{1} << property);
        node->resource_references.push_back({property,local ? UI_PRECEDENCE_LOCAL : UI_PRECEDENCE_STYLE,key});
    }
    for (const auto& [name, literal] : element.properties) {
        if (name.starts_with(".Style.") && name != ".Style.Value")
            apply_setter(*node, std::string_view(name).substr(7), literal);
    }
    for (const auto& reference : node->resource_references) {
        std::optional<BindingValue> value;
        if (resolve_style_resource(*node,reference.property,reference.key,{},value) != UI_OK ||
            (value && apply_resource_value(*node,reference.property,reference.layer,value) != UI_OK))
            throw std::range_error("Incompatible dynamic resource");
    }
    for (const auto& track : node->storyboard)
        for (const auto& frame : track.timeline.frames)
            if (properties::validate(*node,track.property,frame.value.value) != UI_OK)
                throw std::range_error("Invalid Storyboard target value");
    if (element.has_value) properties::written(*node, UI_PROPERTY_VALUE);
    node->children.reserve(element.children.size());
    auto* result = node.get();
    if (!element.id.empty()) {
        if (std::any_of(named.begin(), named.end(), [&](const ui_node* item) {
                return item->fleck_id == element.id && item->component_scope == scope;
            })) {
            throw std::range_error("Duplicate Fleck Id");
        }
        node->fleck_id = element.id;
        named.push_back(result);
    }
    if (!element.anchor.empty()) {
        if (!is_popup(node->kind) && node->kind != UI_LABEL) {
            throw std::range_error("Anchor requires a label or popup");
        }
        anchors.push_back({result, &element.anchor, element.offset});
    }
    try { components::prepare(*node, element, model); }
    catch (const components::Error&) { throw; }
    catch (const std::range_error& error) { throw components::Error(element, error.what()); }
    staged.push_back(std::move(node));
    for (const auto& child : element.children) {
        result->children.push_back(stage_fleck_node(app, child, result, staged, named, anchors, offset, model, result->component ? result->component : scope));
    }
    // An indexed Value is validated against the staged children.
    if (auto value = element.properties.find(".Style.Value"); value != element.properties.end()) {
        offset = element.offset;
        apply_setter(*result, "Value", value->second);
        result->fleck_value = result->value;
    }
    std::sort(result->triggers.begin(),result->triggers.end(),[](const auto& left,const auto& right) { return left.order < right.order; });
    if (properties::update_triggers(*result) != UI_OK) throw std::range_error("Invalid active trigger");
    return result;
}

#if defined(_WIN32) || defined(SUNFLECK_X11)
static bool deliver_drop(ui_app& app, int x, int y, ui_drop_kind kind, const ui_drop_data* data)
{
    if (app.closing || app.drawing || (data && kind==UI_DROP_OBJECT && data->owner!=&app)) return false;
    const auto& candidates=app.track_drop_targets ? app.debug_paint_order : app.interactive;
    for(auto it=candidates.rbegin();it!=candidates.rend();++it) {
        auto* hit=*it;
        if (!app.hit_allowed(hit,x,y) || !app.contains(hit->visible,x,y)) continue;
        for(auto* n=hit;n;n=n->inheritance_parent()) {
            if (kind==UI_DROP_TEXT && n->allow_text_drop && !n->drop_handler) {
                if(data) {
                    app.replace_node_text(*n,utf8_text::decode(data->text));
                    app.publish_editor_text(*n);
                }
                return true;
            }
            if (n->drop_handler && (n->drop_kinds & kind)) {
                if (!data) return true;
                struct Restore {bool& flag; bool previous; ~Restore(){flag=previous;}} restore{app.dispatching,std::exchange(app.dispatching,true)};
                return n->drop_handler(n,data,n->drop_context)!=0;
            }
        }
        break;
    }
    return false;
}
#endif

#ifdef SUNFLECK_ATSPI
static bool x11_accessible_action(void* raw, uint64_t id, x11_accessibility::Action action)
{
    auto* context = static_cast<ui_window_context*>(raw);
    auto& app = *context->application;
    WindowScope scope(app, context);
    if (!app.window || app.closing || app.drawing || app.dispatching) { return false; }
    const auto found = std::find_if(app.interactive.begin(), app.interactive.end(), [id](const ui_node* node) {
        return node->debug_id == id && node->is_enabled() && node->is_visible();
    });
    if (found == app.interactive.end()) { return false; }
    auto& node = **found;
    if (action == x11_accessibility::Action::focus) {
        XSetInputFocus(app.display, app.window, RevertToParent, CurrentTime);
        XFlush(app.display);
        app.assign_focus(&node);
        app.keyboard_focus = true;
        app.dirty = true;
    } else {
        app.activate_node(node);
    }
    return true;
}

static void update_x11_accessibility(ui_app& app)
{
    if (!app.accessibility_window || !app.mapped) { return; }
    Window child = 0;
    int origin_x = 0, origin_y = 0;
    if (!XTranslateCoordinates(app.display, app.window, DefaultRootWindow(app.display), 0, 0, &origin_x, &origin_y, &child)) {
        return;
    }
    std::vector<x11_accessibility::Element> elements;
    elements.push_back({0, 0, {}, app.window_title_utf8, {}, origin_x, origin_y, app.width, app.height});
    elements.back().active = app.focused;
    const auto visit = [&](auto& self, ui_node& node, uint64_t parent) -> void {
        if (!node.is_visible()) { return; }
        const auto r = node.visible;
        if (app.owns_input(&node) && r.right > r.left && r.bottom > r.top &&
            (is_interactive(node.kind) || node.kind == UI_LABEL || node.kind == UI_TEXT_BLOCK ||
             node.kind == UI_PROGRESS_BAR || node.kind == UI_TREE_VIEW || node.kind == UI_TAB_VIEW || node.kind == UI_LIST_VIEW)) {
            auto label = is_editor(node.kind) ? node.fleck_id : utf8_text::encode(node.display_text());
            if (label.empty()) { label = node.fleck_id; }
            if (label.empty() && is_editor(node.kind)) { label = node.kind == UI_PASSWORD_BOX ? "Password" : "Text"; }
            const bool focusable = std::find(app.interactive.begin(), app.interactive.end(), &node) != app.interactive.end();
            const bool checked = (node.kind == UI_CHECK_BOX || node.kind == UI_SWITCH ||
                node.kind == UI_RADIO_BUTTON || node.kind == UI_TOGGLE_BUTTON) && node.value;
            elements.push_back({node.debug_id, parent, node.kind, std::move(label),
                is_editor(node.kind) && node.kind != UI_PASSWORD_BOX ? utf8_text::encode(node.display_text()) : std::string{},
                origin_x + r.left, origin_y + r.top, r.right - r.left, r.bottom - r.top,
                node.is_enabled(), focusable, app.focused && app.focus == &node, checked,
                is_editor(node.kind) && node.kind != UI_PASSWORD_BOX ? static_cast<int>(node.caret) : 0});
            parent = node.debug_id;
        }
        for (auto* child_node : node.children) { self(self, *child_node, parent); }
    };
    if (app.root) { visit(visit, *app.root, 0); }
    x11_accessibility::update(app.accessibility_window, std::move(elements));
}
#endif

#ifdef _WIN32
static bool receive_drop(void* owner, POINT p, ui_drop_kind kind, const ui_drop_data* data)
{
    auto* context = static_cast<ui_window_context*>(owner);
    auto& app = *context->application;
    WindowScope scope(app, context);
    return deliver_drop(app, p.x, p.y, kind, data);
}

static HRESULT access_window(void* context, accessibility::Request& request)
{
    using accessibility::Action;
    auto& app = *static_cast<ui_app*>(context);
    if (app.closing || app.drawing || !app.window) return UIA_E_ELEMENTNOTAVAILABLE;
    const bool inspect = request.action == Action::inspect;
    if (!inspect && !request.target) {
        if (request.action != Action::focus) return UIA_E_INVALIDOPERATION;
        SetFocus(app.window);
        return S_OK;
    }
    POINT origin{};
    ClientToScreen(app.window, &origin);
    if (inspect) {
        accessibility::Element window;
        GetWindowRect(app.window,&window.screen);
        window.name=app.window_title; window.enabled=true; window.focusable=true;
        window.focused=app.focused && !app.focus;
        request.elements.push_back(std::move(window));
    }
    ui_node* target=nullptr;
    const auto visit = [&](auto& self, ui_node& node, uint64_t parent) -> void {
        if (!node.is_visible()) return;
        const auto r=node.visible;
        if (r.right>r.left && r.bottom>r.top) {
            const bool focusable=std::find(app.interactive.begin(),app.interactive.end(),&node)!=app.interactive.end();
            if (is_interactive(node.kind) || node.kind==UI_LABEL || node.kind==UI_TEXT_BLOCK || node.kind==UI_PROGRESS_BAR ||
                node.kind==UI_TREE_VIEW || node.kind==UI_TAB_VIEW || node.kind==UI_LIST_VIEW || node.kind==UI_DOCUMENT_VIEW) {
                if (node.debug_id==request.target) target=&node;
                if (inspect) {
                    accessibility::Element e;
                    e.id=node.debug_id; e.parent=parent; e.kind=node.kind; e.number=node.value;
                    e.enabled=node.is_enabled(); e.focusable=focusable; e.focused=app.focused && app.focus==&node;
                    e.expanded=node.expanded;
                    e.selected=node.kind==UI_RADIO_BUTTON ? node.value!=0 : node.kind==UI_TREE_ITEM ? node.tree_selected :
                        (node.kind==UI_LIST_ITEM || node.kind==UI_TAB_ITEM) && node.is_selected_item();
                    e.screen={origin.x+r.left,origin.y+r.top,origin.x+r.right,origin.y+r.bottom};
                    e.automation_id=utf8_text::decode(node.fleck_id);
                    e.name=is_editor(node.kind) ? e.automation_id : node.display_text();
                    if (e.name.empty()) e.name=e.automation_id;
                    if (is_editor(node.kind) && node.kind!=UI_PASSWORD_BOX) e.value=node.display_text();
                    request.elements.push_back(std::move(e));
                }
                parent=node.debug_id;
            }
        }
        for(auto* child:node.children) self(self,*child,parent);
    };
    if(app.root) visit(visit,*app.root,0);
    if(inspect) return S_OK;
    if(!target) return UIA_E_ELEMENTNOTAVAILABLE;
    if(!target->is_enabled()) return UIA_E_ELEMENTNOTENABLED;
    switch(request.action) {
    case Action::focus:
        if(std::find(app.interactive.begin(),app.interactive.end(),target)==app.interactive.end()) return UIA_E_INVALIDOPERATION;
        SetFocus(app.window); app.assign_focus(target); app.keyboard_focus=true; app.dirty=true; break;
    case Action::invoke: case Action::toggle: case Action::select:
        app.activate_node(*target); break;
    case Action::expand: case Action::collapse:
        target->expanded=request.action==Action::expand;
        if(target->items_source) queue_items_refresh(*target);
        app.dirty=true; break;
    case Action::number:
        if(target->kind!=UI_SLIDER && target->kind!=UI_SPIN_BOX) return UIA_E_INVALIDOPERATION;
        app.invoke(*target,request.number); break;
    case Action::text:
        if(!is_editor(target->kind)) return UIA_E_INVALIDOPERATION;
        app.replace_node_text(*target,request.text);
        app.publish_editor_text(*target);
        break;
    default: return UIA_E_INVALIDOPERATION;
    }
    return S_OK;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* context = reinterpret_cast<ui_window_context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!context || !context->application) {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    auto* app = context->application;
    WindowScope scope(*app, context);
    if (app->debug_picking) {
        const int x = static_cast<short>(LOWORD(lparam)), y = static_cast<short>(HIWORD(lparam));
        switch (message) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            app->update_debug_hover(x, y);
            return 0;
        }
        case WM_MOUSELEAVE:
            app->update_debug_hover(-1, -1);
            return 0;
        case WM_LBUTTONUP:
            app->debug_selection = app->debug_hit(x, y);
            ui_app_set_debug_picking(app, 0);
            return 0;
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: case WM_CONTEXTMENU:
        case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR: case WM_IME_CHAR:
        case WM_PASTE: case WM_CUT: case WM_CLEAR: case WM_UNDO:
            // Picking observes the UI without editing, scrolling or invoking it.
            // Keep IME lifecycle and system window management messages flowing.
            return 0;
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
            return TRUE;
        case WM_KEYDOWN:
            return 0;
        case WM_KEYUP:
            if (wparam == VK_ESCAPE) { ui_app_set_debug_picking(app, 0); return 0; }
            return 0;
        }
    }
    switch (message) {
    case WM_GETOBJECT:
        if (static_cast<LONG>(lparam)==UiaRootObjectId && !app->closing) {
            try {
                if (!app->accessibility_window) app->accessibility_window=accessibility::create(window);
                return accessibility::provider(app->accessibility_window,wparam,lparam);
            } catch (...) { return 0; }
        }
        return DefWindowProcW(window,message,wparam,lparam);
    case accessibility::request_message:
        return accessibility::dispatch(app->accessibility_window,wparam,app,access_window);
    case WM_NCDESTROY:
        if (std::exchange(app->drop_registered,false)) win_drop::detach(window);
        app->accessibility_window.reset();
        SetWindowLongPtrW(window,GWLP_USERDATA,0);
        return DefWindowProcW(window,message,wparam,lparam);
    case WM_CREATE:
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
        app->refresh_system_colors();
        app->dirty = true;
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCCALCSIZE:
        if (app->workspace) { return 0; }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCPAINT:
        if (app->workspace) { return 0; }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCACTIVATE:
        if (app->workspace) { app->dirty = true; return TRUE; }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCHITTEST:
        if (app->workspace) {
            POINT point{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))}; ScreenToClient(window, &point);
            const int edge = app->window_frame().resize;
            if (!IsZoomed(window)) {
                const bool left = point.x < edge, right = point.x >= app->width - edge;
                const bool top = point.y < edge, bottom = point.y >= app->height - edge;
                if (top && left) { return HTTOPLEFT; } if (top && right) { return HTTOPRIGHT; }
                if (bottom && left) { return HTBOTTOMLEFT; } if (bottom && right) { return HTBOTTOMRIGHT; }
                if (left) { return HTLEFT; } if (right) { return HTRIGHT; }
                if (top) { return HTTOP; } if (bottom) { return HTBOTTOM; }
            }
            return app->caption_part(point.x, point.y) == UI_DOCK_PART_WINDOW_TITLE ? HTCAPTION : HTCLIENT;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_GETMINMAXINFO:
        if (app->workspace) {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                limits->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left, monitor.rcWork.top - monitor.rcMonitor.top};
                limits->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left, monitor.rcWork.bottom - monitor.rcWork.top};
            }
            limits->ptMinTrackSize = {MulDiv(200, app->dpi, 96), MulDiv(120, app->dpi, 96)};
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCLBUTTONDOWN:
        if (wparam == HTCAPTION && app->dock_window && app->workspace) {
            POINT point{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
            const auto screen = point;
            ScreenToClient(window, &point);
            if (IsZoomed(window)) {
                ShowWindow(window, SW_RESTORE);
                RECT restored{}; GetWindowRect(window, &restored);
                SetWindowPos(window, nullptr, screen.x - std::min(100L, (restored.right - restored.left) / 2), screen.y - app->window_frame().height / 2,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                point = screen; ScreenToClient(window, &point);
            }
            if (docking::begin_window_drag(*app->workspace, app->dock_window, point.x, point.y)) {
                GetWindowRect(window, &app->native_move_origin);
                app->native_grab = {screen.x - app->native_move_origin.left, screen.y - app->native_move_origin.top};
                app->native_dock_move = true;
                SetForegroundWindow(window); SetFocus(window); SetCapture(window);
                return 0;
            }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_CLOSE:
        if (!app->dock_window || !app->workspace || docking::close_window(*app->workspace, app->dock_window)) { app->closing = true; }
        return 0;
    case WM_SIZE:
        app->maximized = wparam == SIZE_MAXIMIZED;
        app->width = LOWORD(lparam);
        app->height = HIWORD(lparam);
        app->dirty = true;
        if (app->opened && !app->closing && app->width > 0 && app->height > 0) {
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_DPICHANGED: {
        app->dpi = LOWORD(wparam);
        const auto& rect = *reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->dirty = true;
        return 0;
    }
    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
        if (app->gallery || app->root) {
            POINT point{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
            ScreenToClient(window, &point);
            const bool horizontal = message == WM_MOUSEHWHEEL;
            const double steps = static_cast<double>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
            app->scroll_gallery(horizontal ? steps : -steps, point.x, point.y, horizontal);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_LBUTTONDOWN: {
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        const auto part = app->caption_part(x, y);
        if (part >= UI_DOCK_PART_WINDOW_MINIMIZE && part <= UI_DOCK_PART_WINDOW_CLOSE) {
            app->chrome_pressed = part; app->caption_hover(x, y); app->dirty = true; SetCapture(window); return 0;
        }
        if (part == UI_DOCK_PART_WINDOW_TITLE) {
            POINT point{x, y}; ClientToScreen(window, &point);
            return SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        }
        app->hover(x, y);
        app->pressed_control = app->hovered_control;
        app->dirty = app->dirty || app->pressed_control;
        app->drag_origin={x,y};
        app->begin_drag(x, y);
        if ((app->pressed_control && !app->pressed_control->drag_text.empty()) || app->dragged_control || (app->workspace && app->workspace->drag.prepared)) {
            SetCapture(window);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        if (app->native_dock_move && app->workspace) {
            POINT point{x, y}; ClientToScreen(window, &point);
            SetWindowPos(window, nullptr, point.x - app->native_grab.x, point.y - app->native_grab.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            ScreenToClient(window, &point);
            app->workspace->drag.active = true;
            docking::pointer(*app->workspace, app->dock_window, UI_POINTER_MOVE, point.x, point.y);
            return 0;
        }
        app->caption_hover(x, y);
        if (app->chrome_hover != UI_DOCK_PART_COUNT || app->chrome_pressed != UI_DOCK_PART_COUNT) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0}; TrackMouseEvent(&tracking); return 0;
        }
        if ((wparam & MK_LBUTTON) && app->pressed_control && !app->pressed_control->drag_text.empty() &&
            !(app->workspace && app->workspace->drag.prepared) &&
            (std::abs(x-app->drag_origin.x)>=GetSystemMetricsForDpi(SM_CXDRAG,app->dpi) ||
             std::abs(y-app->drag_origin.y)>=GetSystemMetricsForDpi(SM_CYDRAG,app->dpi))) {
            auto* source=std::exchange(app->pressed_control,nullptr);
            if (GetCapture()==window) ReleaseCapture();
            ui_drop_data data{UI_DROP_TEXT,source->drag_text.c_str()};
            ui_node_start_drag(source,&data);
            return 0;
        }
        if (app->pointer_handled(UI_POINTER_MOVE,x,y)) return 0;
        if (auto* node = app->dragged_control) {
            app->update_drag_value(*node, x, y);
        } else {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            app->hover(x, y);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        app->caption_hover(-1, -1);
        app->dirty = (std::exchange(app->pressed_control, nullptr) != nullptr) || app->dirty;
        app->hover(-1, -1);
        return 0;
    case WM_CANCELMODE:
        app->chrome_pressed = UI_DOCK_PART_COUNT;
        if (app->native_dock_move) {
            app->native_dock_move = false;
            SetWindowPos(window, nullptr, app->native_move_origin.left, app->native_move_origin.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        app->cancel_pointer(app->pointer_x, app->pointer_y);
        if (GetCapture() == window) {
            ReleaseCapture();
        }
        return 0;
    case WM_CAPTURECHANGED:
        app->chrome_pressed = UI_DOCK_PART_COUNT;
        if (app->native_dock_move) {
            app->native_dock_move = false;
            SetWindowPos(window, nullptr, app->native_move_origin.left, app->native_move_origin.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        app->cancel_pointer(app->pointer_x, app->pointer_y);
        return 0;
    case WM_RBUTTONUP:
        app->context_menu(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
        return 0;
    case WM_LBUTTONUP: {
        if (app->chrome_pressed != UI_DOCK_PART_COUNT) {
            const auto part = std::exchange(app->chrome_pressed, UI_DOCK_PART_COUNT);
            const auto hit = app->caption_part(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
            if (GetCapture() == window) { ReleaseCapture(); }
            if (hit == part) { app->caption_action(part); }
            app->dirty = true; return 0;
        }
        if (app->native_dock_move && app->workspace) {
            app->native_dock_move = false;
            docking::pointer(*app->workspace, app->dock_window, UI_POINTER_UP, static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
            if (GetCapture() == window) { ReleaseCapture(); }
            return 0;
        }
        app->dirty = (std::exchange(app->pressed_control, nullptr) != nullptr) || app->dirty;
        if (app->end_pointer_event(static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam)))) {
            if (GetCapture() == window) ReleaseCapture();
            return 0;
        }
        if (app->dragged_control) {
            app->finish_drag(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            return 0;
        }
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        app->click(x, y);
        if (GetCapture() == window && app->workspace && !app->workspace->drag.prepared) { ReleaseCapture(); }
        if (app->focus && is_editor(app->focus->kind)) {
            SetFocus(window);
            app->place_ime(true);
        }
        return 0;
    }
    case WM_SETFOCUS:
        app->focused = true;
        app->reset_caret();
        return 0;
    case WM_KILLFOCUS:
        app->ime_composing = false;
        app->focused = false;
        app->dirty = true;
        app->place_ime();
        return 0;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (app->focused) {
            const unsigned modifiers = (GetKeyState(VK_CONTROL)<0 ? UI_KEY_CTRL : 0) |
                (GetKeyState(VK_SHIFT)<0 ? UI_KEY_SHIFT : 0) | (GetKeyState(VK_MENU)<0 ? UI_KEY_ALT : 0);
            unsigned key = unsigned(wparam);
            if (key >= VK_F1 && key <= VK_F24) key = UI_KEY_F1 + key - VK_F1;
            else if (!((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))) key = 0;
            if (fleck::valid_gesture(key, modifiers) && app->key_gesture(key,modifiers)) return 0;
        }
        if (message == WM_SYSKEYDOWN) return DefWindowProcW(window,message,wparam,lparam);
        switch (wparam) {
        case VK_RETURN: case VK_ESCAPE:
            if (app->focus && app->focus->collection_item_id && app->focus->kind==UI_TEXT_BOX) {
                binding_app_error(app,ui_data_grid_finish_edit(app->focus,wparam==VK_RETURN));
                return 0;
            }
            break;
        case VK_LEFT: if (app->editor_key(ui_app::EditKey::left,GetKeyState(VK_SHIFT)<0,GetKeyState(VK_CONTROL)<0)) { return 0; } break;
        case VK_RIGHT: if (app->editor_key(ui_app::EditKey::right,GetKeyState(VK_SHIFT)<0,GetKeyState(VK_CONTROL)<0)) { return 0; } break;
        case VK_UP: if (app->editor_key(ui_app::EditKey::up,GetKeyState(VK_SHIFT)<0)) { return 0; } break;
        case VK_DOWN: if (app->editor_key(ui_app::EditKey::down,GetKeyState(VK_SHIFT)<0)) { return 0; } break;
        case VK_HOME: if (app->editor_key(ui_app::EditKey::home,GetKeyState(VK_SHIFT)<0,GetKeyState(VK_CONTROL)<0)) { return 0; } break;
        case VK_END: if (app->editor_key(ui_app::EditKey::end,GetKeyState(VK_SHIFT)<0,GetKeyState(VK_CONTROL)<0)) { return 0; } break;
        case VK_DELETE: if (app->editor_key(ui_app::EditKey::remove,GetKeyState(VK_SHIFT)<0,GetKeyState(VK_CONTROL)<0)) { return 0; } break;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_KEYUP:
        if (app->focused) {
            if (wparam == VK_ESCAPE) {
                if (app->native_dock_move) {
                    app->native_dock_move = false;
                    SetWindowPos(window, nullptr, app->native_move_origin.left, app->native_move_origin.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    if (GetCapture() == window) { ReleaseCapture(); }
                }
                if (app->workspace) { docking::cancel(*app->workspace); }
                for (const auto& node : app->nodes) {
                    if (!app->owns_input(node.get())) { continue; }
                    if (node->expanded || (is_popup(node->kind) && node->value)) {
                        node->expanded = false;
                        if (is_popup(node->kind)) {
                            node->value = 0;
                        }
                        app->dirty = true;
                    }
                }
            } else if (wparam == VK_SPACE || wparam == VK_RETURN) {
                app->key(false, false, 0);
            } else if (wparam == VK_TAB) {
                app->key(true, GetKeyState(VK_SHIFT) < 0, 0);
            } else if (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_UP || wparam == VK_DOWN) {
                app->key(false, false, wparam == VK_LEFT || wparam == VK_UP ? -5 : 5, true, wparam == VK_UP || wparam == VK_DOWN);
            }
        }
        return 0;
    case WM_CHAR:
        if (app->focus && is_editor(app->focus->kind)) {
            app->edit(window, static_cast<wchar_t>(wparam));
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_IME_ENDCOMPOSITION:
        app->ime_composing = false;
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_IME_STARTCOMPOSITION:
        app->ime_composing = true;
        [[fallthrough]];
    case WM_IME_NOTIFY: {
        const auto result = DefWindowProcW(window, message, wparam, lparam);
        if (message == WM_IME_STARTCOMPOSITION || wparam == IMN_OPENCANDIDATE) {
            app->place_ime(true);
        }
        return result;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        // During interactive resizing the native modal loop dispatches paint before poll resumes.
        if (app->opened && app->dirty && !app->closing && app->width > 0 && app->height > 0) {
            try {
                if (!app->render(false)) {
                    app->render_failed = true;
                }
            } catch (...) {
                app->render_failed = true;
            }
        }
        if (app->width > 0 && app->height > 0 &&
            app->dib.size() == static_cast<std::size_t>(app->width) * app->height * 4) {
            BITMAPINFO bitmap{};
            bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmap.bmiHeader.biWidth = app->width;
            bitmap.bmiHeader.biHeight = -app->height;
            bitmap.bmiHeader.biPlanes = 1;
            bitmap.bmiHeader.biBitCount = 32;
            bitmap.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(dc, 0, 0, app->width, app->height, 0, 0, 0, app->height,
                              app->dib.data(), &bitmap, DIB_RGB_COLORS);
        }
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}
#endif
}

uint32_t ui_api_version(void) { return UI_API_VERSION; }
ui_result ui_app_create(const ui_app_options* options, ui_app** out_app)
{
    if (!out_app) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_app = nullptr;
    if (!options || options->struct_size < sizeof(*options)) return UI_ERROR_INVALID_ARGUMENT;
    if (options->api_version != UI_API_VERSION) return UI_ERROR_VERSION_MISMATCH;
    if (options && (
                    options->renderer > UI_ANGLE || (options->flags & ~(UI_HEADLESS | UI_SHOW_RENDERER | UI_DEBUG_PIPE)))) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (options && options->renderer != UI_AUTO && options->renderer != UI_SOFTWARE) {
        bool supported = false;
        switch (options->renderer) {
#ifdef SUNFLECK_GLES3
        case UI_GLES3:
            supported = true;
            break;
#endif
#ifdef _WIN32
        case UI_ANGLE:
            supported = true;
            break;
#endif
#ifdef SUNFLECK_D3D11
        case UI_D3D11:
            supported = true;
            break;
#endif
#ifdef SUNFLECK_METAL
        case UI_METAL:
            supported = true;
            break;
#endif
        default:
            break;
        }
        if (!supported || (options->flags & UI_HEADLESS)) {
            return UI_ERROR_UNSUPPORTED;
        }
    }
    try {
        auto app = std::make_unique<ui_app>();
        app->headless = options && (options->flags & UI_HEADLESS);
#ifdef _WIN32
        BOOL client_animation = TRUE;
        if (!app->headless && SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&client_animation,0))
            app->reduced_motion = !client_animation;
#endif
        if (options) {
            app->show_renderer = (options->flags & UI_SHOW_RENDERER) != 0;
            app->renderer = static_cast<ui_renderer>(options->renderer);
        }
        if (options && (options->flags & UI_DEBUG_PIPE)) {
            const auto status = ui_app_enable_debug_server(app.get(), std::getenv("SUNFLECK_DEBUG_PIPE"));
            if (status != UI_OK) { return status; }
        }
        *out_app = app.release();
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result fleck::can_load(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    return app->opened || app->closing || (app->workspace && app->workspace->notifying) ? UI_ERROR_INVALID_STATE : UI_OK;
}

static bool accepts_text(ui_node_kind kind);

template<class Entry, class Object>
static bool valid_fleck_exports(ui_app* app, const Entry* entries, size_t count, Object* Entry::* object)
{
    if (count > 512 || (count && !entries)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        if (!entry.name || !binding_path(entry.name) ||
            !(entry.*object) || (entry.*object)->owner != app ||
            std::any_of(entries, entries + i, [&](const Entry& other) {
                return std::string_view(other.name) == entry.name;
            })) {
            return false;
        }
    }
    return true;
}

static ui_result apply_fleck_bindings(ui_app* app, ui_node& node, const fleck::Element& element,
                                     bool resolve, const ui_fleck_data_context* context, ui_model* model, const char*& message)
{
    node.command_mode = element.async_command ? UI_COMMAND_ASYNC : UI_COMMAND_SYNC;
    for (const auto& binding : element.bindings) {
        if (!resolve) {
            const auto status = components::validate_binding(node, binding);
            if (status != UI_OK) { message = "Invalid component property, binding source, type or mode"; return status; }
            continue;
        }
        const ui_binding_options options{sizeof(options), 1, binding.property, binding.path.c_str(), binding.mode, binding.trigger, binding.format.c_str()};
        auto status = binding_validate(node, options);
        if (status == UI_OK && resolve) { status = ui_node_bind_property(&node, model, &options); }
        if (status != UI_OK) { message = node.binding_message.empty() ? "Invalid property Binding: target, path, type, mode or model" : node.binding_message.c_str(); return status; }
    }
    return UI_OK;
}

static ui_result refresh_items(ui_node& host, ui_model* source, bool update_selection=true, bool validate_only=false);
static ui_result install_fleck_page(ui_app* app, fleck::Page&& page, std::string_view input, ui_fleck_diagnostic& result,
                                    bool resolve_bindings, const ui_fleck_data_context* context, ui_model* model, bool refresh)
{
    if (model && model->owner != app) { return UI_ERROR_INVALID_ARGUMENT; }
    if (context && (!valid_fleck_exports(app, context->text_sources, context->text_source_count,
                                         &ui_fleck_text_export::source) ||
                    !valid_fleck_exports(app, context->commands, context->command_count,
                                         &ui_fleck_command_export::command))) {
        fleck::report(input, 0, UI_FLECK_VALUE, "Invalid Fleck DataContext exports", result);
        return UI_ERROR_INVALID_ARGUMENT;
    }
    std::size_t offset = 0;
    std::unique_ptr<ui_model, decltype(&ui_model_destroy)> exported_model(nullptr, ui_model_destroy);
    if (resolve_bindings && context && !model) {
        ui_model* exports = nullptr;
        if (const auto status = ui_model_create(app, &exports); status != UI_OK) { return status; }
        exported_model.reset(exports);
        for (size_t i = 0; i < context->text_source_count; ++i) {
            const auto& entry = context->text_sources[i];
            if (const auto status = ui_model_add_text_source(exports, entry.name, entry.source); status != UI_OK) { return status; }
        }
        for (size_t i = 0; i < context->command_count; ++i) {
            const auto& entry = context->commands[i];
            if (const auto status = ui_model_add_command(exports, entry.name, entry.command); status != UI_OK) { return status; }
        }
        model = exports;
    }
    try {
        if (page.document_kind != "Window") { throw std::range_error("Load a Window page; register UserControl and ControlTemplate resources first"); }
        components::expand(app->component_registry, page.root);
        auto title = decode_ui_text(page.title.c_str());
        std::vector<std::unique_ptr<ui_node>> staged;
        std::vector<ui_node*> named;
        std::vector<FleckAnchor> anchors;
        auto* root = stage_fleck_node(app, page.root, nullptr, staged, named, anchors, offset, model);
        if (model && !root->has_local_data_context) {
            root->has_local_data_context = true;
            root->local_data_context = model;
            properties::written(*root, UI_PROPERTY_DATA_CONTEXT);
        }
        auto source = std::make_shared<devtools::Source>();
        source->page_id = refresh ? app->fleck_page.source->page_id : devtools::next_id(app);
        if (refresh) { source->uri = app->fleck_page.source->uri; }
        source->model = page.model;
        source->model_id = model ? model->debug_id : 0;
        source->fingerprint = fleck::fingerprint(page);
        std::vector<ui_node*> page_nodes;
        page_nodes.reserve(staged.size());
        for (const auto& node : staged) { node->source = source; page_nodes.push_back(node.get()); }
        for (const auto& anchor : anchors) {
            offset = anchor.offset;
            const auto target = std::find_if(named.begin(), named.end(), [&](const ui_node* node) {
                return node->fleck_id == *anchor.id && node->component_scope == anchor.node->component_scope;
            });
            if (target == named.end() || *target == anchor.node) {
                throw std::range_error("Invalid Fleck anchor target");
            }
            anchor.node->anchor = *target;
        }
        if (refresh) {
            std::map<std::pair<std::string, std::string>, const ui_node*> edited_contexts;
            for (auto* old : app->fleck_page.nodes) {
                if (old->data_context_edited && !old->fleck_id.empty())
                    edited_contexts.emplace(std::make_pair(old->scope_path, old->fleck_id), old);
            }
            for (auto& next : staged) {
                const ui_node* oldContext = nullptr;
                if (!edited_contexts.empty() && !next->fleck_id.empty()) {
                    auto found = edited_contexts.find({next->scope_path, next->fleck_id});
                    if (found != edited_contexts.end()) oldContext = found->second;
                } else if (next.get() == root && next->fleck_id.empty() && app->fleck_page.root->fleck_id.empty() &&
                           app->fleck_page.root->data_context_edited) oldContext = app->fleck_page.root;
                const bool sameComponent = oldContext && ((!oldContext->component && !next->component) ||
                    (oldContext->component && next->component && oldContext->component->type->key == next->component->type->key));
                if (oldContext && oldContext->kind == next->kind && sameComponent) {
                    next->has_local_data_context = oldContext->has_local_data_context;
                    next->local_data_context = oldContext->local_data_context;
                    next->data_context_edited = true;
                    if (const auto context = oldContext->property_slots.find(UI_PROPERTY_DATA_CONTEXT); context != oldContext->property_slots.end()) {
                        next->property_slots[UI_PROPERTY_DATA_CONTEXT] = context->second;
                        next->local_properties |= oldContext->local_properties & (uint64_t{1} << UI_PROPERTY_DATA_CONTEXT);
                    }
                }
                if (!next->component || next->fleck_id.empty()) { continue; }
                auto previous = std::find_if(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(), [&](auto* old) {
                    return old->component && old->scope_path == next->scope_path && old->fleck_id == next->fleck_id && old->component->type->key == next->component->type->key;
                });
                if (previous == app->fleck_page.nodes.end()) { continue; }
                if (fleck::range_template(next->kind) && next->fleck_value == (*previous)->fleck_value &&
                    ((*previous)->local_properties & (uint64_t{1} << UI_PROPERTY_VALUE))) {
                    const auto binds_value = [](const fleck::Element& element) {
                        return std::any_of(element.bindings.begin(), element.bindings.end(), [](const auto& binding) {
                            return binding.property == UI_PROPERTY_VALUE;
                        });
                    };
                    if (!binds_value(next->component->declaration) && !binds_value((*previous)->component->declaration)) {
                        ui_binding_value base{};
                        if (auto status = ui_node_get_base_property(*previous, UI_PROPERTY_VALUE, &base); status != UI_OK) return status;
                        if (auto status = ui_node_set_property(next.get(), UI_PROPERTY_VALUE, &base); status != UI_OK) return status;
                    }
                }
                for (auto& [name, value] : next->component->values) {
                    auto old = (*previous)->component->values.find(name);
                    const auto& old_literals = (*previous)->component->declaration.properties;
                    const auto& new_literals = next->component->declaration.properties;
                    auto old_literal = old_literals.find(name), new_literal = new_literals.find(name);
                    bool same_literal = (old_literal == old_literals.end() && new_literal == new_literals.end()) ||
                        (old_literal != old_literals.end() && new_literal != new_literals.end() && old_literal->second == new_literal->second);
                    if (same_literal && old != (*previous)->component->values.end() && old->second.value.kind == value.value.kind) {
                        value.layers = old->second.layers;
                        value.layers.values.erase(UI_PRECEDENCE_ANIMATION);
                        value.layers.values.erase(UI_PRECEDENCE_INHERITED);
                    }
                }
                if (auto status = components::resolve_retained_values(*next->component); status != UI_OK) {
                    components::report(next->component->declaration, "Retained component property is invalid", result);
                    return status;
                }
            }
        }
        // Staging uses preorder, so each descriptor pairs with its newly created node.
        std::size_t index = 0;
        const auto bind = [&](const auto& self, const fleck::Element& element) -> ui_result {
            const char* message = nullptr;
            auto& node = *staged[index++];
            auto* inherited = model;
            const auto status = resolve_bindings ? components::bind(node, element, inherited) : apply_fleck_bindings(app, node, element, false, context, model, message);
            if (!message) { message = "Invalid component or property binding"; }
            if (status != UI_OK) {
                components::report(element, message, result);
                return status;
            }
            if (resolve_bindings && node.items_source &&
                (node.item_template || !node.item_templates_by_label.empty() || !node.tree_children_path.empty())) {
                if (auto checked=refresh_items(node,node.items_source,false,true); checked!=UI_OK) {
                    components::report(element,"Invalid item template or item model binding",result);
                    return checked;
                }
            }
            for (const auto& child : element.children) {
                if (const auto child_status = self(self, child); child_status != UI_OK) {
                    return child_status;
                }
            }
            return UI_OK;
        };
        if (const auto status = bind(bind, page.root); status != UI_OK) {
            return status;
        }
        for (auto& node : staged) { components::validate_parts(*node); }
        if (refresh) {
            auto declared_dock = docking::from_fleck(app, *root);
            auto* active_dock = app->workspace.get();
            const bool remove_dock = active_dock && !declared_dock;
            if (remove_dock && (!active_dock->host->source ||
                std::any_of(active_dock->items.begin(), active_dock->items.end(), [&](const auto& item) {
                    return std::find(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(), item->content) == app->fleck_page.nodes.end();
                }))) {
                throw std::range_error("Live refresh cannot remove a programmatically registered Dock workspace or item");
            }
            std::map<const ui_node*, ui_node*> remap;
            std::map<std::pair<std::string, std::string>, ui_node*> old_ids;
            std::set<const ui_node*> reused;
            for (auto* node : app->fleck_page.nodes) {
                if (!node->fleck_id.empty()) { old_ids.emplace(std::pair{node->scope_path, node->fleck_id}, node); }
            }
            for (size_t i = 0; i < staged.size(); ++i) {
                auto* next = staged[i].get();
                ui_node* existing = nullptr;
                if (next == root && active_dock && active_dock->host == app->root && next->kind == UI_DOCK_HOST) {
                    existing = active_dock->host;
                } else if (!next->fleck_id.empty()) {
                    const auto found = old_ids.find(std::pair{next->scope_path, next->fleck_id});
                    if (found != old_ids.end()) { existing = found->second; }
                } else if (!next->parent) {
                    existing = app->root;
                } else {
                    const auto* parent = remap.at(next->parent);
                    if (parent != next->parent && parent->children.size() == next->parent->children.size()) {
                        const auto position = std::find(next->parent->children.begin(), next->parent->children.end(), next);
                        auto* candidate = parent->children[static_cast<size_t>(position - next->parent->children.begin())];
                        if (candidate->fleck_id.empty()) { existing = candidate; }
                    }
                }
                // Component wrappers are instance-bound. Replacing a host must retire its handle.
                if (next->component) { existing = nullptr; }
                if (existing && (existing->kind != next->kind || bool(existing->component) != bool(next->component) ||
                    (existing->component && existing->component->type->key != next->component->type->key))) { existing = nullptr; }
                if (existing && !reused.insert(existing).second) {
                    throw std::range_error("Live refresh matched an existing node more than once");
                }
                remap.emplace(next, existing ? existing : next);
            }
            std::optional<docking::Layout> updated_layout;
            std::vector<std::unique_ptr<ui_dock_item>> refreshed_items;
            std::vector<size_t> retained_items;
            std::vector<size_t> programmatic_items;
            if (declared_dock && active_dock) {
                if (remap.at(declared_dock->host) != active_dock->host) {
                    throw std::range_error("Live refresh requires the same Dock host");
                }
                updated_layout = active_dock->layout;
                bool removed_items = false;
                retained_items.reserve(declared_dock->items.size());
                for (size_t i = 0; i < active_dock->items.size(); ++i) {
                    const auto& item = active_dock->items[i];
                    if (std::find(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(), item->content) == app->fleck_page.nodes.end()) {
                        if (docking::find(*declared_dock, item->id)) {
                            throw std::range_error("A declared Dock item conflicts with a programmatically registered item");
                        }
                        programmatic_items.push_back(i);
                        declared_dock->defaults.emplace(item->id, active_dock->defaults.at(item->id));
                        continue;
                    }
                    if (!docking::find(*declared_dock, item->id)) {
                        docking::remove(*updated_layout, item->id);
                        updated_layout->placements.erase(item->id);
                        removed_items = true;
                        if (updated_layout->expanded == item->id) { updated_layout->expanded.clear(); }
                    }
                }
                if (declared_dock->items.size() + programmatic_items.size() > 512) {
                    throw std::range_error("Too many Dock items after live refresh");
                }
                refreshed_items.resize(declared_dock->items.size() + programmatic_items.size());
                if (removed_items) { docking::normalize(*updated_layout); }
                const auto previous_layout = *updated_layout;
                for (const auto& item : declared_dock->items) {
                    const auto found = std::find_if(active_dock->items.begin(), active_dock->items.end(),
                        [&](const auto& current) { return current->id == item->id; });
                    if (found == active_dock->items.end()) {
                        retained_items.push_back(active_dock->items.size());
                        updated_layout->placements.emplace(item->id, docking::Placement{});
                        std::string target;
                        const auto& placement = declared_dock->layout.placements.at(item->id);
                        for (const auto& peer : declared_dock->layout.regions.at(placement.group).tabs) {
                            const auto group = docking::group_of(*updated_layout, peer);
                            if (group && !docking::window_of(*updated_layout, group)) { target = peer; break; }
                        }
                        docking::place(*updated_layout, item->id, item->kind, target, UI_DOCK_CENTER, placement.index);
                        continue;
                    }
                    const auto state = updated_layout->placements.at(item->id).state;
                    if ((state == UI_DOCK_FLOATING && !(item->capabilities & UI_DOCK_CAN_FLOAT)) ||
                        (state == UI_DOCK_AUTO_HIDDEN && item->kind == UI_DOCK_TOOL_KIND && !(item->capabilities & UI_DOCK_CAN_AUTO_HIDE))) {
                        throw std::range_error("New Dock capabilities conflict with the current layout");
                    }
                    if ((*found)->kind != item->kind) {
                        // The content wrapper changes type and retires with its item handle.
                        // Keep a legal placement without mixing document/tool tabs.
                        if (updated_layout->expanded == item->id) { updated_layout->expanded.clear(); }
                        if (state == UI_DOCK_DOCKED || state == UI_DOCK_FLOATING) {
                            docking::remove(*updated_layout, item->id);
                            docking::normalize(*updated_layout);
                            docking::return_to(*updated_layout, *item);
                        } else if (state == UI_DOCK_AUTO_HIDDEN && item->kind == UI_DOCK_DOCUMENT_KIND) {
                            docking::place(*updated_layout, item->id, item->kind, "", UI_DOCK_CENTER, UINT32_MAX);
                        }
                        auto& placement = updated_layout->placements.at(item->id);
                        if (item->kind == UI_DOCK_DOCUMENT_KIND && placement.resume == UI_DOCK_AUTO_HIDDEN) {
                            placement.resume = UI_DOCK_DOCKED;
                        }
                        retained_items.push_back(active_dock->items.size());
                        continue;
                    }
                    if ((*found)->content != remap.at(item->content)) {
                        throw std::range_error("Live refresh could not retain Dock item content");
                    }
                    retained_items.push_back(static_cast<size_t>(found - active_dock->items.begin()));
                }
                for (auto& [id, region] : updated_layout->regions) {
                    const auto previous = previous_layout.regions.find(id);
                    if (previous != previous_layout.regions.end() &&
                        std::find(region.tabs.begin(), region.tabs.end(), previous->second.active) != region.tabs.end()) {
                        region.active = previous->second.active;
                    }
                }
                declared_dock->host->workspace = active_dock;
            }
            const std::set<ui_node*> old_page(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end());
            if (declared_dock && !active_dock) {
                declared_dock->host = remap.at(declared_dock->host);
                for (auto& item : declared_dock->items) {
                    item->content = remap.at(item->content);
                }
            }
            const auto retired = [&](ui_node* node) { return old_page.contains(node) && !reused.contains(node); };
#if defined(SUNFLECK_X11)
            // Positioned/server-side preedit does not expose composition start/end.
            // Keep its editor alive until XIM focus leaves, including in Dock windows.
            const auto xim_retired = [&](const ui_window_context& window) {
                return window.xim_focused && retired(window.focus);
            };
            if (xim_retired(*app) || std::any_of(app->dock_windows.begin(), app->dock_windows.end(),
                [&](const auto& entry) { return xim_retired(*entry.second); })) { return UI_ERROR_INVALID_STATE; }
#endif
            const auto value_bound = [](const ui_node& node) {
                return std::any_of(node.declared_bindings.begin(), node.declared_bindings.end(),
                    [](const auto& binding) { return binding.property == UI_PROPERTY_VALUE; });
            };
            std::set<ui_node*> retained_radio_groups;
            for (const auto& next : staged) {
                auto* existing = remap.at(next.get());
                if (existing == next.get()) { continue; }
                const auto radio_count = [](const ui_node& parent) {
                    return std::count_if(parent.children.begin(), parent.children.end(),
                        [](const auto* child) { return child->kind == UI_RADIO_BUTTON; });
                };
                if (radio_count(*next) == 0 || radio_count(*next) != radio_count(*existing)) { continue; }
                const auto unchanged = std::all_of(next->children.begin(), next->children.end(), [&](auto* child) {
                    if (child->kind != UI_RADIO_BUTTON) { return true; }
                    auto* previous = remap.at(child);
                    return previous != child && previous->parent == existing && previous->fleck_value == child->fleck_value &&
                        !value_bound(*previous) && !value_bound(*child);
                });
                if (unchanged) { retained_radio_groups.insert(existing); }
            }
            const auto added = staged.size() - reused.size();
            if (added > app->nodes.max_size() - app->nodes.size()) { throw std::bad_alloc(); }
            app->nodes.reserve(app->nodes.size() + added);
            for (auto*& node : named) { node = remap.at(node); }
            for (auto*& node : page_nodes) { node = remap.at(node); }
            for (auto* existing : app->fleck_page.nodes) {
                if (retired(existing) && std::any_of(existing->bindings.begin(), existing->bindings.end(),
                    [](const auto& binding) { return binding.pending; })) {
                    return UI_ERROR_INVALID_STATE;
                }
                if (existing->parent && !old_page.contains(existing->parent)) {
                    throw std::range_error("Live refresh cannot replace externally parented nodes");
                }
                for (auto* child : existing->children) {
                    if (!old_page.contains(child)) {
                        if (declared_dock && active_dock && existing == active_dock->host &&
                            std::any_of(programmatic_items.begin(), programmatic_items.end(),
                                [&](size_t i) { return active_dock->items[i]->content == child; })) { continue; }
                        throw std::range_error("Live refresh cannot replace externally appended children");
                    }
                }
            }
            for (size_t i = 0; i < staged.size(); ++i) {
                auto& next = *staged[i];
                auto& existing = *page_nodes[i];
                next.parent = next.parent ? remap.at(next.parent) : nullptr;
                if (next.content_owner) next.content_owner = remap.at(next.content_owner);
                if (next.presented_button) next.presented_button = remap.at(next.presented_button);
                for (auto*& child : next.children) { child = remap.at(child); }
                next.anchor = next.anchor ? remap.at(next.anchor) : nullptr;
                if (next.component) {
                    next.component->host = &existing;
                }
                if (&next == &existing) { continue; }
                bool preserve_draft = false;
                for (const auto& binding : existing.bindings) {
                    if (!binding.pending) { continue; }
                    const auto match = std::find_if(next.bindings.begin(), next.bindings.end(), [&](const auto& candidate) {
                        return candidate.model == binding.model && candidate.property == binding.property &&
                            candidate.path == binding.path && candidate.format == binding.format &&
                            candidate.mode == binding.mode && candidate.trigger == binding.trigger;
                    });
                    if (match == next.bindings.end() || binding.property != UI_PROPERTY_TEXT ||
                        !is_editor(next.kind) || existing.text_source) { return UI_ERROR_INVALID_STATE; }
                    match->pending = true;
                    preserve_draft = true;
                }
                next.debug_id = existing.debug_id;
                if (next.is_focus_scope && existing.is_focus_scope) next.logical_focus_id = existing.logical_focus_id;
                next.dock_window = remove_dock ? 0 : existing.dock_window;
                next.offset = existing.offset;
                next.horizontal_offset = existing.horizontal_offset;
                if (next.fleck_value == existing.fleck_value) {
                    next.expanded = existing.expanded;
                    if (!value_bound(next) && !value_bound(existing)) {
                        if (is_selector(next.kind) && existing.value < existing.children.size()) {
                            auto* selected = existing.children[existing.value];
                            const auto found = std::find_if(next.children.begin(), next.children.end(), [&](auto* candidate) {
                                if (candidate == selected) return true;
                                // Template hosts are recreated on refresh. Match a named item within its selector.
                                return selected->kind == UI_LIST_ITEM && candidate->kind == UI_LIST_ITEM &&
                                    !selected->fleck_id.empty() && selected->fleck_id == candidate->fleck_id &&
                                    selected->scope_path == candidate->scope_path;
                            });
                            if (found != next.children.end()) {
                                next.value = static_cast<unsigned>(found - next.children.begin());
                            }
                        } else if ((next.kind == UI_RADIO_BUTTON && retained_radio_groups.contains(next.parent)) ||
                            next.kind == UI_TREE_ITEM || next.kind == UI_CHECK_BOX || next.kind == UI_SWITCH || next.kind == UI_TOGGLE_BUTTON ||
                            next.kind == UI_SLIDER || next.kind == UI_SPIN_BOX || next.kind == UI_SPLIT_VIEW) {
                            next.value = existing.value;
                        }
                        if (auto slot = existing.property_slots.find(UI_PROPERTY_VALUE);
                            slot != existing.property_slots.end() && slot->second.values.contains(UI_PRECEDENCE_ANIMATION)) {
                            ui_binding_value base{};
                            if (ui_node_get_base_property(&existing,UI_PROPERTY_VALUE,&base) == UI_OK)
                                next.value = static_cast<unsigned>(base.integer);
                        }
                        if (next.value == existing.value && (existing.local_properties & (uint64_t{1} << UI_PROPERTY_VALUE))) {
                            properties::written(next, UI_PROPERTY_VALUE);
                        }
                    }
                }
                next.panel_callbacks = existing.panel_callbacks;
                next.tree_selected = existing.tree_selected;
                if (!next.component_scope) {
                    next.action = existing.action;
                    next.context = existing.context;
                    next.activation_handlers = existing.activation_handlers;
                    next.pointer_handlers = existing.pointer_handlers;
                    next.command_bindings = existing.command_bindings;
                    for (const auto& [gesture, binding] : existing.input_bindings)
                        if (binding.local) next.input_bindings.insert_or_assign(gesture, binding);
                    if (existing.local_routed_command) {
                        next.routed_command = existing.routed_command;
                        next.local_routed_command = true;
                    }
                }
                next.action_concurrency = existing.action_concurrency;
                next.bounds = existing.bounds;
                next.visible = existing.visible;
                next.scroll_limit = existing.scroll_limit;
                next.scrollbar = existing.scrollbar;
                next.scrollbar_thumb = existing.scrollbar_thumb;
                if (is_editor(next.kind)) {
                    if (preserve_draft || (next.declared_bindings.empty() && existing.declared_bindings.empty() && !existing.text_source)) {
                        next.text = existing.text;
                        if (existing.local_properties & (uint64_t{1} << UI_PROPERTY_TEXT)) properties::written(next, UI_PROPERTY_TEXT);
                    }
                    next.caret = std::min(existing.caret, next.display_text().size());
                    next.select_all = existing.select_all;
                    next.selection_anchor=existing.selection_anchor ? std::optional<size_t>{std::min(*existing.selection_anchor,next.text.size())} : std::nullopt;
                    next.caret_trailing=existing.caret_trailing;
                    next.anchor_trailing=existing.anchor_trailing;
                }
            }
            if (declared_dock && active_dock) {
                for (const auto i : programmatic_items) {
                    declared_dock->host->children.push_back(active_dock->items[i]->content);
                }
            }
            static_assert(std::is_nothrow_move_assignable_v<ui_node>);
            for (auto& node : app->nodes) {
                if (retired(node->anchor)) { node->anchor = nullptr; }
            }
            if (retired(app->focus)) {
                app->focus = nullptr;
                app->keyboard_focus = false;
            }
            if (retired(app->hovered_control)) { app->hovered_control = nullptr; }
#if defined(SUNFLECK_X11)
            if (retired(app->paste_target)) { app->paste_target = nullptr; }
#endif
            app->interactive.clear();
            app->scrollable.clear();
            app->overlays.clear();
            app->debug_paint_order.clear();
            app->debug_hover = 0;
            for (auto& [id, window] : app->dock_windows) {
                (void)id;
                if (retired(window->focus)) { window->focus = nullptr; window->keyboard_focus = false; }
                if (retired(window->hovered_control)) { window->hovered_control = nullptr; }
#if defined(SUNFLECK_X11)
                if (retired(window->paste_target)) { window->paste_target = nullptr; }
#endif
                window->interactive.clear();
                window->scrollable.clear();
                window->overlays.clear();
                window->debug_paint_order.clear();
                window->debug_hover = 0;
                window->dirty = true;
            }
            for (auto* old : old_page) {
                if (retired(old) && old->debug_id == app->debug_selection) { app->debug_selection = 0; }
            }
            for (auto& diagnostic : app->diagnostics) {
                if (!diagnostic.active) { continue; }
                if (std::any_of(old_page.begin(), old_page.end(), [&](auto* node) {
                    return node->debug_id == diagnostic.object && retired(node);
                })) {
                    diagnostic.active = false;
                    diagnostic.sequence = ++app->diagnostic_sequence;
                }
            }
            if (remove_dock) {
                for (auto& [id, window] : app->dock_windows) {
                    (void)id;
                    WindowScope scope(*app, window.get());
                    app->close_window();
                }
                app->dock_windows.clear();
                app->workspace.reset();
            }
            for (auto* old : old_page) { components::shutdown(*old); }
            std::erase_if(app->nodes, [&](const auto& node) { return retired(node.get()); });
            for (size_t i = 0; i < staged.size(); ++i) {
                if (page_nodes[i] == staged[i].get()) {
                    app->nodes.push_back(std::move(staged[i]));
                } else {
                    *page_nodes[i] = std::move(*staged[i]);
                    if (auto& source=page_nodes[i]->element_property_source) source->node=page_nodes[i];
                }
            }
            app->fleck_page.named = std::move(named);
            app->root = remap.at(root);
            app->fleck_page.root = app->root;
            app->fleck_page.nodes = std::move(page_nodes);
            app->fleck_page.source = std::move(source);
            if (declared_dock && active_dock && !remove_dock) {
                for (size_t i = 0; i < declared_dock->items.size(); ++i) {
                    auto& declared = *declared_dock->items[i];
                    const auto previous = retained_items[i];
                    if (previous == active_dock->items.size()) {
                        declared.dock = active_dock;
                        refreshed_items[i] = std::move(declared_dock->items[i]);
                        continue;
                    }
                    auto item = std::move(active_dock->items[previous]);
                    item->title.swap(declared.title);
                    item->icon.swap(declared.icon);
                    item->min_width = declared.min_width;
                    item->min_height = declared.min_height;
                    item->capabilities = declared.capabilities;
                    refreshed_items[i] = std::move(item);
                }
                for (size_t i = 0; i < programmatic_items.size(); ++i) {
                    refreshed_items[declared_dock->items.size() + i] = std::move(active_dock->items[programmatic_items[i]]);
                }
                active_dock->items.swap(refreshed_items);
                std::swap(active_dock->layout, *updated_layout);
                active_dock->hovered.reset();
                active_dock->pressed.reset();
                active_dock->initial.swap(declared_dock->initial);
                active_dock->defaults.swap(declared_dock->defaults);
            } else if (declared_dock) {
                app->workspace = std::move(declared_dock);
            }
            for (auto* node : app->fleck_page.nodes) {
                try { components::mount(*node); } catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
            }
            app->dirty = true;
            return UI_OK;
        }
        if (staged.size() > app->nodes.max_size() - app->nodes.size()) {
            throw std::bad_alloc();
        }
        app->nodes.reserve(app->nodes.size() + staged.size());
        auto workspace = docking::from_fleck(app, *root);
        if (app->workspace && (!app->fleck_page.root || app->workspace->host->source != app->fleck_page.source)) {
            throw std::range_error("A programmatic Dock workspace is already installed");
        }
        if (app->fleck_page.root) {
            app->inactive_fleck_pages.reserve(app->inactive_fleck_pages.size() + 1);
            app->fleck_page.workspace = std::move(app->workspace);
            app->inactive_fleck_pages.push_back(std::move(app->fleck_page));
            app->fleck_page = {};
        }
        for (auto& node : staged) {
            app->nodes.push_back(std::move(node));
        }
        app->window_title = std::move(title);
        app->fleck_page.named = std::move(named);
        app->fleck_page.nodes = std::move(page_nodes);
        app->fleck_page.source = std::move(source);
        app->window_title_utf8 = std::move(page.title);
        app->configured_window = true;
        app->width = page.width;
        app->height = page.height;
        if (workspace) { app->workspace = std::move(workspace); }
        const auto installed = ui_app_set_root(app, root);
        if (installed == UI_OK) {
            app->fleck_page.root = root;
            app->fleck_page.exported_model = exported_model.release();
            for (auto* node : app->fleck_page.nodes) {
                try { components::mount(*node); } catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
            }
        }
        return installed;
    } catch (const components::Error& error) {
        fleck::report(input, offset, UI_FLECK_VALUE, error.what(), result);
        result.line = error.line; result.column = error.column;
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::range_error& error) {
        fleck::report(input, offset, UI_FLECK_VALUE, error.what(), result);
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        fleck::report(input, offset, UI_FLECK_RUNTIME, "Unable to allocate Fleck page", result);
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result fleck::install(ui_app* app, Page&& page, std::string_view input, ui_fleck_diagnostic& result,
                         bool resolve_bindings, const ui_fleck_data_context* context, ui_model* model)
{
    return install_fleck_page(app, std::move(page), input, result, resolve_bindings, context, model, false);
}

ui_result ui_app_load_fleck(ui_app* app, const char* utf8, size_t length, ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_with_context(app, utf8, length, nullptr, diagnostic);
}

ui_result ui_app_refresh_fleck(ui_app* app, const char* utf8, size_t length, ui_model* model, ui_fleck_diagnostic* diagnostic)
{
    ui_fleck_diagnostic local{};
    auto& result = diagnostic ? *diagnostic : local;
    result = {};
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!utf8 || (model && model->owner != app)) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto started = std::chrono::steady_clock::now();
    const auto queue_generation = app->fleck_queue_generation;
    const auto record = [&](ui_result status) {
        auto& report = app->fleck_refresh_report;
        ++report.sequence;
        report.result = status;
        report.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
        report.diagnostic = result;
        if (app->fleck_queue_generation == queue_generation) { app->queued_fleck_refresh.reset(); }
        return status;
    };
    const auto window_busy = [](const ui_window_context& window) {
        return window.dragged_control || window.pressed_control || window.ime_composing ||
            window.chrome_pressed != UI_DOCK_PART_COUNT || window.native_dock_move
#if defined(SUNFLECK_X11)
            || window.chrome_manual
#endif
            ;
    };
    const auto uses_model = [&](const ui_model* candidate) {
        return candidate == model || candidate == app->fleck_page.exported_model ||
            std::any_of(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(), [&](const auto* node) {
                return binding_data_context(*node) == candidate || std::any_of(node->bindings.begin(), node->bindings.end(), [&](const auto& binding) { return binding.model == candidate; });
            });
    };
    if (!app->opened || app->closing || !app->fleck_page.source || app->fleck_page.nodes.empty() ||
        app->root != app->fleck_page.nodes.front() || app->dispatching || app->drawing || app->current_action ||
        (app->workspace && (app->workspace->notifying || app->workspace->drag.prepared || app->workspace->menu)) ||
        window_busy(*app) || app->current != app->main_state.get() ||
        std::any_of(app->dock_windows.begin(), app->dock_windows.end(),
            [&](const auto& entry) { return window_busy(*entry.second); }) ||
        std::any_of(app->models.begin(), app->models.end(), [&](const auto& candidate) {
            return (candidate->notifying || candidate->callbacks) && uses_model(candidate.get());
        }) ||
        std::any_of(app->commands.begin(), app->commands.end(), [&](const auto& command) {
            return command->run && std::any_of(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(),
                [&](const auto* node) { return node->command == command.get(); });
        }) ||
        std::any_of(app->fleck_page.nodes.begin(), app->fleck_page.nodes.end(), [](const auto* node) {
            return node->running_actions || node->deferred_actions;
        })) { return UI_ERROR_INVALID_STATE; }
    const std::string_view input(utf8, length);
    try {
        fleck::Page page;
        if (!fleck::parse(input, page, result, app->component_registry.dictionaries)) { return record(UI_ERROR_INVALID_ARGUMENT); }
        auto* exported = app->fleck_page.exported_model;
        auto* refresh_model = model ? model : exported;
        ui_result status;
        {
            struct RefreshScope { ui_app* app; ~RefreshScope() { app->dispatching = false; } } scope{app};
            app->dispatching = true;
            const auto state = [&](ui_fleck_refresh_phase phase, ui_result result) {
                if (!app->fleck_state_callback) { return UI_OK; }
                try { return app->fleck_state_callback(app, phase, result, app->fleck_state_context); }
                catch (...) { return UI_ERROR_INVALID_STATE; }
            };
            const auto captured = state(UI_FLECK_REFRESH_CAPTURE, UI_OK);
            if (captured != UI_OK) {
                fleck::report(input, 0, UI_FLECK_RUNTIME, "Application state capture rejected Fleck refresh", result);
                return record(captured);
            }
            try {
                status = install_fleck_page(app, std::move(page), input, result, true, nullptr, refresh_model, true);
            } catch (const std::bad_alloc&) {
                if (const auto completed = state(UI_FLECK_REFRESH_COMPLETE, UI_ERROR_OUT_OF_MEMORY); completed != UI_OK) { binding_app_error(app, completed); }
                throw;
            } catch (...) {
                if (const auto completed = state(UI_FLECK_REFRESH_COMPLETE, UI_ERROR_INVALID_STATE); completed != UI_OK) { binding_app_error(app, completed); }
                fleck::report(input, 0, UI_FLECK_RUNTIME, "Unexpected Fleck refresh staging failure", result);
                return record(UI_ERROR_INVALID_STATE);
            }
            if (const auto completed = state(UI_FLECK_REFRESH_COMPLETE, status); completed != UI_OK) { binding_app_error(app, completed); }
        }
        if (status == UI_OK && exported && refresh_model != exported) {
            (void)ui_model_destroy(exported);
        }
        return status == UI_ERROR_INVALID_STATE && result.code == UI_FLECK_NONE ? status : record(status);
    } catch (const std::bad_alloc&) {
        fleck::report(input, 0, UI_FLECK_RUNTIME, "Unable to allocate Fleck page", result);
        return record(UI_ERROR_OUT_OF_MEMORY);
    }
}

ui_result ui_app_set_fleck_refresh_state_callback(ui_app* app, ui_fleck_refresh_state_callback callback, void* context)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->dispatching || app->drawing || app->closing) { return UI_ERROR_INVALID_STATE; }
    app->fleck_state_callback = callback;
    app->fleck_state_context = context;
    return UI_OK;
}

ui_result ui_app_queue_fleck_refresh(ui_app* app, const char* utf8, size_t length, ui_model* model)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if ((!utf8 && length) || length > 1024 * 1024 || (model && model->owner != app)) { return UI_ERROR_INVALID_ARGUMENT; }
    if (!utf8) {
        app->queued_fleck_refresh.reset();
        ++app->fleck_queue_generation;
        return UI_OK;
    }
    if (!app->opened || app->closing || !app->fleck_page.source || app->root != app->fleck_page.root) { return UI_ERROR_INVALID_STATE; }
    try {
        ui_app::QueuedFleckRefresh request{std::string(utf8, length), app->fleck_page.source->page_id, model ? model->debug_id : 0};
        app->queued_fleck_refresh = std::move(request);
        ++app->fleck_queue_generation;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_app_get_fleck_refresh_status(ui_app* app, ui_fleck_refresh_status* status)
{
    if (!status || status->struct_size != sizeof(*status) || status->version != 1) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto result = check(app); result != UI_OK) { return result; }
    const auto& report = app->fleck_refresh_report;
    *status = {sizeof(*status), 1, report.sequence, report.result,
        app->queued_fleck_refresh || app->applying_queued_fleck_refresh ? 1u : 0u, report.elapsed_us, report.diagnostic};
    return UI_OK;
}

static void poll_fleck_refresh(ui_app* app)
{
    if (!app->queued_fleck_refresh) { return; }
    auto request = std::move(*app->queued_fleck_refresh);
    app->queued_fleck_refresh.reset();
    const auto generation = app->fleck_queue_generation;
    ui_model* model = nullptr;
    if (request.model) {
        const auto found = std::find_if(app->models.begin(), app->models.end(), [&](const auto& model) { return model->debug_id == request.model; });
        if (found != app->models.end()) { model = found->get(); }
    }
    if (!app->fleck_page.source || request.page != app->fleck_page.source->page_id || (request.model && !model) || app->closing) {
        auto& report = app->fleck_refresh_report;
        ++report.sequence;
        report.result = UI_ERROR_INVALID_STATE;
        report.elapsed_us = 0;
        fleck::report(request.text, 0, UI_FLECK_RUNTIME, "Queued refresh page or model is no longer active", report.diagnostic);
        return;
    }
    ui_fleck_diagnostic diagnostic{};
    struct ApplyingScope { ui_app* app; ~ApplyingScope() { app->applying_queued_fleck_refresh = false; } } scope{app};
    app->applying_queued_fleck_refresh = true;
    auto previous_resources = app->component_registry.documents;
    auto previous_dictionaries = app->component_registry.dictionaries;
    std::vector<ui_fleck_resource_entry> entries;
    for (const auto& resource : request.resources) { entries.push_back({resource.uri.c_str(), resource.removed ? nullptr : resource.text.c_str(), resource.text.size()}); }
    auto status = entries.empty() ? UI_OK : ui_app_register_fleck_resources(app, entries.data(), entries.size(), &diagnostic);
    if (status == UI_OK) { status = ui_app_refresh_fleck(app, request.text.data(), request.text.size(), model, &diagnostic); }
    else { auto& report = app->fleck_refresh_report; ++report.sequence; report.result = status; report.elapsed_us = 0; report.diagnostic = diagnostic; }
    if (status != UI_OK) { app->component_registry.documents.swap(previous_resources); app->component_registry.dictionaries.swap(previous_dictionaries); }
    if (status == UI_ERROR_INVALID_STATE && diagnostic.code == UI_FLECK_NONE && app->fleck_queue_generation == generation) {
        app->queued_fleck_refresh = std::move(request);
    }
}

ui_result ui_app_load_fleck_with_context(ui_app* app, const char* utf8, size_t length,
                                        const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_with_model(app, utf8, length, nullptr, context, diagnostic);
}

ui_result ui_app_load_fleck_with_model(ui_app* app, const char* utf8, size_t length,
    ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    ui_fleck_diagnostic local{};
    auto& result = diagnostic ? *diagnostic : local;
    result = {};
    if (const auto status = fleck::can_load(app); status != UI_OK) {
        return status;
    }
    if (!utf8) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    const std::string_view input(utf8, length);
    try {
        fleck::Page page;
        return fleck::parse(input, page, result, app->component_registry.dictionaries)
            ? fleck::install(app, std::move(page), input, result, true, context, model) : UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        fleck::report(input, 0, UI_FLECK_RUNTIME, "Unable to allocate Fleck page", result);
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_app_get_fleck_root(ui_app* app, ui_node** out_root)
{
    if (!out_root) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_root = nullptr;
    if (const auto status = check(app); status != UI_OK) { return status; }
    *out_root = app->fleck_page.root;
    return *out_root ? UI_OK : UI_ERROR_INVALID_STATE;
}

ui_result ui_app_discard_fleck_page(ui_app* app, ui_node* root)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!root || app->dispatching || app->drawing || app->current != app->main_state.get()) { return UI_ERROR_INVALID_STATE; }
    auto page = std::find_if(app->inactive_fleck_pages.begin(), app->inactive_fleck_pages.end(),
        [&](const auto& entry) { return entry.root == root; });
    if (page == app->inactive_fleck_pages.end() || (page->workspace && page->workspace->notifying)) { return UI_ERROR_INVALID_STATE; }
    const auto owns = [&](const ui_node* node) { return node && node->source == page->source; };
    for (auto* node : page->nodes) {
        if ((node->parent && !owns(node->parent)) ||
            std::any_of(node->children.begin(), node->children.end(), [&](auto* child) { return !owns(child); }) ||
            node->running_actions || node->deferred_actions ||
            std::any_of(node->bindings.begin(), node->bindings.end(), [](const auto& binding) { return binding.pending; })) {
            return UI_ERROR_INVALID_STATE;
        }
    }
    if (page->exported_model) {
        if (const auto status = ui_model_destroy(page->exported_model); status != UI_OK) { return status; }
    }
    page->workspace.reset();
    for (auto& node : app->nodes) {
        if (owns(node->anchor)) { node->anchor = nullptr; }
        if (owns(node.get())) {
            for (const auto& binding : node->declared_bindings) {
                devtools::recover(*node, binding.property, binding.path);
            }
        }
    }
    for (auto* node : page->nodes) { components::shutdown(*node); }
    std::erase_if(app->nodes, [&](const auto& node) { return owns(node.get()); });
    app->inactive_fleck_pages.erase(page);
    return UI_OK;
}

ui_result ui_app_find_fleck_node(ui_app* app, const char* id, ui_node** out_node)
{
    if (!out_node) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_node = nullptr;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!id) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    const auto found = std::find_if(app->fleck_page.named.begin(), app->fleck_page.named.end(),
        [&](const ui_node* node) { return node->fleck_id == id && !node->component_scope; });
    if (found == app->fleck_page.named.end()) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_node = *found;
    return UI_OK;
}

ui_result ui_app_set_window_options(ui_app* app, const char* title, uint32_t width, uint32_t height)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (app->opened || app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    if (!title || width == 0 || height == 0 || width > 4096 || height > 4096) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto wide_title = decode_ui_text(title);
        std::string utf8_title(title);
        app->window_title = std::move(wide_title);
        app->window_title_utf8 = std::move(utf8_title);
        app->width = width;
        app->height = height;
        app->configured_window = true;
        return UI_OK;
    } catch (const std::range_error&) {
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_app_open_window(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (app->opened) {
        return UI_ERROR_INVALID_STATE;
    }
    if (!app->headless) {
#ifdef _WIN32
        const auto instance = GetModuleHandleW(nullptr);
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.lpszClassName = L"SunfleckPreview";
        if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return UI_ERROR_PLATFORM;
        }
        const auto previous_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        RECT bounds{0, 0, 480, 320};
        if (app->configured_window) {
            const auto dpi = GetDpiForSystem();
            bounds.right = MulDiv(app->width, dpi, 96);
            bounds.bottom = MulDiv(app->height, dpi, 96);
            if (!app->workspace) { AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi); }
        }
        app->window = CreateWindowExW(0, window_class.lpszClassName, app->window_title.c_str(),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance, app->current);
        if (previous_dpi) {
            SetThreadDpiAwarenessContext(previous_dpi);
        }
        if (!app->window) {
            return UI_ERROR_PLATFORM;
        }
        app->drop_registered=SUCCEEDED(win_drop::attach(app->window,app->current,receive_drop));
        app->dpi = GetDpiForWindow(app->window);
        app->sync_window_frame();
        ShowWindow(app->window, SW_SHOW);
#elif defined(SUNFLECK_X11)
        app->display = XOpenDisplay(nullptr);
        if (!app->display) {
            return UI_ERROR_PLATFORM;
        }
        auto* d = app->display;
        const int screen = DefaultScreen(d);
        if (DefaultVisual(d, screen)->c_class != TrueColor) {
            app->close_x11();
            return UI_ERROR_UNSUPPORTED;
        }
        const int mm = DisplayWidthMM(d, screen);
        if (mm > 0) {
            app->dpi = std::clamp(static_cast<unsigned>(DisplayWidth(d, screen) * 25.4 / mm + 0.5), 48u, 384u);
        }
        const auto dimension = [&](int logical) {
            return app->configured_window ? std::max(1u, (logical * app->dpi + 48) / 96)
                                          : static_cast<unsigned>(logical);
        };
        app->window = XCreateSimpleWindow(d, RootWindow(d, screen), 0, 0,
            dimension(app->width), dimension(app->height), 0, 0, app->x11_background());
        app->wm_protocols = XInternAtom(d, "WM_PROTOCOLS", False);
        app->wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
        app->clipboard = XInternAtom(d, "CLIPBOARD", False);
        app->utf8 = XInternAtom(d, "UTF8_STRING", False);
        app->targets = XInternAtom(d, "TARGETS", False);
        app->paste_property = XInternAtom(d, "SUNFLECK_PASTE", False);
        XSetWMProtocols(d, app->window, &app->wm_delete, 1);
        x11_drop::attach(app->drop_target, d, app->window);
        XStoreName(d, app->window, app->window_title_utf8.c_str());
        const Atom title = XInternAtom(d, "_NET_WM_NAME", False);
        XChangeProperty(d, app->window, title, app->utf8, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(app->window_title_utf8.data()),
                        static_cast<int>(app->window_title_utf8.size()));
        if (XSupportsLocale()) {
            if (!std::getenv("XMODIFIERS")) {
                XSetLocaleModifiers("@im=ibus");
                app->input_method = XOpenIM(d, nullptr, nullptr, nullptr);
            }
            if (!app->input_method) {
                XSetLocaleModifiers("");
                app->input_method = XOpenIM(d, nullptr, nullptr, nullptr);
            }
        }
        if (app->input_method) {
            char** missing = nullptr;
            char* default_string = nullptr;
            int missing_count = 0;
            app->xim_fontset = XCreateFontSet(d, "-*-*-medium-r-normal--14-*-*-*-*-*-*-*,fixed",
                &missing, &missing_count, &default_string);
            if (missing) {
                XFreeStringList(missing);
            }
            XIMStyles* styles = nullptr;
            if (app->xim_fontset &&
                !XGetIMValues(app->input_method, XNQueryInputStyle, &styles, nullptr) && styles) {
                for (const XIMStyle style : {XIMPreeditPosition | XIMStatusNothing,
                                             XIMPreeditPosition | XIMStatusNone}) {
                    auto* const end = styles->supported_styles + styles->count_styles;
                    if (std::find(styles->supported_styles, end, style) == end) {
                        continue;
                    }
                    XPoint initial{0, 0};
                    if (auto attributes = XVaCreateNestedList(0, XNSpotLocation, &initial,
                                                              XNFontSet, app->xim_fontset, nullptr)) {
                        app->input_context = XCreateIC(app->input_method, XNInputStyle, style,
                            XNClientWindow, app->window, XNFocusWindow, app->window,
                            XNPreeditAttributes, attributes, nullptr);
                        XFree(attributes);
                    }
                    if (app->input_context) {
                        app->xim_positioned = true;
                        break;
                    }
                }
                XFree(styles);
            }
            if (!app->input_context) {
                app->input_context = XCreateIC(app->input_method, XNInputStyle,
                    XIMPreeditNothing | XIMStatusNothing, XNClientWindow, app->window,
                    XNFocusWindow, app->window, nullptr);
            }
        }
        long input_mask = 0;
        if (app->input_context) {
            XGetICValues(app->input_context, XNFilterEvents, &input_mask, nullptr);
        }
        XSelectInput(d, app->window, ExposureMask | StructureNotifyMask | PropertyChangeMask | ButtonPressMask | ButtonReleaseMask |
            KeyPressMask | KeyReleaseMask | FocusChangeMask | PointerMotionMask | LeaveWindowMask | input_mask);
        app->sync_window_frame(true);
        XMapWindow(d, app->window);
        XFlush(d);
#ifdef SUNFLECK_ATSPI
        app->accessibility_window = x11_accessibility::open(app->current, x11_accessible_action);
#endif
#elif defined(SUNFLECK_MACOS)
        try {
            auto window = std::make_unique<macos::Window>();
            if (!window->open(app->window_title_utf8.c_str(), app->configured_window ? app->width : 480,
                              app->configured_window ? app->height : 320, app->current, macos_event)) {
                return UI_ERROR_PLATFORM;
            }
            app->window = std::move(window);
        } catch (const std::bad_alloc&) {
            return UI_ERROR_OUT_OF_MEMORY;
        }
#else
        return UI_ERROR_UNSUPPORTED;
#endif
        try {
#ifdef SUNFLECK_METAL
            if (app->renderer == UI_AUTO || app->renderer == UI_METAL) {
                app->gpu = app->window->metal();
            }
#endif
#ifdef SUNFLECK_SKIA_D3D12
            if (app->renderer == UI_AUTO) {
                auto gpu = std::make_unique<SkiaD3d12Surface>();
                if (gpu->open(app->window)) app->gpu = std::move(gpu);
            }
#endif
#ifdef SUNFLECK_D3D11
            if (app->renderer == UI_D3D11
#ifndef SUNFLECK_SKIA_D3D12
                || app->renderer == UI_AUTO
#endif
            ) {
                auto gpu = std::make_unique<D3d11Surface>();
                if (gpu->open(app->window)) {
                    app->gpu = std::move(gpu);
                }
            }
#endif
#ifdef SUNFLECK_GLES3
#ifdef _WIN32
            if (app->renderer == UI_GLES3 || app->renderer == UI_ANGLE) {
                auto gpu = std::make_unique<GlesSurface>();
                if (gpu->open(EGL_DEFAULT_DISPLAY, app->window, 0)) {
                    app->gpu = std::move(gpu);
                }
            }
#else
            if (app->renderer == UI_AUTO || app->renderer == UI_GLES3) {
                auto gpu = std::make_unique<GlesSurface>();
                const auto visual = XVisualIDFromVisual(DefaultVisual(app->display, DefaultScreen(app->display)));
                if (gpu->open(app->display, app->window, static_cast<int>(visual))) {
                    app->gpu = std::move(gpu);
                }
            }
#endif
#endif
            app->update_renderer_title();
        } catch (const std::bad_alloc&) {
            app->close_window();
            return UI_ERROR_OUT_OF_MEMORY;
        }
        if (!app->gpu) {
            if (app->renderer == UI_ANGLE) {
                std::fprintf(stderr, "Sunfleck: ANGLE unavailable, using CPU rendering\n");
            } else if (app->renderer != UI_AUTO && app->renderer != UI_SOFTWARE) {
                app->close_window();
                return UI_ERROR_PLATFORM;
            }
        }
    }
    app->opened = true;
#ifdef SUNFLECK_MACOS
    if (app->window) {
        app->window->show();
    }
#endif
    return UI_OK;
}

static ui_result poll_window(ui_app* app, uint32_t timeout_ms)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!app->opened || app->dispatching) {
        return UI_ERROR_INVALID_STATE;
    }
    app->sync_window_frame();
    bool handled = false;
#ifdef SUNFLECK_MACOS
    if (app->window && !app->closing) {
        handled = macos::Window::poll(app->dirty && app->window->visible() ? 0 : timeout_ms);
    }
#endif
#ifdef _WIN32
    if (app->window && !app->closing) {
        if (!(app->dirty && app->width > 0 && app->height > 0) && timeout_ms) {
            if (MsgWaitForMultipleObjectsEx(0, nullptr, timeout_ms == INFINITE ? INFINITE - 1 : timeout_ms,
                                            QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_FAILED) {
                return UI_ERROR_PLATFORM;
            }
        }
        MSG msg{};
        unsigned events = 0;
        // Text services and other Win32 helpers own windows on this UI thread too.
        // Filtering by the preview HWND can strand their messages and stall input.
        while (!app->closing && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(msg.wParam));
                app->closing = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            handled = true;
            if (++events >= 256 || (events >= 32 && app->dragged_control && app->dirty)) {
                break;
            }
        }
    }
#endif
#if defined(SUNFLECK_X11)
    if (app->display && !app->closing) {
        auto* d = app->display;
#ifdef SUNFLECK_ATSPI
        x11_accessibility::poll();
        if (app->accessibility_window) { timeout_ms = std::min(timeout_ms, 50u); }
#endif
        const bool caret = app->mapped && app->focused && app->focus && is_editor(app->focus->kind) &&
            !app->focus->select_all && !app->focus->text_stops.empty();
        const bool spinner = app->mapped && std::any_of(app->nodes.begin(), app->nodes.end(), [&](const auto& node) {
            return app->owns_input(node.get()) && node->kind == UI_SPINNER && node->visible.right > node->visible.left &&
                node->visible.bottom > node->visible.top;
        });
        if (caret || spinner || app->drop_target.pending) {
            timeout_ms = std::min(timeout_ms, 100u);
        }
        if ((!app->dirty || !app->mapped) && timeout_ms && !XPending(d)) {
            pollfd connection{ConnectionNumber(d), POLLIN, 0};
            const int result = poll(&connection, 1, static_cast<int>(std::min(timeout_ms, 2147483647u)));
            if ((result < 0 && errno != EINTR) || (connection.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                return UI_ERROR_PLATFORM;
            }
        }
        unsigned events = 0;
        while (!app->closing && XPending(d)) {
            XEvent event{};
            XNextEvent(d, &event);
            handled = true;
            if (x11_drop::event(app->drop_target, d, app->window, event,
                [](void* context, int x, int y, ui_drop_kind kind, const ui_drop_data* data) {
                    return deliver_drop(*static_cast<ui_app*>(context), x, y, kind, data);
                }, app)) {
                if (++events >= 256) { break; }
                continue;
            }
            if (XFilterEvent(&event, None)) {
                continue;
            }
            switch (event.type) {
            case MapNotify:
                app->mapped = true;
                app->dirty = true;
                if (std::exchange(app->pending_native_focus, false) && app->focus) {
                    XSetInputFocus(d, app->window, RevertToParent, CurrentTime);
                    XFlush(d);
                }
                break;
            case UnmapNotify:
                x11_drop::cancel(app->drop_target, d, app->window);
                app->cancel_drag();
                app->pressed_control = nullptr;
                app->chrome_pressed = UI_DOCK_PART_COUNT;
                if (app->chrome_manual) { XUngrabPointer(d, CurrentTime); app->chrome_manual = false; }
                app->native_dock_move = false;
                app->mapped = false;
                break;
            case Expose:
                app->dirty = true;
                break;
            case ConfigureNotify:
                if (app->dock_window && app->workspace && !app->workspace->drag.prepared && app->native_position_known && !app->native_move_cancelled &&
                    app->width == event.xconfigure.width && app->height == event.xconfigure.height) {
                    Window root = 0, child = 0; int sx = 0, sy = 0, x = 0, y = 0, wx = 0, wy = 0; unsigned mask = 0;
                    XTranslateCoordinates(d, app->window, DefaultRootWindow(d), 0, 0, &wx, &wy, &child);
                    if ((wx != app->native_x || wy != app->native_y) && XQueryPointer(d, app->window, &root, &child, &sx, &sy, &x, &y, &mask) &&
                        (mask & Button1Mask) && y < 0 && docking::begin_window_drag(*app->workspace, app->dock_window, x, y)) {
                        app->native_dock_move = true;
                        app->native_origin_x = app->native_x; app->native_origin_y = app->native_y;
                        app->chrome_origin_width = app->width; app->chrome_origin_height = app->height;
                        app->workspace->drag.active = true;
                    }
                }
                app->width = event.xconfigure.width;
                app->height = event.xconfigure.height;
                app->xim_spot = {-1, -1};
                app->dirty = true;
                break;
            case PropertyNotify:
                if (app->workspace && event.xproperty.atom == XInternAtom(d, "_NET_WM_STATE", False)) {
                    Atom actual = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* data = nullptr;
                    if (XGetWindowProperty(d, app->window, event.xproperty.atom, 0, 1024, False, XA_ATOM, &actual, &format, &count, &remaining, &data) == Success && format == 32) {
                        const auto horizontal = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False), vertical = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
                        bool h = false, v = false;
                        for (unsigned long i = 0; i < count; ++i) { const auto atom = reinterpret_cast<Atom*>(data)[i]; h |= atom == horizontal; v |= atom == vertical; }
                        app->maximized = h && v; app->dirty = true;
                    }
                    if (data) { XFree(data); }
                }
                break;
            case FocusIn:
                if (event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ||
                    event.xfocus.detail == NotifyInferior) {
                    break;
                }
                app->focused = true;
                app->reset_caret();
                app->sync_xim_focus();
                break;
            case FocusOut:
                if (event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ||
                    event.xfocus.detail == NotifyInferior) {
                    break;
                }
                app->focused = false;
                app->dirty = true;
                app->sync_xim_focus();
                break;
            case KeyPress:
                app->edit_x11(event.xkey);
                break;
            case SelectionRequest:
                app->selection_request(event.xselectionrequest);
                break;
            case SelectionNotify:
                app->selection_notify(event.xselection);
                break;
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    if (app->begin_frame_drag(event.xbutton.x, event.xbutton.y, event.xbutton.time)) { break; }
                    const auto part = app->caption_part(event.xbutton.x, event.xbutton.y);
                    if (part >= UI_DOCK_PART_WINDOW_MINIMIZE && part <= UI_DOCK_PART_WINDOW_CLOSE) {
                        if (XGrabPointer(d, app->window, False, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime) == GrabSuccess) { app->chrome_pressed = part; }
                        app->caption_hover(event.xbutton.x, event.xbutton.y); app->dirty = true; break;
                    }
                    app->hover(event.xbutton.x, event.xbutton.y);
                    app->pressed_control = app->hovered_control;
                    app->dirty = app->dirty || app->pressed_control;
                    app->begin_drag(event.xbutton.x, event.xbutton.y);
                }
                break;
            case MotionNotify:
                if (app->chrome_manual) { app->move_frame(event.xmotion.x, event.xmotion.y); break; }
                app->caption_hover(event.xmotion.x, event.xmotion.y);
                if (app->chrome_hover != UI_DOCK_PART_COUNT || app->chrome_pressed != UI_DOCK_PART_COUNT) { break; }
                if (app->pointer_handled(UI_POINTER_MOVE,event.xmotion.x,event.xmotion.y)) break;
                if (auto* node = app->dragged_control) {
                    app->update_drag_value(*node, event.xmotion.x, event.xmotion.y);
                } else {
                    app->hover(event.xmotion.x, event.xmotion.y);
                }
                break;
            case LeaveNotify:
                app->caption_hover(-1, -1);
                app->hover(-1, -1);
                break;
            case ButtonRelease: {
                const auto& button = event.xbutton;
                if (button.button == Button4 || button.button == Button5) {
                    app->scroll_gallery(button.button == Button5 ? 1 : -1, button.x, button.y);
                } else if (button.button == Button3) {
                    app->context_menu(button.x, button.y);
                } else if (button.button == Button1) {
                    if (app->chrome_pressed != UI_DOCK_PART_COUNT) {
                        const auto part = std::exchange(app->chrome_pressed, UI_DOCK_PART_COUNT);
                        XUngrabPointer(d, CurrentTime);
                        if (app->caption_part(button.x, button.y) == part) { app->caption_action(part); }
                        app->dirty = true; break;
                    }
                    if (app->chrome_manual || app->native_dock_move) {
                        app->chrome_manual = false; app->native_dock_move = false; app->chrome_resize = -1;
                        XUngrabPointer(d, CurrentTime);
                        if (app->workspace && app->workspace->drag.prepared) { (void)docking::pointer(*app->workspace, app->dock_window, UI_POINTER_UP, button.x, button.y); }
                        XFlush(d); break;
                    }
                    app->dirty = (std::exchange(app->pressed_control, nullptr) != nullptr) || app->dirty;
                    if (app->end_pointer_event(button.x,button.y)) break;
                    if (app->dragged_control) {
                        app->finish_drag(button.x, button.y);
                    } else {
                        app->click(button.x, button.y);
                        app->sync_xim_focus();
                    }
                }
                break;
            }
            case KeyRelease: {
                const auto key = XLookupKeysym(&event.xkey, 0);
                if (app->focused && key == XK_Escape) {
                    app->cancel_frame_drag();
                    if (app->workspace) { docking::cancel(*app->workspace); }
                    for (const auto& node : app->nodes) {
                        if (!app->owns_input(node.get())) { continue; }
                        node->expanded = node->kind == UI_COMBO_BOX ? false : node->expanded;
                        if (is_popup(node->kind)) {
                            node->value = 0;
                        }
                    }
                    app->dirty = true;
                } else if (app->focused && (key == XK_space || key == XK_Return)) {
                    app->key(false, false, 0);
                } else if (app->focused && key == XK_Tab) {
                    app->key(true, (event.xkey.state & ShiftMask) != 0, 0);
                } else if (app->focused && (key == XK_Left || key == XK_Right || key == XK_Up || key == XK_Down)) {
                    app->key(false, false, key == XK_Left || key == XK_Up ? -5 : 5, true, key == XK_Up || key == XK_Down);
                }
                break;
            }
            case ClientMessage:
                if (event.xclient.message_type == app->wm_protocols && event.xclient.format == 32 &&
                    static_cast<Atom>(event.xclient.data.l[0]) == app->wm_delete) {
                    if (!app->dock_window || !app->workspace || docking::close_window(*app->workspace, app->dock_window)) { app->closing = true; }
                }
                break;
            case DestroyNotify:
                app->window = 0;
                app->closing = true;
                break;
            }
            if (++events >= 32 && app->dragged_control && app->dirty) {
                break;
            }
        }
        if (app->window) {
            x11_drop::expire(app->drop_target, d, app->window);
        }
        if (!app->closing && app->window && app->dock_window) {
            Window root = 0, child = 0; int sx = 0, sy = 0, x = 0, y = 0; unsigned mask = 0;
            if (app->native_move_cancelled && XQueryPointer(d, app->window, &root, &child, &sx, &sy, &x, &y, &mask) && !(mask & Button1Mask)) { app->native_move_cancelled = false; }
            if (app->native_dock_move && app->workspace) {
                char keys[32]{}; XQueryKeymap(d, keys);
                const auto escape = XKeysymToKeycode(d, XK_Escape);
                if (escape && (keys[escape / 8] & (1 << (escape % 8)))) {
                    app->cancel_frame_drag();
                } else if (!app->workspace->drag.prepared) { app->native_dock_move = false; }
                else if (XQueryPointer(d, app->window, &root, &child, &sx, &sy, &x, &y, &mask)) {
                    int wx = 0, wy = 0; XTranslateCoordinates(d, app->window, DefaultRootWindow(d), 0, 0, &wx, &wy, &child);
                    if (wx != app->native_origin_x || wy != app->native_origin_y) { app->workspace->drag.active = true; }
                    (void)docking::pointer(*app->workspace, app->dock_window, UI_POINTER_MOVE, x, y);
                    if (!(mask & Button1Mask)) {
                        app->native_dock_move = false;
                        if (app->chrome_manual) { XUngrabPointer(d, CurrentTime); app->chrome_manual = false; }
                        (void)docking::pointer(*app->workspace, app->dock_window, UI_POINTER_UP, x, y);
                    }
                } else { app->native_dock_move = false; docking::cancel(*app->workspace); }
            }
            XTranslateCoordinates(d, app->window, DefaultRootWindow(d), 0, 0, &app->native_x, &app->native_y, &child);
            app->native_position_known = true;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - app->animation_tick).count();
        const auto tick = elapsed / 100;
        if (tick != app->last_animation_tick && (caret || spinner)) {
            app->last_animation_tick = tick;
            const bool visible = !caret || (tick / 5) % 2 == 0;
            app->dirty = app->dirty || app->caret_visible != visible || spinner;
            app->caret_visible = visible;
            if (spinner) {
                for (const auto& node : app->nodes) {
                    if (!app->owns_input(node.get())) { continue; }
                    if (node->kind == UI_SPINNER) {
                        node->value = tick % 8;
                    }
                }
            }
        }
    }
#endif
    if (app->render_failed) {
        app->render_failed = false;
        app->closing = true;
        app->close_window();
        return UI_ERROR_PLATFORM;
    }
    if (app->closing) {
        app->close_window();
        return UI_EXITED;
    }
#if defined(SUNFLECK_X11)
    if (!app->headless && !app->mapped) {
        return handled ? UI_OK : UI_TIMEOUT;
    }
#endif
#ifdef _WIN32
    if (app->window && IsIconic(app->window)) return handled ? UI_OK : UI_TIMEOUT;
    if (app->window) {
        const auto now = app->frame_time();
        const auto blink = GetCaretBlinkTime();
        const bool blinking = app->focused && app->focus && is_editor(app->focus->kind) && !app->focus->select_all &&
            blink && blink != INFINITE && std::find(app->interactive.begin(),app->interactive.end(),app->focus) != app->interactive.end();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now-app->hosted_caret_tick).count();
        const bool visible = !blinking || (std::max(0LL,static_cast<long long>(elapsed))/blink)%2 == 0;
        if (app->caret_visible != visible) { app->caret_visible = visible; app->dirty = true; }
        if (app->animated_spinner) {
            const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()/100;
            const auto phase = app->reduced_motion ? 0u : static_cast<unsigned>(tick%8);
            for (const auto& node : app->nodes) {
                if (node->kind == UI_SPINNER && node->value != phase && app->owns_input(node.get()) &&
                    node->visible.right > node->visible.left && node->visible.bottom > node->visible.top) {
                    node->value = phase;
                    app->dirty = true;
                }
            }
        }
    }
#endif
#ifdef SUNFLECK_MACOS
    if (app->window && !app->window->visible()) {
        return handled ? UI_OK : UI_TIMEOUT;
    }
    if (app->window && app->focused && app->focus && is_editor(app->focus->kind)) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(app->frame_time() - app->hosted_caret_tick);
        const bool visible = (elapsed.count() / 500) % 2 == 0;
        if (visible != app->caret_visible) {
            app->caret_visible = visible;
            app->dirty = true;
        }
    }
#endif
    if (app->headless && app->hosted_input && app->focused && app->focus && is_editor(app->focus->kind)) {
        const auto& view = app->focus->hosted_editor;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - app->hosted_caret_tick).count();
        const bool visible = view.composing || (elapsed / 500) % 2 == 0;
        if (view.first == view.last && visible != app->caret_visible) { app->caret_visible = visible; app->dirty = true; }
    }
    if (app->dirty && app->width > 0 && app->height > 0) {
        try {
            if (!app->render()) {
                app->closing = true;
                app->close_window();
                return UI_ERROR_PLATFORM;
            }
#ifdef SUNFLECK_ATSPI
            update_x11_accessibility(*app);
#endif
        } catch (const std::bad_alloc&) {
            return UI_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return UI_ERROR_PLATFORM;
        }
        return UI_OK;
    }
    if (app->headless && timeout_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
    return handled ? UI_OK : UI_TIMEOUT;
}

namespace {
ui_window_context* find_window(ui_app& app, unsigned id)
{
    if (app.dock_window == id) { return &app; }
    if (!id) { return app.main_state.get(); }
    const auto found = app.dock_windows.find(id);
    return found == app.dock_windows.end() ? nullptr : found->second.get();
}
ui_result sync_dock_windows(ui_app& app)
{
    if (!app.workspace) { return UI_OK; }
    auto& dock = *app.workspace;
    struct PendingFocus { ui_node* node; unsigned window; bool keyboard; bool native; };
    std::vector<PendingFocus> pending_focus;
    std::map<ui_node*, unsigned> content_windows;
    for (const auto& item : dock.items) {
        const auto& placement = dock.layout.placements.at(item->id);
        unsigned target = UINT32_MAX - 1;
        const auto group = dock.layout.regions.find(placement.group);
        if ((placement.state == UI_DOCK_DOCKED || placement.state == UI_DOCK_FLOATING) && group != dock.layout.regions.end() && group->second.active == item->id) {
            target = docking::window_of(dock.layout, placement.group);
        } else if (dock.layout.expanded == item->id) { target = 0; }
        content_windows[item->content] = target;
    }
    const auto destination = [&](ui_node* node) -> unsigned {
        for (auto* parent = node; parent; parent = parent->parent) {
            const auto found = content_windows.find(parent);
            if (found != content_windows.end()) { return found->second; }
        }
        return UINT32_MAX;
    };
    const auto inspect = [&](ui_window_context& context) {
        const auto target = destination(context.focus);
        if (target != UINT32_MAX && target != context.dock_window) {
            if (target != UINT32_MAX - 1) { pending_focus.push_back({context.focus, target, context.keyboard_focus, context.focused}); }
            context.focus = nullptr; context.keyboard_focus = false; context.dirty = true;
        }
        if (destination(context.hovered_control) != UINT32_MAX && destination(context.hovered_control) != context.dock_window) { context.hovered_control = nullptr; }
        if (destination(context.pressed_control) != UINT32_MAX && destination(context.pressed_control) != context.dock_window) { context.pressed_control = nullptr; }
        std::erase_if(context.interactive, [&](auto* node) { const auto target_window = destination(node); return target_window != UINT32_MAX && target_window != context.dock_window; });
        std::erase_if(context.scrollable, [&](auto* node) { const auto target_window = destination(node); return target_window != UINT32_MAX && target_window != context.dock_window; });
    };
    inspect(app);
    for (auto& [id, context] : app.dock_windows) { (void)id; inspect(*context); }
    if (!dock.initial) {
        const auto status = ui_dock_capture_default(&dock);
        if (status != UI_OK) { return status; }
    }
    for (auto it = app.dock_windows.begin(); it != app.dock_windows.end();) {
        if (std::none_of(dock.layout.floating.begin(), dock.layout.floating.end(), [&](const auto& w) { return w.id == it->first; })) {
            if (dock.drag.window == it->first || dock.drag.target_window == it->first) { docking::cancel(dock); }
            { WindowScope scope(app, it->second.get()); app.close_window(); }
            dock.views.erase(it->first); it = app.dock_windows.erase(it);
        } else { ++it; }
    }
    for (auto& floating : dock.layout.floating) {
        if (app.dock_windows.contains(floating.id)) { continue; }
        auto context = std::make_unique<ui_window_context>(); context->application = &app; context->dock_window = floating.id;
        context->session = app.active_session;
        if (app.next_window_id == UINT32_MAX) { return UI_ERROR_OUT_OF_MEMORY; }
        context->debug_window_id = app.next_window_id++;
        context->dock_root = std::make_unique<ui_node>(); context->dock_root->owner = &app;
        context->dock_root->window_owner = app.active_session;
        context->dock_root->kind = UI_DOCK_HOST; context->dock_root->workspace = &dock; context->dock_root->dock_window = floating.id;
        context->root = context->dock_root.get(); context->configured_window = true;
        context->width = floating.bounds.width; context->height = floating.bounds.height;
        context->window_title = L"Sunfleck Dock"; context->window_title_utf8 = "Sunfleck Dock";
        auto* window = context.get(); app.dock_windows.emplace(floating.id, std::move(context));
        ui_result result;
        {
            WindowScope scope(app, window);
            result = ui_app_open_window(&app);
            if (result == UI_OK && !app.headless) {
#ifdef _WIN32
                const auto scale = [&](int value) { return MulDiv(value, app.dpi, 96); };
                POINT point{scale(floating.bounds.x), scale(floating.bounds.y)};
                const auto monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
                MONITORINFO info{sizeof(info)};
                if (GetMonitorInfoW(monitor, &info)) {
                    point.x = std::clamp(point.x, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - scale(floating.bounds.width)));
                    point.y = std::clamp(point.y, info.rcWork.top, std::max(info.rcWork.top, info.rcWork.bottom - scale(floating.bounds.height)));
                }
                // Keep ordinary top-level stacking: ownership would pin this
                // window above the main window even when the main window is active.
                SetWindowPos(app.window, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#elif defined(SUNFLECK_X11)
                const auto scale = [&](int value) { return value * static_cast<int>(app.dpi) / 96; };
                const int screen = DefaultScreen(app.display);
                const int x = std::clamp(scale(floating.bounds.x), 0, std::max(0, DisplayWidth(app.display, screen) - scale(floating.bounds.width)));
                const int y = std::clamp(scale(floating.bounds.y), 0, std::max(0, DisplayHeight(app.display, screen) - scale(floating.bounds.height)));
                // Group related windows without dialog/transient stacking rules.
                XWMHints hints{}; hints.flags = WindowGroupHint; hints.window_group = main_window(app).window;
                XSetWMHints(app.display, app.window, &hints);
                XMoveWindow(app.display, app.window, x, y); XFlush(app.display);
#elif defined(SUNFLECK_MACOS)
                app.window->move(floating.bounds.x, floating.bounds.y);
#endif
            }
        }
        if (result != UI_OK) {
            { WindowScope scope(app, window); app.close_window(); }
            app.dock_windows.erase(floating.id); return result;
        }
    }
    for (const auto& focus : pending_focus) {
        auto* target = find_window(app, focus.window);
        if (!target) { continue; }
        target->focus = focus.node; target->keyboard_focus = focus.keyboard; target->dirty = true;
        if (!focus.native || app.headless) { continue; }
#ifdef _WIN32
        SetFocus(target->window);
#elif defined(SUNFLECK_X11)
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(target->display, target->window, &attributes) && attributes.map_state == IsViewable) {
            XSetInputFocus(target->display, target->window, RevertToParent, CurrentTime);
            XFlush(target->display);
        } else {
            target->pending_native_focus = true;
        }
#elif defined(SUNFLECK_MACOS)
        if (target->window) { target->window->show(); }
#endif
    }
    return UI_OK;
}
}

#ifdef _WIN32
static void sync_drag_overlay(ui_app& app)
{
    const auto clear = [&] {
        if (app.dock_overlay) { DestroyWindow(app.dock_overlay); app.dock_overlay = nullptr; }
        if (app.workspace && app.workspace->external_overlay) { app.workspace->external_overlay = false; dock_app_dirty(&app); }
    };
    if (app.headless || !app.workspace || !app.workspace->drag.active || app.workspace->drag.target_window == UINT32_MAX) { clear(); return; }
    auto* target = find_window(app, app.workspace->drag.target_window);
    if (!target || !target->window || target->closing || target->width <= 0 || target->height <= 0) { clear(); return; }
    if (!app.dock_overlay) {
        WNDCLASSW type{}; type.lpfnWndProc = DefWindowProcW; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = L"SunfleckDockOverlay";
        RegisterClassW(&type);
        app.dock_overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
            type.lpszClassName, L"", WS_POPUP, 0, 0, 1, 1, main_window(app).window, nullptr, type.hInstance, nullptr);
        if (!app.dock_overlay) { return; }
        app.workspace->external_overlay = true; dock_app_dirty(&app);
    }
    std::vector<FillRectCommand> commands;
    docking::paint_drag_overlay(*app.workspace, app.workspace->drag.target_window, commands);
    std::vector<uint8_t> pixels;
    rasterize(pixels, target->width, target->height, commands, PixelOrder::Bgra);
    BITMAPINFO bitmap{}; bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bitmap.bmiHeader.biWidth = target->width;
    bitmap.bmiHeader.biHeight = -target->height; bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32; bitmap.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(nullptr), memory = CreateCompatibleDC(dc); void* bits = nullptr;
    HBITMAP surface = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (memory && surface && bits) {
        const auto previous = SelectObject(memory, surface);
        std::memcpy(bits, pixels.data(), pixels.size());
        POINT position{0, 0}; ClientToScreen(target->window, &position);
        POINT source{0, 0}; SIZE size{target->width, target->height}; BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(app.dock_overlay, dc, &position, &size, memory, &source, 0, &blend, ULW_ALPHA);
        SetWindowPos(app.dock_overlay, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SelectObject(memory, previous);
    }
    if (surface) { DeleteObject(surface); } if (memory) { DeleteDC(memory); } if (dc) { ReleaseDC(nullptr, dc); }
}
#elif defined(SUNFLECK_X11)
static void clear_drag_overlay(ui_app& app)
{
    if (app.dock_overlay_display) {
        if (app.dock_overlay) { XDestroyWindow(app.dock_overlay_display, app.dock_overlay); }
        XCloseDisplay(app.dock_overlay_display);
        app.dock_overlay_display = nullptr; app.dock_overlay = 0;
    }
    app.dock_overlay_commands.clear(); app.dock_overlay_width = 0; app.dock_overlay_height = 0;
    if (app.workspace && app.workspace->external_overlay) { app.workspace->external_overlay = false; dock_app_dirty(&app); }
}
static void sync_drag_overlay(ui_app& app)
{
    if (app.headless || !app.workspace || !app.workspace->drag.active || app.workspace->drag.target_window == UINT32_MAX) { clear_drag_overlay(app); return; }
    auto* target = find_window(app, app.workspace->drag.target_window);
    if (!target || !target->window || target->closing || target->width <= 0 || target->height <= 0) { clear_drag_overlay(app); return; }
    if (!app.dock_overlay_display) {
        app.dock_overlay_display = XOpenDisplay(nullptr);
        if (!app.dock_overlay_display) { return; }
        int event = 0, error = 0, major = 0, minor = 0;
        if (!XShapeQueryExtension(app.dock_overlay_display, &event, &error) ||
            !XShapeQueryVersion(app.dock_overlay_display, &major, &minor) || (major == 1 && minor < 1)) { clear_drag_overlay(app); return; }
    }
    auto* d = app.dock_overlay_display;
    const auto root = DefaultRootWindow(d);
    if (!app.dock_overlay) {
        XSetWindowAttributes attributes{}; attributes.override_redirect = True; attributes.background_pixel = 0;
        app.dock_overlay = XCreateWindow(d, root, 0, 0, 1, 1, 0, CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel, &attributes);
        XStoreName(d, app.dock_overlay, "Sunfleck Dock Preview");
        XShapeCombineRectangles(d, app.dock_overlay, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
        app.workspace->external_overlay = true; dock_app_dirty(&app);
    }
    std::vector<FillRectCommand> commands;
    docking::paint_drag_overlay(*app.workspace, app.workspace->drag.target_window, commands);
    Window child = 0; int x = 0, y = 0;
    XTranslateCoordinates(target->display, target->window, DefaultRootWindow(target->display), 0, 0, &x, &y, &child);
    XMoveResizeWindow(d, app.dock_overlay, x, y, target->width, target->height);
    XRaiseWindow(d, app.dock_overlay);
    if (app.dock_overlay_width == target->width && app.dock_overlay_height == target->height &&
        std::equal(commands.begin(), commands.end(), app.dock_overlay_commands.begin(), app.dock_overlay_commands.end(), [](const auto& a, const auto& b) {
            return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.color == b.color;
        })) { XFlush(d); return; }
    std::vector<uint8_t> pixels;
    rasterize(pixels, target->width, target->height, commands, PixelOrder::Rgba);
    auto* image = XCreateImage(d, DefaultVisual(d, DefaultScreen(d)), DefaultDepth(d, DefaultScreen(d)), ZPixmap,
        0, nullptr, target->width, target->height, 32, 0);
    if (!image) { return; }
    image->data = static_cast<char*>(std::calloc(image->bytes_per_line, target->height));
    if (!image->data) { XDestroyImage(image); return; }
    const size_t stride = (static_cast<size_t>(target->width) + 7) / 8;
    std::vector<char> mask(stride * target->height);
    // A shaped, stippled preview also works without a compositing window manager.
    constexpr unsigned threshold[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const auto* visual = DefaultVisual(d, DefaultScreen(d));
    for (int y = 0; y < target->height; ++y) {
        for (int x = 0; x < target->width; ++x) {
            const auto* pixel = pixels.data() + (static_cast<size_t>(y) * target->width + x) * 4;
            const unsigned alpha = pixel[3];
            if (alpha <= threshold[y % 4][x % 4] * 16 + 7) { continue; }
            mask[static_cast<size_t>(y) * stride + x / 8] |= static_cast<char>(1u << (x % 8));
            const auto channel = [&](int index, unsigned long bits) { return ui_app::x11_channel(std::min(255u, pixel[index] * 255u / alpha), bits); };
            XPutPixel(image, x, y, channel(0, visual->red_mask) | channel(1, visual->green_mask) | channel(2, visual->blue_mask));
        }
    }
    const auto bitmap = XCreateBitmapFromData(d, root, mask.data(), target->width, target->height);
    XShapeCombineMask(d, app.dock_overlay, ShapeBounding, 0, 0, bitmap, ShapeSet); XFreePixmap(d, bitmap);
    auto gc = XCreateGC(d, app.dock_overlay, 0, nullptr);
    XMapRaised(d, app.dock_overlay);
    XPutImage(d, app.dock_overlay, gc, image, 0, 0, 0, 0, target->width, target->height);
    XFreeGC(d, gc); XDestroyImage(image); XFlush(d);
    app.dock_overlay_commands = std::move(commands); app.dock_overlay_width = target->width; app.dock_overlay_height = target->height;
}
#endif

static ui_result poll_session(ui_app* app, uint32_t timeout_ms)
{
    try {
        poll_fleck_refresh(app);
        const auto sync = sync_dock_windows(*app);
        if (sync != UI_OK) { return sync; }
        if (app->dirty) { for (auto& [id, window] : app->dock_windows) { (void)id; window->dirty = true; } }
        // Bound the latency of queued cross-thread model notifications without
        // making worker threads touch Win32/X11 window state.
        auto result = poll_window(app, std::min(timeout_ms, 16u));
        if (result != UI_OK && result != UI_TIMEOUT) {
            for (auto& [id, window] : app->dock_windows) { (void)id; WindowScope scope(*app, window.get()); app->close_window(); }
#ifdef _WIN32
            if (app->dock_overlay) { DestroyWindow(app->dock_overlay); app->dock_overlay = nullptr; }
#elif defined(SUNFLECK_X11)
            clear_drag_overlay(*app);
#endif
            return result;
        }
        for (auto& [id, window] : app->dock_windows) {
            WindowScope scope(*app, window.get());
            const auto status = poll_window(app, 0);
            if (status != UI_OK && status != UI_TIMEOUT && status != UI_EXITED) { return status; }
            if (status == UI_OK) { result = UI_OK; }
            if (status == UI_EXITED && app->workspace) { (void)docking::close_window(*app->workspace, id); }
            if (!app->headless && app->workspace) {
                auto found = std::find_if(app->workspace->layout.floating.begin(), app->workspace->layout.floating.end(), [&](const auto& w) { return w.id == id; });
                if (found != app->workspace->layout.floating.end()) {
                    auto& bounds = found->bounds;
                    bounds.width = std::max(64, app->width * 96 / static_cast<int>(app->dpi)); bounds.height = std::max(64, app->height * 96 / static_cast<int>(app->dpi));
#ifdef _WIN32
                    RECT rect{}; if (app->window && GetWindowRect(app->window, &rect)) { bounds.x = MulDiv(rect.left, 96, app->dpi); bounds.y = MulDiv(rect.top, 96, app->dpi); }
#elif defined(SUNFLECK_X11)
                    int x = 0, y = 0; Window child = 0;
                    if (app->display && app->window && XTranslateCoordinates(app->display, app->window, DefaultRootWindow(app->display), 0, 0, &x, &y, &child)) {
                        bounds.x = x * 96 / static_cast<int>(app->dpi); bounds.y = y * 96 / static_cast<int>(app->dpi);
                    }
#elif defined(SUNFLECK_MACOS)
                    if (app->window) {
                        const auto native = app->window->bounds();
                        bounds.x = native.x;
                        bounds.y = native.y;
                    }
#endif
                }
            }
        }
#if defined(_WIN32) || defined(SUNFLECK_X11)
        sync_drag_overlay(*app);
#endif
        return result;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; } catch (...) { return UI_ERROR_PLATFORM; }
}

static ui_result sample_animation(ui_app::Playback& playback, double ms) {
    auto sample = playback.timeline.sample(ms);
    auto* app = playback.node->owner;
    if (app->reduced_motion) {
        sample.value = playback.timeline.auto_reverse ? playback.timeline.frames.front().value : playback.timeline.frames.back().value;
        sample.completed = true;
    }
    const bool notify = sample.completed && !playback.ended && app->animation_completed;
    if (notify) {
        try { app->completions.reserve(app->completions.size()+1); }
        catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
    }
    const auto result = sample.completed && !playback.hold
        ? ui_node_clear_property_layer(playback.node, playback.property, UI_PRECEDENCE_ANIMATION)
        : ui_node_set_property_layer(playback.node, playback.property, UI_PRECEDENCE_ANIMATION, &sample.value.value);
    if (result == UI_OK) {
        playback.ended = sample.completed;
        if (notify) app->completions.push_back({playback.node,playback.lifetime,playback.property});
    }
    return result;
}

ui_result ui_node_start_animation(ui_node* node, ui_property property, const ui_timeline* options) {
    if (!node || !options || !options->frames || options->frame_count < 2 || !options->duration_ms ||
        options->auto_reverse > 1 || options->hold_end > 1) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    ui_property_descriptor descriptor{sizeof(descriptor),1};
    if (ui_property_describe(node->kind,property,&descriptor) != UI_OK ||
        !(descriptor.flags & UI_PROPERTY_ANIMATABLE)) return UI_ERROR_INVALID_ARGUMENT;
    try {
        animation::Timeline timeline;
        timeline.duration_ms = options->duration_ms;
        timeline.delay_ms = options->delay_ms;
        timeline.repeat = options->repeat_count;
        timeline.auto_reverse = options->auto_reverse != 0;
        timeline.frames.reserve(options->frame_count);
        double previous = -1;
        for (uint32_t i = 0; i < options->frame_count; ++i) {
            const auto& frame = options->frames[i];
            if (!std::isfinite(frame.position) || frame.position <= previous || frame.position > 1 ||
                (i == 0 && frame.position != 0) || (i+1 == options->frame_count && frame.position != 1) ||
                frame.easing < UI_EASE_LINEAR || frame.easing > UI_EASE_IN_OUT) return UI_ERROR_INVALID_ARGUMENT;
            previous = frame.position;
            auto value = visual_layout::normalize(property,frame.value);
            if (value.kind == UI_VALUE_LENGTH && value.integer != UI_LENGTH_PIXELS) return UI_ERROR_INVALID_ARGUMENT;
            if (auto status = properties::validate(*node,property,value); status != UI_OK) return status;
            BindingValue owned;
            if (auto status = binding_copy_value(value,owned,descriptor.kind); status != UI_OK) return status;
            timeline.frames.push_back({frame.position,std::move(owned),static_cast<animation::Easing>(frame.easing)});
        }
        if (!node->animation_lifetime) node->animation_lifetime = std::make_shared<char>();
        auto& active = node->owner->animations;
        active.reserve(active.size()+1);
        ui_app::Playback playback{node,node->animation_lifetime,property,std::move(timeline),node->owner->frame_time()};
        playback.hold = options->hold_end != 0;
        if (auto status = sample_animation(playback,0); status != UI_OK) return status;
        std::erase_if(active,[&](const auto& p){ return p.lifetime.expired() || (p.node == node && p.property == property); });
        active.push_back(std::move(playback));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

static ui_result play_tracks(ui_node* node, const std::vector<animation::Track>& tracks, std::string_view group = {}) {
    try {
        std::vector<ui_app::Playback> prepared;
        std::vector<std::pair<ui_property,properties::Slot>> before;
        auto& active = node->owner->animations;
        active.reserve(active.size()+tracks.size());
        if (!node->animation_lifetime) node->animation_lifetime = std::make_shared<char>();
        const auto now = node->owner->frame_time();
        for (const auto& track : tracks) {
            for (const auto& frame : track.timeline.frames)
                if (auto status = properties::validate(*node,track.property,frame.value.value); status != UI_OK) return status;
            auto found = node->property_slots.find(track.property);
            before.emplace_back(track.property,found == node->property_slots.end() ? properties::Slot{} : found->second);
            prepared.push_back({node,node->animation_lifetime,track.property,track.timeline,now});
            prepared.back().hold = track.hold;
            prepared.back().state_group = group;
        }
        std::vector<ui_property> leaving;
        for (const auto& old : active) {
            if (!group.empty() && old.node == node && old.state_group == group && !old.lifetime.expired() &&
                std::none_of(tracks.begin(), tracks.end(), [&](const auto& track){ return track.property == old.property; })) {
                leaving.push_back(old.property);
                auto slot = node->property_slots.find(old.property);
                before.emplace_back(old.property,slot == node->property_slots.end() ? properties::Slot{} : slot->second);
            }
        }
        const auto completion_count = node->owner->completions.size();
        for (size_t i = 0; i < before.size(); ++i) {
            const auto status = i < prepared.size() ? sample_animation(prepared[i],0) :
                ui_node_clear_property_layer(node,before[i].first,UI_PRECEDENCE_ANIMATION);
            if (status != UI_OK) {
                node->owner->completions.resize(completion_count);
                while (i > 0) {
                    const auto& [property,slot] = before[--i];
                    if (slot.values.contains(UI_PRECEDENCE_ANIMATION))
                        ui_node_set_property_layer(node,property,UI_PRECEDENCE_ANIMATION,&slot.effective()->value);
                    else ui_node_clear_property_layer(node,property,UI_PRECEDENCE_ANIMATION);
                }
                return status;
            }
        }
        std::erase_if(active,[&](const auto& old){ return old.node == node && old.state_group == group &&
            std::find(leaving.begin(),leaving.end(),old.property) != leaving.end(); });
        for (auto& p : prepared) {
            std::erase_if(active,[&](const auto& old){ return old.lifetime.expired() || (old.node == node && old.property == p.property); });
            active.push_back(std::move(p));
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

static void update_host_state(ui_node& node, const char* group, const char* state,
                              std::string_view& current, const char* fallback) {
    const auto states = node.visual_groups.find(group);
    if (states == node.visual_groups.end()) return;
    if (!states->second.contains(state) && fallback) state = fallback;
    if (current == state) return;
    const auto next = states->second.find(state);
    // A partial template can omit the destination state. Leave the group's old tracks.
    const auto status = next == states->second.end() ? play_tracks(&node, {}, group) :
        play_tracks(&node, next->second, group);
    if (status == UI_OK) current = state;
}

ui_result ui_node_begin_storyboard(ui_node* node) {
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (node->storyboard.empty()) return UI_ERROR_INVALID_STATE;
    return play_tracks(node,node->storyboard);
}

ui_result ui_node_register_visual_state(ui_node* node, const char* group, const char* name, const char* storyboard_xml) {
    if (!node || !group || !name || !fleck::valid_binding_name(group) || !fleck::valid_binding_name(name))
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    try {
        std::vector<animation::Track> tracks;
        if (storyboard_xml && !fleck::read_storyboard(storyboard_xml,node->kind,tracks)) return UI_ERROR_INVALID_ARGUMENT;
        for (const auto& track : tracks)
            for (const auto& frame : track.timeline.frames)
                if (auto status = properties::validate(*node,track.property,frame.value.value); status != UI_OK) return status;
        node->visual_groups[group].insert_or_assign(name,std::move(tracks));
        const std::string_view group_name(group);
        if (group_name == "CommonStates") node->common_input_state = {};
        else if (group_name == "CheckStates") node->check_state = {};
        else if (group_name == "FocusStates") node->focus_state = {};
        else if (group_name == "SelectionStates") node->selection_state = {};
        else if (group_name == "ValidationStates") node->validation_state = {};
        else return UI_OK;
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_go_to_visual_state(ui_node* node, const char* group, const char* name) {
    if (!node || !group || !name) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    const auto states = node->visual_groups.find(group);
    if (states == node->visual_groups.end()) return UI_ERROR_INVALID_ARGUMENT;
    const auto found = states->second.find(name);
    if (found == states->second.end()) return UI_ERROR_INVALID_ARGUMENT;
    return play_tracks(node,found->second,group);
}

ui_result ui_node_control_animation(ui_node* node, ui_property property, ui_animation_action action, uint32_t ms) {
    if (!node || action < UI_ANIMATION_PAUSE || action > UI_ANIMATION_STOP) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    auto& active = node->owner->animations;
    auto it = std::find_if(active.begin(),active.end(),[&](const auto& p){
        return p.node == node && p.property == property && !p.lifetime.expired();
    });
    if (it == active.end()) return UI_ERROR_INVALID_STATE;
    if (action == UI_ANIMATION_STOP) {
        if (auto status = ui_node_clear_property_layer(node,property,UI_PRECEDENCE_ANIMATION); status != UI_OK) return status;
        active.erase(it);
        return UI_OK;
    }
    const auto now = node->owner->frame_time();
    const auto position = action == UI_ANIMATION_SEEK ? double(ms) : it->position(now);
    if (auto status = sample_animation(*it,position); status != UI_OK) return status;
    if (it->ended && !it->hold) active.erase(it);
    else {
        it->offset_ms = position;
        it->started = now;
        if (action != UI_ANIMATION_SEEK) it->paused = action == UI_ANIMATION_PAUSE;
    }
    return UI_OK;
}

ui_result ui_app_advance_animation_clock(ui_app* app,uint32_t elapsed_ms) {
    if (auto status = check(app); status != UI_OK) return status;
    if (!app->manual_frame_time) app->manual_frame_time = animation::Clock::now();
    *app->manual_frame_time += std::chrono::milliseconds(elapsed_ms);
    return UI_OK;
}

ui_result ui_app_set_animation_completed(ui_app* app,ui_animation_completed callback,void* context) {
    if (auto status = check(app); status != UI_OK) return status;
    app->completions.clear();
    app->animation_completed = callback;
    app->animation_context = context;
    return UI_OK;
}

namespace {
ui_result update_resource(ui_app* app, const char* key, const ui_binding_value* input, bool theme, ui_node* scope = nullptr)
{
    if (!app || !key || !fleck::valid_binding_name(key)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(app); status != UI_OK) return status;
    try {
        const std::string name(key);
        if (scope && !scope->resources) {
            if (!input) return UI_OK;
            scope->resources = std::make_unique<ui_node::Resources>();
        }
        auto& dictionary = scope ? scope->resources->overrides : theme ? app->theme_values : app->application_resources;
        auto next = dictionary;
        if (input) {
            if (input->kind == UI_VALUE_COMMAND || input->kind == UI_VALUE_MODEL) return UI_ERROR_INVALID_ARGUMENT;
            BindingValue copy;
            if (auto status = binding_copy_value(*input,copy,input->kind); status != UI_OK) return status;
            next.insert_or_assign(name,std::move(copy));
        } else next.erase(name);
        std::vector<ResourceUpdate> changes;
        for (const auto& node : app->nodes) {
            if (auto status = prepare_resource_updates(*node,{&dictionary,&next},changes,&name); status != UI_OK) return status;
        }
        if (auto status = apply_resource_updates(changes); status != UI_OK) return status;
        dictionary.swap(next);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
}
ui_result ui_node_set_resource_reference(ui_node* node, ui_property property, ui_value_precedence layer, const char* key) {
    if (!node || !key || (layer != UI_PRECEDENCE_STYLE && layer != UI_PRECEDENCE_LOCAL)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (!fleck::dynamic_style_target(node->kind,properties::name(property),key,property)) return UI_ERROR_INVALID_ARGUMENT;
    try {
        ui_node::ResourceReference reference{property,layer,key};
        std::optional<BindingValue> value;
        if (auto status = resolve_style_resource(*node,property,reference.key,{},value); status != UI_OK) return status;
        node->resource_references.reserve(node->resource_references.size()+1);
        if (auto status = apply_resource_value(*node,property,layer,value); status != UI_OK) return status;
        std::erase_if(node->resource_references,[&](const auto& old) { return old.property == property && old.layer == layer; });
        node->resource_references.push_back(std::move(reference));
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_node_set_resource(ui_node* node, const char* key, const ui_binding_value* value) {
    return node ? update_resource(node->owner,key,value,false,node) : UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_app_set_resource(ui_app* app, const char* key, const ui_binding_value* value) {
    return update_resource(app,key,value,false);
}
ui_result ui_app_set_theme_resource(ui_app* app, const char* key, const ui_binding_value* value) {
    return update_resource(app,key,value,true);
}
ui_result ui_app_set_reduced_motion(ui_app* app,uint32_t enabled) {
    if (enabled > 1) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(app); status != UI_OK) return status;
    app->reduced_motion = enabled != 0;
    binding_app_dirty(app);
    return UI_OK;
}


ui_result ui_app_poll(ui_app* app, uint32_t timeout_ms)
{
#ifdef __EMSCRIPTEN__
    if (timeout_ms) { return UI_ERROR_UNSUPPORTED; }
#endif
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->dispatching || app->polling || app->window_callback_depth ||
        app->current != app->main_state.get() || (app->workspace && app->workspace->notifying)) { return UI_ERROR_INVALID_STATE; }
    struct PollScope { ui_app* app; ~PollScope() { app->polling = false; devtools::dispatch_diagnostics(app); } } scope{app};
    app->polling = true;
    try {
        const auto frame_time = app->frame_time();
        for (auto it = app->animations.begin(); it != app->animations.end();) {
            if (it->lifetime.expired()) { it = app->animations.erase(it); continue; }
            if ((!it->paused || app->reduced_motion) && !it->ended) {
                if (auto status = sample_animation(*it,it->position(frame_time)); status != UI_OK) return status;
            }
            if (it->ended && !it->hold) it = app->animations.erase(it);
            else ++it;
        }
        binding_flush_focus(app);
        if (app->debug_pipe) { app->debug_pipe->poll(app); }
        if (app->debug_highlight && (std::chrono::steady_clock::now() >= app->debug_highlight_until ||
            std::none_of(app->nodes.begin(), app->nodes.end(), [&](const auto& n) { return n->debug_id == app->debug_highlight; }))) {
            app->debug_highlight = 0;
            binding_app_dirty(app);
        }
        if (app->binding_error != UI_OK) { return std::exchange(app->binding_error, UI_OK); }
        bool any_opened = false, any_live = false;
        for_each_session(*app, [&] {
            any_opened |= app->opened;
            any_live |= app->opened && !app->ended && !app->closing;
        });
        if (any_opened && !any_live) { async_app_shutdown(app); }
        std::vector<ui_app::Posted> posted;
        { std::lock_guard lock(app->post_mutex); posted.swap(app->posted); }
        {
            struct DispatchScope { ui_app* app; ~DispatchScope() { app->dispatching = false; } } dispatch{app};
            app->dispatching = true;
            auto completed = std::move(app->completions);
            for (const auto& item : completed) {
                if (!item.lifetime.expired() && app->animation_completed)
                    app->animation_completed(item.node,item.property,app->animation_context);
            }
            for (const auto& item : posted) {
                try { if (!item.cancellation || !item.cancellation->load()) { item.work(item.context); } }
                catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
                if (item.release) { item.release(item.context); }
            }
            async_command_poll(app);
        }
        ui_result result = UI_TIMEOUT;
        if (std::exchange(app->has_pending_items,false)) {
            std::vector<uint64_t> pending_items;
            for (const auto& node : app->nodes)
                if (node->items_refresh_pending) pending_items.push_back(node->debug_id);
            for (auto id : pending_items) {
                ui_node* node = nullptr;
                if (ui_app_find_debug_node(app,id,&node) != UI_OK) continue;
                if (auto status = refresh_items(*node,node->items_source); status != UI_OK) {
                    app->has_pending_items = true;
                    return status;
                }
            }
        }
        bool opened = false, alive = false, waited = false;
        for_each_session(*app, [&] {
            if (!app->opened) { return; }
            opened = true;
            if (app->ended) { return; }
            const auto status = poll_session(app, waited ? 0 : std::min(timeout_ms, 16u));
            waited = true;
            if (status == UI_EXITED) { app->ended = true; return; }
            alive = true;
            if (status != UI_OK && status != UI_TIMEOUT) { result = status; }
            else if (status == UI_OK && result == UI_TIMEOUT) { result = UI_OK; }
        });
        if (app->binding_error != UI_OK) { return std::exchange(app->binding_error, UI_OK); }
        if (result != UI_OK && result != UI_TIMEOUT) { return result; }
        if (opened && !alive) { async_app_shutdown(app); }
        return !opened ? UI_ERROR_INVALID_STATE : !alive ? UI_EXITED : result;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_PLATFORM; }
}

ui_result ui_app_run(ui_app* app)
{
#ifdef __EMSCRIPTEN__
    (void)app;
    return UI_ERROR_UNSUPPORTED;
#else
    for (;;) {
        const auto result = ui_app_poll(app, 1000);
        if (result != UI_OK && result != UI_TIMEOUT) {
            return result;
        }
    }
#endif
}

ui_result ui_node_create(ui_app* app, ui_node_kind kind, ui_node** out_node)
{
    if (!out_node) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_node = nullptr;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    for (const auto& model : app->models) { if (model->callbacks || model->notifying) { return UI_ERROR_INVALID_STATE; } }
    if (kind < UI_STACK_PANEL || kind > UI_PATH) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    try {
        if (app->nodes.size() == app->nodes.max_size()) { return UI_ERROR_OUT_OF_MEMORY; }
        auto node = std::make_unique<ui_node>();
        node->debug_id = devtools::next_id(app);
        node->owner = app;
        node->window_owner = app->active_session;
        node->kind = kind;
        if (kind == UI_SPLIT_VIEW || kind == UI_DOCK_SPLIT) {
            node->value = 50;
        }
        app->nodes.push_back(std::move(node));
        *out_node = app->nodes.back().get();
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_node_append(ui_node* parent, ui_node* child)
{
    if (!parent || !child || parent->owner != child->owner || parent->window_owner != child->window_owner || (!is_container(parent->kind) && !inline_container(parent->kind))) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (!accepts_child(*parent, child->kind)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = parent->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    SessionScope scope(*app, parent->window_owner);
    if (parent->items_source || parent->items_refresh_pending || parent->collection_item_id || app->closing || child->parent || app->root == child) {
        return UI_ERROR_INVALID_STATE;
    }
    if (app->workspace && app->workspace->notifying) { return UI_ERROR_INVALID_STATE; }
    for (auto* node = parent; node; node = node->parent) {
        if (node == child) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
    }
    try {
        std::vector<ResourceUpdate> changes;
        const ResourceChange attachment{nullptr,nullptr,child,parent};
        auto prepare = [&](auto& self, ui_node& descendant) -> ui_result {
            if (auto status = prepare_resource_updates(descendant,attachment,changes); status != UI_OK) return status;
            for (auto* item : descendant.children) if (auto status = self(self,*item); status != UI_OK) return status;
            return UI_OK;
        };
        if (auto status = prepare(prepare,*child); status != UI_OK) return status;
        parent->children.push_back(child);
        child->parent = parent;
        auto status = apply_resource_updates(changes);
        if (status == UI_OK) {
            status = binding_refresh_data_context(*child);
            if (status != UI_OK) undo_resource_updates(changes,changes.size());
        }
        if (status != UI_OK) {
            child->parent = nullptr;
            parent->children.pop_back();
            return status;
        }
        binding_app_dirty(app);
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_node_set_layout_value(ui_node* node, ui_property property, const ui_binding_value* value)
{
    if (node && !node->applying_property) return ui_node_set_property(node, property, value);
    if (!node || !value || !visual_layout::property(property)) return UI_ERROR_INVALID_ARGUMENT;
    if (const auto status = check(node->owner); status != UI_OK) return status;
    try {
        auto next = node->layout;
        if (!visual_layout::set(next, property, *value) || !visual_layout::valid(next)) return UI_ERROR_INVALID_ARGUMENT;
        binding_detach(*node, property);
        properties::written(*node, property);
        node->layout = std::move(next);
        if (node->kind == UI_DOCK_DOCUMENT || node->kind == UI_DOCK_TOOL) {
            if (property == UI_PROPERTY_MIN_WIDTH) node->dock_min_width = std::max(32.0,node->layout.min_width);
            if (property == UI_PROPERTY_MIN_HEIGHT) node->dock_min_height = std::max(32.0,node->layout.min_height);
            auto* host = node;
            while (host && !host->workspace) host = host->parent;
            if (host && host->workspace) {
                for (auto& item : host->workspace->items) if (item->content == node) {
                    item->min_width = node->dock_min_width;
                    item->min_height = node->dock_min_height;
                }
            }
        }
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_node_get_layout_value(const ui_node* node, ui_property property, ui_binding_value* value)
{
    if (!node || !value || !visual_layout::property(property)) return UI_ERROR_INVALID_ARGUMENT;
    if (const auto status = check(node->owner); status != UI_OK) return status;
    *value = {};
    const auto& l = node->layout;
    if (visual_layout::numeric(property)) {
        const double fields[] = {l.width, l.min_width, l.max_width, l.min_height, l.max_height};
        *value=visual_layout::length(fields[property - UI_PROPERTY_WIDTH]);
    } else if (property==UI_PROPERTY_MARGIN) {
        value->kind=UI_VALUE_THICKNESS; value->geometry=l.margin.data();
    } else {
        value->kind = UI_VALUE_STRING;
        value->string = property == UI_PROPERTY_HORIZONTAL_ALIGNMENT ? visual_layout::horizontal[l.horizontal] : visual_layout::vertical[l.vertical];
    }
    return UI_OK;
}

ui_result ui_node_set_height(ui_node* node, int32_t height)
{
    if (height < -1 || height > 4096) return UI_ERROR_INVALID_ARGUMENT;
    if (node && !node->applying_property) { ui_binding_value v{}; v.kind = UI_VALUE_INT; v.integer = height; return ui_node_set_property(node, UI_PROPERTY_HEIGHT, &v); }
    if (!node || height < -1 || height > 4096) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    binding_detach(*node, UI_PROPERTY_HEIGHT);
    properties::written(*node, UI_PROPERTY_HEIGHT);
    node->height = height;
    binding_app_dirty(node->owner);
    return UI_OK;
}
ui_result ui_node_get_height(const ui_node* node, int32_t* height)
{
    if (!node || !height) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    if (!std::isfinite(node->height) || node->height>INT32_MAX || std::floor(node->height)!=node->height) return UI_ERROR_INVALID_STATE;
    *height = int32_t(node->height);
    return UI_OK;
}

ui_result ui_node_set_weight(ui_node* node, uint32_t weight)
{
    if (node && !node->applying_property) {
        ui_binding_value value{}; value.kind=UI_VALUE_INT; value.integer=weight;
        return ui_node_set_property(node,UI_PROPERTY_WEIGHT,&value);
    }
    if (!node || weight > 1024) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    node->weight = weight;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_set_layout(ui_node* node, double spacing, double padding)
{
    if (node && !node->applying_property) return properties::set_metric_pair(*node, UI_PROPERTY_SPACING, spacing, UI_PROPERTY_PADDING, padding);
    if (!node || !is_container(node->kind) || !visual_layout::valid_metric(UI_PROPERTY_SPACING,spacing) || !visual_layout::valid_metric(UI_PROPERTY_PADDING,padding)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    node->spacing = spacing;
    node->padding = padding;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_set_orientation(ui_node* node, ui_orientation orientation) {
    if (!node || (node->kind != UI_STACK_PANEL && node->kind != UI_WRAP_PANEL && node->kind != UI_SEPARATOR) ||
        (orientation != UI_HORIZONTAL && orientation != UI_VERTICAL)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    node->orientation = orientation;
    invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE);
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_set_canvas_position(ui_node* node, double left, double top) {
    if (!node || !visual_layout::valid_coordinate(left) || !visual_layout::valid_coordinate(top)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (node->canvas_left != left || node->canvas_top != top) {
        node->canvas_left = left;
        node->canvas_top = top;
        if (node->parent) invalidate_layout(*node->parent, UI_PROPERTY_AFFECTS_ARRANGE);
        binding_app_dirty(node->owner);
    }
    return UI_OK;
}

ui_result ui_node_set_z_index(ui_node* node, int32_t value) {
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (node->z_index != value) {
        node->z_index = value;
        binding_app_dirty(node->owner);
    }
    return UI_OK;
}

ui_result ui_grid_set_tracks(ui_node* node, const ui_grid_track* rows, uint32_t row_count,
                             const ui_grid_track* columns, uint32_t column_count) {
    if (!node || node->kind != UI_GRID || row_count > 256 || column_count > 256 ||
        (row_count && !rows) || (column_count && !columns)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    auto convert = [](const ui_grid_track* source, unsigned count, std::vector<grid_layout::Track>& output) {
        for (unsigned i=0; i<count; ++i) {
            const auto [unit,value] = source[i];
            if (unit < UI_GRID_PIXELS || unit > UI_GRID_STAR || !std::isfinite(value) ||
                value < 0 || value > 10000000 || (unit == UI_GRID_STAR && value == 0)) return false;
            output.push_back({static_cast<grid_layout::Unit>(unit), value});
        }
        return true;
    };
    try {
        std::vector<grid_layout::Track> new_rows, new_columns;
        if (!convert(rows,row_count,new_rows) || !convert(columns,column_count,new_columns)) return UI_ERROR_INVALID_ARGUMENT;
        node->grid.rows.swap(new_rows);
        node->grid.columns.swap(new_columns);
        invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE);
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

bool panel_drawing(ui_app* app, bool active) { return app && std::exchange(app->drawing, active); }

ui_result ui_panel_set_callbacks(ui_node* node, const ui_panel_callbacks* callbacks) {
    if (!node || node->kind != UI_PANEL) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (callbacks && (callbacks->struct_size != sizeof(*callbacks) || !callbacks->measure || !callbacks->arrange)) return UI_ERROR_INVALID_ARGUMENT;
    if (callbacks && callbacks->version != 1) return UI_ERROR_VERSION_MISMATCH;
    try {
        std::shared_ptr<ui_panel_callbacks> next;
        if (callbacks) {
            struct Registration : ui_panel_callbacks {
                ~Registration() { if (release) release(context); }
            };
            auto owned = std::make_shared<Registration>();
            static_cast<ui_panel_callbacks&>(*owned) = *callbacks;
            next = std::move(owned);
        }
        node->panel_callbacks = std::move(next);
        node->panel_desired = {};
        node->panel_slots.clear();
        invalidate_layout(*node, UI_PROPERTY_AFFECTS_MEASURE);
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_scroll_set_policies(ui_node* node, ui_scroll_policy horizontal, ui_scroll_policy vertical) {
    if (!node || node->kind != UI_SCROLL_VIEW || horizontal < UI_SCROLL_DISABLED || horizontal > UI_SCROLL_VISIBLE ||
        vertical < UI_SCROLL_DISABLED || vertical > UI_SCROLL_VISIBLE) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    node->horizontal_scroll = horizontal;
    node->vertical_scroll = vertical;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_scroll_get_offset(const ui_node* node, double* left, double* top) {
    if (!node || node->kind != UI_SCROLL_VIEW || !left || !top) { return UI_ERROR_INVALID_ARGUMENT; }
    if (auto status = check(node->owner); status != UI_OK) { return status; }
    const double scale = node->owner->dpi / 96.;
    *left = node->horizontal_offset / scale;
    *top = node->offset / scale;
    return UI_OK;
}

ui_result ui_scroll_to(ui_node* node, double left, double top) {
    if (!node || node->kind != UI_SCROLL_VIEW || !std::isfinite(left) || !std::isfinite(top)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    const double scale = node->owner->dpi/96.;
    node->horizontal_offset = std::max(0.,left*scale);
    node->offset = std::max(0.,top*scale);
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_bring_into_view(ui_node* node) {
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    if (!node->is_visible()) return UI_ERROR_INVALID_STATE;
    auto* app = node->owner;
    if (app->dirty && !app->render(false)) return UI_ERROR_PLATFORM;
    // A fully clipped container can skip arranging its descendants. Reveal it
    // first so nested viewports and the target have current geometry.
    for (auto* ancestor = node->parent; ancestor; ancestor = ancestor->parent) {
        if (!ancestor->is_visible()) return UI_ERROR_INVALID_STATE;
        if ((ancestor->clip_to_bounds || ancestor->kind == UI_SCROLL_VIEW) &&
            (ancestor->visible.left >= ancestor->visible.right || ancestor->visible.top >= ancestor->visible.bottom)) {
            const auto result = ui_node_bring_into_view(ancestor);
            if (result != UI_OK) return result;
            if (!app->render(false)) return UI_ERROR_PLATFORM;
            break;
        }
    }
    auto target = node->bounds;
    const auto adjust = [](int start, int end, int viewport_start, int viewport_end) {
        if (start < viewport_start) return start - viewport_start;
        return end > viewport_end ? std::min(start-viewport_start,end-viewport_end) : 0;
    };
    for (auto* parent = node->parent; parent; parent = parent->parent) {
        if (parent->kind != UI_SCROLL_VIEW) continue;
        const auto viewport = parent->viewport;
        const double x = std::clamp(parent->horizontal_offset+adjust(target.left,target.right,viewport.left,viewport.right),0.,parent->horizontal_limit);
        const double y = std::clamp(parent->offset+adjust(target.top,target.bottom,viewport.top,viewport.bottom),0.,parent->scroll_limit);
        int dx = int(std::round(x-parent->horizontal_offset)), dy = int(std::round(y-parent->offset));
        parent->horizontal_offset = x; parent->offset = y;
        target.left -= dx; target.right -= dx; target.top -= dy; target.bottom -= dy;
    }
    binding_app_dirty(app);
    return UI_OK;
}

ui_result ui_grid_set_shared_size_group(ui_node* grid, ui_orientation axis, uint32_t index, const char* group) {
    if (!grid || grid->kind != UI_GRID || (axis != UI_HORIZONTAL && axis != UI_VERTICAL) ||
        !group || (*group && !grid_layout::valid_group(group))) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(grid->owner); status != UI_OK) return status;
    auto& tracks = axis == UI_HORIZONTAL ? grid->grid.columns : grid->grid.rows;
    if (index >= tracks.size()) return UI_ERROR_INVALID_ARGUMENT;
    try {
        tracks[index].group = group;
        invalidate_layout(*grid, UI_PROPERTY_AFFECTS_MEASURE);
        binding_app_dirty(grid->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_set_grid_cell(ui_node* node, uint32_t row, uint32_t column, uint32_t rows, uint32_t columns) {
    if (!node || row >= 256 || column >= 256 || !rows || !columns || rows > 256 || columns > 256)
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    node->grid.cell = {row,column,rows,columns};
    invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE);
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_set_dock(ui_node* node, ui_dock_edge edge)
{
    if (!node || edge < UI_DOCK_LEFT || edge > UI_DOCK_BOTTOM) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    node->dock = edge;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_node_set_anchor(ui_node* node, ui_node* anchor)
{
    if (!node || !anchor || node == anchor || node->owner != anchor->owner || node->window_owner != anchor->window_owner ||
        (!is_popup(node->kind) && node->kind != UI_LABEL)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    node->anchor = anchor;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_app_set_root(ui_app* app, ui_node* root)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!root || root->owner != app || root->window_owner != app->active_session || root->parent || is_inline(root->kind)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->workspace && app->workspace->notifying) { return UI_ERROR_INVALID_STATE; }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    auto page = std::find_if(app->inactive_fleck_pages.begin(), app->inactive_fleck_pages.end(),
        [&](const auto& entry) { return entry.root == root; });
    if (page != app->inactive_fleck_pages.end()) {
        if (!app->fleck_page.root || app->dispatching || app->drawing || app->current_action ||
            app->current != app->main_state.get() || app->ime_composing || app->pressed_control ||
            app->dragged_control || (app->workspace && (app->workspace->drag.prepared || app->workspace->menu))) {
            return UI_ERROR_INVALID_STATE;
        }
        for (const auto& [id, window] : app->dock_windows) {
            (void)id;
            if (window->ime_composing || window->pressed_control || window->dragged_control) {
                return UI_ERROR_INVALID_STATE;
            }
        }
        for (auto& [id, window] : app->dock_windows) {
            (void)id;
            WindowScope scope(*app, window.get());
            app->close_window();
        }
        app->dock_windows.clear();
        if (app->workspace) {
            app->workspace->views.clear();
            app->workspace->hovered.reset();
            app->workspace->pressed.reset();
        }
        app->fleck_page.workspace = std::move(app->workspace);
        std::swap(app->fleck_page, *page);
        app->workspace = std::move(app->fleck_page.workspace);
        app->debug_selection = 0;
    }
    if (app->dragged_control) {
        app->cancel_drag();
#ifdef _WIN32
        if (app->window && GetCapture() == app->window) {
            ReleaseCapture();
        }
#endif
    }
    app->root = root;
    app->hovered_control = nullptr;
    app->pressed_control = nullptr;
    app->focus = nullptr;
    app->keyboard_focus = false;
    app->interactive.clear();
    app->scrollable.clear();
    app->overlays.clear();
    app->debug_paint_order.clear();
    app->debug_hover = 0;
    app->dirty = true;
    return UI_OK;
}

static bool accepts_text(ui_node_kind kind)
{
    return (has_content(kind) || !is_container(kind) || kind == UI_COMBO_BOX || kind == UI_TAB_ITEM || kind == UI_TREE_ITEM || kind == UI_DOCK_DOCUMENT || kind == UI_DOCK_TOOL) &&
        kind != UI_IMAGE && kind != UI_ICON && kind != UI_SEPARATOR &&
        kind != UI_SLIDER && kind != UI_PROGRESS_BAR;
}

static bool valid_node_text(const ui_node& node, const std::wstring& text)
{
    return !is_editor(node.kind) || node.kind == UI_TEXT_AREA ||
        text.find_first_of(L"\r\n") == std::wstring::npos;
}

ui_result binding_apply_text(ui_node& node, const std::wstring& text)
{
    if (!valid_node_text(node, text)) return UI_ERROR_INVALID_ARGUMENT;
    SessionScope scope(*node.owner, node.window_owner);
    try {
        node.owner->replace_node_text(node, text);
        binding_detach(node, UI_PROPERTY_TEXT);
        properties::written(node, UI_PROPERTY_TEXT);
        node.text_source = nullptr;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_node_set_text(ui_node* node, const char* utf8)
{
    if (node && !node->applying_property) { ui_binding_value v{}; v.kind = UI_VALUE_STRING; v.string = utf8; return ui_node_set_property(node, UI_PROPERTY_TEXT, &v); }
    if (!node || !utf8 || !accepts_text(node->kind)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    try {
        const auto text = decode_ui_text(utf8);
        return binding_apply_text(*node, text);
    } catch (const std::range_error&) {
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_node_get_text(ui_node* node, char* utf8, uint32_t* size)
{
    if (!node || !size) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
#ifdef _WIN32
    const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, node->display_text().c_str(),
                                          -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return UI_ERROR_INVALID_STATE;
    }
    const auto available = utf8 ? *size : 0;
    *size = needed;
    if (utf8 && (available < *size || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        node->display_text().c_str(), -1, utf8, needed, nullptr, nullptr))) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    return UI_OK;
#else
    try {
        const auto text = utf8_text::encode(node->display_text());
        const auto available = utf8 ? *size : 0;
        *size = static_cast<uint32_t>(text.size() + 1);
        if (utf8) {
            if (available < *size) {
                return UI_ERROR_INVALID_ARGUMENT;
            }
            std::memcpy(utf8, text.c_str(), *size);
        }
        return UI_OK;
    } catch (const std::range_error&) {
        return UI_ERROR_INVALID_STATE;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
#endif
}

static void retire_visual_nodes(ui_app* app, const std::set<ui_node*>& old, ui_window_session::FleckPageSession* page, std::vector<std::unique_ptr<ui_node>>* pool = nullptr)
{
    auto clear_window = [&](ui_window_context& window) {
        if (old.contains(window.focus)) {
            window.focus = nullptr;
            window.keyboard_focus = false;
            window.host_text.observe(nullptr);
            window.ime_composing = false;
#ifdef _WIN32
            if (window.window) {
                if (auto ime = ImmGetContext(window.window)) {
                    ImmNotifyIME(ime, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                    ImmReleaseContext(window.window, ime);
                }
            }
#endif
        }
        if (old.contains(window.hovered_control)) { window.hovered_control = nullptr; }
        const bool cancelled = old.contains(window.pressed_control) || old.contains(window.dragged_control);
        if (old.contains(window.pressed_control)) { window.pressed_control = nullptr; }
        if (old.contains(window.dragged_control)) { window.dragged_control = nullptr; }
        if (cancelled) {
            window.host_pointer_down = false;
            window.pointer_down_handled = true;
#ifdef _WIN32
            if (window.window && GetCapture() == window.window) ReleaseCapture();
#endif
        }
#if defined(SUNFLECK_X11)
        if (old.contains(window.paste_target)) { window.paste_target = nullptr; }
#endif
        std::erase_if(window.interactive,[&](auto* node){return old.contains(node);});
        std::erase_if(window.scrollable,[&](auto* node){return old.contains(node);});
        std::erase_if(window.debug_paint_order,[&](auto* node){return old.contains(node);});
        std::erase_if(window.overlays,[&](const auto& overlay){return old.contains(overlay.owner);});
        window.debug_hover = 0; window.dirty = true;
    };
    clear_window(main_window(*app));
    for (auto& [id, window] : app->dock_windows) {
        (void)id;
        clear_window(window.get() == app->current ? static_cast<ui_window_context&>(*app) : *window);
    }
    for (auto* node : old) {
        if (node->debug_id == app->debug_selection) app->debug_selection = 0;
        for (const auto& binding : node->bindings)
            if (binding.invalid) devtools::recover(*node,binding.property,binding.path);
    }
    if (old.contains(app->hovered_control)) { app->hovered_control = nullptr; }

    for (auto& node : app->nodes) { if (old.contains(node->anchor)) { node->anchor = nullptr; } }
    if (page) {
        std::erase_if(page->nodes, [&](auto* node) { return old.contains(node); });
        std::erase_if(page->named, [&](auto* node) { return old.contains(node); });
    }
    std::erase_if(app->nodes, [&](auto& node) {
        if (!old.contains(node.get())) return false;
        if (pool) pool->push_back(std::move(node));
        return true;
    });
}

void prepare_grid_columns(ui_node& host, uint32_t first, uint32_t count)
{
    auto& table = *host.table;
    if (table.first==first && table.visible==count && host.item_template) return;
    auto visual = std::make_shared<fleck::Element>();
    visual->kind = UI_GRID;
    visual->spacing = 0;
    for (uint32_t i=0;i<count;++i) {
        const auto& column = table.columns[first+i];
        fleck::Element cell;
        cell.kind = UI_TEXT_BLOCK;
        cell.properties["Grid.Column"] = std::to_string(i);
        fleck::Binding binding;
        binding.path = column.path;
        binding.mode = UI_BIND_ONE_WAY;
        cell.bindings.push_back(std::move(binding));
        visual->children.push_back(std::move(cell));
        if (i) visual->properties["ColumnDefinitions"] += ",";
        visual->properties["ColumnDefinitions"] += std::to_string(column.width);
    }
    table.first = first;
    table.visible = count;
    host.item_template = std::move(visual);
    host.realized_source_id = 0;
    host.items_refresh_pending = true;
}

ui_result ui_data_grid_set_columns(ui_node* grid,const ui_data_grid_column* columns,uint32_t count)
{
    if (!grid || grid->kind!=UI_DATA_GRID || (count && !columns) || count>512) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(grid->owner);status!=UI_OK) return status;
    try {
        auto table = std::make_unique<DataGridState>();
        table->automatic = !count;
        for (uint32_t i=0;i<count;++i) {
            const auto& col = columns[i];
            if (!col.path || !binding_path(col.path) || !std::isfinite(col.width) || col.width<16 || col.width>4096 || col.editable>1)
                return UI_ERROR_INVALID_ARGUMENT;
            table->columns.push_back({col.path,utf8_text::decode(col.header ? col.header : col.path),col.width,col.editable!=0});
        }
        grid->table = std::move(table);
        grid->item_template.reset();
        grid->realized_source_id = 0;
        queue_items_refresh(*grid);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
}

ui_result ui_data_grid_sort_column(ui_node* grid, uint32_t column, uint32_t descending)
{
    if (!grid || grid->kind!=UI_DATA_GRID || descending>1) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(grid->owner);status!=UI_OK) return status;
    if (!grid->table || column>=grid->table->columns.size()) return UI_ERROR_INVALID_ARGUMENT;
    try {
        auto& table=*grid->table;
        table.sort_path=table.columns[column].path;
        table.descending=descending!=0;
        table.sorted_revision=-1;
        queue_items_refresh(*grid);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_data_grid_finish_edit(ui_node* cell, uint32_t commit)
{
    if (!cell || !cell->grid_cell_editor || commit>1) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(cell->owner);status!=UI_OK) return status;
    auto edit = std::find_if(cell->bindings.begin(),cell->bindings.end(),[](const auto& binding){
        return binding.property==UI_PROPERTY_TEXT && binding.mode==UI_BIND_TWO_WAY;
    });
    if (edit==cell->bindings.end()) return UI_ERROR_INVALID_STATE;
    if (commit) return edit->pending ? ui_node_update_source(cell,UI_PROPERTY_TEXT) : UI_OK;
    edit->pending=false;
    return ui_model_notify_property_changed(edit->model,edit->path.c_str());
}

static ui_result update_grid_order(DataGridState& table, const TextCollection& rows)
{
    if (table.sort_path.empty() || table.sorted_revision==rows.revision) return UI_OK;
    std::vector<BindingValue> keys(rows.items.size());
    std::set<uint64_t> models;
    std::vector<uint32_t> order;
    order.reserve(rows.items.size());
    for (const auto& row : rows.items) {
        auto index=static_cast<uint32_t>(order.size());
        order.push_back(index);
        if (row.model && row.model->properties.contains(table.sort_path)) {
            models.insert(row.model->debug_id);
            if (auto status=binding_read_value(*row.model,table.sort_path,keys[index]);status!=UI_OK) return status;
        }
    }
    auto less = [&](uint32_t a,uint32_t b){
        auto& left=keys[a]; auto& right=keys[b];
        if (left.value.kind!=right.value.kind) return left.value.kind<right.value.kind;
        if (left.value.kind==UI_VALUE_STRING) return left.text<right.text;
        if (left.value.kind==UI_VALUE_DOUBLE) return left.value.number<right.value.number;
        return left.value.integer<right.value.integer;
    };
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return table.descending ? less(b,a) : less(a,b);});
    table.order=std::move(order);
    table.sorted_models.swap(models);
    table.sorted_revision=rows.revision;
    return UI_OK;
}

ui_result ui_data_grid_set_column_width(ui_node* grid,uint32_t column,double width)
{
    if (!grid || grid->kind!=UI_DATA_GRID || !std::isfinite(width) || width<16 || width>4096) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(grid->owner);status!=UI_OK) return status;
    if (!grid->table || column>=grid->table->columns.size()) return UI_ERROR_INVALID_ARGUMENT;
    grid->table->columns[column].width = width;
    grid->item_template.reset();
    queue_items_refresh(*grid);
    return UI_OK;
}

static ui_result refresh_items(ui_node& host, ui_model* source, bool update_selection, bool validate_only)
{
    auto* app = host.owner;
    SessionScope scope(*app, host.window_owner);
    try {
        if (host.kind==UI_DATA_GRID && source) {
            if (!host.table) host.table = std::make_unique<DataGridState>();
            auto& table = *host.table;
            if (table.automatic && (source->debug_id!=host.realized_source_id || table.columns.empty())) {
                auto row = std::find_if(source->collection->items.begin(),source->collection->items.end(),[](const auto& item){return item.model!=nullptr;});
                if (row!=source->collection->items.end()) {
                    table.columns.clear();
                    for (const auto& [path,property] : row->model->properties) {
                        if (property.kind>UI_VALUE_DOUBLE) continue;
                        table.columns.push_back({path,utf8_text::decode(path),120,property.set!=nullptr});
                    }
                }
            }
            if (source->debug_id!=host.realized_source_id) table.sorted_revision=-1;
            if (auto status=update_grid_order(table,*source->collection);status!=UI_OK) return status;
            if (!host.item_template && !table.columns.empty())
                prepare_grid_columns(host,0,std::min<uint32_t>(4,static_cast<uint32_t>(table.columns.size())));
        }
        std::map<uint64_t,ui_node*> existing;
        std::set<ui_node*> retired(host.children.begin(),host.children.end());
        const bool same_source = source && source->debug_id == host.realized_source_id;
        if (same_source) {
            for (auto* node : host.children) existing.emplace(node->collection_item_id,node);
        }
        std::vector<ui_node*> next;
        struct NewRow { size_t position; uint64_t identity; std::wstring text; ui_model* context; };
        std::vector<NewRow> additions;
        unsigned index = host.value;
        if (source) {
            const auto& items = source->collection->items;
            const uint32_t total = static_cast<uint32_t>(items.size());
            index = total ? std::min(index,total-1) : 0;
            if (update_selection && same_source && host.selected_identity && total && items[index].identity != host.selected_identity) {
                auto selected = std::find_if(items.begin(),items.end(),[&](const auto& item){return item.identity == host.selected_identity;});
                if (selected != items.end()) index = static_cast<unsigned>(selected-items.begin());
            }
            const uint32_t first = std::min(host.first_realized,total);
            const uint32_t count = host.kind == UI_TREE_ITEM && !host.expanded ? 0 : (host.kind == UI_LIST_VIEW || host.kind == UI_DATA_GRID) ? std::min(host.realized_limit,total-first) : total;
            next.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                const auto& item = items[host.kind==UI_DATA_GRID && !host.table->order.empty() ? host.table->order[first+i] : first+i];
                if (const auto found = existing.find(item.identity); found != existing.end()) {
                    next.push_back(found->second);
                    retired.erase(found->second);
                } else {
                    additions.push_back({next.size(),item.identity,utf8_text::decode(item.text),item.model});
                    next.push_back(nullptr);
                }
            }
        }
        auto owns_host = [&](const auto& page) {
            return std::find(page.nodes.begin(),page.nodes.end(),&host) != page.nodes.end();
        };
        auto* page = owns_host(app->fleck_page) ? &app->fleck_page : nullptr;
        for (auto& inactive : app->inactive_fleck_pages) if (owns_host(inactive)) { page = &inactive; break; }
        struct TemplateContent {
            std::vector<std::unique_ptr<ui_node>> nodes;
            std::vector<ui_node*> content;
        };
        std::vector<TemplateContent> templates;
        size_t descendant_count = 0;
        if (host.item_template || !host.item_templates_by_label.empty()) {
            templates.resize(additions.size());
            for (size_t i=0; i<additions.size(); ++i) {
                auto& parts = templates[i];
                std::vector<ui_node*> named;
                std::vector<FleckAnchor> anchors;
                size_t offset = 0;
                auto* model = additions[i].context;
                auto visual = host.item_template;
                if (auto selected = host.item_templates_by_label.find(additions[i].text); selected != host.item_templates_by_label.end())
                    visual = selected->second;
                if (!visual) continue;
                if (host.kind==UI_DATA_GRID && model) {
                    auto editable = std::make_shared<fleck::Element>(*visual);
                    for (size_t index=0;index<editable->children.size();++index) {
                        auto& cell=editable->children[index];
                        const auto& col=host.table->columns[host.table->first+index];
                        auto field=model->properties.find(col.path);
                        if (field==model->properties.end()) { cell.bindings.clear(); continue; }
                        if (col.editable && field->second.kind<=UI_VALUE_DOUBLE && field->second.set) {
                            cell.kind=UI_TEXT_BOX;
                            cell.bindings.front().mode=UI_BIND_TWO_WAY;
                            cell.bindings.front().trigger=UI_UPDATE_LOST_FOCUS;
                        }
                    }
                    visual=std::move(editable);
                }
                auto* root = stage_fleck_node(app,*visual,nullptr,parts.nodes,named,anchors,offset,model);
                root->has_local_data_context = true;
                root->local_data_context = model;
                parts.content.push_back(root);
                for (const auto& anchor : anchors) {
                    auto found = std::find_if(named.begin(),named.end(),[&](auto* n){return n->fleck_id==*anchor.id && n->component_scope==anchor.node->component_scope;});
                    if (found == named.end() || *found == anchor.node) return UI_ERROR_INVALID_ARGUMENT;
                    anchor.node->anchor = *found;
                }
                size_t at = 0;
                auto bind = [&](auto& self,const fleck::Element& visual) -> ui_result {
                    auto& n = *parts.nodes[at++];
                    n.source = host.source;
                    n.collection_item_id = additions[i].identity;
                    n.grid_cell_editor = host.kind == UI_DATA_GRID && n.kind == UI_TEXT_BOX;
                    if (auto status=components::bind(n,visual,binding_data_context(n));status!=UI_OK) return status;
                    for (const auto& child : visual.children)
                        if (auto status=self(self,child);status!=UI_OK) return status;
                    return UI_OK;
                };
                if (auto status=bind(bind,*visual);status!=UI_OK) return status;
                descendant_count += parts.nodes.size();
            }
        }
        const auto collect = [&](auto& self,ui_node* n) -> void {
            for (auto* child : n->children) { retired.insert(child); self(self,child); }
        };
        for (auto* n : host.children) if (retired.contains(n)) collect(collect,n);
        std::vector<std::unique_ptr<ui_node>> tree_rows;
        if (host.kind == UI_TREE_VIEW || host.kind == UI_TREE_ITEM) {
            tree_rows.reserve(additions.size());
            for (const auto& row : additions) {
                auto tree = std::make_unique<ui_node>(app,UI_TREE_ITEM);
                tree->parent = &host;
                tree->window_owner = host.window_owner;
                tree->debug_id = devtools::next_id(app);
                tree->collection_item_id = row.identity;
                tree->has_local_data_context = true;
                tree->local_data_context = row.context;
                tree->text = row.text;
                tree->tree_children_path = host.tree_children_path;
                tree->tree_label_path = host.tree_label_path;
                if (row.context && !tree->tree_children_path.empty()) {
                    for (auto* ancestor=&host; ancestor; ancestor=ancestor->parent)
                        if (ancestor->kind==UI_TREE_ITEM && ancestor->local_data_context==row.context) return UI_ERROR_INVALID_ARGUMENT;
                    for (auto property : {UI_PROPERTY_TEXT,UI_PROPERTY_ITEMS_SOURCE}) {
                        const auto& path = property == UI_PROPERTY_TEXT ? tree->tree_label_path : tree->tree_children_path;
                        ui_binding_options binding{sizeof(binding),1,property,path.c_str(),UI_BIND_ONE_WAY,UI_UPDATE_PROPERTY_CHANGED,nullptr};
                        if (auto status=ui_node_bind_property(tree.get(),row.context,&binding);status!=UI_OK) return status;
                    }
                }
                tree_rows.push_back(std::move(tree));
            }
        }
        if (validate_only) return UI_OK;
        std::vector<std::unique_ptr<ui_node>> pool;
        pool.reserve(std::max(retired.size(),additions.size()));
        for (size_t i=retired.size();i<additions.size();++i) pool.push_back(std::make_unique<ui_node>());
        if (page) page->nodes.reserve(page->nodes.size()+additions.size()+descendant_count);
        app->nodes.reserve(app->nodes.size()+additions.size()+descendant_count);
        host.layout_children.clear();
        for (auto* n : retired) components::shutdown(*n);
        if (!retired.empty()) retire_visual_nodes(app,retired,page,&pool);
        size_t template_index = 0, tree_index = 0;
        for (auto& row : additions) {
            auto child = std::move(pool.back());
            pool.pop_back();
            *child = ui_node(app,host.kind == UI_ITEMS_CONTROL ? UI_TEXT_BLOCK : UI_LIST_ITEM);
            child->window_owner = host.window_owner;
            child->debug_id = devtools::next_id(app);
            child->collection_item_id = row.identity;
            child->text = std::move(row.text);
            child->has_local_data_context = true;
            child->local_data_context = row.context;
            child->parent = &host;
            if (!tree_rows.empty()) child = std::move(tree_rows[tree_index++]);
            child->source = host.source;
            if (!templates.empty() && !templates[template_index++].content.empty()) {
                auto& parts = templates[template_index-1];
                parts.content.front()->parent = child.get();
                child->children.swap(parts.content);
                for (auto& part : parts.nodes) {
                    if (page) page->nodes.push_back(part.get());
                    app->nodes.push_back(std::move(part));
                }
            }
            next[row.position] = child.get();
            if (page) page->nodes.push_back(child.get());
            app->nodes.push_back(std::move(child));
        }
        host.children.swap(next);
        if (host.kind != UI_TREE_ITEM) {
            host.value = source ? index : 0;
            host.selected_identity = source && !source->collection->items.empty() ? source->collection->items[index].identity : 0;
        }
        host.realized_source_id = source ? source->debug_id : 0;
        host.items_source = source;
        host.items_refresh_pending = false;
        for (const auto& row : additions) {
            auto mount = [&](auto& self,ui_node* n) -> void {
                try { components::mount(*n); } catch (...) { binding_app_error(app,UI_ERROR_INVALID_STATE); }
                for (auto* child : n->children) self(self,child);
            };
            if (!host.children[row.position]->children.empty()) mount(mount,host.children[row.position]);
        }
        invalidate_layout(host,UI_PROPERTY_AFFECTS_MEASURE);
        binding_app_dirty(app);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::exception&) { return UI_ERROR_INVALID_ARGUMENT; }
}

ui_result realize_list_range(ui_node& host, uint32_t first, uint32_t count)
{
    if (host.first_realized == first && host.realized_limit == count && !host.items_refresh_pending) return UI_OK;
    const auto old_first = std::exchange(host.first_realized,first);
    const auto old_count = std::exchange(host.realized_limit,count);
    auto result = refresh_items(host,host.items_source,host.items_refresh_pending);
    if (result != UI_OK) { host.first_realized = old_first; host.realized_limit = old_count; }
    return result;
}

ui_result ui_node_set_items_source(ui_node* node, ui_model* source)
{
    ui_binding_value value{};
    value.kind = UI_VALUE_MODEL;
    value.model = source;
    return ui_node_set_property(node,UI_PROPERTY_ITEMS_SOURCE,&value);
}

ui_result ui_node_set_item_template(ui_node* node,const char* label,const char* xml,size_t size)
{
    if (!node || (node->kind!=UI_LIST_VIEW && node->kind!=UI_ITEMS_CONTROL) || (!xml && size))
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(node->owner); status!=UI_OK) return status;
    try {
        std::shared_ptr<const fleck::Element> visual;
        if (xml) {
            fleck::Page page;
            ui_fleck_diagnostic diagnostic{};
            if (!fleck::parse(std::string_view(xml,size),page,diagnostic) || page.document_kind!="Window")
                return UI_ERROR_INVALID_ARGUMENT;
            components::expand(node->owner->component_registry,page.root);
            visual=std::make_shared<fleck::Element>(std::move(page.root));
        }
        auto templates=node->item_templates_by_label;
        if (label) {
            const auto key=utf8_text::decode(label);
            if (visual) templates[key]=visual;
            else templates.erase(key);
        }
        node->item_templates_by_label.swap(templates);
        if (!label) node->item_template.swap(visual);
        if (!node->items_source) return UI_OK;
        auto previous=std::exchange(node->realized_source_id,0);
        auto status=refresh_items(*node,node->items_source,false);
        if (status!=UI_OK) {
            node->realized_source_id=previous;
            node->item_templates_by_label.swap(templates);
            if (!label) node->item_template.swap(visual);
        }
        return status;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::exception&) { return UI_ERROR_INVALID_ARGUMENT; }
}

ui_result ui_node_get_item_identity(ui_node* node, uint64_t* identity)
{
    if (!node || !identity) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    *identity = node->collection_item_id;
    return UI_OK;
}

ui_result ui_node_set_items(ui_node* node, const char* const* labels, uint32_t count)
{
    if (!node || (count && !labels) || (node->kind != UI_ITEMS_CONTROL &&
        node->kind != UI_LIST_VIEW && node->kind != UI_COMBO_BOX)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = node->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    SessionScope scope(*app, node->window_owner);
    if (node->items_source || node->items_refresh_pending || !node->children.empty() || app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    if (count > app->nodes.max_size() - app->nodes.size() || count > node->children.max_size()) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
    try {
        std::vector<std::unique_ptr<ui_node>> items;
        items.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto child = std::make_unique<ui_node>();
            child->debug_id = devtools::next_id(app);
            child->owner = app;
            child->window_owner = node->window_owner;
            child->kind = node->kind == UI_ITEMS_CONTROL ? UI_TEXT_BLOCK : UI_LIST_ITEM;
            child->parent = node;
            if (const auto status = ui_node_set_text(child.get(), labels[i]); status != UI_OK) {
                return status;
            }
            items.push_back(std::move(child));
        }
        app->nodes.reserve(app->nodes.size() + count);
        node->children.reserve(count);
        for (auto& child : items) {
            node->children.push_back(child.get());
            app->nodes.push_back(std::move(child));
        }
        binding_app_dirty(app);
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_node_set_brush(ui_node* node,ui_brush_target target,const ui_brush_desc* brush)
{
    if (!node || target<UI_BRUSH_BACKGROUND || target>UI_BRUSH_OPACITY_MASK ||
        (target!=UI_BRUSH_OPACITY_MASK && node->kind!=UI_BORDER && !drawing::shape_kind(node->kind))) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(node->owner); status!=UI_OK) return status;
    try {
        std::shared_ptr<const drawing::Brush> next;
        if (brush) {
            auto copy=drawing::Brush::copy(*brush);
            if (!copy) return UI_ERROR_INVALID_ARGUMENT;
            next=std::make_shared<drawing::Brush>(std::move(*copy));
        }
        node->brushes[target]=std::move(next);
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_path_set_data(ui_node* node,const char* data) {
    if (!node || node->kind!=UI_PATH || !data) return UI_ERROR_INVALID_ARGUMENT;
    if (const auto status=check(node->owner); status!=UI_OK) return status;
    try {
        auto path=drawing::parse_path(data);
        if (!path) return UI_ERROR_INVALID_ARGUMENT;
        node->geometry=std::make_shared<drawing::Geometry>(std::move(*path));
        invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE);
        binding_app_dirty(node->owner); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_shape_set_stroke_thickness(ui_node* node,double thickness) {
    if (!node || !drawing::shape_kind(node->kind) || !(thickness>=0 && thickness<=4096)) return UI_ERROR_INVALID_ARGUMENT;
    if (const auto status=check(node->owner); status!=UI_OK) return status;
    node->stroke_thickness=thickness;
    invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE); binding_app_dirty(node->owner); return UI_OK;
}
ui_result ui_node_set_effect(ui_node* node,const ui_effect_desc* effect)
{
    if (!node || (effect && !drawing::valid_effect(*effect))) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(node->owner); status!=UI_OK) return status;
    node->effect=effect ? *effect : ui_effect_desc{};
    binding_app_dirty(node->owner);
    return UI_OK;
}

const GalleryImage& resolved_image(const ui_node& node)
{
    if (node.image_source.empty()) { return node.image; }
    const auto found = node.owner->image_resources.find(node.image_source);
    static const GalleryImage missing;
    return found == node.owner->image_resources.end() ? missing : found->second;
}

ui_result ui_app_register_image_rgba(ui_app* app, const char* key, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!key || !*key || std::string_view(key).size() > 1024 || !rgba ||
        !width || !height || width > 4096 || height > 4096) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        (void)utf8_text::decode(key);
        GalleryImage image;
        image.rgb.assign(rgba, rgba + size_t(width) * height * 4);
        image.width = width;
        image.height = height;
        image.channels = 4;
        app->image_resources.insert_or_assign(key, std::move(image));
        binding_app_dirty(app);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
}

static ui_result set_image_pixels(ui_node* node, const uint8_t* rgb, uint32_t width, uint32_t height, unsigned channels)
{
    if (!node || (node->kind != UI_IMAGE && node->kind != UI_ICON && node->kind != UI_ICON_BUTTON) ||
        !rgb || !width || !height || width > 4096 || height > 4096) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    try {
        node->image.rgb.assign(rgb, rgb + width * height * channels);
        node->image.channels = channels;
        node->image.width = width;
        node->image.height = height;
        binding_app_dirty(node->owner);
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_node_set_render_transform(ui_node* node, ui_matrix matrix, ui_point origin) {
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    for (double value : {matrix.m11,matrix.m12,matrix.m21,matrix.m22,matrix.offset_x,matrix.offset_y,origin.x,origin.y})
        if (!std::isfinite(value)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    const double coefficients[] = {matrix.m11,matrix.m12,matrix.m21,matrix.m22,matrix.offset_x,matrix.offset_y};
    const double point[] = {origin.x,origin.y};
    ui_binding_value value{};
    value.kind = UI_VALUE_MATRIX; value.geometry = coefficients;
    if (const auto status = ui_node_set_property(node,UI_PROPERTY_RENDER_TRANSFORM,&value); status != UI_OK) return status;
    value.kind = UI_VALUE_POINT; value.geometry = point;
    return ui_node_set_property(node,UI_PROPERTY_RENDER_TRANSFORM_ORIGIN,&value);
}
ui_result ui_node_to_window(ui_node* node, ui_point local, ui_point* window) {
    if (!node || !window || !std::isfinite(local.x) || !std::isfinite(local.y)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    const double s = node->owner->dpi/96.;
    auto box = node->layout_cache.arranged;
    auto p = affine::map(node->world_transform,box.left+local.x*s,box.top+local.y*s);
    *window = {p.x/s,p.y/s}; return UI_OK;
}
ui_result ui_node_from_window(ui_node* node, ui_point window, ui_point* local) {
    if (!node || !local || !std::isfinite(window.x) || !std::isfinite(window.y)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    const auto inverse = affine::inverse(node->world_transform);
    if (!inverse) return UI_ERROR_INVALID_STATE;
    const double s = node->owner->dpi/96.;
    auto p = affine::map(*inverse,window.x*s,window.y*s);
    auto box = node->layout_cache.arranged;
    *local = {(p.x-box.left)/s,(p.y-box.top)/s}; return UI_OK;
}

ui_result ui_node_set_stretch(ui_node* node, ui_stretch stretch) {
    if (!node || (node->kind != UI_IMAGE && node->kind != UI_ICON && node->kind != UI_VIEWBOX) ||
        stretch < UI_STRETCH_NONE || stretch > UI_STRETCH_UNIFORM_TO_FILL) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    node->stretch = stretch;
    invalidate_layout(*node,UI_PROPERTY_AFFECTS_MEASURE);
    binding_app_dirty(node->owner);
    return UI_OK;
}
ui_result ui_node_set_image_rgba(ui_node* node, const uint8_t* rgba, uint32_t width, uint32_t height) {
    return set_image_pixels(node,rgba,width,height,4);
}

ui_result ui_node_set_image(ui_node* node, const uint8_t* rgb, uint32_t width, uint32_t height) {
    return set_image_pixels(node,rgb,width,height,3);
}
namespace {
bool indexed(ui_node_kind kind)
{
    return devtools::indexed(kind);
}

bool numeric(ui_node_kind kind)
{
    return devtools::numeric(kind);
}

bool has_value(ui_node_kind kind)
{
    return devtools::has_value(kind);
}
}

ui_result ui_node_set_value(ui_node* node, uint32_t value)
{
    if (node && !node->applying_property) { ui_binding_value v{}; v.kind = UI_VALUE_INT; v.integer = value; return ui_node_set_property(node, UI_PROPERTY_VALUE, &v); }
    if (!node || !has_value(node->kind) ||
        (indexed(node->kind) ? value >= node->item_count()
            : value > (numeric(node->kind) ? 100u : 1u))) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    if (node->kind == UI_RADIO_BUTTON && value && node->parent) {
        for (auto* sibling : node->parent->children) {
            if (sibling->kind == UI_RADIO_BUTTON) {
                sibling->value = 0;
                if (sibling != node) properties::capture(*sibling, UI_PROPERTY_VALUE);
            }
        }
    }
    binding_detach(*node, UI_PROPERTY_VALUE);
    properties::written(*node, UI_PROPERTY_VALUE);
    const bool changed = node->value != value;
    node->value = value;
    if (node->items_source && indexed(node->kind)) {
        node->selected_identity = node->items_source->collection->items[value].identity;
    }
    if (node->kind == UI_TREE_ITEM) {
        node->expanded = value != 0;
        if (node->items_source) queue_items_refresh(*node);
    }
    binding_app_dirty(node->owner);
    if (changed && node->component && fleck::range_template(node->kind))
        binding_app_error(node->owner, ui_model_notify_property_changed(node->component->properties.get(), "Value"));
    return UI_OK;
}

ui_result ui_node_get_value(ui_node* node, uint32_t* value)
{
    if (!node || !value || !has_value(node->kind)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    *value = node->value;
    return UI_OK;
}

ui_result ui_node_set_input_binding(ui_node* node,unsigned key,unsigned modifiers,const char* command_name)
{
    return ui_node_set_key_binding(node,key,modifiers,UI_EVENT_BUBBLE,command_name);
}

ui_result ui_node_set_key_binding(ui_node* node,unsigned key,unsigned modifiers,ui_event_phase phase,const char* command_name)
{
    if (phase < UI_EVENT_PREVIEW || phase > UI_EVENT_BUBBLE) return UI_ERROR_INVALID_ARGUMENT;
    if (!node || !fleck::valid_gesture(key,modifiers) || (command_name && !fleck::valid_binding_name(command_name)))
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    try {
        unsigned gesture = fleck::gesture_code(key,modifiers,phase);
        if (command_name) node->input_bindings.insert_or_assign(gesture,ui_node::InputBinding{command_name, true});
        else node->input_bindings.erase(gesture);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_set_routed_command(ui_node* node, const char* name)
{
    if (!node || !is_interactive(node->kind) || (name && !fleck::valid_binding_name(name))) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    try { node->routed_command = name ? name : ""; node->local_routed_command = name != nullptr; return UI_OK; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_set_command_binding(ui_node* node, const char* name, ui_command* command)
{
    if (!node || !name || !fleck::valid_binding_name(name) || (command && command->owner != node->owner))
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    try {
        if (command) node->command_bindings.insert_or_assign(name,command);
        else node->command_bindings.erase(name);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_set_drop_handler(ui_node* node,uint32_t kinds,ui_drop_handler handler,void* context)
{
    if (!node || (kinds & ~(UI_DROP_TEXT|UI_DROP_FILES|UI_DROP_OBJECT)) || (handler && !kinds)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(node->owner);status!=UI_OK) return status;
    if (handler && !node->owner->track_drop_targets) {
        node->owner->track_drop_targets=true;
        binding_app_dirty(node->owner);
    }
    node->drop_handler=handler;
    node->drop_context=handler ? context : nullptr;
    node->drop_kinds=handler ? kinds : 0;
    return UI_OK;
}

ui_result ui_node_start_drag(ui_node* node,const ui_drop_data* data)
{
    if(!node || !data) return UI_ERROR_INVALID_ARGUMENT;
    auto* app=node->owner;
    if (auto status=check(app);status!=UI_OK) return status;
#ifdef _WIN32
    if(!app->window || !app->drop_registered) return UI_ERROR_UNSUPPORTED;
    if(app->closing || !app->owns_input(node) || !node->accepts_input()) return UI_ERROR_INVALID_STATE;
    struct Restore { bool& flag; bool previous; ~Restore(){flag=previous;} } guard{app->dispatching,std::exchange(app->dispatching,true)};
    try {return win_drop::drag(app->window,app,*data);}
    catch (const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;}
    catch (...) {return UI_ERROR_INVALID_ARGUMENT;}
#else
    return UI_ERROR_UNSUPPORTED;
#endif
}

ui_result ui_node_set_pointer_handler(ui_node* node, ui_pointer_action action, ui_event_phase phase, ui_pointer_handler handler, void* context)
{
    if (!node || action < UI_POINTER_DOWN || action > UI_POINTER_CANCEL || phase < UI_EVENT_PREVIEW || phase > UI_EVENT_BUBBLE)
        return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    try {
        if (handler) node->pointer_handlers.insert_or_assign({action,phase},ui_node::PointerHandler{handler,context});
        else node->pointer_handlers.erase({action,phase});
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_node_set_activation_handler(ui_node* node, ui_event_phase phase, ui_activation_handler handler, void* context)
{
    if (!node || phase < UI_EVENT_PREVIEW || phase > UI_EVENT_BUBBLE) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = check(node->owner); status != UI_OK) return status;
    node->activation_handlers[phase] = {handler,context};
    return UI_OK;
}

ui_result ui_node_set_action(ui_node* node, ui_node_action action, void* context)
{
    return ui_node_set_action_with_concurrency(node, action, context, UI_ACTION_SINGLE_FLIGHT);
}

ui_result ui_node_set_action_with_concurrency(ui_node* node, ui_node_action action, void* context,
                                             ui_action_concurrency concurrency)
{
    if (concurrency != UI_ACTION_SINGLE_FLIGHT && concurrency != UI_ACTION_ALLOW_OVERLAP) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (!node || (!is_interactive(node->kind) && !indexed(node->kind))) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    binding_detach(*node, UI_PROPERTY_COMMAND);
    properties::written(*node, UI_PROPERTY_COMMAND);
    const bool was_busy = node->action_busy();
    node->action_concurrency = concurrency;
    node->action = action;
    node->context = context;
    node->command = nullptr;
    properties::capture(*node, UI_PROPERTY_COMMAND);
    if (was_busy != node->action_busy()) { binding_app_dirty(node->owner); }
    return UI_OK;
}

ui_result ui_node_defer_action(ui_node* node)
{
    if (!node) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = node->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    auto* dispatch = app->current_action;
    if (!dispatch || dispatch->node != node || dispatch->deferred) {
        return UI_ERROR_INVALID_STATE;
    }
    dispatch->deferred = true;
    ++node->deferred_actions;
    return UI_OK;
}

ui_result ui_node_complete_action(ui_node* node)
{
    if (!node) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    if (!node->deferred_actions) {
        return UI_ERROR_INVALID_STATE;
    }
    --node->deferred_actions;
    binding_app_dirty(node->owner);
    return UI_OK;
}

ui_result ui_text_source_create(ui_app* app, const char* utf8, ui_text_source** out_source)
{
    if (!out_source) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_source = nullptr;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!utf8) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->closing || app->sources.size() >= 512) {
        return UI_ERROR_INVALID_STATE;
    }
    try {
        auto source = std::make_unique<ui_text_source>();
        source->owner = app;
        source->value = decode_ui_text(utf8);
        app->sources.push_back(std::move(source));
        *out_source = app->sources.back().get();
        return UI_OK;
    } catch (const std::range_error&) {
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_text_source_set(ui_text_source* source, const char* utf8)
{
    if (!source || !utf8) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = source->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    for (const auto& [model, path] : source->property_observers) {
        (void)path;
    }
    try {
        auto text = decode_ui_text(utf8);
        if (text.find_first_of(L"\r\n") != std::wstring::npos &&
            std::any_of(app->nodes.begin(), app->nodes.end(), [&](const auto& node) {
                if (!is_editor(node->kind) || node->kind == UI_TEXT_AREA) { return false; }
                if (node->text_source == source) { return true; }
                return std::any_of(node->bindings.begin(), node->bindings.end(), [&](const auto& binding) {
                    return binding.property == UI_PROPERTY_TEXT && std::any_of(source->property_observers.begin(), source->property_observers.end(), [&](const auto& observer) {
                        return binding.model == observer.first && binding.path == observer.second;
                    });
                });
            })) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        app->update_text_source(*source, text);
        return UI_OK;
    } catch (const std::range_error&) {
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_text_source_destroy(ui_text_source* source)
{
    if (!source) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = source->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    for (const auto& [model, path] : source->property_observers) {
        (void)path;
        if (model->callbacks || model->notifying) { return UI_ERROR_INVALID_STATE; }
    }
    for (const auto& [model, path] : source->property_observers) {
        for (auto& node : app->nodes) { std::erase_if(node->bindings, [&](const auto& b) { return b.model == model && b.path == path; }); }
        for (auto& adapter : model->adapters) { if (adapter->source == source) { adapter->source = nullptr; } }
        model->properties.erase(path);
    }
    for (const auto& node : app->nodes) {
        if (node->text_source == source) {
            node->text_source = nullptr;
            binding_app_dirty(app);
        }
    }
    std::erase_if(app->sources, [&](const auto& item) { return item.get() == source; });
    return UI_OK;
}

ui_result ui_node_bind_text(ui_node* node, ui_text_source* source, ui_binding_mode mode)
{
    if (!node || (source && source->owner != node->owner) ||
        (mode != UI_BIND_ONE_WAY && mode != UI_BIND_ONE_TIME && mode != UI_BIND_TWO_WAY)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = node->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    SessionScope scope(*app, node->window_owner);
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    if (mode == UI_BIND_ONE_WAY) {
        if (node->kind != UI_TEXT_BLOCK) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        binding_detach(*node, UI_PROPERTY_TEXT);
        properties::written(*node, UI_PROPERTY_TEXT);
        if (source) {
            node->text.clear();
        }
        if (node->text_source != source) {
            node->text_source = source;
            binding_app_dirty(app);
        }
        properties::capture(*node, UI_PROPERTY_TEXT);
        return UI_OK;
    }
    if (mode == UI_BIND_ONE_TIME) {
        if (!source || !accepts_text(node->kind) || !valid_node_text(*node, source->value)) {
            return UI_ERROR_INVALID_ARGUMENT;
        }
        try {
            app->replace_node_text(*node, source->value);
            binding_detach(*node, UI_PROPERTY_TEXT);
            properties::written(*node, UI_PROPERTY_TEXT);
            node->text_source = nullptr;
            properties::capture(*node, UI_PROPERTY_TEXT);
            return UI_OK;
        } catch (const std::bad_alloc&) {
            return UI_ERROR_OUT_OF_MEMORY;
        }
    }
    if (!is_editor(node->kind)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (!source) {
        binding_detach(*node, UI_PROPERTY_TEXT);
        properties::written(*node, UI_PROPERTY_TEXT);
        node->text_source = nullptr;
        properties::capture(*node, UI_PROPERTY_TEXT);
        return UI_OK;
    }
    if (!valid_node_text(*node, source->value)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (node->text_source == source) {
        return UI_OK;
    }
    try {
        // Reserve the editor limit before attaching so live propagation needs no allocations.
        node->text.reserve(4096);
        source->value.reserve(4096);
        app->replace_node_text(*node, source->value);
        binding_detach(*node, UI_PROPERTY_TEXT);
        properties::written(*node, UI_PROPERTY_TEXT);
        node->text_source = source;
        properties::capture(*node, UI_PROPERTY_TEXT);
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_command_create(ui_app* app, ui_node_action action, void* context, ui_command** out_command)
{
    if (!out_command) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *out_command = nullptr;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!action) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->closing || app->stopping || app->dispatching || app->commands.size() >= 512) {
        return UI_ERROR_INVALID_STATE;
    }
    try {
        app->commands.push_back(std::make_unique<ui_command>(ui_command{app, action, context}));
        *out_command = app->commands.back().get();
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_command_destroy(ui_command* command)
{
    if (!command) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = command->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (app->dispatching) { return UI_ERROR_INVALID_STATE; }
    async_command_dispose(*command);
    for (const auto& model : app->models) { if (model->callbacks || model->notifying) { return UI_ERROR_INVALID_STATE; } }
    for (auto& model : app->models) {
        for (auto& adapter : model->adapters) { if (adapter->command == command) { adapter->command = nullptr; } }
    }
    for (const auto& node : app->nodes) {
        std::erase_if(node->command_bindings,[&](const auto& entry){ return entry.second == command; });
        if (node->command == command) {
            node->command = nullptr;
        }
    }
    components::clear_reference(app, UI_VALUE_COMMAND, command);
    std::erase_if(app->commands, [&](const auto& item) { return item.get() == command; });
    return UI_OK;
}

ui_result ui_node_bind_command(ui_node* node, ui_command* command)
{
    if (node && !node->applying_property) { ui_binding_value v{}; v.kind = UI_VALUE_COMMAND; v.command = command; return ui_node_set_property(node, UI_PROPERTY_COMMAND, &v); }
    if (!node || (!is_interactive(node->kind) && !indexed(node->kind)) ||
        (command && command->owner != node->owner)) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (const auto status = check(node->owner); status != UI_OK) {
        return status;
    }
    if (node->owner->closing && command && !binding_releasing_model()) {
        return UI_ERROR_INVALID_STATE;
    }
    if (command && ((node->command_mode == UI_COMMAND_ASYNC && !command->asynchronous()) ||
        (node->command_mode == UI_COMMAND_SYNC && command->asynchronous()))) { return UI_ERROR_INVALID_ARGUMENT; }
    binding_detach(*node, UI_PROPERTY_COMMAND);
    properties::written(*node, UI_PROPERTY_COMMAND);
    const bool was_busy = node->action_busy();
    if (command) {
        node->action = nullptr;
        node->context = nullptr;
    }
    node->command = command;
    if (command) {
        node->action_concurrency = UI_ACTION_SINGLE_FLIGHT;
    }
    if (was_busy != node->action_busy()) { binding_app_dirty(node->owner); }
    return UI_OK;
}

ui_result ui_theme_init(ui_theme* theme, uint32_t dark, uint32_t compact)
{
    if (!theme || dark > 1 || compact > 1) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    *theme = builtin_palette(UI_BUILTIN_SUNFLECK_LITE, dark != 0, compact != 0);
    return UI_OK;
}

static ui_result apply_theme_palette(ui_app* app, const ui_theme* theme)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!theme || theme->struct_size < sizeof(ui_theme) || theme->gap > 64 ||
        theme->control_height < 16 || theme->control_height > 128 ||
        theme->window_padding > 64 || theme->gallery_gap > 64 ||
        std::any_of(std::begin(theme->colors), std::end(theme->colors),
                    [](uint32_t rgb) { return rgb > 0xffffff; })) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    const auto& previous = app->gallery_style;
    if (!std::equal(std::begin(theme->colors), std::end(theme->colors), std::begin(previous.colors)) ||
        theme->gap != previous.gap || theme->control_height != previous.control_height ||
        theme->window_padding != previous.window_padding || theme->gallery_gap != previous.gallery_gap) {
        app->gallery_style = *theme;
        app->gallery_style.struct_size = sizeof(ui_theme);
#if defined(SUNFLECK_X11)
        if (app->display && app->window) {
            XSetWindowBackground(app->display, app->window, app->x11_background());
            XFlush(app->display);
        }
#endif
        app->dirty = true;
    }
    return UI_OK;
}

ui_result ui_app_set_theme(ui_app* app, const ui_theme* theme)
{
    const auto status = apply_theme_palette(app, theme);
    const auto shape = builtin_themes[UI_BUILTIN_SUNFLECK_LITE].shape;
    if (status == UI_OK && app->theme_shape != shape) {
        app->theme_shape = shape;
        app->dirty = true;
    }
    if (status == UI_OK) {
        const bool custom = !app->theme_resource.colors.empty() || !app->theme_resource.metrics.empty() || !app->theme_resource.templates.empty() || !app->theme_resource.skin.empty();
        app->theme_resource = {}; app->theme_resource.palette = *theme; app->theme_resource.shape = shape;
        if (app->dirty || custom) { dock_app_dirty(app); }
    }
    return status;
}

ui_result ui_app_set_builtin_theme(ui_app* app, ui_builtin_theme theme, uint32_t dark, uint32_t compact)
{
    if (!ui_builtin_theme_name(theme) || dark > 1 || compact > 1) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    const auto palette = builtin_palette(theme, dark != 0, compact != 0);
    const auto status = apply_theme_palette(app, &palette);
    if (status == UI_OK) {
        const auto shape = builtin_themes[theme].shape;
        if (app->theme_shape != shape) {
            app->theme_shape = shape;
            app->dirty = true;
        }
        const bool custom = !app->theme_resource.colors.empty() || !app->theme_resource.metrics.empty() || !app->theme_resource.templates.empty() || !app->theme_resource.skin.empty();
        app->theme_resource = {}; app->theme_resource.palette = palette; app->theme_resource.shape = shape;
        if (app->dirty || custom) { dock_app_dirty(app); }
    }
    return status;
}

ui_result ui_app_set_style(ui_app* app, uint32_t dark, uint32_t compact)
{
    return ui_app_set_builtin_theme(app, UI_BUILTIN_SUNFLECK_LITE, dark, compact);
}

ui_result ui_node_set_theme_color(ui_node* node, ui_theme_color token, uint32_t rgb)
{
    if (!node || token < UI_THEME_BACKGROUND || token >= UI_THEME_COLOR_COUNT || rgb > 0xffffff) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    auto* app = node->owner;
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    const unsigned bit = 1u << token;
    if (!(node->theme_override_mask & bit) || node->theme_overrides[token] != rgb) {
        node->theme_override_mask |= bit;
        node->theme_overrides[token] = rgb;
        app->dirty = true;
    }
    return UI_OK;
}

ui_result ui_app_set_gallery(ui_app* app, uint32_t dark, uint32_t compact)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (dark > 1 || compact > 1) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    if (!app->gallery) {
        app->gallery = true;
        app->dirty = true;
    }
    return ui_app_set_style(app, dark, compact);
}

ui_result ui_app_set_gallery_image(ui_app* app, const uint8_t* rgb, uint32_t width, uint32_t height)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!rgb || !width || !height || width > 128 || height > 128) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (!app->gallery || app->closing) {
        return UI_ERROR_INVALID_STATE;
    }
    try {
        GalleryImage image;
        image.rgb.assign(rgb, rgb + width * height * 3);
        image.width = width;
        image.height = height;
        app->gallery_image = std::move(image);
        app->dirty = true;
        return UI_OK;
    } catch (const std::bad_alloc&) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
}

ui_result ui_app_create_window(ui_app* app, ui_window** out_window)
{
#ifdef __EMSCRIPTEN__
    (void)app;
    if (out_window) { *out_window = nullptr; }
    return UI_ERROR_UNSUPPORTED;
#endif
    if (!out_window) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_window = nullptr;
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->polling || app->dispatching || app->drawing || app->current_action || app->window_callback_depth ||
        (app->workspace && app->workspace->notifying)) { return UI_ERROR_INVALID_STATE; }
    if (app->next_window_id == UINT32_MAX) { return UI_ERROR_OUT_OF_MEMORY; }
    try {
        auto window = std::make_unique<ui_window>();
        window->owner = app;
        window->id = app->next_window_id++;
        window->main_state->application = app;
        window->main_state->session = window.get();
        window->main_state->debug_window_id = window->id;
        auto* handle = window.get();
        app->windows.push_back(std::move(window));
        *out_window = handle;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

ui_result ui_app_get_current_window(ui_app* app, ui_window** out_window)
{
    if (!out_window) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_window = nullptr;
    if (const auto status = check(app); status != UI_OK) { return status; }
    *out_window = app->active_session;
    return UI_OK;
}

ui_result ui_window_invoke(ui_window* window, ui_window_callback callback, void* context)
{
    if (!window || !callback) { return UI_ERROR_INVALID_ARGUMENT; }
    auto* app = window->owner;
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->polling || app->dispatching || app->drawing || app->current_action || (app->workspace && app->workspace->notifying) ||
        (app->window_callback_depth && app->active_session != window)) { return UI_ERROR_INVALID_STATE; }
    SessionScope scope(*app, window);
    struct CallbackScope { ui_app* app; ~CallbackScope() { --app->window_callback_depth; } } callback_scope{app};
    ++app->window_callback_depth;
    try { return callback(app, context); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
    catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_window_request_close(ui_window* window)
{
    return ui_window_invoke(window, [](ui_app* app, void*) { return ui_app_request_close(app); }, nullptr);
}

ui_result ui_window_get_id(ui_window* window, uint32_t* id)
{
    if (!window || !id) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(window->owner); status != UI_OK) { return status; }
    *id = window->id;
    return UI_OK;
}

ui_result ui_app_request_close(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!app->opened) {
        return UI_ERROR_INVALID_STATE;
    }
    app->closing = true;
    return UI_OK;
}

void ui_app_destroy(ui_app* app)
{
    if (check(app) != UI_OK || app->dispatching || app->polling || app->window_callback_depth || app->current_action || app->drawing || (app->workspace && app->workspace->notifying)) {
        return;
    }
    for (const auto& model : app->models) { if (model->callbacks || model->notifying) { return; } }
    async_app_shutdown(app);
    for (auto& command : app->commands) { async_command_dispose(*command); }
    for_each_session(*app, [&] {
        for (auto& [id, window] : app->dock_windows) { (void)id; WindowScope scope(*app, window.get()); app->close_window(); }
        app->close_window();
#ifdef _WIN32
        if (app->dock_overlay) { DestroyWindow(app->dock_overlay); app->dock_overlay = nullptr; }
#elif defined(SUNFLECK_X11)
        clear_drag_overlay(*app);
#endif
    });
    for (const auto& item : app->posted) { if (item.release) { item.release(item.context); } }
    for (auto& node : app->nodes) { components::shutdown(*node); }
    app->nodes.clear();
#ifdef _WIN32
    windows_text::discard_font_layouts(app->font_resources);
#endif
    delete app;
}

ui_result ui_app_activate_button(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) {
        return status;
    }
    if (!app->headless || !app->opened || app->closing || app->dispatching) {
        return UI_ERROR_INVALID_STATE;
    }
    if (app->root) {
        const auto button = std::find_if(app->interactive.begin(), app->interactive.end(),
            [](const ui_node* node) { return node->kind == UI_BUTTON; });
        if (button == app->interactive.end()) {
            return UI_ERROR_INVALID_STATE;
        }
        app->activate_node(**button);
    } else {
        app->activate();
    }
    return UI_OK;
}

ui_result ui_app_post(ui_app* app, ui_post_callback work, void* context, ui_post_callback release)
{
    return ui_app_post_cancellable(app, async_command_token(app), work, context, release);
}
ui_result ui_app_post_cancellable(ui_app* app, const ui_cancellation_token* token,
    ui_post_callback work, void* context, ui_post_callback release)
{
    if (!app || !work) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        std::lock_guard lock(app->post_mutex);
        if (app->stopping || ui_cancellation_requested(token)) { return UI_CANCELLED; }
        if (app->posted.size() >= 1024) { return UI_ERROR_INVALID_STATE; }
        app->posted.push_back({work, release, context, token ? token->requested : nullptr});
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
bool async_has_posts(ui_app* app, const ui_cancellation_token& token)
{
    std::lock_guard lock(app->post_mutex);
    return std::any_of(app->posted.begin(), app->posted.end(), [&](const auto& item) { return item.cancellation == token.requested; });
}
void async_cancel_posts(ui_app* app, const ui_cancellation_token& token)
{
    // Never invoke a release callback while holding the cross-thread queue lock.
    // Extract one at a time so cancellation needs no allocation.
    for (;;) {
        ui_app::Posted cancelled{};
        {
            std::lock_guard lock(app->post_mutex);
            auto found = std::find_if(app->posted.begin(), app->posted.end(), [&](const auto& item) { return item.cancellation == token.requested; });
            if (found == app->posted.end()) { break; }
            cancelled = std::move(*found);
            app->posted.erase(found);
        }
        try { if (cancelled.release) { cancelled.release(cancelled.context); } }
        catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
    }
}
void async_app_shutdown(ui_app* app)
{
    std::vector<ui_app::Posted> cancelled;
    {
        std::lock_guard lock(app->post_mutex);
        if (app->stopping) { return; }
        app->stopping = true;
        cancelled.swap(app->posted);
    }
    const bool previous = async_app_set_dispatching(app, true);
    binding_app_error(app, ui_app_cancel_commands(app));
    for (const auto& item : cancelled) {
        try { if (item.release) { item.release(item.context); } }
        catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
    }
    async_app_set_dispatching(app, previous);
}
ui_result binding_app_check(ui_app* app) { return check(app); }
windows_text::FontResources windows_text::app_fonts(const ui_app* app) { return app ? app->font_resources : FontResources{}; }

ui_result ui_app_register_font(ui_app* app,const void* bytes,size_t size,ui_font_id* id) {
    if (auto status=check(app);status!=UI_OK) return status;
    if (!id || !bytes || !size || size>64*1024*1024) return UI_ERROR_INVALID_ARGUMENT;
    if (app->drawing || !app->next_font_id) return UI_ERROR_INVALID_STATE;
#ifdef _WIN32
    try {
        auto next=windows_text::register_font(app->font_resources,bytes,size,app->next_font_id);
        windows_text::discard_font_layouts(app->font_resources);
        app->font_resources=std::move(next); *id=app->next_font_id++;
        binding_app_dirty(app); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (...) { return UI_ERROR_PLATFORM; }
#else
    return UI_ERROR_UNSUPPORTED;
#endif
}
ui_result ui_app_unregister_font(ui_app* app,ui_font_id id) {
    if (auto status=check(app);status!=UI_OK) return status;
    if (!id) return UI_ERROR_INVALID_ARGUMENT;
    if (app->drawing) return UI_ERROR_INVALID_STATE;
#ifdef _WIN32
    try {
        auto next=windows_text::unregister_font(app->font_resources,id);
        windows_text::discard_font_layouts(app->font_resources);
        app->font_resources=std::move(next); binding_app_dirty(app); return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (...) { return UI_ERROR_PLATFORM; }
#else
    return UI_ERROR_UNSUPPORTED;
#endif
}
ui_result ui_app_font_family_count(ui_app* app,ui_font_id id,uint32_t* count) {
    if (auto status=check(app);status!=UI_OK) return status;
    if (!count || !id) return UI_ERROR_INVALID_ARGUMENT;
#ifdef _WIN32
    try { *count=uint32_t(windows_text::font_families(app->font_resources,id).size()); return UI_OK; }
    catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
#else
    return UI_ERROR_UNSUPPORTED;
#endif
}
ui_result ui_app_font_family(ui_app* app,ui_font_id id,uint32_t index,char* text,size_t* size) {
    if (auto status=check(app);status!=UI_OK) return status;
    if (!size || !id) return UI_ERROR_INVALID_ARGUMENT;
#ifdef _WIN32
    try {
        const auto& families=windows_text::font_families(app->font_resources,id);
        if (index>=families.size()) return UI_ERROR_INVALID_ARGUMENT;
        const auto& family=families[index]; const auto needed=family.size()+1;
        if (!text) { *size=needed; return UI_OK; }
        if (*size<needed) { *size=needed; return UI_ERROR_INVALID_ARGUMENT; }
        std::memcpy(text,family.c_str(),needed); *size=needed; return UI_OK;
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
#else
    (void)index; (void)text; return UI_ERROR_UNSUPPORTED;
#endif
}
void devtools::report(ui_node& node, ui_model* model, const ui_binding_options& options, ui_result status,
                      const char* code, const char* phase, const char* message, const char* actual_type) noexcept
{
    try {
        auto* app = node.owner;
        node.binding_message = message ? message : "Binding failed";
        const auto property = options.property == UI_PROPERTY_CONTENT ? UI_PROPERTY_TEXT : options.property;
        const auto path = options.path ? options.path : "";
        auto found = std::find_if(app->diagnostics.begin(), app->diagnostics.end(), [&](const auto& d) {
            return d.object == node.debug_id && d.property == property && d.path == path;
        });
        if (found == app->diagnostics.end()) {
            if (app->diagnostics.size() == 2048) {
                auto retired = std::find_if(app->diagnostics.begin(), app->diagnostics.end(), [](const auto& d) { return !d.active; });
                app->diagnostics.erase(retired == app->diagnostics.end() ? app->diagnostics.begin() : retired);
            }
            app->diagnostics.emplace_back();
            found = std::prev(app->diagnostics.end());
        }
        auto& entry = *found;
        entry.object = node.debug_id;
        entry.model = model ? model->debug_id : 0;
        entry.fleck_id = node.fleck_id;
        entry.property = property;
        entry.path = path;
        entry.code = code;
        entry.phase = phase;
        entry.message = node.binding_message;
        entry.status = status;
        entry.expected_type = property == UI_PROPERTY_TEXT ? (options.mode == UI_BIND_TWO_WAY || (options.mode == UI_BIND_DEFAULT && is_editor(node.kind)) ? "string" : "string|bool|integer|number")
            : text_format::property(property) ? devtools::value_type(text_format::kind(property)) : visual_state::property(property) ? devtools::value_type(visual_state::kind(property)) : border::property(property) ? devtools::value_type(border::kind(property)) : property == UI_PROPERTY_DATA_CONTEXT ? "model" : visual_layout::scalar_metric(property) ? "number" : property == UI_PROPERTY_WEIGHT ? "integer" : visual_layout::dimension(property) ? "length" : property == UI_PROPERTY_MARGIN ? "thickness" : visual_layout::property(property) ? "string" : property == UI_PROPERTY_IS_ENABLED ? "bool" : property == UI_PROPERTY_COMMAND ? "command" : devtools::boolean_target(node.kind) ? "bool|integer(0..1)" : "integer(0..100)";
        entry.actual_type = "unknown";
        if (model && options.path) {
            const auto registered = model->properties.find(options.path);
            if (registered != model->properties.end()) { entry.actual_type = devtools::value_type(registered->second.kind); }
        }
        if (actual_type) { entry.actual_type = actual_type; }
        entry.source = node.source;
        entry.location = node.location;
        entry.active = true;
        ++entry.occurrences;
        entry.sequence = ++app->diagnostic_sequence;
    } catch (...) { /* Instrumentation must not change binding failure behavior. */ }
}
void devtools::recover(ui_node& node, ui_property property, std::string_view path) noexcept
{
    const auto target = property == UI_PROPERTY_CONTENT ? UI_PROPERTY_TEXT : property;
    for (auto& entry : node.owner->diagnostics) {
        if (entry.object == node.debug_id && entry.property == target && entry.path == path && entry.active) {
            entry.active = false;
            entry.sequence = ++node.owner->diagnostic_sequence;
        }
    }
}
std::string devtools::diagnostics_json(ui_app* app, uint64_t after_sequence)
{
    using docking::json::quote;
    std::ostringstream json;
    json << "{\"version\":1,\"sequence\":\"" << app->diagnostic_sequence << "\",\"diagnostics\":[";
    bool comma = false;
    for (const auto& entry : app->diagnostics) {
        if (entry.sequence <= after_sequence) { continue; }
        if (std::exchange(comma, true)) { json << ','; }
        auto property_location = entry.location;
        const auto attribute_name = properties::name(entry.property);
        const auto attribute = std::find_if(entry.location.attributes.begin(), entry.location.attributes.end(), [&](const auto& item) {
            return item.name == attribute_name || (entry.property == UI_PROPERTY_TEXT && item.name == "Content");
        });
        if (attribute != entry.location.attributes.end()) { property_location.element = attribute->value; }
        json << "{\"sequence\":\"" << entry.sequence << "\",\"object\":\"" << entry.object << "\",\"model\":\"" << entry.model
            << "\",\"page\":\"" << (entry.source ? entry.source->page_id : 0)
            << "\",\"fleckId\":" << quote(entry.fleck_id) << ",\"property\":" << entry.property << ",\"path\":" << quote(entry.path)
            << ",\"code\":" << quote(entry.code) << ",\"phase\":" << quote(entry.phase) << ",\"message\":" << quote(entry.message)
            << ",\"status\":" << entry.status << ",\"active\":" << (entry.active ? "true" : "false") << ",\"occurrences\":" << entry.occurrences
            << ",\"expectedType\":" << quote(entry.expected_type) << ",\"actualType\":" << quote(entry.actual_type)
            << ",\"source\":" << location_json(entry.source.get(), entry.location)
            << ",\"propertySource\":" << location_json(entry.source.get(), property_location) << "}";
    }
    return json.str() + "]}";
}
void devtools::dispatch_diagnostics(ui_app* app) noexcept
{
    if (app->dispatching) { return; }
    struct DispatchScope { ui_app& app; ~DispatchScope() { app.dispatching = false; } } scope{*app};
    app->dispatching = true;
    const auto sequence = app->diagnostic_sequence;
    std::optional<std::string> snapshot, log;
    try {
        if (app->diagnostic_callback && app->delivered_diagnostic_sequence != sequence) { snapshot = diagnostics_json(app); }
        if (app->diagnostic_log_callback && app->logged_diagnostic_sequence != sequence) {
            log = diagnostics_json(app, app->logged_diagnostic_sequence);
        }
    } catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); return; }
    if (snapshot && app->diagnostic_callback) {
        try {
            app->delivered_diagnostic_sequence = sequence;
            app->diagnostic_callback(snapshot->c_str(), app->diagnostic_context);
        } catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
    }
    if (log && app->diagnostic_log_callback) {
        try {
            app->logged_diagnostic_sequence = sequence;
            app->diagnostic_log_callback(log->c_str(), app->diagnostic_log_context);
        } catch (...) { binding_app_error(app, UI_ERROR_INVALID_STATE); }
    }
}
ui_result ui_node_get_validation(ui_node* node,ui_property property,ui_validation_result* result)
{
    if (!node || !result || !properties::valid(property)) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status=check(node->owner);status!=UI_OK) return status;
    *result={UI_OK,"",""};
    const auto target=properties::canonical(property);
    for (const auto& binding : node->bindings) {
        if (binding.property!=target || !binding.invalid) continue;
        *result={UI_ERROR_INVALID_STATE,"invalid_binding","Bound value failed validation"};
        for (const auto& entry : node->owner->diagnostics) {
            if (entry.active && entry.object==node->debug_id && entry.property==target && entry.path==binding.path) {
                *result={entry.status,entry.code.c_str(),entry.message.c_str()};
                break;
            }
        }
        break;
    }
    return UI_OK;
}
ui_result ui_app_get_binding_diagnostics(ui_app* app, char* output, size_t* size)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    try { return devtools::copy_json(devtools::diagnostics_json(app), output, size); }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_app_set_diagnostic_callback(ui_app* app, ui_diagnostic_callback callback, void* context)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    app->diagnostic_callback = callback;
    app->diagnostic_context = context;
    app->delivered_diagnostic_sequence = 0;
    return UI_OK;
}
ui_result ui_app_dispatch_diagnostics(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->dispatching) { return UI_ERROR_INVALID_STATE; }
    devtools::dispatch_diagnostics(app);
    return UI_OK;
}
ui_result ui_app_set_diagnostic_log_callback(ui_app* app, ui_diagnostic_callback callback, void* context)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->dispatching) { return UI_ERROR_INVALID_STATE; }
    app->diagnostic_log_callback = callback;
    app->diagnostic_log_context = context;
    app->logged_diagnostic_sequence = 0;
    return UI_OK;
}
uint64_t devtools::next_id(ui_app* app)
{
    if (app->next_debug_id == UINT64_MAX) { throw std::bad_alloc(); }
    return app->next_debug_id++;
}
namespace {
ui_node* projected_child(ui_node& host, ui_node& visual) {
    if (visual.content_owner == &host) return &visual;
    if (visual.component) return nullptr;
    for (auto* child : visual.children)
        if (auto* projected = projected_child(host, *child)) return projected;
    return nullptr;
}

ui_result read_tree(ui_node* node, ui_tree_kind tree) {
    if (!node || (tree != UI_VISUAL_TREE && tree != UI_LOGICAL_TREE)) return UI_ERROR_INVALID_ARGUMENT;
    return check(node->owner);
}
}

ui_result ui_node_get_parent(ui_node* node, ui_tree_kind tree, ui_node** parent) {
    if (!parent) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = read_tree(node, tree); status != UI_OK) return status;
    *parent = node->parent;
    if (tree == UI_LOGICAL_TREE) {
        if (node->content_owner) *parent = node->content_owner;
        else if (node->parent && node->parent->component) *parent = nullptr;
    }
    return UI_OK;
}

ui_result ui_node_get_child_count(ui_node* node, ui_tree_kind tree, uint32_t* count) {
    if (!count) return UI_ERROR_INVALID_ARGUMENT;
    if (auto status = read_tree(node, tree); status != UI_OK) return status;
    *count = uint32_t(node->children.size());
    if (tree == UI_LOGICAL_TREE && !node->children.empty()) {
        if (node->component) *count = projected_child(*node, *node->children.front()) ? 1u : 0u;
        else if (node->children.front()->content_owner) *count = 0;
    }
    return UI_OK;
}

ui_result ui_node_get_child(ui_node* node, ui_tree_kind tree, uint32_t index, ui_node** child) {
    if (!child) return UI_ERROR_INVALID_ARGUMENT;
    uint32_t count;
    if (auto status = ui_node_get_child_count(node, tree, &count); status != UI_OK) return status;
    if (index >= count) return UI_ERROR_INVALID_ARGUMENT;
    *child = tree == UI_LOGICAL_TREE && node->component ? projected_child(*node, *node->children.front()) : node->children[index];
    return UI_OK;
}

ui_result ui_node_get_debug_id(ui_node* node, uint64_t* id)
{
    if (!node || !id) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    *id = node->debug_id;
    return UI_OK;
}
ui_result ui_dock_item_get_debug_id(ui_dock_item* item, uint64_t* id)
{
    if (const auto status = docking::check(item); status != UI_OK) { return status; }
    return ui_node_get_debug_id(item->content, id);
}

ui_result ui_app_find_debug_node(ui_app* app, uint64_t id, ui_node** output)
{
    if (!output) { return UI_ERROR_INVALID_ARGUMENT; }
    *output = nullptr;
    if (const auto status = check(app); status != UI_OK) { return status; }
    for (const auto& node : app->nodes) { if (node->debug_id == id) { *output = node.get(); return UI_OK; } }
    return UI_ERROR_INVALID_ARGUMENT;
}
ui_result ui_app_set_fleck_source_uri(ui_app* app, const char* uri)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!uri || !app->fleck_page.source) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        (void)utf8_text::decode(uri);
        app->fleck_page.source->uri = uri;
        return UI_OK;
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_app_attach_fleck_source_map(ui_app* app, const char* text, size_t size, ui_fleck_diagnostic* diagnostic)
{
    if (diagnostic) { *diagnostic = {}; }
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!text || !app->fleck_page.source) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        const auto map = docking::json::Reader({text, size}).parse();
        (void)map.at("version").number(1, 1);
        if (map.at("fingerprint").string() != app->fleck_page.source->fingerprint) { throw std::range_error("Source map does not match this Fleck page"); }
        const auto& entries = map.at("nodes").array();
        if (entries.size() != app->fleck_page.nodes.size()) { throw std::range_error("Source map node count mismatch"); }
        const auto read_span = [](const docking::json::Value& value) {
            constexpr int limit = 1024 * 1024;
            devtools::Span span;
            span.offset = value.at("offset").number(0, limit);
            span.length = value.at("length").number(0, limit);
            if (span.length > limit - span.offset) { throw std::range_error("Source range exceeds Fleck input size"); }
            span.line = value.at("line").number(1, limit + 1);
            span.column = value.at("column").number(1, limit + 1);
            span.end_line = value.at("endLine").number(span.line, limit + 1);
            span.end_column = value.at("endColumn").number(span.line == span.end_line ? span.column : 1, limit + 1);
            return span;
        };
        std::vector<devtools::Location> locations;
        locations.reserve(entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            if (entry.at("kind").number(0, UI_PATH) != app->fleck_page.nodes[i]->kind || entry.at("id").string() != app->fleck_page.nodes[i]->fleck_id) { throw std::range_error("Source map node mismatch"); }
            const auto& location = entry.at("location");
            devtools::Location next;
            if (location.has("uri")) { next.source_uri = location.at("uri").string(); }
            next.element = read_span(location.at("range"));
            for (const auto& attribute : location.at("attributes").array()) {
                const auto& name = attribute.at("name").string();
                if ((!devtools::attribute_allowed(app->fleck_page.nodes[i]->kind, name) &&
                     !(app->fleck_page.nodes[i]->component && (name == "DataContext" || name == "Template" || app->fleck_page.nodes[i]->component->values.contains(name)))) ||
                    std::any_of(next.attributes.begin(), next.attributes.end(), [&](const auto& a) { return a.name == name; })) { throw std::range_error("Invalid source attribute"); }
                const auto range = read_span(attribute.at("range"));
                const auto value = read_span(attribute.at("value"));
                if (range.offset < next.element.offset || range.offset + range.length > next.element.offset + next.element.length ||
                    value.offset < range.offset || value.offset + value.length > range.offset + range.length) { throw std::range_error("Source attribute outside element"); }
                next.attributes.push_back({name, range, value});
            }
            locations.push_back(std::move(next));
        }
        auto uri = map.at("uri").string();
        auto model = map.at("model").string();
        app->fleck_page.source->uri = std::move(uri);
        app->fleck_page.source->model = std::move(model);
        for (size_t i = 0; i < locations.size(); ++i) { app->fleck_page.nodes[i]->location = std::move(locations[i]); }
        return UI_OK;
    } catch (const std::range_error& error) {
        if (diagnostic) { fleck::report({}, 0, UI_FLECK_VALUE, error.what(), *diagnostic); }
        return UI_ERROR_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_app_set_debug_highlight(ui_app* app, uint64_t id, uint32_t lease_ms)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (id && (!lease_ms || lease_ms > 5000)) { return UI_ERROR_INVALID_ARGUMENT; }
    if (id && std::none_of(app->nodes.begin(), app->nodes.end(), [id](const auto& n) { return n->debug_id == id; })) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    if (app->debug_highlight != id) { binding_app_dirty(app); }
    app->debug_highlight = id;
    app->debug_highlight_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(lease_ms);
    return UI_OK;
}
ui_result ui_app_set_debug_picking(ui_app* app, uint32_t enabled)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
#ifndef _WIN32
    if (!app->headless) { return UI_ERROR_UNSUPPORTED; }
#endif
    if (enabled > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto busy = [](const ui_window_context& window) {
        return window.dragged_control || window.pressed_control || window.native_dock_move || window.ime_composing ||
            window.host_pointer_down || window.hosted_window.drag || window.chrome_pressed != UI_DOCK_PART_COUNT;
    };
    if (enabled) {
        bool available = false, blocked = false;
        for_each_session(*app, [&] {
            if (!main_window(*app).opened || app->ended || main_window(*app).closing) { return; }
            available = true;
            blocked = blocked || busy(*app) || busy(main_window(*app)) ||
                (app->workspace && app->workspace->drag.prepared) ||
                std::any_of(app->dock_windows.begin(), app->dock_windows.end(), [&](const auto& entry) {
                    return entry.second.get() != app->current && busy(*entry.second);
                });
        });
        if (!available || blocked) { return UI_ERROR_INVALID_STATE; }
    }
    app->debug_picking = enabled != 0;
    if (enabled) { app->debug_selection = 0; }
    const auto reset = [](ui_window_context& window) {
        window.debug_paint_order.clear();
        window.debug_hover = 0;
        window.dirty = true;
    };
    for_each_session(*app, [&] {
        reset(*app);
        if (app->current != app->main_state.get()) { reset(*app->main_state); }
        for (auto& [id, window] : app->dock_windows) {
            (void)id;
            if (window.get() != app->current) { reset(*window); }
        }
    });
    return UI_OK;
}

static ui_result debug_snapshot_json(ui_app* app, std::string& output)
{
    try {
        using docking::json::quote;
        const auto rectangle = [](layout::Rect r) { return "[" + std::to_string(r.left) + "," + std::to_string(r.top) + "," + std::to_string(r.right) + "," + std::to_string(r.bottom) + "]"; };
        std::ostringstream json;
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        json << "{\"version\":1,\"timestamp\":" << timestamp << ",\"selection\":\"" << app->debug_selection
            << "\",\"picking\":" << (app->debug_picking ? "true" : "false")
            << ",\"highlight\":\"" << app->debug_highlight << "\"";
        const auto& refresh = app->fleck_refresh_report;
        if (refresh.sequence || app->queued_fleck_refresh || app->applying_queued_fleck_refresh) {
            json << ",\"fleckRefresh\":{\"sequence\":\"" << refresh.sequence
                << "\",\"result\":" << refresh.result << ",\"elapsedMicroseconds\":" << refresh.elapsed_us
                << ",\"pending\":" << (app->queued_fleck_refresh || app->applying_queued_fleck_refresh ? "true" : "false") << ",\"message\":" << quote(refresh.diagnostic.message) << "}";
        }
        json << ",\"windows\":[";
        bool window_comma = false;
        const auto write_window = [&](const ui_window_context& window, unsigned id) {
            if (std::exchange(window_comma, true)) { json << ','; }
            json << "{\"id\":" << id << ",\"title\":" << quote(window.window_title_utf8)
                << ",\"root\":\"" << (window.root ? window.root->debug_id : 0)
                << "\",\"width\":" << window.width << ",\"height\":" << window.height
                << ",\"dpi\":" << window.dpi << ",\"opened\":" << (window.opened && !window.closing ? "true" : "false")
                << ",\"ownerWindow\":" << app->active_session->id;
            if (!window.dock_window) {
                const auto& state = app->fleck_refresh_report;
                json << ",\"fleckRefresh\":{\"sequence\":\"" << state.sequence << "\",\"result\":" << state.result
                    << ",\"elapsedMicroseconds\":" << state.elapsed_us << ",\"pending\":"
                    << (app->queued_fleck_refresh || app->applying_queued_fleck_refresh ? "true" : "false")
                    << ",\"message\":" << quote(state.diagnostic.message) << "}";
            }
            json << "}";
        };
        for_each_session(*app, [&] {
            write_window(main_window(*app), app->active_session->id);
            for (const auto& [id, window] : app->dock_windows) {
                (void)id;
                const auto& context = app->current == window.get() ? static_cast<ui_window_context&>(*app) : *window;
                write_window(context, context.debug_window_id);
            }
        });
        json << "],\"dockItems\":[";
        std::map<const ui_node*, unsigned> item_windows;
        bool separator = false;
        for_each_session(*app, [&] {
            if (!app->workspace) { return; }
            const auto& dock = *app->workspace;
            for (const auto& item : dock.items) {
                const auto placement = dock.layout.placements.find(item->id);
                if (placement == dock.layout.placements.end()) { continue; }
                const auto& place = placement->second;
                const auto region = dock.layout.regions.find(place.group);
                const auto local = place.state == UI_DOCK_FLOATING ? docking::window_of(dock.layout, place.group) : 0;
                const auto* context = find_window(*app, local);
                const auto window = local && context ? context->debug_window_id : app->active_session->id;
                if (std::exchange(separator, true)) { json << ','; }
                item_windows.emplace(item->content, window);
                json << "{\"id\":" << quote(item->id) << ",\"title\":" << quote(item->title)
                    << ",\"object\":\"" << item->content->debug_id << "\",\"window\":" << window
                    << ",\"ownerWindow\":" << app->active_session->id
                    << ",\"group\":" << place.group << ",\"state\":" << place.state
                    << ",\"active\":" << (region != dock.layout.regions.end() && region->second.active == item->id ? "true" : "false") << "}";
            }
        });
        json << "],\"nodes\":[";
        bool comma = false;
        std::set<const devtools::Source*> inactive_sources;
        for_each_session(*app, [&] {
            for (const auto& page : app->inactive_fleck_pages) { inactive_sources.insert(page.source.get()); }
        });
        for (const auto& node : app->nodes) {
            if (inactive_sources.contains(node->source.get())) { continue; }
            if (std::exchange(comma, true)) { json << ','; }
            uint32_t length = 0;
            if (ui_node_get_text(node.get(), nullptr, &length) != UI_OK) { return UI_ERROR_INVALID_STATE; }
            std::string text(length, '\0');
            if (ui_node_get_text(node.get(), text.data(), &length) != UI_OK) { return UI_ERROR_INVALID_STATE; }
            text.resize(length - 1);
            unsigned window = node->window_owner ? node->window_owner->id : 0;
            for (auto* ancestor = node.get(); ancestor && !item_windows.empty(); ancestor = ancestor->parent) {
                const auto item = item_windows.find(ancestor);
                if (item != item_windows.end()) { window = item->second; break; }
            }
            ui_property_source context_source{};
            (void)ui_node_get_data_context_source(node.get(), &context_source);
            auto* data_context = binding_data_context(*node);
            json << "{\"id\":\"" << node->debug_id << "\",\"parent\":\"" << (node->parent ? node->parent->debug_id : 0)
                << "\",\"page\":\"" << (node->source ? node->source->page_id : 0) << "\",\"fleckId\":" << quote(node->fleck_id)
                << ",\"siblingIndex\":" << (node->parent ? std::distance(node->parent->children.begin(), std::find(node->parent->children.begin(), node->parent->children.end(), node.get())) : 0)
                << ",\"kind\":" << node->kind << ",\"type\":" << quote(devtools::control_name(node->kind))
                << ",\"text\":" << quote(node->kind == UI_PASSWORD_BOX ? "" : text) << ",\"value\":" << node->value
                << ",\"enabled\":" << (node->is_enabled() ? "true" : "false") << ",\"focused\":" << (binding_node_focused(node.get()) ? "true" : "false")
                << ",\"bounds\":" << rectangle(node->bounds) << ",\"clip\":" << rectangle(node->visible)
                << ",\"layoutBounds\":" << rectangle(node->layout_cache.arranged)
                << ",\"worldTransform\":[" << node->world_transform.m11 << ',' << node->world_transform.m12 << ','
                << node->world_transform.m21 << ',' << node->world_transform.m22 << ',' << node->world_transform.offset_x
                << ',' << node->world_transform.offset_y << ']'
                << ",\"componentType\":" << quote(node->component ? node->component->type->key.first + ":" + node->component->type->key.second : "")
                << ",\"scopePath\":" << quote(node->scope_path)
                << ",\"dataContext\":{\"model\":\"" << (data_context ? data_context->debug_id : 0)
                << "\",\"source\":" << quote(properties::source_name(context_source)) << "}"
                << ",\"layoutWork\":{\"measure\":" << node->layout_cache.measures
                << ",\"arrange\":" << node->layout_cache.arranges
                << ",\"measureHits\":" << node->layout_cache.measure_hits
                << ",\"arrangeHits\":" << node->layout_cache.arrange_hits
                << ",\"invalidations\":" << node->layout_cache.invalidations << "}"
                << (node->items_source ? ",\"items\":{\"count\":" + std::to_string(node->item_count()) +
                    ",\"firstRealized\":" + std::to_string(node->first_realized) + ",\"realized\":" + std::to_string(node->children.size()) + "}" : "")
                << ",\"properties\":" << devtools::node_properties_json(*node)
                << ",\"window\":" << window << ",\"source\":" << devtools::location_json(node->source.get(), node->location) << ",\"animations\":[";
            bool animation_comma = false;
            for (const auto& track : app->animations) {
                if (track.node != node.get() || track.lifetime.expired()) continue;
                if (std::exchange(animation_comma,true)) json << ',';
                json << "{\"property\":" << quote(properties::name(track.property))
                     << ",\"paused\":" << (track.paused ? "true" : "false")
                     << ",\"holding\":" << (track.ended ? "true" : "false") << '}';
            }
            json << "],\"bindings\":[";
            bool binding_comma = false;
            for (const auto& binding : node->declared_bindings) {
                if (std::exchange(binding_comma, true)) { json << ','; }
                const auto live = std::find_if(node->bindings.begin(), node->bindings.end(), [&](const auto& b) { return b.property == binding.property && b.path == binding.path; });
                json << "{\"model\":\"" << binding.model_id << "\",\"property\":" << binding.property << ",\"path\":" << quote(binding.path)
                    << ",\"mode\":" << binding.mode << ",\"trigger\":" << binding.trigger << ",\"attached\":" << (binding.model ? "true" : "false")
                    << ",\"usesDataContext\":" << (binding.uses_data_context ? "true" : "false")
                    << ",\"pending\":" << (live != node->bindings.end() && live->pending ? "true" : "false") << "}";
            }
            json << "]}";
        }
        json << "],\"models\":[";
        comma = false;
        for (const auto& model : app->models) {
            if (std::exchange(comma, true)) { json << ','; }
            json << devtools::model_json(model.get(), "");
        }
        json << "],\"bindingDiagnostics\":" << devtools::diagnostics_json(app) << "}";
        output = json.str();
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_app_enable_debug_server(ui_app* app, const char* name)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!name) { app->debug_pipe.reset(); return UI_OK; }
    if (!*name || std::strlen(name) > 240 || std::strpbrk(name, "\\/ \t\r\n")) { return UI_ERROR_INVALID_ARGUMENT; }
#ifdef _WIN32
    try {
        (void)utf8_text::decode(name);
        if (app->debug_pipe && app->debug_pipe->name() == name) { return UI_OK; }
        auto next = std::make_unique<devtools::DebugPipe>(name);
        app->debug_pipe = std::move(next);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_PLATFORM; }
#else
    return UI_ERROR_UNSUPPORTED;
#endif
}
ui_result ui_app_get_debug_snapshot(ui_app* app, char* output, size_t* size)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    std::string json;
    const auto status = debug_snapshot_json(app, json);
    return status == UI_OK ? devtools::copy_json(json, output, size) : status;
}
std::vector<std::unique_ptr<ui_command>>& async_app_commands(ui_app* app) { return app->commands; }
bool async_app_set_dispatching(ui_app* app, bool dispatching) { return std::exchange(app->dispatching, dispatching); }
std::vector<std::unique_ptr<ui_node>>& binding_app_nodes(ui_app* app) { return app->nodes; }
void queue_items_refresh(ui_node& node)
{
    node.items_refresh_pending = true;
    node.owner->has_pending_items = true;
    binding_app_dirty(node.owner);
}

void binding_app_dirty(ui_app* app)
{
    app->dirty = true;
    const auto dirty = [](ui_window_session& session) {
        session.main_state->dirty = true;
        for (auto& [id, window] : session.dock_windows) { (void)id; window->dirty = true; }
    };
    dirty(*app);
    if (app->active_session != &app->primary_window) { dirty(app->primary_window); }
    for (const auto& window : app->windows) { if (app->active_session != window.get()) { dirty(*window); } }
}
void reconcile_input_state(ui_node& changed)
{
    auto* app = changed.owner;
    if (!app) return;
    const auto unavailable = [](const ui_node* node) { return node && (!node->is_visible() || !node->is_enabled()); };
    const auto pointer_unavailable = [](const ui_node* node) { return node && !node->accepts_input(); };
    const auto clear = [&](ui_window_context& window) {
        if (unavailable(window.focus)) {
            window.focus = nullptr; window.keyboard_focus = false; window.ime_composing = false;
            window.host_text.observe(nullptr);
#ifdef _WIN32
            if (window.window) {
                if (auto ime = ImmGetContext(window.window)) { ImmNotifyIME(ime,NI_COMPOSITIONSTR,CPS_CANCEL,0); ImmReleaseContext(window.window,ime); }
            }
#endif
        }
        const bool release = pointer_unavailable(window.pressed_control) || pointer_unavailable(window.dragged_control);
        if (pointer_unavailable(window.hovered_control)) window.hovered_control = nullptr;
        if (pointer_unavailable(window.pressed_control)) window.pressed_control = nullptr;
        if (pointer_unavailable(window.dragged_control)) {
            window.dragged_control->dragging = false;
            if (window.dragged_control->kind == UI_DOCUMENT_VIEW) document::pointer(*window.dragged_control,3,0,0);
            window.dragged_control = nullptr;
        }
        if (release) {
            window.host_pointer_down = false;
#ifdef _WIN32
            if (window.window && GetCapture() == window.window) ReleaseCapture();
#endif
        }
#if defined(SUNFLECK_X11)
        if (unavailable(window.paste_target)) window.paste_target = nullptr;
#endif
        std::erase_if(window.interactive,unavailable);
        std::erase_if(window.scrollable,pointer_unavailable);
        std::erase_if(window.overlays,[&](const auto& overlay) { return unavailable(overlay.owner); });
        window.dirty = true;
    };
    const auto session = [&](ui_window_session& value) {
        clear(*value.main_state);
        for (auto& [id,window] : value.dock_windows) { (void)id; clear(*window); }
    };
    clear(*app); session(*app);
    if (app->active_session != &app->primary_window) session(app->primary_window);
    for (const auto& window : app->windows) if (app->active_session != window.get()) session(*window);
}
void binding_app_error(ui_app* app, ui_result status)
{
    if (app->binding_error == UI_OK) { app->binding_error = status; }
}
ui_result ui_node_get_logical_focus(ui_node* scope, ui_node** target)
{
    if (!scope || !target || !scope->is_focus_scope) return UI_ERROR_INVALID_ARGUMENT;
    auto& app = *scope->owner;
    if (auto status = check(&app); status != UI_OK) return status;
    *target = nullptr;
    if (!scope->logical_focus_id) return UI_OK;
    SessionScope session(app,scope->window_owner);
    for (const auto& n : app.nodes) {
        if (n->debug_id == scope->logical_focus_id && n->is_visible() && n->is_enabled() && n->focus_scope() == scope) {
            *target = n.get();
            break;
        }
    }
    return UI_OK;
}

ui_result ui_node_focus(ui_node* node)
{
    if (!node) return UI_ERROR_INVALID_ARGUMENT;
    auto& app = *node->owner;
    if (auto status = check(&app); status != UI_OK) return status;
    if (node->is_focus_scope) {
        ui_node* target = nullptr;
        if (auto status = ui_node_get_logical_focus(node,&target); status != UI_OK) return status;
        if (target) node = target;
    }
    if (!node->is_visible() || !node->is_enabled()) return UI_ERROR_INVALID_STATE;
    SessionScope session(app,node->window_owner);
    auto select = [&](ui_window_context& window) {
        if (std::find(window.interactive.begin(),window.interactive.end(),node) == window.interactive.end()) return false;
        WindowScope context(app,&window == &app ? app.current : &window);
        app.assign_focus(node);
        app.keyboard_focus = true;
        app.reset_caret();
        return true;
    };
    if (select(app)) return UI_OK;
    if (app.current != app.main_state.get() && select(*app.main_state)) return UI_OK;
    for (auto& [id,window] : app.dock_windows)
        if (window.get() != app.current && select(*window)) return UI_OK;
    return UI_ERROR_INVALID_STATE;
}

bool binding_node_focused(ui_node* node)
{
    auto* app = node->owner;
    if (!node->is_enabled() || !node->is_visible()) { return false; }
    const auto focused = [&](const ui_window_context& window) { return window.focus == node && ((app->headless && !window.hosted_input) || window.focused); };
    bool result = focused(*app);
    const auto inspect = [&](const ui_window_session& session) {
        if (session.main_state.get() != app->current && focused(*session.main_state)) { result = true; }
        for (const auto& [id, window] : session.dock_windows) {
            (void)id;
            if (window.get() != app->current && focused(*window)) { result = true; }
        }
    };
    inspect(*app);
    if (app->active_session != &app->primary_window) { inspect(app->primary_window); }
    for (const auto& window : app->windows) { if (window.get() != app->active_session) { inspect(*window); } }
    return result;
}
ui_result ui_model_create(ui_app* app, ui_model** out_model)
{
    if (!out_model) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_model = nullptr;
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (app->models.size() >= 512) { return UI_ERROR_INVALID_STATE; }
    try {
        auto model = std::make_unique<ui_model>();
        model->debug_id = devtools::next_id(app);
        model->owner = app;
        app->models.push_back(std::move(model));
        *out_model = app->models.back().get();
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}
ui_result ui_model_destroy(ui_model* model)
{
    if (!model) { return UI_ERROR_INVALID_ARGUMENT; }
    auto* app = model->owner;
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (model->callbacks || model->notifying || app->dispatching) { return UI_ERROR_INVALID_STATE; }
    std::vector<ui_command*> bound_commands;
    for (auto& command : app->commands) {
        if (async_uses_model(*command,model) || (command->run && std::any_of(app->nodes.begin(), app->nodes.end(), [&](const auto& node) {
            return node->command == command.get() && std::any_of(node->bindings.begin(), node->bindings.end(),
                [&](const auto& binding) { return binding.model == model && binding.property == UI_PROPERTY_COMMAND; });
        }))) { bound_commands.push_back(command.get()); }
    }
    if (const auto status = binding_release_data_context(*model); status != UI_OK) { return status; }
    for_each_session(*app, [&] {
        if (app->fleck_page.exported_model == model) { app->fleck_page.exported_model = nullptr; }
        for (auto& page : app->inactive_fleck_pages) {
            if (page.exported_model == model) { page.exported_model = nullptr; }
        }
    });
    for (auto* command : bound_commands) { binding_app_error(app, ui_command_cancel(command)); }
    for (auto* command : bound_commands) { async_command_dispose(*command); }
    for (const auto& owner : app->models) {
        if (!owner->collection) continue;
        for (auto& item : owner->collection->items)
            if (item.model == model) item.model = nullptr;
    }
    components::model_destroyed(*model);
    for (auto& node : app->nodes) {
        if (node->local_data_context == model) node->local_data_context = nullptr;
        std::erase_if(node->bindings, [&](const auto& b) { return b.model == model; });
        for (auto& declaration : node->declared_bindings) {
            if (declaration.model == model) { declaration.model = nullptr; devtools::recover(*node, declaration.property, declaration.path); }
        }
    }
    for (auto& source : app->sources) { std::erase_if(source->property_observers, [&](const auto& observer) { return observer.first == model; }); }
    std::erase_if(app->models, [&](const auto& m) { return m.get() == model; });
    return UI_OK;
}
ui_result ui_node_set_enabled(ui_node* node, uint32_t enabled)
{
    if (node && !node->applying_property) { ui_binding_value v{}; v.kind = UI_VALUE_BOOL; v.integer = enabled; return ui_node_set_property(node, UI_PROPERTY_IS_ENABLED, &v); }
    if (!node || enabled > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    binding_detach(*node, UI_PROPERTY_IS_ENABLED);
    properties::written(*node, UI_PROPERTY_IS_ENABLED);
    if (node->enabled != (enabled != 0)) { node->enabled = enabled != 0; reconcile_input_state(*node); binding_app_dirty(node->owner); }
    return UI_OK;
}
ui_result ui_node_get_enabled(ui_node* node, uint32_t* enabled)
{
    if (!node || !enabled) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    *enabled = node->is_enabled();
    return UI_OK;
}

namespace {
struct HostedDispatch {
    ui_app& app;
    bool previous;
    explicit HostedDispatch(ui_app& value) : app(value), previous(std::exchange(value.dispatching, true)) {}
    ~HostedDispatch() { app.dispatching = previous; }
};
struct HostedViewScope {
    std::optional<WindowScope> scope;
    bool valid = false;
    explicit HostedViewScope(ui_app* app) {
        if (!app || check(app) != UI_OK) { return; }
        if (app->dock_window == app->hosted_view) { valid = true; return; }
        auto* target = find_window(*app, app->hosted_view);
        if (target) { scope.emplace(*app, target); valid = true; }
    }
};
ui_result check_host(ui_app* app)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!app->headless) { return UI_ERROR_UNSUPPORTED; }
    if (!app->opened || app->closing || app->ended || app->dispatching || app->polling ||
        app->window_callback_depth || app->active_session != &app->primary_window) { return UI_ERROR_INVALID_STATE; }
    app->hosted_input = true;
    return UI_OK;
}
}

ui_result ui_host_select_view(ui_app* app, uint32_t id)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!app->headless) { return UI_ERROR_UNSUPPORTED; }
    if (app->dispatching || app->polling || app->closing || !find_window(*app, id)) { return UI_ERROR_INVALID_STATE; }
    app->hosted_view = id;
    return UI_OK;
}

uint32_t ui_host_capabilities(void)
{ return UI_HOST_CAP_TEXT | UI_HOST_CAP_SCROLL | UI_HOST_CAP_DOCK_VIEWS | UI_HOST_CAP_SEMANTICS | UI_HOST_CAP_EXTERNAL_COMMANDS | UI_HOST_CAP_DRAW_COMMANDS; }

ui_result ui_host_set_render_mode(ui_app* app, uint32_t mode)
{
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (mode > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    if (mode && std::endian::native != std::endian::little) { return UI_ERROR_UNSUPPORTED; }
    if (app->hosted_command_rendering != (mode != 0)) {
        app->hosted_command_rendering = mode != 0;
        binding_app_dirty(app);
    }
    return UI_OK;
}

ui_result ui_host_draw_commands(ui_app* app, const uint8_t** data, uint32_t* size)
{
    if (!data || !size) { return UI_ERROR_INVALID_ARGUMENT; }
    *data = nullptr; *size = 0;
    HostedViewScope scope(app);
    if (!scope.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (!app->hosted_command_rendering || app->dirty || app->hosted_commands.empty()) { return UI_ERROR_INVALID_STATE; }
    using HostedRect = ui_app::HostedRect;
    static_assert(sizeof(HostedRect) == 20 && offsetof(HostedRect, color) == 16);
    const auto& wire = app->hosted_wire_commands;
    if (wire.size() > UINT32_MAX / sizeof(HostedRect)) return UI_ERROR_OUT_OF_MEMORY;
    *data = reinterpret_cast<const uint8_t*>(wire.data());
    *size = static_cast<uint32_t>(wire.size() * sizeof(HostedRect));
    return UI_OK;
}

ui_result ui_host_context_menu(ui_app* app, int32_t x, int32_t y)
{
    HostedViewScope scope(app);
    if (!scope.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (x < -32768 || x > 32768 || y < -32768 || y > 32768) { return UI_ERROR_INVALID_ARGUMENT; }
    try { app->context_menu(x, y); return UI_OK; }
    catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
    catch (...) { return UI_ERROR_INVALID_STATE; }
}

const uint8_t* ui_host_pixels(ui_app* app, uint32_t* width, uint32_t* height)
{
    if (check(app) != UI_OK) { return nullptr; }
    return dock_app_pixels(app, app->hosted_view, width, height);
}

ui_result ui_host_semantic_action(ui_app* app, uint64_t id, uint32_t action)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (action > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        auto found = std::find_if(app->interactive.begin(), app->interactive.end(), [&](auto* n) { return n->debug_id == id; });
        if (found == app->interactive.end() || !(*found)->is_enabled()) { return UI_ERROR_INVALID_STATE; }
        auto* node = *found;
        if (node->visible.right <= node->visible.left || node->visible.bottom <= node->visible.top) { return UI_ERROR_INVALID_STATE; }
        app->assign_focus(node); app->focused = true; app->keyboard_focus = true; app->dirty = true;
        if (action) { app->key(false, false, 0); }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_snapshot(ui_app* app, char* output, size_t* size)
{
    if (const auto status = check(app); status != UI_OK) { return status; }
    if (!app->headless || !size) { return UI_ERROR_INVALID_ARGUMENT; }
    if (app->dispatching || app->polling || app->closing) { return UI_ERROR_INVALID_STATE; }
    try {
        using docking::json::quote;
        std::ostringstream json;
        json << "{\"version\":1,\"views\":[";
        std::vector<unsigned> ids{0};
        if (app->workspace) { for (const auto& w : app->workspace->layout.floating) { ids.push_back(w.id); } }
        bool comma = false;
        for (unsigned id : ids) {
            auto* target = app->dock_window == id ? app->current : find_window(*app, id);
            if (!target) { continue; }
            WindowScope scope(*app, target);
            if (std::exchange(comma, true)) { json << ','; }
            int x = 0, y = 0;
            if (id && app->workspace) { for (const auto& w : app->workspace->layout.floating) { if (w.id == id) { x = w.bounds.x; y = w.bounds.y; } } }
            json << "{\"id\":" << id << ",\"x\":" << x << ",\"y\":" << y
                 << ",\"width\":" << app->width << ",\"height\":" << app->height << ",\"dpi\":" << app->dpi << ",\"nodes\":[";
            bool node_comma = false;
            auto semantic_nodes = app->interactive;
            const auto collect_text = [&](auto&& self, ui_node* node) -> void {
                if (!node) { return; }
                if ((node->kind == UI_TEXT_BLOCK || node->kind == UI_LABEL) &&
                    (!node->text_selectable || !node->is_enabled() || node->display_text().empty())) { semantic_nodes.push_back(node); }
                for (auto* child : node->children) { self(self, child); }
            };
            collect_text(collect_text, app->root);
            for (auto* node : semantic_nodes) {
                const auto& r = node->visible;
                if (r.right <= r.left || r.bottom <= r.top) { continue; }
                const char* role = node->kind == UI_DOCUMENT_VIEW ? "article" : node->kind == UI_TEXT_BLOCK || node->kind == UI_LABEL ? "status" : is_editor(node->kind) ? "textbox" :
                    node->kind == UI_CHECK_BOX || node->kind == UI_SWITCH || node->kind == UI_TOGGLE_BUTTON ? "checkbox" :
                    node->kind == UI_RADIO_BUTTON ? "radio" : node->kind == UI_SLIDER ? "slider" :
                    node->kind == UI_TREE_ITEM ? "treeitem" : node->kind == UI_LIST_ITEM ? "option" :
                    node->kind == UI_TAB_ITEM ? "tab" : "button";
                uint32_t length = 0;
                if (ui_node_get_text(node, nullptr, &length) != UI_OK) { continue; }
                std::string label(length, '\0');
                if (ui_node_get_text(node, label.data(), &length) != UI_OK) { continue; }
                if (!label.empty()) { label.pop_back(); }
                if(node->kind == UI_DOCUMENT_VIEW && node->document) label = node->fleck_id;
                if (std::exchange(node_comma, true)) { json << ','; }
                json << "{\"id\":\"" << node->debug_id << "\",\"parent\":\"" << (node->parent ? node->parent->debug_id : 0)
                     << "\",\"role\":" << quote(role) << ",\"name\":" << quote(is_editor(node->kind) ? (node->fleck_id.empty() ? "Text" : node->fleck_id) : label)
                     << ",\"value\":" << quote(node->kind == UI_PASSWORD_BOX ? "" : label)
                     << ",\"enabled\":" << (node->is_enabled() ? "true" : "false")
                     << ",\"focused\":" << (app->focus == node && app->focused ? "true" : "false")
                     << ",\"checked\":" << (node->value ? "true" : "false")
                     << ",\"number\":" << node->value << ",\"expanded\":" << (node->expanded ? "true" : "false")
                     << ",\"selected\":" << ((node->kind == UI_TREE_ITEM ? node->tree_selected : node->parent && node->parent->value < node->parent->children.size() && node->parent->children[node->parent->value] == node) ? "true" : "false")
                     << ",\"x\":" << r.left << ",\"y\":" << r.top << ",\"width\":" << r.right-r.left << ",\"height\":" << r.bottom-r.top << '}';
            }
            json << "]}";
        }
        json << "]}";
        return devtools::copy_json(json.str(), output, size);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_resize(ui_app* app, uint32_t width, uint32_t height, uint32_t dpi)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    // Bound pixel allocation and integer layout arithmetic for untrusted hosts.
    if (!width || !height || width > 8192 || height > 8192 ||
        uint64_t(width) * height > 16777216 || dpi < 48 || dpi > 768) { return UI_ERROR_INVALID_ARGUMENT; }
    if (app->width != int(width) || app->height != int(height) || app->dpi != dpi) {
        app->hosted_window.scale(app->dpi, dpi);
        if (app->dock_window && app->workspace) {
            for (auto& floating : app->workspace->layout.floating) {
                if (floating.id != app->dock_window) { continue; }
                floating.bounds.x = static_cast<int>(int64_t(floating.bounds.x) * dpi / app->dpi);
                floating.bounds.y = static_cast<int>(int64_t(floating.bounds.y) * dpi / app->dpi);
                floating.bounds.width = int(width); floating.bounds.height = int(height);
            }
        }
        app->width = int(width); app->height = int(height); app->dpi = dpi; app->dirty = true;
        if (!app->dock_window && app->workspace) {
            for (auto& floating : app->workspace->layout.floating) {
                auto* child = find_window(*app, floating.id);
                if (!child || !child->maximized) { continue; }
                auto& bounds = floating.bounds;
                bounds.x = bounds.y = 0;
                bounds.width = child->width = static_cast<int>(uint64_t(width) * child->dpi / dpi);
                const int child_height = static_cast<int>(uint64_t(height) * child->dpi / dpi);
                if (child->hosted_window.minimized_height) { child->hosted_window.minimized_height = child_height; }
                else { bounds.height = child->height = child_height; }
                child->dirty = true;
            }
        }
        if (app->focus) { app->focus->hosted_editor.reveal = true; }
    }
    return UI_OK;
}

ui_result ui_host_set_font(const void* bytes, size_t size)
{
#ifdef SUNFLECK_FREETYPE
    return freetype_set_font(bytes, size);
#else
    (void)bytes; (void)size;
    return UI_ERROR_UNSUPPORTED;
#endif
}

ui_result ui_host_debug_pointer(ui_app* app, uint32_t view_id, ui_pointer_action action, int32_t x, int32_t y)
{
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (!app->debug_picking) { return UI_ERROR_INVALID_STATE; }
    auto* target = find_window(*app, view_id);
    if (!target) { return UI_ERROR_INVALID_ARGUMENT; }
    WindowScope view(*app, target == app ? app->current : target);
    try {
        if (action == UI_POINTER_CANCEL) { return ui_app_set_debug_picking(app, 0); }
        if (action != UI_POINTER_MOVE && action != UI_POINTER_UP) { return UI_ERROR_INVALID_ARGUMENT; }
        if (app->dirty && !app->render(false)) { return UI_ERROR_PLATFORM; }
        if (action == UI_POINTER_UP) {
            app->debug_selection = app->debug_hit(x, y);
            return ui_app_set_debug_picking(app, 0);
        }
        // A highlighted node in a different hosted view must disappear when crossing canvases.
        const auto clear_hover = [&](ui_window_context& window) {
            if (&window != app->current && window.debug_hover) {
                window.debug_hover = 0;
                window.dirty = true;
            }
        };
        clear_hover(*app->main_state);
        for (auto& [id, window] : app->dock_windows) {
            (void)id;
            clear_hover(*window);
        }
        app->update_debug_hover(x, y);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_pointer(ui_app* app, ui_pointer_action action, int32_t x, int32_t y)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (action < UI_POINTER_DOWN || action > UI_POINTER_CANCEL ||
        x < -32768 || x > 32768 || y < -32768 || y > 32768) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        if (action == UI_POINTER_CANCEL) {
            auto& frame = app->hosted_window;
            if ((frame.drag == 1 || frame.drag == 2) && app->workspace) {
                for (auto& floating : app->workspace->layout.floating) {
                    if (floating.id != app->dock_window) { continue; }
                    floating.bounds = frame.drag_origin;
                    app->width = floating.bounds.width; app->height = floating.bounds.height;
                }
            }
            frame.drag = 0; app->chrome_pressed = UI_DOCK_PART_COUNT;
            app->cancel_pointer(x, y);
            app->dirty = true;
            return UI_OK;
        }
        if (app->dirty && !app->render(false)) { return UI_ERROR_PLATFORM; }
        if (app->workspace && app->dock_window) {
            auto& windows = app->workspace->layout.floating;
            auto floating = std::find_if(windows.begin(), windows.end(), [&](const auto& w) { return w.id == app->dock_window; });
            if (floating != windows.end()) {
                auto& bounds = floating->bounds;
                auto& frame = app->hosted_window;
                app->caption_hover(x, y);
                if (action == UI_POINTER_DOWN && (app->chrome_hover != UI_DOCK_PART_COUNT ||
                    (x >= app->width - 8 && y >= app->height - 8))) {
                    app->chrome_pressed = app->chrome_hover;
                    frame.drag = app->chrome_hover == UI_DOCK_PART_WINDOW_TITLE ? 1 : app->chrome_hover == UI_DOCK_PART_COUNT ? 2 : 3;
                    if ((frame.drag <= 2 && app->maximized) || (frame.drag == 2 && frame.minimized_height)) {
                        frame.drag = 0; app->chrome_pressed = UI_DOCK_PART_COUNT; return UI_OK;
                    }
                    frame.grab_x = x; frame.grab_y = y;
                    if (frame.drag <= 2) { frame.drag_origin = bounds; }
                    app->dirty = true; return UI_OK;
                }
                if (frame.drag) {
                    if (action == UI_POINTER_MOVE && frame.drag == 1) {
                        bounds.x += x - frame.grab_x; bounds.y += y - frame.grab_y;
                        app->dirty = true;
                    } else if (action == UI_POINTER_MOVE && frame.drag == 2) {
                        bounds.width = app->width = std::clamp(frame.drag_origin.width + x - frame.grab_x, 160, 4096);
                        bounds.height = app->height = std::clamp(frame.drag_origin.height + y - frame.grab_y, 100, 4096);
                        app->dirty = true;
                    } else if (action == UI_POINTER_UP) {
                        const auto pressed = std::exchange(app->chrome_pressed, UI_DOCK_PART_COUNT);
                        frame.drag = 0; app->dirty = true;
                        if (pressed == app->chrome_hover) {
                            if (pressed == UI_DOCK_PART_WINDOW_CLOSE) { app->caption_action(pressed); }
                            else if (pressed == UI_DOCK_PART_WINDOW_MAXIMIZE) {
                                frame.toggle_maximize(bounds, app->maximized, main_window(*app).width, main_window(*app).height);
                                app->width = bounds.width; app->height = bounds.height;
                            } else if (pressed == UI_DOCK_PART_WINDOW_MINIMIZE) {
                                frame.toggle_minimize(bounds, app->window_frame().height + 4);
                                app->height = bounds.height;
                            }
                        }
                    }
                    return UI_OK;
                }
            }
        }
        app->hover(x, y);
        if (action == UI_POINTER_DOWN) {
            if (app->host_pointer_down) { return UI_ERROR_INVALID_STATE; }
            app->host_pointer_down = true;
            app->focused = true;
            if (app->focus && app->focus->text_selectable) { app->assign_focus(nullptr); }
            app->pressed_control = app->hovered_control;
            app->begin_drag(x, y);
            app->dirty = true;
        } else if (action == UI_POINTER_MOVE) {
            if (app->pointer_handled(action,x,y)) return UI_OK;
            if (app->dragged_control) { app->update_drag_value(*app->dragged_control, x, y); }
        } else if (std::exchange(app->host_pointer_down, false)) {
            if (app->end_pointer_event(x,y)) return UI_OK;
            auto* pressed = std::exchange(app->pressed_control, nullptr);
            app->dirty = true;
            if (app->dragged_control) { app->finish_drag(x, y); }
            else if ((app->workspace && app->workspace->drag.prepared) ||
                     (pressed && pressed == app->hovered_control) || !app->root) { app->click(x, y); }
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_set_text_selectable(ui_node* node, uint32_t enabled)
{
    if (!node || node->kind != UI_TEXT_BLOCK || enabled > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = check(node->owner); status != UI_OK) { return status; }
    if (node->text_selectable != bool(enabled)) {
        node->text_selectable = enabled != 0;
        node->clear_selection();
        node->owner->dirty = true;
    }
    return UI_OK;
}

ui_result ui_host_selected_text(ui_app* app, char* output, size_t* size)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    try {
        auto* node = app->focus;
        std::string selected;
        if (node && node->text_selectable && node->accepts_input()) {
            const auto& text = node->display_text();
            const auto [first, last] = node->selection(text.size());
            selected = utf8_text::encode(std::wstring_view(text).substr(first, last - first));
        }
        return devtools::copy_json(selected, output, size);
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_scroll(ui_app* app, int32_t x, int32_t y, int32_t dy)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (x < -32768 || x > 32768 || y < -32768 || y > 32768 || dy < -32768 || dy > 32768) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        if (app->dirty && !app->render(false)) { return UI_ERROR_PLATFORM; }
        app->scroll_nodes(dy, x, y, false);
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_key_gesture(ui_app* app,unsigned key,unsigned modifiers,uint32_t* handled)
{
    if (!handled || !fleck::valid_gesture(key,modifiers)) return UI_ERROR_INVALID_ARGUMENT;
    *handled = 0;
    HostedViewScope view(app);
    if (!view.valid) return UI_ERROR_INVALID_STATE;
    if (auto status = check_host(app); status != UI_OK) return status;
    *handled = app->key_gesture(key,modifiers);
    return UI_OK;
}

ui_result ui_host_key_event(ui_app* app, ui_host_key key, uint32_t shift)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (key < UI_HOST_TAB || key > UI_HOST_DOWN || shift > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        if (app->dirty && !app->render(false)) { return UI_ERROR_PLATFORM; }
        if (key == UI_HOST_ESCAPE) { return ui_host_pointer(app, UI_POINTER_CANCEL, 0, 0); }
        if (key >= UI_HOST_LEFT) {
            app->key(false,false,key == UI_HOST_LEFT || key == UI_HOST_UP ? -5 : 5,true,key == UI_HOST_UP || key == UI_HOST_DOWN);
        } else {
            app->key(key == UI_HOST_TAB, shift != 0, key == UI_HOST_PREVIOUS ? -5 : key == UI_HOST_NEXT ? 5 : 0);
        }
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_focus(ui_app* app, uint32_t focused)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (focused > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    app->focused = focused != 0;
    app->dirty = true;
    if (focused && app->dock_window && app->workspace) {
        auto& floating = app->workspace->layout.floating;
        auto found = std::find_if(floating.begin(), floating.end(), [&](const auto& w) { return w.id == app->dock_window; });
        if (found != floating.end()) { std::rotate(found, std::next(found), floating.end()); }
    }
    if (!focused) { return ui_host_pointer(app, UI_POINTER_CANCEL, 0, 0); }
    return UI_OK;
}

ui_result ui_host_text_read(ui_app* app, ui_host_text_state* state)
{
    HostedViewScope view(app);
    if (!view.valid) { return UI_ERROR_INVALID_STATE; }
    if (const auto status = check_host(app); status != UI_OK) { return status; }
    if (!state || state->struct_size != sizeof(*state) || state->version != 1) { return UI_ERROR_INVALID_ARGUMENT; }
    HostedDispatch dispatch(*app);
    try {
        if (app->dirty) {
            if (!app->render(false)) { return UI_ERROR_PLATFORM; }
            // A geometry query must not consume the frame that the host still
            // needs to present. Poll(0) will report it on the next host tick.
            app->dirty = true;
        }
        auto* node = app->focus;
        if (!app->focused || !node || !is_editor(node->kind) || !node->is_enabled() ||
            std::find(app->interactive.begin(), app->interactive.end(), node) == app->interactive.end() ||
            node->visible.right <= node->visible.left || node->visible.bottom <= node->visible.top) { node = nullptr; }
        auto& edit = app->host_text;
        edit.observe(node);
        *state = {}; state->struct_size = sizeof(*state); state->version = 1;
        if (!node) { return UI_OK; }
        state->active = 1; state->session = edit.id; state->revision = edit.revision;
        state->multiline = app->allows_line_breaks(*node); state->password = node->kind == UI_PASSWORD_BOX;
        state->left = node->bounds.left; state->top = node->bounds.top; state->right = node->bounds.right; state->bottom = node->bounds.bottom;
        state->clip_left = node->visible.left; state->clip_top = node->visible.top; state->clip_right = node->visible.right; state->clip_bottom = node->visible.bottom;
        state->selection_start = edit.start; state->selection_end = edit.end; state->selection_backward = edit.backward;
        state->foreground = app->gallery_style.colors[UI_THEME_TEXT]; state->background = app->gallery_style.colors[UI_THEME_CONTROL];
        const auto font = text_format::effective(*node);
        TextFormatScope font_scope(font);
        state->font_size = int(std::lround(font.size * app->dpi / 96.0));
        if (font.foreground_set) state->foreground = font.foreground;
        state->text_left = node->text_bounds.left; state->text_top = node->text_bounds.top;
        state->text_right = node->text_bounds.right; state->text_bottom = node->text_bounds.bottom;
        state->line_height = text_line_height(app->dpi);
        state->caret_x = node->hosted_editor.caret_x; state->caret_y = node->hosted_editor.caret_y;
        state->scroll_x = node->hosted_editor.scroll_x; state->scroll_y = node->hosted_editor.scroll_y;
        // Context swaps move short strings through inline storage. Return an
        // App-owned snapshot whose address survives restoring the main view.
        app->hosted_text_snapshot = edit.utf8;
        state->text = app->hosted_text_snapshot.c_str(); state->text_size = static_cast<uint32_t>(app->hosted_text_snapshot.size());
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_text_update(ui_app* app, uint32_t session, uint32_t revision,
    const char* utf8, uint32_t size, uint32_t first, uint32_t last, uint32_t backward)
{
    HostedViewScope host_scope(app);
    if (!host_scope.valid) { return UI_ERROR_INVALID_STATE; }
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    if (const auto status = ui_host_text_read(app, &state); status != UI_OK) { return status; }
    if (!state.active || state.session != session || state.revision != revision) { return UI_ERROR_INVALID_STATE; }
    if ((!utf8 && size) || size > 1024 * 1024 || first > last || backward > 1) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        auto text = hosted_text::decode(std::string_view(utf8 ? utf8 : "", size));
        if (!state.multiline && text.find_first_of(L"\r\n") != std::wstring::npos) { return UI_ERROR_INVALID_ARGUMENT; }
        const auto start = hosted_text::native_offset(text, first);
        const auto end = hosted_text::native_offset(text, last);
        auto& node = *app->focus;
#ifdef _WIN32
        const auto font = text_format::effective(node);
        TextFormatScope font_scope(font);
        const auto boundaries = windows_text::clusters(text);
        if (!std::binary_search(boundaries.begin(), boundaries.end(), start) ||
            !std::binary_search(boundaries.begin(), boundaries.end(), end)) return UI_ERROR_INVALID_ARGUMENT;
#endif
        HostedDispatch dispatch(*app);
        const bool changed = node.text != text;
        node.text = std::move(text); node.caret = backward ? start : end;
        node.select_all = first != last && start == 0 && end == node.text.size();
        node.selection_anchor=backward?end:start; node.anchor_trailing=backward; node.caret_trailing=!backward; node.caret_column.reset();
        node.pending_surrogate = 0;
        if (changed) { node.text_stops.clear(); app->publish_editor_text(node); }
        app->host_text.accept(node, first, last, backward);
        app->reset_caret();
        return UI_OK;
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}


ui_result ui_host_text_preview(ui_app* app, uint32_t session, uint32_t revision,
    const char* utf8, uint32_t size, const ui_host_composition_range* range)
{
    if (!range || range->struct_size != sizeof(*range) || range->version != 1) { return UI_ERROR_INVALID_ARGUMENT; }
    const auto first = range->selection_start, last = range->selection_end, backward = range->backward;
    const auto composition_first = range->composition_start, composition_last = range->composition_end;
    HostedViewScope host_scope(app);
    if (!host_scope.valid) { return UI_ERROR_INVALID_STATE; }
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    if (const auto status = ui_host_text_read(app, &state); status != UI_OK) { return status; }
    if (!state.active || state.session != session || state.revision != revision) { return UI_ERROR_INVALID_STATE; }
    if ((!utf8 && size) || size > 1024 * 1024 || first > last || backward > 1 || composition_first > composition_last) {
        return UI_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto text = hosted_text::decode(std::string_view(utf8 ? utf8 : "", size));
        if (!state.multiline && text.find_first_of(L"\r\n") != std::wstring::npos) { return UI_ERROR_INVALID_ARGUMENT; }
        const auto start = hosted_text::native_offset(text, first), end = hosted_text::native_offset(text, last);
        const auto cs = hosted_text::native_offset(text, composition_first), ce = hosted_text::native_offset(text, composition_last);
#ifdef _WIN32
        const auto font = text_format::effective(*app->focus);
        TextFormatScope font_scope(font);
        const auto boundaries = windows_text::clusters(text);
        for (auto offset : {start, end, cs, ce})
            if (!std::binary_search(boundaries.begin(), boundaries.end(), offset)) return UI_ERROR_INVALID_ARGUMENT;
#endif
        auto& view = app->focus->hosted_editor;
        view.draft = std::move(text); view.active = true; view.composing = true; view.reveal = true;
        view.first = start; view.last = end; view.caret = backward ? start : end;
        view.composition_first = cs; view.composition_last = ce;
        app->reset_caret();
        return UI_OK;
    } catch (const std::range_error&) { return UI_ERROR_INVALID_ARGUMENT; }
      catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_text_hit_test(ui_app* app, uint32_t session, uint32_t revision,
    int32_t x, int32_t y, uint32_t* offset)
{
    if (!offset || x < -32768 || x > 32768 || y < -32768 || y > 32768) { return UI_ERROR_INVALID_ARGUMENT; }
    HostedViewScope host_scope(app);
    if (!host_scope.valid) { return UI_ERROR_INVALID_STATE; }
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    if (const auto status = ui_host_text_read(app, &state); status != UI_OK) { return status; }
    if (!state.active || state.session != session || state.revision != revision) { return UI_ERROR_INVALID_STATE; }
    try {
        const auto& node = *app->focus;
        const auto& text = node.hosted_editor.composing ? node.hosted_editor.draft : node.text;
        const auto local = ui_app::layout_point(node,x,y);
        const TextStop* nearest = nullptr;
        for (const auto& stop : node.text_stops) {
            if (stop.index > text.size() || (stop.index < text.size() && ui_app::low_surrogate(text[stop.index]))) { continue; }
            const auto distance = [&](const TextStop& item) {
                return std::pair{std::abs(item.y + int(state.line_height) / 2 - local.y), std::abs(item.x - local.x)};
            };
            if (!nearest || distance(stop) < distance(*nearest)) { nearest = &stop; }
        }
        *offset = hosted_text::utf16_offset(text, nearest ? nearest->index : text.size());
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
      catch (...) { return UI_ERROR_INVALID_STATE; }
}

ui_result ui_host_text_scroll(ui_app* app, uint32_t session, uint32_t revision, int32_t dx, int32_t dy)
{
    if (dx < -32768 || dx > 32768 || dy < -32768 || dy > 32768) { return UI_ERROR_INVALID_ARGUMENT; }
    HostedViewScope host_scope(app);
    if (!host_scope.valid) { return UI_ERROR_INVALID_STATE; }
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    if (const auto status = ui_host_text_read(app, &state); status != UI_OK) { return status; }
    if (!state.active || state.session != session || state.revision != revision) { return UI_ERROR_INVALID_STATE; }
    auto& view = app->focus->hosted_editor;
    view.scroll_x = std::clamp(view.scroll_x + dx, 0, 100000000);
    view.scroll_y = std::clamp(view.scroll_y + dy, 0, 100000000);
    view.reveal = false; app->dirty = true;
    return UI_OK;
}

const uint8_t* ui_app_pixels(const ui_app* app, uint32_t* width, uint32_t* height)
{
    if (check(app) != UI_OK || !app->headless || app->pixels.empty() || !width || !height) {
        return nullptr;
    }
    *width = app->width;
    *height = app->height;
    return app->pixels.data();
}

ui_result dock_app_check(ui_app* app)
{
    const auto status = check(app);
    if (status != UI_OK) { return status; }
    return main_window(*app).closing ? UI_ERROR_INVALID_STATE : UI_OK;
}
ui_result dock_app_check_workspace(ui_dock* dock)
{
    if (const auto status = check(dock->owner); status != UI_OK) { return status; }
    SessionScope scope(*dock->owner, dock->host->window_owner);
    return dock_app_check(dock->owner);
}
ui_result dock_app_pointer(ui_dock* dock, unsigned window, ui_pointer_action action, int x, int y)
{
    auto* app = dock->owner;
    SessionScope scope(*app, dock->host->window_owner);
    if (app->workspace.get() != dock) { return UI_ERROR_INVALID_STATE; }
    if (!dock->views.contains(window)) { return UI_ERROR_INVALID_ARGUMENT; }
    docking::pointer(*dock, window, action, x, y);
    return UI_OK;
}
const uint8_t* dock_workspace_pixels(ui_dock* dock, unsigned window, uint32_t* width, uint32_t* height)
{
    SessionScope scope(*dock->owner, dock->host->window_owner);
    return dock->owner->workspace.get() == dock ? dock_app_pixels(dock->owner, window, width, height) : nullptr;
}
uint32_t dock_app_callback(ui_dock& dock, ui_dock_item* item, ui_dock_event_kind event)
{
    SessionScope scope(*dock.owner, dock.host->window_owner);
    dock.notifying = true;
    uint32_t result = 1;
    try { result = dock.callback(&dock, item, event, dock.callback_context); } catch (...) { }
    dock.notifying = false;
    return result;
}
ui_dock* dock_app_workspace(ui_app* app) { return app->workspace.get(); }
void dock_app_set_workspace(ui_app* app, std::unique_ptr<ui_dock> dock) { app->workspace = std::move(dock); }
void dock_app_dirty(ui_app* app)
{
    binding_app_dirty(app);
}
docking::Theme& dock_app_theme(ui_app* app) { return app->theme_resource; }
std::map<std::string, docking::Skin>& dock_app_skins(ui_app* app) { return app->dock_skins; }
void dock_app_apply_theme(ui_app* app, docking::Theme theme)
{
    app->gallery_style = theme.palette; app->theme_shape = theme.shape; app->theme_resource = std::move(theme); dock_app_dirty(app);
}
ui_node* dock_app_root(ui_dock* dock)
{
    SessionScope scope(*dock->owner, dock->host->window_owner);
    return main_window(*dock->owner).root;
}
const uint8_t* dock_app_pixels(ui_app* app, unsigned window, uint32_t* width, uint32_t* height)
{
    if (!app->headless || !width || !height) { return nullptr; }
    auto* context = find_window(*app, window);
    if (!context) { return nullptr; }
    if (app->hosted_command_rendering && context->pixels.empty() && !context->hosted_commands.empty()) {
        try { rasterize(context->pixels, context->width, context->height, context->hosted_commands); }
        catch (...) { return nullptr; }
    }
    if (context->pixels.empty()) { return nullptr; }
    *width = context->width; *height = context->height; return context->pixels.data();
}
bool dock_app_locate(ui_app* app, unsigned source, int x, int y, unsigned& target, int& tx, int& ty)
{
    auto* origin = find_window(*app, source);
    if (!origin || !app->workspace) { return false; }
    int sx = x, sy = y;
#ifdef SUNFLECK_MACOS
    double screen_x = x, screen_y = y;
    if (origin->window) { origin->window->convert_point(screen_x, screen_y, true); }
#endif
#ifdef _WIN32
    if (!app->headless) { POINT point{x, y}; ClientToScreen(origin->window, &point); sx = point.x; sy = point.y; }
#elif defined(SUNFLECK_X11)
    if (!app->headless) { Window child = 0; XTranslateCoordinates(origin->display, origin->window, DefaultRootWindow(origin->display), x, y, &sx, &sy, &child); }
#endif
    if (app->headless && source) {
        for (const auto& w : app->workspace->layout.floating) { if (w.id == source) { sx += w.bounds.x; sy += w.bounds.y; } }
    }
    std::vector<unsigned> ids;
    for (const auto& floating : app->workspace->layout.floating) { ids.push_back(floating.id); }
    std::reverse(ids.begin(), ids.end()); ids.push_back(0);
    for (const auto id : ids) {
        if (id == source && source && app->workspace->drag.window_move) { continue; }
        auto* context = find_window(*app, id);
        if (!context || !context->opened) { continue; }
        int local_x = sx, local_y = sy;
#ifdef _WIN32
        if (!app->headless) {
            POINT point{sx, sy}; ScreenToClient(context->window, &point); local_x = point.x; local_y = point.y;
            const auto hit = WindowFromPoint(POINT{sx, sy});
            const bool through_source = app->workspace->drag.window_move && hit == origin->window;
            if (hit != context->window && hit != app->dock_overlay && !through_source && !IsChild(context->window, hit) && context->dock_window != source) { continue; }
        }
#elif defined(SUNFLECK_X11)
        if (!app->headless) { Window child = 0; XTranslateCoordinates(context->display, DefaultRootWindow(context->display), context->window, sx, sy, &local_x, &local_y, &child); }
#elif defined(SUNFLECK_MACOS)
        if (!app->headless) {
            if (!context->window || !context->window->visible()) { continue; }
            double cx = screen_x, cy = screen_y;
            context->window->convert_point(cx, cy, false);
            local_x = static_cast<int>(std::floor(cx));
            local_y = static_cast<int>(std::floor(cy));
        }
#endif
        if (app->headless && id) { for (const auto& w : app->workspace->layout.floating) { if (w.id == id) { local_x -= w.bounds.x; local_y -= w.bounds.y; } } }
        const auto view = app->workspace->views.find(id);
        if (view != app->workspace->views.end() && ui_app::contains(view->second.area, local_x, local_y)) {
            target = id; tx = local_x; ty = local_y; return true;
        }
    }
    return false;
}
ui_rect dock_app_screen(ui_app* app, unsigned window, int x, int y)
{
    auto* context = find_window(*app, window);
    if (!context) { return {x, y, 480, 320}; }
#ifdef _WIN32
    if (!app->headless) { POINT point{x, y}; ClientToScreen(context->window, &point); x = MulDiv(point.x, 96, context->dpi); y = MulDiv(point.y, 96, context->dpi); }
#elif defined(SUNFLECK_X11)
    if (!app->headless) { Window child = 0; int sx = x, sy = y; XTranslateCoordinates(context->display, context->window, DefaultRootWindow(context->display), x, y, &sx, &sy, &child); x = sx * 96 / static_cast<int>(context->dpi); y = sy * 96 / static_cast<int>(context->dpi); }
#elif defined(SUNFLECK_MACOS)
    if (context->window) {
        double sx = x, sy = y;
        context->window->convert_point(sx, sy, true);
        x = static_cast<int>(std::lround(sx));
        y = static_cast<int>(std::lround(sy));
    }
#endif
    return {x, y, 480, 320};
}

namespace {
ui_result document_check(ui_node* node) {
    if(!node || node->kind!=UI_DOCUMENT_VIEW) return UI_ERROR_INVALID_ARGUMENT;
    const auto result=check(node->owner); if(result!=UI_OK) return result;
    return node->document?UI_OK:UI_ERROR_INVALID_STATE;
}
ui_result document_string(const std::string& value,char* output,size_t* size) {
    if(!size) return UI_ERROR_INVALID_ARGUMENT;
    const auto required=value.size()+1;
    if(!output) {*size=required;return UI_OK;}
    if(*size<required) {*size=required;return UI_ERROR_INVALID_ARGUMENT;}
    std::memcpy(output,value.c_str(),required);*size=required;return UI_OK;
}
}
ui_result ui_document_load(ui_node* node,const char* json,size_t size) {
    if(!node||node->kind!=UI_DOCUMENT_VIEW||!json) return UI_ERROR_INVALID_ARGUMENT;
    if(auto result=check(node->owner);result!=UI_OK) return result;
    try {auto next=document::parse(std::string_view(json,size));node->document=std::move(next);node->offset=0;binding_app_dirty(node->owner);return UI_OK;}
    catch(const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;} catch(...) {return UI_ERROR_INVALID_ARGUMENT;}
}
ui_result ui_document_scroll_to(ui_node* node,const char* heading) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    if(!heading) return UI_ERROR_INVALID_ARGUMENT;
    try {
        if(std::none_of(node->document->blocks.begin(),node->document->blocks.end(),[&](const auto& b){return b.id==heading;})) return UI_ERROR_INVALID_ARGUMENT;
        node->document->pending_anchor=heading;binding_app_dirty(node->owner);return UI_OK;
    } catch(const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;}
}
ui_result ui_document_set_scroll(ui_node* node,int32_t offset) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    if(offset<0) return UI_ERROR_INVALID_ARGUMENT;
    node->offset=offset;binding_app_dirty(node->owner);return UI_OK;
}
ui_result ui_document_get_scroll(ui_node* node,int32_t* offset) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    if(!offset) return UI_ERROR_INVALID_ARGUMENT;*offset=static_cast<int32_t>(std::lround(node->offset));return UI_OK;
}
ui_result ui_document_selection(ui_node* node,char* text,size_t* size) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    try {return document_string(document::selected(*node->document),text,size);}
    catch(const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;} catch(...) {return UI_ERROR_INVALID_STATE;}
}
ui_result ui_document_select_all(ui_node* node) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    node->document->first=0;node->document->last=node->document->plain.size();binding_app_dirty(node->owner);return UI_OK;
}
ui_result ui_document_take_event(ui_node* node,ui_document_event* event,char* text,size_t* size) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    if(!event) return UI_ERROR_INVALID_ARGUMENT;
    *event=node->document->event;
    auto r=document_string(node->document->event_value,text,size);
    if(r==UI_OK&&text) {node->document->event=UI_DOCUMENT_NONE;node->document->event_value.clear();}return r;
}
ui_result ui_document_scroll_horizontal(ui_node* node,int32_t x,int32_t y,int32_t delta) {
    if(auto r=document_check(node);r!=UI_OK) return r;
    if(delta < -32768 || delta > 32768) return UI_ERROR_INVALID_ARGUMENT;
    try {document::horizontal(*node,x,y,delta);binding_app_dirty(node->owner);return UI_OK;}
    catch(const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;}
}
ui_result ui_document_set_font(uint32_t role,const void* bytes,size_t size) {
#ifdef SUNFLECK_FREETYPE
    return freetype_document_font(role,bytes,size);
#else
    (void)role;(void)bytes;(void)size;return UI_ERROR_UNSUPPORTED;
#endif
}

components::Registry& components::registry(ui_app* app) { return app->component_registry; }

ui_result ui_component_apply_template(ui_component* component, const char* namespace_uri, const char* name, ui_fleck_diagnostic* diagnostic)
{
    if (!component || component->destroyed || !component->host || !component->is_control || !namespace_uri || !name) { return UI_ERROR_INVALID_ARGUMENT; }
    auto* app = component->host->owner;
    if (auto status = check(app); status != UI_OK) { return status; }
    if (app->dispatching || app->polling || app->current_action) { return UI_ERROR_INVALID_STATE; }
    ui_fleck_diagnostic local{}; if (!diagnostic) { diagnostic = &local; } *diagnostic = {};
    auto* host = component->host;
    SessionScope session(*app, host->window_owner);
    decltype(host->children) saved_children;
    bool saved = false, callback_started = false;
    try {
        saved_children = host->children;
        saved = true;
        components::Key selected_template{namespace_uri, name};
        auto declaration = component->declaration;
        declaration.template_namespace = namespace_uri; declaration.template_name = name;
        components::expand(app->component_registry, declaration);
        std::vector<std::unique_ptr<ui_node>> staged;
        std::vector<ui_node*> named;
        std::vector<FleckAnchor> anchors;
        std::size_t offset = 0;
        auto* root = stage_fleck_node(app, declaration.children.front(), host, staged, named, anchors, offset,
                                     binding_data_context(*component->host), component->shared_from_this());
        for (const auto& anchor : anchors) {
            auto found = std::find_if(named.begin(), named.end(), [&](auto* node) { return node->component_scope == anchor.node->component_scope && node->fleck_id == *anchor.id; });
            if (found == named.end() || *found == anchor.node) { throw std::range_error("Invalid template anchor"); }
            anchor.node->anchor = *found;
        }
        size_t index = 0;
        auto bind = [&](auto& self, const fleck::Element& element) -> void {
            auto& node = *staged[index++];
            auto* model = binding_data_context(node);
            if (components::bind(node, element, model) != UI_OK) { throw std::range_error("Invalid template binding"); }
            for (const auto& child : element.children) { self(self, child); }
        };
        bind(bind, declaration.children.front());
        host->children = {root};
        for (const auto& part : component->type->parts) {
            ui_node* found = nullptr;
            const auto status = ui_component_find_template_part(component, part.name.c_str(), &found);
            if ((status != UI_OK && part.required) || (found && found->kind != part.kind)) { throw std::range_error("Missing or incompatible template part: " + part.name); }
        }
        for (auto& node : staged) { components::validate_parts(*node); }
        for (auto& node : staged) { components::mount(*node); }
        if (component->type->lifecycle) {
            struct CallbackGuard { ui_app* app; bool previous; ~CallbackGuard() { app->dispatching = previous; } } guard{app, std::exchange(app->dispatching, true)};
            callback_started = true;
            if (component->type->lifecycle(component, UI_COMPONENT_TEMPLATE_APPLIED, component->type->context) != UI_OK) { throw std::range_error("Template callback failed"); }
        }
        std::set<ui_node*> old;
        auto collect = [&](auto& self, ui_node* node) -> void { old.insert(node); for (auto* child : node->children) { self(self, child); } };
        for (auto* child : saved_children) { collect(collect, child); }
        auto* page = &app->fleck_page;
        if (std::find(page->nodes.begin(), page->nodes.end(), host) == page->nodes.end()) {
            auto found = std::find_if(app->inactive_fleck_pages.begin(), app->inactive_fleck_pages.end(), [&](const auto& candidate) { return std::find(candidate.nodes.begin(), candidate.nodes.end(), host) != candidate.nodes.end(); });
            page = found == app->inactive_fleck_pages.end() ? nullptr : &*found;
        }
        app->nodes.reserve(app->nodes.size() + staged.size());
        if (page) { page->nodes.reserve(page->nodes.size() + staged.size()); page->named.reserve(page->named.size() + named.size()); }
        for (auto* node : old) { components::shutdown(*node); }
        retire_visual_nodes(app, old, page);
        for (auto& node : staged) { node->source = host->source; if (page) { page->nodes.push_back(node.get()); } app->nodes.push_back(std::move(node)); }
        if (page) { page->named.insert(page->named.end(), named.begin(), named.end()); }
        declaration.children = std::move(declaration.supplied_content);
        component->declaration = std::move(declaration);
        component->selected_template = std::move(selected_template);
        binding_app_dirty(app);
        return UI_OK;
    } catch (const std::exception& error) {
        if (saved) { host->children = std::move(saved_children); }
        // Let behavior code reacquire parts from the retained template after a rejected callback.
        if (callback_started && component->type->lifecycle) {
            const bool previous = std::exchange(app->dispatching, true);
            try { (void)component->type->lifecycle(component, UI_COMPONENT_TEMPLATE_APPLIED, component->type->context); } catch (...) { }
            app->dispatching = previous;
        }
        fleck::report({}, 0, UI_FLECK_VALUE, error.what(), *diagnostic); return UI_ERROR_INVALID_ARGUMENT;
    }
}

ui_result ui_component_create(ui_app* app, const char* namespace_uri, const char* name, ui_model* model, ui_component** component, ui_fleck_diagnostic* diagnostic) {
    if(auto status=check(app);status!=UI_OK){return status;}
    if(!namespace_uri||!name||!component||(model&&model->owner!=app)||app->dispatching){return UI_ERROR_INVALID_ARGUMENT;}*component=nullptr;
    ui_fleck_diagnostic local{};if(!diagnostic){diagnostic=&local;}*diagnostic={};
    try {
        fleck::Element element;element.type_namespace=namespace_uri;element.type_name=name;element.kind=UI_PANEL;
        components::expand(app->component_registry,element);
        std::vector<std::unique_ptr<ui_node>> staged;std::vector<ui_node*> named;std::vector<FleckAnchor> anchors;size_t offset=0;
        auto* root=stage_fleck_node(app,element,nullptr,staged,named,anchors,offset,model);
        for(const auto& anchor:anchors){auto found=std::find_if(named.begin(),named.end(),[&](auto* node){return node->component_scope==anchor.node->component_scope&&node->fleck_id==*anchor.id;});if(found==named.end()||*found==anchor.node){throw std::range_error("Invalid component anchor");}anchor.node->anchor=*found;}
        size_t index=0;
        auto bind=[&](auto& self,const fleck::Element& declaration)->void{
            auto& node=*staged[index++];auto* context=model;
            if(components::bind(node,declaration,context)!=UI_OK){throw std::range_error("Invalid component binding");}
            for(const auto& child:declaration.children){self(self,child);}
        };bind(bind,element);
        for(auto& node:staged){components::validate_parts(*node);}
        app->nodes.reserve(app->nodes.size()+staged.size());
        std::vector<ui_node*> created;for(auto& node:staged){created.push_back(node.get());app->nodes.push_back(std::move(node));}
        for(auto* node:created){try{components::mount(*node);}catch(...){binding_app_error(app,UI_ERROR_INVALID_STATE);}}
        *component=root->component.get();return UI_OK;
    }catch(const std::bad_alloc&){return UI_ERROR_OUT_OF_MEMORY;}
    catch(const std::exception& error){fleck::report({},0,UI_FLECK_VALUE,error.what(),*diagnostic);return UI_ERROR_INVALID_ARGUMENT;}
}

ui_result ui_app_queue_fleck_refresh_with_resources(ui_app* app,const char* utf8,size_t length,const ui_fleck_resource_entry* entries,size_t count,ui_model* model) {
    if(auto status=check(app);status!=UI_OK){return status;}
    if((!entries&&count)||count>1024){return UI_ERROR_INVALID_ARGUMENT;}
    try {
        std::vector<ui_app::QueuedFleckRefresh::Resource> resources;
        size_t total=0;
        for(size_t i=0;i<count;++i){const auto& entry=entries[i];if(!entry.uri||(!entry.utf8&&entry.length)||entry.length>1024*1024){return UI_ERROR_INVALID_ARGUMENT;}total+=entry.length;if(total>9*1024*1024){return UI_ERROR_INVALID_ARGUMENT;}resources.push_back({entry.uri,entry.utf8?std::string(entry.utf8,entry.length):"",!entry.utf8});}
        auto status=ui_app_queue_fleck_refresh(app,utf8,length,model);
        if(status==UI_OK&&app->queued_fleck_refresh){app->queued_fleck_refresh->resources=std::move(resources);}return status;
    }catch(const std::bad_alloc&){return UI_ERROR_OUT_OF_MEMORY;}
}
