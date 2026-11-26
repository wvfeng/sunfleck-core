#include <sunfleck/ui.h>
#include <cstdio>
#include <thread>
#include <vector>

#define REQUIRE(value) do { if (!(value)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); return 1; } } while (false)
namespace {
struct State { ui_completion* pending = nullptr; unsigned starts = 0, cancels = 0, finishes = 0; ui_result result = UI_OK; };
void begin(void* raw, ui_completion* token) { auto& s = *static_cast<State*>(raw); s.pending = token; ++s.starts; }
void cancel(void* raw) { ++static_cast<State*>(raw)->cancels; }
void complete(void* raw, ui_result result) { auto& s = *static_cast<State*>(raw); ++s.finishes; s.result = result; }
}
int main()
{
    ui_app* app = nullptr;
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    REQUIRE(ui_app_create(&options, &app) == UI_OK);
    REQUIRE(ui_host_resize(app, 400, 200, 96) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_set_render_mode(app, 1) == UI_ERROR_INVALID_STATE);
    State state;
    const ui_external_command_options async{sizeof(async), 1, begin, cancel, complete, &state};
    ui_command* command = nullptr;
    REQUIRE(ui_command_create_external(app, &async, &command) == UI_OK);
    ui_node* button = nullptr;
    REQUIRE(ui_node_create(app, UI_BUTTON, &button) == UI_OK);
    REQUIRE(ui_node_bind_command(button, command) == UI_OK);
    REQUIRE(ui_node_set_command_mode(button, UI_COMMAND_ASYNC) == UI_OK);
    REQUIRE(ui_node_set_command_mode(button, UI_COMMAND_SYNC) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_app_set_root(app, button) == UI_OK);
    REQUIRE(ui_app_open_window(app) == UI_OK);
    REQUIRE(ui_host_resize(app, 400, 200, 144) == UI_OK);
    REQUIRE(ui_host_resize(app, 0, 200, 96) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_resize(app, 8192, 8192, 96) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    uint32_t width = 0, height = 0;
    REQUIRE(ui_app_pixels(app, &width, &height) && width == 400 && height == 200);
    const auto* original = ui_app_pixels(app, &width, &height);
    const std::vector<uint8_t> reference(original, original + width * height * 4);
    const uint8_t* commands = nullptr;
    uint32_t bytes = 0;
    REQUIRE(ui_host_draw_commands(app, &commands, &bytes) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_set_render_mode(app, 2) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_set_render_mode(app, 1) == UI_OK);
    REQUIRE(ui_host_draw_commands(app, &commands, &bytes) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    REQUIRE(!ui_app_pixels(app, &width, &height)); // No eager CPU rasterization.
    REQUIRE(ui_host_draw_commands(app, &commands, &bytes) == UI_OK && commands && bytes >= 20 && bytes % 20 == 0);
    REQUIRE(ui_host_draw_commands(app, nullptr, &bytes) == UI_ERROR_INVALID_ARGUMENT);
    const auto* fallback = ui_host_pixels(app, &width, &height);
    REQUIRE(fallback && std::vector<uint8_t>(fallback, fallback + width * height * 4) == reference);
    REQUIRE(ui_host_set_render_mode(app, 0) == UI_OK);
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    REQUIRE(ui_app_pixels(app, &width, &height));
    REQUIRE(ui_host_draw_commands(app, &commands, &bytes) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_pointer(app, UI_POINTER_UP, 60, 35) == UI_OK);
    REQUIRE(state.starts == 0);
    REQUIRE(ui_host_pointer(app, UI_POINTER_DOWN, 60, 35) == UI_OK);
    REQUIRE(ui_host_pointer(app, UI_POINTER_CANCEL, 0, 0) == UI_OK);
    REQUIRE(ui_host_pointer(app, UI_POINTER_UP, 60, 35) == UI_OK);
    REQUIRE(state.starts == 0);
    REQUIRE(ui_host_pointer(app, UI_POINTER_DOWN, 60, 35) == UI_OK);
    REQUIRE(ui_host_pointer(app, UI_POINTER_UP, 60, 35) == UI_OK);
    REQUIRE(state.starts == 1 && state.pending);
    REQUIRE(ui_app_activate_button(app) == UI_OK);
    REQUIRE(state.starts == 1); // single flight
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
    ui_result wrong_thread = UI_OK;
    std::thread worker([&] { wrong_thread = ui_completion_complete(state.pending, UI_OK); });
    worker.join();
    REQUIRE(wrong_thread == UI_ERROR_WRONG_THREAD);
#endif
    REQUIRE(ui_completion_complete(state.pending, UI_OK) == UI_OK);
    REQUIRE(ui_completion_complete(state.pending, UI_OK) == UI_ERROR_INVALID_STATE);
    REQUIRE(state.finishes == 0); // completion is not reentrant
    ui_completion_release(state.pending); state.pending = nullptr;
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    REQUIRE(state.finishes == 1 && state.result == UI_OK);
    REQUIRE(ui_app_activate_button(app) == UI_OK);
    REQUIRE(ui_command_cancel(command) == UI_OK);
    REQUIRE(ui_command_cancel(command) == UI_OK);
    REQUIRE(state.cancels == 1);
    REQUIRE(ui_completion_complete(state.pending, UI_OK) == UI_OK);
    ui_completion_release(state.pending); state.pending = nullptr;
    REQUIRE(ui_app_poll(app, 0) == UI_OK);
    REQUIRE(state.finishes == 2 && state.result == UI_CANCELLED);
    REQUIRE(ui_app_activate_button(app) == UI_OK);
    REQUIRE(state.starts == 3);
    ui_app_destroy(app);
    REQUIRE(state.cancels == 2);
    REQUIRE(ui_completion_complete(state.pending, UI_OK) == UI_CANCELLED);
    ui_completion_release(state.pending);
    REQUIRE(state.finishes == 2);
    return 0;
}
