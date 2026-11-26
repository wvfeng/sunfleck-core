#include <sunfleck/ui.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main()
{
    ui_app* app = nullptr;
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    REQUIRE(ui_app_create(&options, &app) == UI_OK);
    const char page[] = R"(<Window xmlns="urn:sunfleck:v1"><DockHost Id="dock"><DockGroup><DockDocument Id="doc" Title="Document"><TextArea Id="editor" Text="Original"/></DockDocument><DockDocument Id="other" Title="Other"><TextBlock Text="Other"/></DockDocument></DockGroup></DockHost></Window>)";
    ui_fleck_diagnostic diagnostic{};
    REQUIRE(ui_app_load_fleck(app, page, sizeof(page)-1, &diagnostic) == UI_OK);
    ui_node *host = nullptr, *editor = nullptr;
    REQUIRE(ui_app_find_fleck_node(app, "dock", &host) == UI_OK);
    REQUIRE(ui_app_find_fleck_node(app, "editor", &editor) == UI_OK);
    ui_dock* dock = nullptr;
    REQUIRE(ui_dock_create(host, &dock) == UI_OK);
    REQUIRE(ui_app_open_window(app) == UI_OK);
    REQUIRE(ui_host_set_render_mode(app, 1) == UI_OK);
    REQUIRE(ui_host_resize(app, 800, 600, 96) == UI_OK);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    ui_dock_item* item = nullptr;
    REQUIRE(ui_dock_find(dock, "doc", &item) == UI_OK);
    const ui_rect rect{80, 90, 400, 300};
    REQUIRE(ui_dock_float(item, &rect, 0) == UI_OK);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    uint32_t count = 0;
    REQUIRE(ui_dock_get_windows(dock, nullptr, &count) == UI_OK);
    std::vector<uint32_t> ids(count);
    REQUIRE(ui_dock_get_windows(dock, ids.data(), &count) == UI_OK);
    REQUIRE(!ids.empty());
    const auto view = ids.back();
    REQUIRE(ui_host_select_view(app, view) == UI_OK);
    const uint8_t* commands = nullptr;
    uint32_t command_bytes = 0;
    REQUIRE(ui_host_draw_commands(app, &commands, &command_bytes) == UI_OK && commands && command_bytes % 20 == 0);
    const auto dimensions = [&](uint32_t w, uint32_t h) {
        uint32_t actual_w = 0, actual_h = 0;
        return ui_app_poll(app, 0) == UI_OK && ui_host_pixels(app, &actual_w, &actual_h) &&
            actual_w == w && actual_h == h;
    };
    const auto caption = [&](int x, int y = 16) {
        return ui_host_pointer(app, UI_POINTER_DOWN, x, y) == UI_OK &&
            ui_host_pointer(app, UI_POINTER_UP, x, y) == UI_OK;
    };
    // Minimize/maximize must retain the full normal rectangle, not the caption height.
    REQUIRE(caption(400 - 110));
    REQUIRE(caption(400 - 66));
    REQUIRE(dimensions(800, 600));
    REQUIRE(caption(800 - 66));
    REQUIRE(dimensions(400, 300));
    // A cancelled corner resize restores its original dimensions.
    REQUIRE(ui_host_pointer(app, UI_POINTER_DOWN, 398, 298) == UI_OK);
    REQUIRE(ui_host_pointer(app, UI_POINTER_MOVE, 448, 348) == UI_OK);
    REQUIRE(dimensions(450, 350));
    REQUIRE(ui_host_pointer(app, UI_POINTER_CANCEL, 0, 0) == UI_OK);
    REQUIRE(dimensions(400, 300));
    REQUIRE(caption(400 - 66));
    REQUIRE(ui_host_select_view(app, 0) == UI_OK);
    REQUIRE(ui_host_resize(app, 1600, 1200, 192) == UI_OK);
    REQUIRE(ui_host_select_view(app, view) == UI_OK);
    REQUIRE(ui_host_resize(app, 1600, 1200, 192) == UI_OK);
    REQUIRE(caption(1600 - 132, 32));
    REQUIRE(dimensions(800, 600));
    REQUIRE(caption(800 - 132, 32));
    REQUIRE(ui_host_select_view(app, 0) == UI_OK);
    REQUIRE(ui_host_resize(app, 1200, 1000, 192) == UI_OK);
    REQUIRE(ui_host_select_view(app, view) == UI_OK);
    REQUIRE(dimensions(1200, 1000));
    REQUIRE(caption(1200 - 132, 32));
    REQUIRE(dimensions(800, 600));
    uint64_t id = 0;
    REQUIRE(ui_node_get_debug_id(editor, &id) == UI_OK);
    REQUIRE(ui_host_semantic_action(app, id, 0) == UI_OK);
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.active);
    const char text[] = "\xe4\xb8\xad\xe6\x96\x87";
    REQUIRE(ui_host_text_update(app, state.session, state.revision, text, sizeof(text)-1, 2, 2, 0) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK);
    REQUIRE(std::string(state.text, state.text_size) == text);
    size_t size = 0;
    REQUIRE(ui_host_snapshot(app, nullptr, &size) == UI_OK);
    std::string snapshot(size, '\0');
    REQUIRE(ui_host_snapshot(app, snapshot.data(), &size) == UI_OK);
    REQUIRE(snapshot.find("textbox") != std::string::npos);
    REQUIRE(ui_host_select_view(app, 0) == UI_OK);
    REQUIRE(ui_dock_reset(dock) == UI_OK);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    REQUIRE(ui_host_select_view(app, view) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_semantic_action(app, id, 0) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.active);
    REQUIRE(std::string(state.text, state.text_size) == text);
    REQUIRE(ui_host_scroll(app, 10, 10, 40000) == UI_ERROR_INVALID_ARGUMENT);
    ui_app_destroy(app);
}
