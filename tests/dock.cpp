#include <sunfleck/dock.hpp>
#include "../src/dock.h"
#include "../src/fleck.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

#define REQUIRE(expression) do { if (!(expression)) { throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expression); } } while (false)

namespace {
struct Fixture {
    sunfleck::App app{UI_SOFTWARE, true};
    sunfleck::Node host = app.createNode(UI_DOCK_HOST);
    sunfleck::Dock dock{host};
    sunfleck::Node editor = app.createNode(UI_TEXT_AREA);
    sunfleck::Node second = app.createNode(UI_TEXT_AREA);
    sunfleck::Node tool = app.createNode(UI_TEXT_AREA);
    sunfleck::DockItem a = dock.add("a", "Editor A", editor);
    sunfleck::DockItem b = dock.add("b", "Editor B", second);
    sunfleck::DockItem t = dock.add("tool", "Explorer", tool, UI_DOCK_TOOL_KIND);
    Fixture()
    {
        app.setWindowOptions("Dock tests", 900, 640); app.setRoot(host);
        editor.setText("Keep this content"); tool.setText("Tool state");
        dock.captureDefault(); app.openWindow(); REQUIRE(app.poll(0) == UI_OK);
    }
};
std::vector<uint8_t> frame(ui_app* app)
{
    uint32_t width = 0, height = 0; const auto* pixels = ui_app_pixels(app, &width, &height);
    REQUIRE(pixels); return {pixels, pixels + static_cast<size_t>(width) * height * 4};
}
void model()
{
    Fixture f;
    REQUIRE(f.a.state() == UI_DOCK_DOCKED);
    const auto original = f.dock.save();
    f.a.move(&f.b, UI_DOCK_POSITION_BOTTOM); f.b.move(&f.a, UI_DOCK_CENTER, 0);
    REQUIRE(f.editor.text() == "Keep this content");
    f.t.autoHide(UI_DOCK_RIGHT); REQUIRE(f.t.state() == UI_DOCK_AUTO_HIDDEN);
    f.t.activate(); REQUIRE(f.app.poll(0) == UI_OK);
    f.t.pin(); REQUIRE(f.t.state() == UI_DOCK_DOCKED);
    REQUIRE(ui_dock_auto_hide(f.a.nativeHandle(), UI_DOCK_LEFT) == UI_ERROR_INVALID_STATE);
    f.t.hide(); REQUIRE(f.t.state() == UI_DOCK_HIDDEN); f.t.show(); REQUIRE(f.t.state() == UI_DOCK_DOCKED);
    bool veto = true;
    f.dock.setEvent([](ui_dock* dock, ui_dock_item* item, ui_dock_event_kind event, void* context) -> uint32_t {
        if (event == UI_DOCK_CLOSING) { REQUIRE(ui_dock_hide(item) == UI_ERROR_INVALID_STATE); REQUIRE(ui_app_poll(dock->owner, 0) == UI_ERROR_INVALID_STATE); return *static_cast<bool*>(context); }
        return 0;
    }, &veto);
    REQUIRE(!f.a.close()); REQUIRE(f.a.state() == UI_DOCK_DOCKED);
    veto = false; REQUIRE(f.a.close()); REQUIRE(f.a.state() == UI_DOCK_CLOSED); f.a.show();
    f.dock.restore(original); REQUIRE(f.dock.save() == original);
    f.dock.reset(); REQUIRE(f.dock.save() == original);
    ui_result thread_result = UI_OK;
    std::thread worker([&] { ui_dock_state state; thread_result = ui_dock_get_state(f.a.nativeHandle(), &state); }); worker.join();
    REQUIRE(thread_result == UI_ERROR_WRONG_THREAD);
    auto extra = f.app.createNode(UI_PANEL);
    auto fixed = f.dock.add("fixed", "Fixed", extra, UI_DOCK_DOCUMENT_KIND, UI_DOCK_CAN_CLOSE);
    const ui_rect bounds{100, 100, 480, 320};
    REQUIRE(ui_dock_float(fixed.nativeHandle(), &bounds, 0) != UI_OK);
    f.a.floatWindow();
    REQUIRE(ui_dock_move(fixed.nativeHandle(), f.a.nativeHandle(), UI_DOCK_CENTER, 0) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_dock_move(f.a.nativeHandle(), f.t.nativeHandle(), UI_DOCK_CENTER, 0) != UI_OK);
}
void return_positions()
{
    Fixture f; auto& model = f.dock.nativeHandle()->layout;
    auto peer = f.dock.add("tool_peer", "Output", f.app.createNode(UI_PANEL), UI_DOCK_TOOL_KIND);
    f.t.move(&peer, UI_DOCK_POSITION_BOTTOM);
    const auto old_group = docking::group_of(model, "tool");
    const auto old_count = model.regions.size();
    f.t.hide(); REQUIRE(!model.regions.contains(old_group)); f.t.show();
    REQUIRE(docking::group_of(model, "tool") == old_group); REQUIRE(model.regions.size() == old_count);
    const auto split = std::find_if(model.regions.begin(), model.regions.end(), [&](const auto& pair) { return pair.second.axis && pair.second.second == old_group; });
    REQUIRE(split != model.regions.end()); REQUIRE(split->second.axis == 2);
    f.t.autoHide(); f.t.hide(); f.t.show(); REQUIRE(f.t.state() == UI_DOCK_AUTO_HIDDEN); f.t.pin();
    REQUIRE(docking::group_of(model, "tool") == old_group);
    peer.move(&f.t); f.t.hide(); peer.hide(); f.t.show(); peer.show();
    REQUIRE(docking::group_of(model, "tool") == docking::group_of(model, "tool_peer"));
    f.t.floatWindow({75, 85, 420, 310}, true);
    const auto window = f.dock.windows().front(); f.t.hide(); peer.hide();
    REQUIRE(f.dock.windows().empty());
    const auto saved = f.dock.save(); f.dock.restore(saved); REQUIRE(f.dock.save() == saved);
    f.t.show(); peer.show(); REQUIRE(f.dock.windows().size() == 1); REQUIRE(f.dock.windows().front() == window);
    REQUIRE(f.t.state() == UI_DOCK_FLOATING); REQUIRE(peer.state() == UI_DOCK_FLOATING);
    REQUIRE(model.floating.front().bounds.x == 75); REQUIRE(model.floating.front().bounds.width == 420);
}
void persistence()
{
    Fixture f;
    f.a.floatWindow({180, 160, 420, 300}); f.t.autoHide(UI_DOCK_BOTTOM);
    const auto saved = f.dock.save();
    REQUIRE(f.app.poll(0) == UI_OK);
    const auto windows = f.dock.windows(); REQUIRE(windows.size() == 1);
    uint32_t width = 0, height = 0;
    REQUIRE(ui_dock_pixels(f.dock.nativeHandle(), windows[0], &width, &height)); REQUIRE(width == 420 && height == 300);
    f.a.move(&f.b); REQUIRE(f.a.state() == UI_DOCK_DOCKED);
    f.dock.restore(saved); REQUIRE(f.dock.save() == saved); REQUIRE(f.editor.text() == "Keep this content");
    for (const auto& invalid : {std::string("{}"), std::string("{\"version\":2}"), saved + "junk", saved.substr(0, saved.size() - 1), std::string("{\"version\":1,\"version\":1}")}) {
        ui_fleck_diagnostic diagnostic{};
        REQUIRE(ui_dock_restore(f.dock.nativeHandle(), invalid.data(), invalid.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        REQUIRE(diagnostic.code != UI_FLECK_NONE); REQUIRE(f.dock.save() == saved);
    }
    // Restoring into a different registration set skips removed IDs and retains new items.
    sunfleck::App other{UI_SOFTWARE, true}; auto host = other.createNode(UI_DOCK_HOST); sunfleck::Dock dock{host};
    auto content = other.createNode(UI_TEXT_AREA); auto a = dock.add("a", "A", content);
    auto fresh = dock.add("fresh", "New document", other.createNode(UI_PANEL));
    dock.restore(saved); REQUIRE(a.state() == UI_DOCK_FLOATING); REQUIRE(fresh.state() == UI_DOCK_DOCKED);
}
void partial_close()
{
    Fixture f; auto& model = f.dock.nativeHandle()->layout;
    f.a.floatWindow({100, 100, 400, 300}, true);
    f.dock.setEvent([](ui_dock*, ui_dock_item* item, ui_dock_event_kind event, void*) -> uint32_t {
        return event == UI_DOCK_CLOSING && item && item->id == "b";
    });
    const auto window = model.floating.front().id;
    REQUIRE(!docking::close_window(*f.dock.nativeHandle(), window));
    REQUIRE(f.a.state() == UI_DOCK_CLOSED); REQUIRE(f.b.state() == UI_DOCK_FLOATING);
    REQUIRE(model.floating.size() == 1); REQUIRE(f.editor.text() == "Keep this content");
    f.dock.setEvent(nullptr); REQUIRE(docking::close_window(*f.dock.nativeHandle(), window));
    REQUIRE(model.floating.empty());
}
void drag()
{
    Fixture f; auto& dock = *f.dock.nativeHandle();
    const auto view = dock.views.at(0);
    const auto tab = std::find_if(view.hits.begin(), view.hits.end(), [](const auto& h) { return h.part == UI_DOCK_PART_TAB && h.item == "a"; }); REQUIRE(tab != view.hits.end());
    const int x = (tab->bounds.left + tab->bounds.right) / 2, y = (tab->bounds.top + tab->bounds.bottom) / 2;
    const auto saved = f.dock.save();
    f.dock.pointer(0, UI_POINTER_DOWN, x, y); f.dock.pointer(0, UI_POINTER_MOVE, -100, -100); f.dock.pointer(0, UI_POINTER_CANCEL, -100, -100);
    REQUIRE(f.dock.save() == saved);
    const auto tool_bounds = dock.views.at(0).groups.at(docking::group_of(dock.layout, "tool"));
    const int tool_x = (tool_bounds.left + tool_bounds.right) / 2, tool_y = (tool_bounds.top + tool_bounds.bottom) / 2;
    f.dock.pointer(0, UI_POINTER_DOWN, x, y); f.dock.pointer(0, UI_POINTER_MOVE, tool_x, tool_y);
    REQUIRE(!dock.drag.allowed); REQUIRE(f.app.poll(0) == UI_OK);
    f.dock.pointer(0, UI_POINTER_UP, tool_x, tool_y); REQUIRE(f.dock.save() == saved);
    f.dock.pointer(0, UI_POINTER_DOWN, x, y); f.dock.pointer(0, UI_POINTER_MOVE, -100, -100); f.dock.pointer(0, UI_POINTER_UP, -100, -100);
    REQUIRE(f.a.state() == UI_DOCK_FLOATING); REQUIRE(f.app.poll(0) == UI_OK);
    const auto windows = f.dock.windows(); REQUIRE(windows.size() == 1);
    auto float_view = dock.views.at(windows[0]);
    const auto header = std::find_if(float_view.hits.begin(), float_view.hits.end(), [](const auto& h) { return h.part == UI_DOCK_PART_TAB; }); REQUIRE(header != float_view.hits.end());
    f.dock.pointer(windows[0], UI_POINTER_DOWN, header->bounds.left + 5, header->bounds.top + 5);
    // Headless floating position is -100,-100; this point maps to the main editor group.
    const auto main_view = dock.views.at(0);
    const auto target_group = docking::group_of(dock.layout, "b"); const auto target = main_view.groups.at(target_group);
    const int tx = (target.left + target.right) / 2 + 100, ty = (target.top + target.bottom) / 2 + 100;
    f.dock.pointer(windows[0], UI_POINTER_MOVE, tx, ty); f.dock.pointer(windows[0], UI_POINTER_UP, tx, ty);
    REQUIRE(f.a.state() == UI_DOCK_DOCKED);
    REQUIRE(f.editor.text() == "Keep this content");
}
void tab_order()
{
    Fixture f; auto& dock = *f.dock.nativeHandle();
    const auto hits = dock.views.at(0).hits;
    const auto a = std::find_if(hits.begin(), hits.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_TAB && hit.item == "a"; });
    const auto b = std::find_if(hits.begin(), hits.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_TAB && hit.item == "b"; });
    REQUIRE(a != hits.end() && b != hits.end());
    const auto region_count = dock.layout.regions.size();
    const int bx = (b->bounds.left + b->bounds.right) / 2, by = (b->bounds.top + b->bounds.bottom) / 2;
    f.dock.pointer(0, UI_POINTER_DOWN, a->bounds.left + 8, a->bounds.top + 8);
    f.dock.pointer(0, UI_POINTER_MOVE, bx, by); REQUIRE(dock.drag.position == UI_DOCK_CENTER);
    f.dock.pointer(0, UI_POINTER_UP, bx, by);
    REQUIRE(dock.layout.regions.size() == region_count);
    const auto& tabs = dock.layout.regions.at(docking::group_of(dock.layout, "a")).tabs;
    REQUIRE(tabs == std::vector<std::string>({"b", "a"}));
    f.a.moveGroup(&f.t, UI_DOCK_POSITION_BOTTOM);
    REQUIRE(docking::group_of(dock.layout, "a") == docking::group_of(dock.layout, "b"));
    REQUIRE(f.editor.text() == "Keep this content");
}
void chrome_and_menu()
{
    Fixture f;
    f.a.activate(); REQUIRE(f.app.poll(0) == UI_OK);
    auto& dock = *f.dock.nativeHandle();
    const auto header = dock.views.at(0).hits;
    const auto menu = std::find_if(header.begin(), header.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_MENU && hit.item == "a"; });
    const auto tab = std::find_if(header.begin(), header.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_TAB && hit.item == "a"; });
    REQUIRE(menu != header.end()); REQUIRE(tab != header.end());
    REQUIRE(menu->bounds.top == tab->bounds.top); REQUIRE(menu->bounds.bottom == tab->bounds.bottom);
    REQUIRE(std::none_of(header.begin(), header.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_FLOAT || hit.part == UI_DOCK_PART_PIN; }));
    f.a.floatWindow(); REQUIRE(f.app.poll(0) == UI_OK);
    const auto window = f.dock.windows().front();
    const auto hits = dock.views.at(window).hits;
    const auto button = std::find_if(hits.begin(), hits.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_MENU; });
    REQUIRE(button != hits.end());
    const int x = (button->bounds.left + button->bounds.right) / 2, y = (button->bounds.top + button->bounds.bottom) / 2;
    f.dock.pointer(window, UI_POINTER_DOWN, x, y); REQUIRE(f.app.poll(0) == UI_OK);
    f.dock.pointer(window, UI_POINTER_UP, x, y); REQUIRE(f.app.poll(0) == UI_OK); REQUIRE(dock.menu);
    const auto rows = dock.views.at(window).hits;
    const auto dock_row = std::find_if(rows.begin(), rows.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_MENU_ITEM && hit.action == 2; });
    REQUIRE(dock_row != rows.end());
    const int row_x = (dock_row->bounds.left + dock_row->bounds.right) / 2, row_y = (dock_row->bounds.top + dock_row->bounds.bottom) / 2;
    f.dock.pointer(window, UI_POINTER_DOWN, row_x, row_y); REQUIRE(f.app.poll(0) == UI_OK);
    f.dock.pointer(window, UI_POINTER_UP, row_x, row_y); REQUIRE(f.app.poll(0) == UI_OK);
    REQUIRE(f.a.state() == UI_DOCK_DOCKED); REQUIRE(!dock.menu); REQUIRE(f.editor.text() == "Keep this content");
}

void direction_targets()
{
    Fixture f; auto& dock = *f.dock.nativeHandle();
    f.a.floatWindow({150, 120, 420, 320}); REQUIRE(f.app.poll(0) == UI_OK);
    const auto window = f.dock.windows().front();
    REQUIRE(docking::begin_window_drag(dock, window, 40, -10));
    const auto area = dock.views.at(0).groups.at(docking::group_of(dock.layout, "b"));
    const int cx = (area.left + area.right) / 2, cy = (area.top + area.bottom) / 2;
    f.dock.pointer(window, UI_POINTER_MOVE, cx - 150, cy - 120); REQUIRE(f.app.poll(0) == UI_OK);
    const auto targets = dock.views.at(0).targets; REQUIRE(targets.size() == 5);
    for (const auto& target : targets) {
        const int x = (target.bounds.left + target.bounds.right) / 2 - 150, y = (target.bounds.top + target.bounds.bottom) / 2 - 120;
        f.dock.pointer(window, UI_POINTER_MOVE, x, y); REQUIRE(f.app.poll(0) == UI_OK);
        REQUIRE(dock.drag.position == target.position); REQUIRE(dock.drag.allowed);
        const auto stable = dock.views.at(0).targets;
        REQUIRE(stable[0].bounds.left == targets[0].bounds.left); REQUIRE(stable[0].bounds.top == targets[0].bounds.top);
        const auto preview = dock.drag.preview;
        REQUIRE(preview.right > preview.left); REQUIRE(preview.bottom > preview.top);
        if (target.position == UI_DOCK_POSITION_LEFT || target.position == UI_DOCK_POSITION_RIGHT) { REQUIRE(preview.right - preview.left <= (area.right - area.left + 1) / 2); }
        if (target.position == UI_DOCK_POSITION_TOP || target.position == UI_DOCK_POSITION_BOTTOM) { REQUIRE(preview.bottom - preview.top <= (area.bottom - area.top + 1) / 2); }
    }
    f.dock.pointer(window, UI_POINTER_MOVE, cx - 150, cy - 120);
    f.dock.pointer(window, UI_POINTER_UP, cx - 150, cy - 120); REQUIRE(f.app.poll(0) == UI_OK);
    REQUIRE(f.a.state() == UI_DOCK_DOCKED);
}
void themes()
{
    Fixture f;
    sunfleck::applyTheme(f.app, UI_BUILTIN_MATERIAL_3, "<Resource Key='dock.tab_height' Value='31'/>");
    REQUIRE(dock_app_theme(f.app.nativeHandle()).shape.button_radius == 64);
    REQUIRE(dock_app_theme(f.app.nativeHandle()).metrics.at("dock.tab_height") == 31);
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        for (unsigned dark = 0; dark < 2; ++dark) {
            for (unsigned compact = 0; compact < 2; ++compact) { f.app.setTheme(static_cast<ui_builtin_theme>(id), dark != 0, compact != 0); REQUIRE(f.app.poll(0) == UI_OK || f.app.poll(0) == UI_TIMEOUT); }
        }
    }
    const std::string resource = R"(<Theme Base="Material3" Dark="true"><Resource Key="primary" Value="#112233"/><Resource Key="dock.title.background" Value="#345678"/><Resource Key="dock.tab_height" Value="30"/><DockTemplate Part="Title"><Stack Background="{Background}" Role="Drag"><Text Text="{Title}" Foreground="{Foreground}"/><Button Role="Close" Text="X" Width="30"/></Stack></DockTemplate><DockTemplate Part="Tab" State="Hover"><Fill Background="primary"/></DockTemplate></Theme>)";
    sunfleck::loadTheme(f.app, resource); REQUIRE(f.app.poll(0) == UI_OK);
    REQUIRE(dock_app_theme(f.app.nativeHandle()).shape.button_radius == 64);
    const auto before = frame(f.app.nativeHandle());
    const std::string invalid = "<Theme><Resource Key='gap' Value='999'/></Theme>";
    REQUIRE(ui_app_load_theme(f.app.nativeHandle(), invalid.data(), invalid.size(), nullptr) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(frame(f.app.nativeHandle()) == before);
    struct PaintChecks { int calls = 0; bool valid = true; } checks;
    sunfleck::registerDockSkin(f.app, "test", [](ui_draw_context* draw, const ui_dock_paint* data, void* context) -> uint32_t {
        auto& checks = *static_cast<PaintChecks*>(context); ++checks.calls;
        checks.valid &= data->version == 1;
        checks.valid &= ui_draw_pop_clip(draw) == UI_ERROR_INVALID_STATE;
        checks.valid &= ui_draw_push_clip(draw, data->bounds) == UI_OK;
        checks.valid &= ui_draw_pop_clip(draw) == UI_OK;
        if (data->part != UI_DOCK_PART_TAB) { return 0; }
        checks.valid &= ui_draw_fill(draw, data->bounds, 0xaa1155, 255, 2) == UI_OK;
        checks.valid &= ui_draw_text(draw, data->bounds, data->title, data->foreground) == UI_OK; return 1;
    }, &checks);
    sunfleck::loadTheme(f.app, "<Theme Base='Fluent2' Skin='test'/>"); REQUIRE(f.app.poll(0) == UI_OK); REQUIRE(checks.calls > 0 && checks.valid);
    f.a.floatWindow(); REQUIRE(f.app.poll(0) == UI_OK);
    sunfleck::loadTheme(f.app, "<Theme Base='Radix' Dark='true'/>"); REQUIRE(f.app.poll(0) == UI_OK);
    uint32_t width = 0, height = 0; REQUIRE(ui_dock_pixels(f.dock.nativeHandle(), f.dock.windows()[0], &width, &height));
}
void chrome_appearance()
{
    Fixture f;
    for (unsigned theme = 0; theme < UI_BUILTIN_THEME_COUNT; ++theme) {
        for (bool dark : {false, true}) {
            for (bool compact : {false, true}) {
                f.app.setTheme(static_cast<ui_builtin_theme>(theme), dark, compact); REQUIRE(f.app.poll(0) == UI_OK);
                const auto& view = f.dock.nativeHandle()->views.at(0);
                const auto tab = std::find_if(view.hits.begin(), view.hits.end(), [](const auto& h) { return h.part == UI_DOCK_PART_TAB && h.item == "b"; }); REQUIRE(tab != view.hits.end());
                const auto group = view.groups.at(tab->group);
                const auto& palette = dock_app_theme(f.app.nativeHandle()).palette;
                uint32_t width = 0, height = 0; const auto* pixels = ui_app_pixels(f.app.nativeHandle(), &width, &height); REQUIRE(pixels);
                const auto color = [&](int x, int y) {
                    REQUIRE(x >= 0 && y >= 0 && static_cast<unsigned>(x) < width && static_cast<unsigned>(y) < height);
                    const auto* pixel = pixels + (static_cast<size_t>(y) * width + x) * 4;
                    return (static_cast<uint32_t>(pixel[0]) << 16) | (pixel[1] << 8) | pixel[2];
                };
                REQUIRE(color(tab->bounds.left + 8, tab->bounds.bottom - 2) == palette.colors[UI_THEME_PRIMARY]);
                REQUIRE(color(group.right - 80, tab->bounds.top + 3) == palette.colors[UI_THEME_PANEL]);
                REQUIRE(color(tab->bounds.left + 8, tab->bounds.top + 3) != color(group.right - 80, tab->bounds.top + 3));
                REQUIRE(color(group.left, (group.top + group.bottom) / 2) == palette.colors[UI_THEME_DIVIDER]);
                const auto frame = docking::window_frame(dock_app_theme(f.app.nativeHandle()), width, 96);
                REQUIRE(group.top >= frame.height); REQUIRE(frame.title.right == frame.minimize.left); REQUIRE(frame.maximize.right == frame.close.left);
                REQUIRE(color(40, 3) == palette.colors[UI_THEME_PANEL]);
                // Rounded client outline follows the native top corners.
                REQUIRE(color(3, 2) != palette.colors[UI_THEME_DIVIDER]);
                REQUIRE(color(3, 2) != palette.colors[UI_THEME_PANEL]);
                REQUIRE(color(3, 2) == color(static_cast<int>(width) - 4, 2));
                const auto menu = std::find_if(view.hits.begin(), view.hits.end(), [&](const auto& h) { return h.part == UI_DOCK_PART_MENU && h.group == tab->group; });
                REQUIRE(menu != view.hits.end());
                REQUIRE(menu->bounds.right <= group.right - 2);
                for (int x = menu->bounds.left; x < menu->bounds.right; ++x) {
                    REQUIRE(color(x, menu->bounds.bottom - 1) == palette.colors[UI_THEME_DIVIDER]);
                }
            }
        }
    }
    sunfleck::loadTheme(f.app, "<Theme Base='Fluent2'><Resource Key='dock.window_title.background' Value='#123456'/><Resource Key='dock.window_title_height' Value='42'/></Theme>");
    REQUIRE(f.app.poll(0) == UI_OK);
    uint32_t width = 0, height = 0; const auto* pixels = ui_app_pixels(f.app.nativeHandle(), &width, &height); REQUIRE(pixels);
    REQUIRE(pixels[(3 * width + 40) * 4] == 0x12);
    REQUIRE(docking::window_frame(dock_app_theme(f.app.nativeHandle()), width, 96).height == 42);
}
void tab_close_buttons()
{
    Fixture f;
    f.a.activate(); REQUIRE(f.app.poll(0) == UI_OK);
    const auto hit = [&](const char* id) {
        const auto& hits = f.dock.nativeHandle()->views.at(0).hits;
        const auto close = std::find_if(hits.begin(), hits.end(), [&](const auto& h) { return h.part == UI_DOCK_PART_CLOSE && h.item == id; });
        REQUIRE(close != hits.end());
        const auto tab = std::find_if(hits.begin(), hits.end(), [&](const auto& h) { return h.part == UI_DOCK_PART_TAB && h.item == id; });
        REQUIRE(tab != hits.end());
        REQUIRE(close->bounds.left >= tab->bounds.left && close->bounds.right < tab->bounds.right);
        REQUIRE(close->bounds.top == tab->bounds.top && close->bounds.bottom == tab->bounds.bottom);
        return *close;
    };
    (void)hit("a"); const auto close_b = hit("b");
    const int x = (close_b.bounds.left + close_b.bounds.right) / 2, y = (close_b.bounds.top + close_b.bounds.bottom) / 2;
    bool veto = true;
    f.dock.setEvent([](ui_dock*, ui_dock_item*, ui_dock_event_kind kind, void* context) -> uint32_t {
        return kind == UI_DOCK_CLOSING && *static_cast<bool*>(context);
    }, &veto);
    const auto click = [&] {
        f.dock.pointer(0, UI_POINTER_DOWN, x, y);
        REQUIRE(!f.dock.nativeHandle()->drag.prepared);
        f.dock.pointer(0, UI_POINTER_UP, x, y); REQUIRE(f.app.poll(0) == UI_OK);
    };
    click(); REQUIRE(f.b.state() == UI_DOCK_DOCKED);
    veto = false; click(); REQUIRE(f.b.state() == UI_DOCK_CLOSED);
    REQUIRE(f.dock.nativeHandle()->layout.regions.at(docking::group_of(f.dock.nativeHandle()->layout, "a")).active == "a");
    REQUIRE(f.second.text().empty());
    auto locked = f.dock.add("locked", "Cannot close", f.app.createNode(UI_TEXT_AREA), UI_DOCK_DOCUMENT_KIND, UI_DOCK_CAN_FLOAT);
    REQUIRE(f.app.poll(0) == UI_OK);
    for (const auto& h : f.dock.nativeHandle()->views.at(0).hits) { REQUIRE(h.item != "locked" || h.part != UI_DOCK_PART_CLOSE); }
}
void glass_toolbar_layout()
{
    // Measure and arrange must reserve the same insets around a glass surface.
    for (unsigned dpi : {96u, 144u, 192u}) {
        for (bool compact : {false, true}) {
            ui_node root{nullptr, UI_STACK_PANEL}, first{nullptr, UI_TOOL_BAR}, second{nullptr, UI_TOOL_BAR};
            ui_node label{nullptr, UI_LABEL}, tall{nullptr, UI_TEXT_BLOCK}, button{nullptr, UI_BUTTON};
            root.spacing = 6; first.spacing = second.spacing = 6;
            root.children = {&first, &second}; first.children = {&label, &tall}; second.children = {&button};
            first.parent = second.parent = &root; label.parent = tall.parent = &first; button.parent = &second;
            const auto style = builtin_palette(UI_BUILTIN_LIQUID_GLASS, false, compact);
            const auto shape = builtin_themes[UI_BUILTIN_LIQUID_GLASS].shape;
            std::vector<FillRectCommand> commands;
            std::vector<ui_node*> interactive, scrollable;
            render_tree(root, {0, 0, 1000, 600}, style, shape, dpi, nullptr, false, false, nullptr, nullptr, commands, interactive, scrollable);
            const int inset = (6 * dpi + 48) / 96;
            REQUIRE(std::abs(label.bounds.top + label.bounds.bottom - first.bounds.top - first.bounds.bottom) <= 1);
            REQUIRE(tall.bounds.top == first.bounds.top + inset);
            REQUIRE(tall.bounds.bottom == first.bounds.bottom - inset);
            REQUIRE(button.bounds.top == second.bounds.top + inset);
            REQUIRE(button.bounds.bottom == second.bounds.bottom - inset);
            REQUIRE(second.bounds.top >= first.bounds.bottom + inset);
            REQUIRE(label.bounds.bottom < button.bounds.top);
        }
    }
}
void floating_padding()
{
    Fixture f;
    f.a.floatWindow({100, 100, 420, 320});
    const auto window = f.dock.windows().front();
    const auto verify = [&](unsigned padding) {
        REQUIRE(f.app.poll(0) == UI_OK);
        uint32_t width = 0, height = 0;
        REQUIRE(ui_dock_pixels(f.dock.nativeHandle(), window, &width, &height));
        const auto& theme = dock_app_theme(f.app.nativeHandle());
        const auto area = f.dock.nativeHandle()->views.at(window).area;
        REQUIRE(area.left == static_cast<int>(padding));
        REQUIRE(area.right == static_cast<int>(width - padding));
        REQUIRE(area.top == docking::window_frame(theme, width, 96).height + static_cast<int>(padding));
        REQUIRE(area.bottom == static_cast<int>(height - padding));
        REQUIRE(f.editor.nativeHandle()->visible.left >= area.left);
        REQUIRE(f.editor.nativeHandle()->visible.bottom <= area.bottom);
        REQUIRE(f.dock.nativeHandle()->views.at(0).area.left == static_cast<int>(theme.palette.window_padding));
        REQUIRE(f.editor.text() == "Keep this content");
    };
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        for (bool dark : {false, true}) {
            for (bool compact : {false, true}) {
                f.app.setTheme(static_cast<ui_builtin_theme>(id), dark, compact); verify(4);
            }
        }
    }
    for (unsigned padding : {0u, 2u, 12u, 64u}) {
        const auto xml = "<Theme Base='LiquidGlass'><Resource Key='window_padding' Value='40'/><Resource Key='dock.floating_padding' Value='" +
            std::to_string(padding) + "'/></Theme>";
        sunfleck::loadTheme(f.app, xml); verify(padding);
    }
    const std::string invalid = "<Theme Base='LiquidGlass'><Resource Key='dock.floating_padding' Value='65'/></Theme>";
    ui_fleck_diagnostic diagnostic{};
    REQUIRE(ui_app_load_theme(f.app.nativeHandle(), invalid.data(), invalid.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(diagnostic.code != UI_FLECK_NONE);
    REQUIRE(dock_app_theme(f.app.nativeHandle()).metrics.at("dock.floating_padding") == 64);
}
void rotated_labels()
{
    for (unsigned dpi : {96u, 144u, 192u}) {
        for (const wchar_t* title : {L"Explorer", L"Properties \u5c5e\u6027 \U0001f4c1"}) {
            const int padding = 8 * dpi / 96, width = text_width(title, dpi) + 2 * padding, height = 28 * dpi / 96;
            std::vector<FillRectCommand> horizontal;
            for (int x = 0; x < width; ++x) {
                horizontal.push_back({x, 0, x + 1, height, {static_cast<uint8_t>(40 + x % 80), 75, 110, 255}});
            }
            auto reference = horizontal;
            append_centered_text(reference, {padding, 0, width, height}, {0, 0, width, height}, {245, 240, 230, 255}, dpi, title);
            std::vector<uint8_t> expected; rasterize(expected, width, height, reference);
            for (bool clockwise : {false, true}) {
                std::vector<FillRectCommand> vertical;
                for (const auto& c : horizontal) {
                    if (clockwise) { vertical.push_back({height - c.bottom, c.left, height - c.top, c.right, c.color}); }
                    else { vertical.push_back({c.top, width - c.right, c.bottom, width - c.left, c.color}); }
                }
                append_rotated_text(vertical, {0, 0, height, width}, {0, 0, height, width}, {245, 240, 230, 255}, dpi, title, clockwise);
                std::vector<uint8_t> actual; rasterize(actual, height, width, vertical);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const int rx = clockwise ? height - y - 1 : y, ry = clockwise ? x : width - x - 1;
                        for (int channel = 0; channel < 4; ++channel) {
                            REQUIRE(actual[(static_cast<size_t>(ry) * height + rx) * 4 + channel] == expected[(static_cast<size_t>(y) * width + x) * 4 + channel]);
                        }
                    }
                }
            }
        }
    }
}
void auto_hide_labels()
{
    Fixture f;
    f.a.activate();
    auto peer = f.dock.add("properties", "Properties and document settings", f.app.createNode(UI_TEXT_BLOCK), UI_DOCK_TOOL_KIND);
    for (unsigned theme = 0; theme < UI_BUILTIN_THEME_COUNT; ++theme) {
        for (bool dark : {false, true}) {
            for (bool compact : {false, true}) {
                f.app.setTheme(static_cast<ui_builtin_theme>(theme), dark, compact);
                for (const auto edge : {UI_DOCK_LEFT, UI_DOCK_RIGHT, UI_DOCK_TOP, UI_DOCK_BOTTOM}) {
                    f.t.autoHide(edge); peer.autoHide(edge); REQUIRE(f.app.poll(0) == UI_OK);
                    const auto& view = f.dock.nativeHandle()->views.at(0);
                    int end = 0;
                    for (const auto* id : {"tool", "properties"}) {
                        const auto hit = std::find_if(view.hits.begin(), view.hits.end(), [&](const auto& h) { return h.part == UI_DOCK_PART_AUTO_HIDE && h.item == id; });
                        REQUIRE(hit != view.hits.end());
                        const bool vertical = edge == UI_DOCK_LEFT || edge == UI_DOCK_RIGHT;
                        const auto& r = hit->bounds;
                        const int length = vertical ? r.bottom - r.top : r.right - r.left;
                        const auto title = id == std::string("tool") ? L"Explorer" : L"Properties and document settings";
                        const int rail = vertical ? r.right - r.left : r.bottom - r.top;
                        REQUIRE(length == std::max(rail, text_width(title, 96) + 16));
                        REQUIRE((vertical ? r.top : r.left) >= end);
                        REQUIRE(r.left >= view.area.left && r.right <= view.area.right);
                        REQUIRE(r.top >= view.area.top && r.bottom <= view.area.bottom);
                        end = vertical ? r.bottom : r.right;
                    }
                    // The document root uses a single group outline, without a
                    // second rounded editor border at the header/body seam.
                    const auto& palette = dock_app_theme(f.app.nativeHandle()).palette;
                    const auto r = f.editor.nativeHandle()->bounds;
                    uint32_t width = 0, height = 0; const auto* pixels = ui_app_pixels(f.app.nativeHandle(), &width, &height);
                    const auto* pixel = pixels + (static_cast<size_t>(r.top + 1) * width + r.right - 2) * 4;
                    REQUIRE(pixel[0] == (palette.colors[UI_THEME_CONTROL] >> 16));
                    REQUIRE(pixel[1] == ((palette.colors[UI_THEME_CONTROL] >> 8) & 255));
                    REQUIRE(pixel[2] == (palette.colors[UI_THEME_CONTROL] & 255));
                }
            }
        }
    }
    // Constrained rails share space without crossing or silently dropping tabs.
    for (int i = 0; i < 10; ++i) {
        const auto id = "long_" + std::to_string(i);
        auto item = f.dock.add(id.c_str(), "A very long tool window title", f.app.createNode(UI_PANEL), UI_DOCK_TOOL_KIND);
        item.autoHide(UI_DOCK_LEFT);
    }
    f.t.autoHide(UI_DOCK_LEFT); peer.autoHide(UI_DOCK_LEFT); REQUIRE(f.app.poll(0) == UI_OK);
    const auto& view = f.dock.nativeHandle()->views.at(0); int end = view.area.top;
    for (const auto& h : view.hits) {
        if (h.part != UI_DOCK_PART_AUTO_HIDE) { continue; }
        REQUIRE(h.bounds.top >= end); REQUIRE(h.bounds.bottom <= view.area.bottom);
        REQUIRE(h.bounds.bottom > h.bounds.top); end = h.bounds.bottom;
    }
    f.t.activate(); REQUIRE(f.app.poll(0) == UI_OK);
    const auto popup = f.dock.nativeHandle()->views.at(0).popup; REQUIRE(popup.right > popup.left && popup.bottom > popup.top);
    f.t.pin(); REQUIRE(f.t.state() == UI_DOCK_DOCKED);
}
void auto_hide_skins()
{
    Fixture f;
    sunfleck::registerDockSkin(f.app, "rail", [](ui_draw_context* draw, const ui_dock_paint* data, void*) -> uint32_t {
        if (data->part != UI_DOCK_PART_AUTO_HIDE) { return 0; }
        REQUIRE(ui_draw_fill(draw, data->bounds, 0x203040, 255, 0) == UI_OK);
        REQUIRE(ui_draw_text(draw, data->bounds, data->title, 0xf0e0d0) == UI_OK); return 1;
    });
    for (bool callback : {false, true}) {
        sunfleck::loadTheme(f.app, callback ? "<Theme Base='Fluent2' Skin='rail'/>" :
            "<Theme Base='Fluent2'><DockTemplate Part='AutoHide'><Fill Background='#203040'><Text Text='{Title}' Foreground='#f0e0d0'/></Fill></DockTemplate></Theme>");
        for (const auto edge : {UI_DOCK_LEFT, UI_DOCK_RIGHT, UI_DOCK_TOP, UI_DOCK_BOTTOM}) {
            f.t.autoHide(edge); REQUIRE(f.app.poll(0) == UI_OK);
            const auto& view = f.dock.nativeHandle()->views.at(0);
            const auto h = std::find_if(view.hits.begin(), view.hits.end(), [](const auto& h) { return h.part == UI_DOCK_PART_AUTO_HIDE; });
            REQUIRE(h != view.hits.end()); const auto r = h->bounds;
            const int width = r.right - r.left, height = r.bottom - r.top;
            const layout::Rect bounds{0, 0, width, height};
            std::vector<FillRectCommand> commands{{0, 0, width, height, {0x20, 0x30, 0x40, 255}}};
            if (edge == UI_DOCK_LEFT || edge == UI_DOCK_RIGHT) {
                append_rotated_text(commands, bounds, bounds, {0xf0, 0xe0, 0xd0, 255}, 96, L"Explorer", true);
            } else { append_centered_text(commands, bounds, bounds, {0xf0, 0xe0, 0xd0, 255}, 96, L"Explorer"); }
            std::vector<uint8_t> expected; rasterize(expected, width, height, commands);
            uint32_t frame_width = 0, frame_height = 0;
            const auto* actual = ui_app_pixels(f.app.nativeHandle(), &frame_width, &frame_height);
            for (int y = 0; y < height; ++y) {
                REQUIRE(std::memcmp(actual + (static_cast<size_t>(r.top + y) * frame_width + r.left) * 4,
                    expected.data() + static_cast<size_t>(y) * width * 4, static_cast<size_t>(width) * 4) == 0);
            }
        }
    }
}
void fleck_mixed_refresh()
{
    sunfleck::App app{UI_SOFTWARE, true};
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockGroup>"
        "<DockDocument Id='a' Title='A'><TextArea Id='editor'/></DockDocument>"
        "<DockDocument Id='b' Title='B'><TextArea/></DockDocument>"
        "</DockGroup></DockHost></Window>";
    app.loadFleck(xml);
    sunfleck::Dock dock{app.findFleckNode("dock")};
    auto document = app.createNode(UI_TEXT_AREA);
    document.setText("Programmatic document draft");
    auto tool = app.createNode(UI_TEXT_AREA);
    tool.setText("Programmatic tool state");
    auto extra = dock.add("extra", "Extra", document);
    auto output = dock.add("output", "Output", tool, UI_DOCK_TOOL_KIND);
    extra.floatWindow({120, 120, 480, 320});
    output.autoHide(UI_DOCK_RIGHT);
    app.openWindow();
    REQUIRE(app.poll(0) == UI_OK);
    const auto saved = dock.save();
    const auto window = dock.windows().front();
    const auto editor_id = app.findFleckNode("editor").debugId();
    unsigned events = 0;
    dock.setEvent([](ui_dock*, ui_dock_item*, ui_dock_event_kind, void* context) -> uint32_t {
        ++*static_cast<unsigned*>(context); return 0;
    }, &events);
    for (unsigned i = 0; i < 4; ++i) {
        auto updated = xml;
        updated.replace(updated.find("Title='A'"), 9, "Title='Updated'");
        app.refreshFleck(updated);
        REQUIRE(app.poll(0) == UI_OK && dock.save() == saved);
        REQUIRE(dock.find("extra").nativeHandle() == extra.nativeHandle());
        REQUIRE(dock.find("output").nativeHandle() == output.nativeHandle());
        REQUIRE(document.text() == "Programmatic document draft" && tool.text() == "Programmatic tool state");
        REQUIRE(dock.windows().front() == window);
        REQUIRE(app.findFleckNode("editor").debugId() == editor_id);
        REQUIRE(document.nativeHandle()->parent == dock.nativeHandle()->host);
        REQUIRE(dock.nativeHandle()->host->children.size() == 3);
    }
    auto changed = xml;
    changed.replace(changed.find("Id='b'"), 6, "Id='c'");
    app.refreshFleck(changed);
    REQUIRE(app.poll(0) == UI_OK && dock.windows().front() == window);
    REQUIRE(dock.find("c").id() == "c" && extra.state() == UI_DOCK_FLOATING);
    REQUIRE(output.state() == UI_DOCK_AUTO_HIDDEN);
    const auto before_failure = dock.save();
    auto conflict = changed;
    conflict.replace(conflict.find("Id='c'"), 6, "Id='extra'");
    ui_fleck_diagnostic diagnostic{};
    REQUIRE(ui_app_refresh_fleck(app.nativeHandle(), conflict.data(), conflict.size(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    constexpr std::string_view plain = "<Window xmlns='urn:sunfleck:v1'><Panel/></Window>";
    REQUIRE(ui_app_refresh_fleck(app.nativeHandle(), plain.data(), plain.size(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(dock.save() == before_failure && document.text() == "Programmatic document draft");
    dock.reset();
    REQUIRE(app.poll(0) == UI_OK);
    REQUIRE(dock.nativeHandle()->items.size() == 4 && extra.state() == UI_DOCK_DOCKED && output.state() == UI_DOCK_DOCKED);
    REQUIRE(events > 0);
    REQUIRE(document.text() == "Programmatic document draft" && tool.text() == "Programmatic tool state");
}
void fleck_item_type_refresh()
{
    for (const bool was_tool : {false, true}) {
        for (unsigned scenario = 0; scenario < (was_tool ? 5u : 4u); ++scenario) {
            const auto page = [&](bool tool, bool can_float = true) {
                const std::string kind = tool ? "DockTool" : "DockDocument";
                const std::string peer = was_tool ? "DockTool" : "DockDocument";
                return "<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockSplit><DockGroup><" + kind +
                    " Id='changed' CanFloat='" + (can_float ? "true" : "false") + "'><TextArea Id='editor'/></" + kind +
                    "></DockGroup><DockGroup><" + peer + " Id='peer'><TextArea Id='peerEditor'/></" + peer +
                    "></DockGroup></DockSplit></DockHost></Window>";
            };
            sunfleck::App app{UI_SOFTWARE, true};
            app.loadFleck(page(was_tool));
            sunfleck::Dock dock{app.findFleckNode("dock")};
            auto changed = dock.find("changed"), peer = dock.find("peer");
            const auto retired_id = app.findFleckNode("changed").debugId();
            auto editor = app.findFleckNode("editor");
            const auto editor_id = editor.debugId();
            editor.setText("Retained draft");
            changed.move(&peer);
            if (scenario == 1) { changed.floatWindow({110, 130, 540, 360}, true); }
            if (scenario == 2) { changed.hide(); }
            if (scenario == 3) { REQUIRE(changed.close()); }
            if (scenario == 4) { changed.autoHide(UI_DOCK_RIGHT); }
            app.openWindow();
            REQUIRE(app.poll(0) == UI_OK);
            const auto state = changed.state();
            const auto peer_state = peer.state();
            const auto windows = dock.windows();
            if (scenario == 1) {
                const auto incompatible = page(!was_tool, false);
                const auto before = dock.save();
                ui_fleck_diagnostic diagnostic{};
                REQUIRE(ui_app_refresh_fleck(app.nativeHandle(), incompatible.data(), incompatible.size(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
                REQUIRE(dock.save() == before && app.findFleckNode("changed").debugId() == retired_id);
            }
            app.refreshFleck(page(!was_tool));
            REQUIRE(app.poll(0) == UI_OK);
            auto replacement = dock.find("changed");
            REQUIRE(replacement.nativeHandle()->kind == (was_tool ? UI_DOCK_DOCUMENT_KIND : UI_DOCK_TOOL_KIND));
            REQUIRE(replacement.state() == (state == UI_DOCK_AUTO_HIDDEN ? UI_DOCK_DOCKED : state));
            REQUIRE(peer.nativeHandle() == dock.find("peer").nativeHandle() && peer.state() == peer_state);
            REQUIRE(app.findFleckNode("editor").debugId() == editor_id && editor.text() == "Retained draft");
            ui_node* retired = nullptr;
            REQUIRE(ui_app_find_debug_node(app.nativeHandle(), retired_id, &retired) == UI_ERROR_INVALID_ARGUMENT);
            REQUIRE(dock.windows() == windows);
            const auto saved = dock.save();
            dock.restore(saved);
            if (dock.save() != saved) {
                std::fprintf(stderr, "Type migration round trip (tool=%d, scenario=%u):\n%s\n%s\n", was_tool, scenario,
                    saved.c_str(), dock.save().c_str());
            }
            REQUIRE(dock.save() == saved);
            replacement.show();
            REQUIRE(app.poll(0) == UI_OK);
            REQUIRE(replacement.state() == (scenario == 1 ? UI_DOCK_FLOATING : UI_DOCK_DOCKED));
            dock.reset();
            REQUIRE(app.poll(0) == UI_OK && replacement.state() == UI_DOCK_DOCKED);
        }
    }
}

void fleck_parity()
{
    const std::string xml = R"(<Window xmlns="urn:sunfleck:v1" Width="900" Height="640"><DockHost Id="dock"><DockSplit Orientation="Horizontal" Ratio="30"><DockGroup><DockTool Id="tool" Title="Explorer" MinWidth="80"><TextArea Text="Tool state"/></DockTool></DockGroup><DockGroup><DockDocument Id="a" Title="Editor A" Icon="D"><TextArea Text="Keep this content"/></DockDocument><DockDocument Id="b" Title="Editor B" CanFloat="false"><TextArea/></DockDocument></DockGroup></DockSplit></DockHost></Window>)";
    fleck::Page page; ui_fleck_diagnostic diagnostic{}; REQUIRE(fleck::parse(xml, page, diagnostic));
    const auto resource = fleck::compile_resource(page); REQUIRE(resource.substr(0, 6) == std::string("FLECK")+char(UI_FLECK_RESOURCE_VERSION)); // Common MinWidth is retained in the tree resource.
    sunfleck::App a{UI_SOFTWARE, true}, b{UI_SOFTWARE, true};
    a.loadFleck(xml.c_str()); sunfleck::detail::check(ui_app_load_fleck_resource(b.nativeHandle(), resource.data(), resource.size(), &diagnostic));
    a.openWindow(); b.openWindow(); REQUIRE(a.poll(0) == UI_OK); REQUIRE(b.poll(0) == UI_OK); REQUIRE(frame(a.nativeHandle()) == frame(b.nativeHandle()));
    sunfleck::Dock dock{a.findFleckNode("dock")}; REQUIRE(dock.find("a").id() == "a");
    REQUIRE(dock.find("a").nativeHandle()->icon == "D");
    const ui_rect bounds{100, 100, 480, 320}; REQUIRE(ui_dock_float(dock.find("b").nativeHandle(), &bounds, 0) != UI_OK);
    auto item = dock.find("a");
    item.floatWindow(bounds);
    dock.find("tool").autoHide(UI_DOCK_LEFT);
    REQUIRE(a.poll(0) == UI_OK);
    const auto saved = dock.save();
    const auto id = a.findFleckNode("a").debugId();
    {
        sunfleck::App restarted{UI_SOFTWARE, true};
        restarted.loadFleck(xml);
        sunfleck::Dock restored{restarted.findFleckNode("dock")};
        restored.restore(saved);
        restarted.openWindow();
        REQUIRE(restarted.poll(0) == UI_OK);
        REQUIRE(restored.save() == saved && restored.find("a").state() == UI_DOCK_FLOATING);
        REQUIRE(restored.find("tool").state() == UI_DOCK_AUTO_HIDDEN);
        const auto windows = restored.windows();
        REQUIRE(windows.size() == 1);
        uint32_t width = 0, height = 0;
        REQUIRE(ui_dock_pixels(restored.nativeHandle(), windows.front(), &width, &height));
        REQUIRE(width == static_cast<uint32_t>(bounds.width) && height == static_cast<uint32_t>(bounds.height));
    }
    auto updated = xml;
    const auto dock_id = a.findFleckNode("dock").debugId();
    updated.replace(updated.find("Id=\"dock\""), 9, "Id=\"renamedDock\"");
    updated.replace(updated.find("Editor A"), 8, "Updated A");
    updated.replace(updated.find("Ratio=\"30\""), 10, "Ratio=\"45\"");
    a.refreshFleck(updated);
    REQUIRE(a.findFleckNode("renamedDock").debugId() == dock_id);
    REQUIRE(a.poll(0) == UI_OK);
    REQUIRE(dock.save() == saved && dock.find("a").nativeHandle() == item.nativeHandle());
    REQUIRE(a.findFleckNode("a").debugId() == id && item.nativeHandle()->title == "Updated A");
    auto wrapped = updated;
    wrapped.insert(wrapped.find("<DockHost"), "<Panel>");
    wrapped.insert(wrapped.find("</Window>"), "</Panel>");
    a.refreshFleck(wrapped);
    REQUIRE(a.poll(0) == UI_OK && dock.save() == saved);
    REQUIRE(a.findFleckNode("renamedDock").debugId() == dock_id);
    REQUIRE(a.findFleckNode("a").debugId() == id);
    a.refreshFleck(updated);
    REQUIRE(a.poll(0) == UI_OK && dock.save() == saved);
    auto changed_capabilities = updated;
    changed_capabilities.replace(changed_capabilities.find("CanFloat=\"false\""), 16, "CanFloat=\"true\"");
    a.refreshFleck(changed_capabilities);
    REQUIRE(dock.save() == saved && (dock.find("b").nativeHandle()->capabilities & UI_DOCK_CAN_FLOAT));
    auto incompatible = changed_capabilities;
    incompatible.insert(incompatible.find("Icon=\"D\""), "CanFloat=\"false\" ");
    REQUIRE(ui_app_refresh_fleck(a.nativeHandle(), incompatible.data(), incompatible.size(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(dock.save() == saved && (item.nativeHandle()->capabilities & UI_DOCK_CAN_FLOAT));
    const auto old_b = a.findFleckNode("b").debugId();
    const auto old_window = dock.windows().front();
    const auto removed = updated.find("<DockDocument Id=\"b\"");
    const auto end = updated.find("</DockDocument>", removed) + std::string_view("</DockDocument>").size();
    updated.replace(removed, end - removed, "<DockDocument Id=\"c\" Title=\"New\"><TextArea/></DockDocument>");
    a.refreshFleck(updated);
    REQUIRE(a.poll(0) == UI_OK && dock.windows().front() == old_window);
    REQUIRE(dock.find("a").nativeHandle() == item.nativeHandle() && dock.find("c").id() == "c");
    ui_node* retired = nullptr;
    REQUIRE(ui_app_find_debug_node(a.nativeHandle(), old_b, &retired) == UI_ERROR_INVALID_ARGUMENT);
    dock.reset();
    REQUIRE(dock.save() != saved && dock.nativeHandle()->layout.regions.at(dock.nativeHandle()->layout.root).ratio == 4500);
    const auto retired_host = a.findFleckNode("renamedDock").debugId();
    dock.find("a").floatWindow(bounds);
    REQUIRE(a.poll(0) == UI_OK && dock.windows().size() == 1);
    constexpr auto plain = "<Window xmlns='urn:sunfleck:v1'><Panel><TextArea Id='retained' Text='Draft'/></Panel></Window>";
    a.refreshFleck(plain);
    REQUIRE(a.poll(0) == UI_OK);
    REQUIRE(ui_app_find_debug_node(a.nativeHandle(), retired_host, &retired) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(a.debugSnapshot().find("\"dockItems\":[]") != std::string::npos);
    auto with_retained = xml;
    with_retained.replace(with_retained.find("<TextArea Text=\"Tool state\"/>"),
        std::string_view("<TextArea Text=\"Tool state\"/>").size(), "<TextArea Id=\"retained\"/>");
    const auto retained_id = a.findFleckNode("retained").debugId();
    a.refreshFleck(with_retained);
    REQUIRE(a.poll(0) == UI_OK);
    REQUIRE(a.findFleckNode("retained").debugId() == retained_id && a.findFleckNode("retained").text() == "Draft");
    sunfleck::Dock replacement{a.findFleckNode("dock")};
    REQUIRE(replacement.find("tool").state() == UI_DOCK_DOCKED);
    const auto other_poll = b.poll(0);
    REQUIRE(other_poll == UI_OK || other_poll == UI_TIMEOUT);
    REQUIRE(b.findFleckNode("dock").nativeHandle() != a.findFleckNode("dock").nativeHandle());
    sunfleck::Dock unaffected{b.findFleckNode("dock")};
    REQUIRE(unaffected.find("a").state() == UI_DOCK_DOCKED);
    {
        sunfleck::App sessions{UI_SOFTWARE, true};
        sessions.loadFleck(xml);
        ui_node* first = nullptr;
        REQUIRE(ui_app_get_fleck_root(sessions.nativeHandle(), &first) == UI_OK);
        const auto first_id = sessions.findFleckNode("dock").debugId();
        sunfleck::Dock first_dock{sessions.findFleckNode("dock")};
        first_dock.find("a").floatWindow(bounds);
        const auto layout = first_dock.save();
        sessions.loadFleck(plain);
        ui_node* second = nullptr;
        REQUIRE(ui_app_get_fleck_root(sessions.nativeHandle(), &second) == UI_OK && second != first);
        sessions.openWindow();
        REQUIRE(ui_app_set_root(sessions.nativeHandle(), first) == UI_OK);
        REQUIRE(sessions.poll(0) == UI_OK && first_dock.save() == layout && first_dock.windows().size() == 1);
        REQUIRE(sessions.debugSnapshot().find("\"fleckId\":\"retained\"") == std::string::npos);
        REQUIRE(ui_app_set_root(sessions.nativeHandle(), second) == UI_OK);
        REQUIRE(sessions.debugSnapshot().find("\"fleckId\":\"dock\"") == std::string::npos);
        REQUIRE(ui_dock_pointer(first_dock.nativeHandle(), 0, UI_POINTER_DOWN, 0, 0) == UI_ERROR_INVALID_STATE);
        REQUIRE(ui_app_discard_fleck_page(sessions.nativeHandle(), first) == UI_OK);
        REQUIRE(ui_app_find_debug_node(sessions.nativeHandle(), first_id, &retired) == UI_ERROR_INVALID_ARGUMENT);
        REQUIRE(sessions.findFleckNode("retained").text() == "Draft");
    }
    // Corrupt a v5 resource and ensure the app remains loadable after rejection.
    sunfleck::App bad{UI_SOFTWARE, true}; REQUIRE(ui_app_load_fleck_resource(bad.nativeHandle(), resource.data(), resource.size() - 1, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    bad.loadFleck(xml.c_str());
}

}
int main()
{
    try { model(); return_positions(); persistence(); partial_close(); drag(); tab_order(); chrome_and_menu(); direction_targets(); themes(); chrome_appearance(); tab_close_buttons(); glass_toolbar_layout(); floating_padding(); rotated_labels(); auto_hide_labels(); auto_hide_skins(); fleck_mixed_refresh(); fleck_item_type_refresh(); fleck_parity(); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
