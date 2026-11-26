#pragma once
#include <sunfleck/ui.hpp>
#include <string_view>

namespace sunfleck {
class DockItem {
    ui_dock_item* item_;
public:
    explicit DockItem(ui_dock_item* item) noexcept : item_(item) {}
    ui_dock_item* nativeHandle() const noexcept { return item_; }
    std::string id() const
    {
        uint32_t size = 0; detail::check(ui_dock_get_id(item_, nullptr, &size));
        std::string text(size, '\0'); detail::check(ui_dock_get_id(item_, text.data(), &size)); text.resize(size - 1); return text;
    }
    ui_dock_state state() const { ui_dock_state state{}; detail::check(ui_dock_get_state(item_, &state)); return state; }
    void setTitle(const char* text) const { detail::check(ui_dock_set_title(item_, text)); }
    void setIcon(const char* icon) const { detail::check(ui_dock_set_icon(item_, icon)); }
    void move(const DockItem* target = nullptr, ui_dock_position position = UI_DOCK_CENTER, uint32_t index = UINT32_MAX) const
    {
        detail::check(ui_dock_move(item_, target ? target->item_ : nullptr, position, index));
    }
    void activate() const { detail::check(ui_dock_activate(item_)); }
    void moveGroup(const DockItem* target, ui_dock_position position = UI_DOCK_CENTER) const
    {
        detail::check(ui_dock_move_group(item_, target ? target->item_ : nullptr, position));
    }
    void hide() const { detail::check(ui_dock_hide(item_)); }
    void show() const { detail::check(ui_dock_show(item_)); }
    // A veto or disabled capability returns false without throwing.
    bool close() const
    {
        const auto status = ui_dock_close(item_); if (status == UI_ERROR_INVALID_STATE) { return false; } detail::check(status); return true;
    }
    void floatWindow(ui_rect bounds = {120, 120, 480, 320}, bool wholeGroup = false) const { detail::check(ui_dock_float(item_, &bounds, wholeGroup)); }
    void autoHide(ui_dock_edge edge = UI_DOCK_LEFT) const { detail::check(ui_dock_auto_hide(item_, edge)); }
    void pin() const { detail::check(ui_dock_pin(item_)); }
};
class Dock {
    ui_dock* dock_;
public:
    explicit Dock(Node host) { detail::check(ui_dock_create(host.nativeHandle(), &dock_)); }
    ui_dock* nativeHandle() const noexcept { return dock_; }
    DockItem add(const char* id, const char* title, Node content, ui_dock_kind kind = UI_DOCK_DOCUMENT_KIND,
                 unsigned capabilities = UI_DOCK_DEFAULT_CAPABILITIES, unsigned minWidth = 96, unsigned minHeight = 64) const
    {
        const ui_dock_item_options options{sizeof(options), 1, id, title, content.nativeHandle(), kind, capabilities, minWidth, minHeight};
        ui_dock_item* item = nullptr; detail::check(ui_dock_register(dock_, &options, &item)); return DockItem(item);
    }
    DockItem find(const char* id) const { ui_dock_item* item = nullptr; detail::check(ui_dock_find(dock_, id, &item)); return DockItem(item); }
    void setEvent(ui_dock_event callback, void* context = nullptr) const { detail::check(ui_dock_set_event(dock_, callback, context)); }
    void captureDefault() const { detail::check(ui_dock_capture_default(dock_)); }
    void reset() const { detail::check(ui_dock_reset(dock_)); }
    std::string save() const
    {
        uint32_t size = 0; detail::check(ui_dock_save(dock_, nullptr, &size)); std::string result(size, '\0');
        detail::check(ui_dock_save(dock_, result.data(), &size)); result.resize(size - 1); return result;
    }
    void restore(std::string_view layout) const { detail::check(ui_dock_restore(dock_, layout.data(), layout.size(), nullptr)); }
    std::vector<uint32_t> windows() const
    {
        uint32_t count = 0; detail::check(ui_dock_get_windows(dock_, nullptr, &count)); std::vector<uint32_t> ids(count);
        detail::check(ui_dock_get_windows(dock_, ids.data(), &count)); return ids;
    }
    void pointer(uint32_t window, ui_pointer_action action, int x, int y) const { detail::check(ui_dock_pointer(dock_, window, action, x, y)); }
};
inline void loadTheme(App& app, std::string_view resource)
{
    detail::check(ui_app_load_theme(app.nativeHandle(), resource.data(), resource.size(), nullptr));
}
inline void applyTheme(App& app, ui_builtin_theme base, std::string_view resources = {}, bool dark = false,
                       bool compact = false, const char* skin = nullptr)
{
    const ui_theme_descriptor theme{sizeof(theme), 1, base, dark ? 1u : 0u, compact ? 1u : 0u,
        skin, resources.data(), resources.size()};
    detail::check(ui_app_apply_theme(app.nativeHandle(), &theme, nullptr));
}
inline void registerDockSkin(App& app, const char* name, ui_dock_painter painter, void* context = nullptr)
{
    detail::check(ui_app_register_dock_skin(app.nativeHandle(), name, painter, context));
}
}
