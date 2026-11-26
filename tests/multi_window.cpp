#include <sunfleck/dock.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/dock_json.h"
#include <cstdio>
#include <set>
#include <regex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
std::string page(const char* title, const char* text)
{
    return "<Window xmlns='urn:sunfleck:v1' Title='" + std::string(title) + "' Width='500' Height='340'>"
        "<DockHost Id='workspace'><DockGroup><DockDocument Id='document' Title='Document'>"
        "<StackPanel Id='panel' IsFocusScope='true'><TextBox Id='editor' Text='Draft'/><TextBlock Id='shared' Text='{Binding Shared}'/>"
        "<Button Id='button' Content='" + text + "'/></StackPanel></DockDocument>"
        "<DockDocument Id='peer' Title='Peer'><TextBlock Text='Peer'/></DockDocument></DockGroup></DockHost></Window>";
}
void run(bool desktop)
{
    sunfleck::App app(UI_SOFTWARE, !desktop);
    sunfleck::PropertyModel model(app);
    std::string shared = "Shared initial";
    model.property("Shared", [&] { return shared; });
    const auto load = [&](const std::string& xml) {
        sunfleck::detail::check(ui_app_load_fleck_with_model(app.nativeHandle(), xml.data(), xml.size(), model.nativeHandle(), nullptr, nullptr));
    };
    load(page("Session A", "A"));
    auto root_a = app.fleckRoot();
    auto editor_a = app.findFleckNode("editor");
    auto button_a = app.findFleckNode("button");
    sunfleck::Dock dock_a(app.findFleckNode("workspace"));
    auto second = app.createWindow();
    require(second.id() != 0, "Independent window did not receive an identity");
    ui_window* primary = nullptr;
    require(ui_app_get_current_window(app.nativeHandle(), &primary) == UI_OK && primary,
        "Primary window handle missing");
    const auto noop = [](ui_app*, void*) -> ui_result { return UI_TIMEOUT; };
    require(ui_window_invoke(primary, noop, nullptr) == UI_TIMEOUT &&
        ui_window_invoke(nullptr, noop, nullptr) == UI_ERROR_INVALID_ARGUMENT &&
        ui_window_invoke(primary, nullptr, nullptr) == UI_ERROR_INVALID_ARGUMENT,
        "Window callback result or argument contract failed");
    ui_result thread_result = UI_OK;
    std::thread worker([&] { thread_result = ui_window_invoke(second.nativeHandle(), noop, nullptr); });
    worker.join();
    require(thread_result == UI_ERROR_WRONG_THREAD, "Window invocation bypassed UI thread ownership");
    ui_node *root_b = nullptr, *editor_b = nullptr, *button_b = nullptr;
    ui_dock* raw_dock_b = nullptr;
    second.withContext([&] {
        require(ui_window_invoke(primary, noop, nullptr) == UI_ERROR_INVALID_STATE,
            "Nested callback switched to another session");
        require(ui_app_poll(app.nativeHandle(), 0) == UI_ERROR_INVALID_STATE, "Window callback allowed recursive Poll");
        ui_window* rejected = nullptr;
        require(ui_app_create_window(app.nativeHandle(), &rejected) == UI_ERROR_INVALID_STATE && !rejected,
            "Window callback allowed session collection mutation");
        load(page("Session B", "B"));
        root_b = app.fleckRoot().nativeHandle();
        editor_b = app.findFleckNode("editor").nativeHandle();
        button_b = app.findFleckNode("button").nativeHandle();
        raw_dock_b = sunfleck::Dock(app.findFleckNode("workspace")).nativeHandle();
    });
    sunfleck::Dock dock_b{app.findDebugNode(root_b->debug_id)};
    struct DockState { ui_app* app; ui_node* root; unsigned calls = 0; } dock_state{app.nativeHandle(), root_b};
    sunfleck::detail::check(ui_dock_set_event(raw_dock_b, [](ui_dock*, ui_dock_item*, ui_dock_event_kind, void* raw) -> uint32_t {
        auto& state = *static_cast<DockState*>(raw);
        ui_node* root = nullptr;
        ui_window* rejected = nullptr;
        if (ui_app_get_fleck_root(state.app, &root) == UI_OK && root == state.root &&
            ui_app_create_window(state.app, &rejected) == UI_ERROR_INVALID_STATE && !rejected) { ++state.calls; }
        return 1;
    }, &dock_state));
    require(dock_b.nativeHandle() == raw_dock_b && editor_a.nativeHandle() != editor_b &&
        app.fleckRoot().nativeHandle() == root_a.nativeHandle(), "Window context leaked page/Dock or lookup identity");
    require(ui_app_set_root(app.nativeHandle(), root_b) == UI_ERROR_INVALID_ARGUMENT &&
        ui_node_append(app.findFleckNode("panel").nativeHandle(), editor_b) == UI_ERROR_INVALID_ARGUMENT,
        "Cross-session parenting/root was accepted");
    try {
        second.withContext([&] { throw std::runtime_error("Context exception"); });
        require(false, "Window callback exception was swallowed");
    } catch (const std::runtime_error& error) {
        require(std::string_view(error.what()) == "Context exception", "Wrong callback exception");
    }
    require(app.fleckRoot().nativeHandle() == root_a.nativeHandle(), "Exception did not restore primary context");
    app.openWindow();
    second.open();
    const auto pump = [&] {
        for (unsigned i = 0; i < (desktop ? 12u : 1u); ++i) {
            const auto result = app.poll(desktop ? 5 : 0);
            require(result == UI_OK || result == UI_TIMEOUT, "One session prematurely terminated the App");
        }
    };
    pump();
    auto scope_a = app.findFleckNode("panel");
    editor_a.focus();
    second.withContext([&] {
        auto scope_b = app.findFleckNode("panel");
        app.findFleckNode("button").focus();
        require(scope_b.logicalFocus().nativeHandle() == button_b, "Secondary logical focus lost its window");
        scope_a.focus();
        require(app.fleckRoot().nativeHandle() == root_b && scope_b.logicalFocus().nativeHandle() == button_b,
            "Restoring primary logical focus changed the secondary session");
    });
    require(scope_a.logicalFocus().nativeHandle() == editor_a.nativeHandle(), "Primary logical focus was replaced");
    shared = "Shared updated";
    model.notify("Shared");
    pump();
    second.withContext([&] { require(app.findFleckNode("shared").text() == shared, "Shared model did not reach secondary window"); });
    require(app.findFleckNode("shared").text() == shared, "Shared model did not reach primary window");
    unsigned captures_a = 0, captures_b = 0;
    app.setFleckRefreshStateCallback([](ui_app*, ui_fleck_refresh_phase phase, ui_result, void* raw) {
        if (phase == UI_FLECK_REFRESH_CAPTURE) { ++*static_cast<unsigned*>(raw); }
        return UI_OK;
    }, &captures_a);
    second.withContext([&] {
        app.setFleckRefreshStateCallback([](ui_app*, ui_fleck_refresh_phase phase, ui_result, void* raw) {
            if (phase == UI_FLECK_REFRESH_CAPTURE) { ++*static_cast<unsigned*>(raw); }
            return UI_OK;
        }, &captures_b);
    });
    button_a.nativeHandle()->running_actions = 1;
    app.queueFleckRefresh(page("Session A", "A refreshed"), model.nativeHandle());
    second.withContext([&] { app.queueFleckRefresh(page("Session B", "B refreshed"), model.nativeHandle()); });
    pump();
    require(app.fleckRefreshStatus().pending && captures_a == 0 && captures_b == 1 && button_a.text() == "A",
        "Busy primary blocked or consumed another session's refresh");
    second.withContext([&] { require(!app.fleckRefreshStatus().pending && app.findFleckNode("button").text() == "B refreshed",
        "Secondary queue did not commit independently"); });
    button_a.nativeHandle()->running_actions = 0;
    pump();
    require(!app.fleckRefreshStatus().pending && captures_a == 1, "Primary queue did not resume");

#ifdef _WIN32
    if (desktop) {
        struct State { ui_app* app; ui_node* expected; unsigned calls = 0; } state{app.nativeHandle(), button_b};
        sunfleck::detail::check(ui_node_set_action(button_b, [](ui_node*, void* raw) {
            auto& state = *static_cast<State*>(raw);
            ui_node* current = nullptr;
            if (ui_app_find_fleck_node(state.app, "button", &current) == UI_OK && current == state.expected) { ++state.calls; }
        }, &state));
        const auto window_b = FindWindowA("SunfleckPreview", "Session B");
        require(window_b != nullptr, "Secondary native window missing");
        const auto bounds = button_b->visible;
        require(bounds.right > bounds.left, "Secondary button has no geometry");
        const auto point = MAKELPARAM(bounds.left + 8, (bounds.top + bounds.bottom) / 2);
        SendMessageW(window_b, WM_LBUTTONDOWN, MK_LBUTTON, point);
        require(ui_app_set_debug_picking(app.nativeHandle(), 1) == UI_ERROR_INVALID_STATE,
            "Picker ignored a pressed control in another session");
        SendMessageW(window_b, WM_LBUTTONUP, 0, point);
        require(state.calls == 1, "Native callback dispatched against another session's page");
        sunfleck::detail::check(ui_node_set_action(button_b, nullptr, nullptr));
    }
#endif
    dock_a.find("document").floatWindow({80, 80, 360, 260});
    dock_b.find("document").floatWindow({600, 100, 360, 260});
    require(dock_state.calls > 0, "Dock callback did not select its owning session or block collection mutation");
    sunfleck::detail::check(ui_dock_set_event(raw_dock_b, nullptr, nullptr));
    pump();
    require(dock_a.windows().size() == 1 && dock_a.windows() == dock_b.windows(), "Fixture needs colliding local floating IDs");
    const auto snapshot = app.debugSnapshot();
    const auto windows = snapshot.substr(snapshot.find("\"windows\":"), snapshot.find("\"dockItems\":") - snapshot.find("\"windows\":"));
    std::set<int> ids;
    const std::regex window_id("\"id\":([0-9]+)");
    for (auto it = std::sregex_iterator(windows.begin(), windows.end(), window_id); it != std::sregex_iterator(); ++it) {
        require(ids.insert(std::stoi((*it)[1])).second, "Snapshot reused a native window identity across sessions");
    }
    require(ids.size() == 4, "Snapshot omitted independent main/floating windows");
    std::set<int> floating_ids;
    const auto items = snapshot.substr(snapshot.find("\"dockItems\":"), snapshot.find("\"nodes\":") - snapshot.find("\"dockItems\":"));
    const std::regex document_window("\"id\":\"document\"[^}]*\"window\":([0-9]+)");
    for (auto it = std::sregex_iterator(items.begin(), items.end(), document_window); it != std::sregex_iterator(); ++it) {
        floating_ids.insert(std::stoi((*it)[1]));
    }
    require(floating_ids.size() == 2, "Snapshot merged same-ID Dock items from separate sessions");
    uint32_t width = 0, height = 0;
    if (!desktop) { require(ui_dock_pixels(raw_dock_b, dock_b.windows().front(), &width, &height) && width > 0 && height > 0,
        "Secondary floating pixels resolved through primary workspace"); }
    editor_a.setText("A draft");
    app.findDebugNode(editor_b->debug_id).setText("B draft");
    pump();
    const auto layout_a = dock_a.save(), layout_b = dock_b.save();
    const auto old_b_id = editor_b->debug_id;
    second.withContext([&] { app.refreshFleck(page("Session B", "B again"), model.nativeHandle()); });
    pump();
    require(dock_a.save() == layout_a && dock_b.save() == layout_b && editor_a.text() == "A draft" &&
        app.findDebugNode(editor_b->debug_id).text() == "B draft", "Secondary refresh changed another session or lost drafts/layout");
    second.withContext([&] {
        auto invalid = page("Session B", "Invalid");
        invalid.replace(invalid.find("Title='Document'"), 16, "Title='Document' CanFloat='false'");
        require(ui_app_refresh_fleck(app.nativeHandle(), invalid.data(), invalid.size(), model.nativeHandle(), nullptr) != UI_OK,
            "Incompatible floating capability was committed");
    });
    require(dock_a.save() == layout_a && dock_b.save() == layout_b, "Rollback changed a live workspace");
    second.withContext([&] { app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><Button Id='button' Content='Replacement'/></Window>"); });
    pump();
    ui_node* retired = nullptr;
    require(ui_app_find_debug_node(app.nativeHandle(), old_b_id, &retired) == UI_ERROR_INVALID_ARGUMENT &&
        dock_a.save() == layout_a && dock_a.windows().size() == 1, "Secondary retirement removed primary objects/floating windows");
#ifdef _WIN32
    if (desktop) {
        const auto window_a = FindWindowA("SunfleckPreview", "Session A");
        require(window_a != nullptr, "Primary native window missing before close");
        SendMessageW(window_a, WM_CLOSE, 0, 0);
    } else
#endif
    { app.requestClose(); }
    pump();
#ifdef _WIN32
    if (desktop) {
        require(ui_app_set_debug_picking(app.nativeHandle(), 1) == UI_OK &&
            ui_app_set_debug_picking(app.nativeHandle(), 0) == UI_OK,
            "Closed primary prevented picking in a live independent window");
    }
#endif
    second.withContext([&] {
        auto label = app.createNode(UI_TEXT_BLOCK);
        label.setText("Alive after primary close");
        app.findFleckNode("button").setText("Still alive");
    });
    pump();
    second.requestClose();
    require(app.poll(0) == UI_EXITED, "App did not exit after its last independent window closed");
}
}

int main(int argc, char**)
{
    try {
        run(argc > 1);
        sunfleck::App secondary_only(UI_SOFTWARE, true);
        auto window = secondary_only.createWindow();
        window.withContext([&] { secondary_only.loadFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock/></Window>"); });
        window.open();
        require(secondary_only.poll(0) == UI_OK, "An unopened primary prevented independent-window polling");
        window.requestClose();
        require(secondary_only.poll(0) == UI_EXITED, "Secondary-only App did not exit");
        std::puts("Independent window sessions passed");
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
