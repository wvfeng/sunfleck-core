#include <sunfleck/ui.h>
#include <sunfleck/web_inspector.h>
#include <emscripten.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>

EM_JS(void, fetch_message, (void* token), {
    const controller = new AbortController();
    Module.networkController = controller;
    const timer = setTimeout(() => controller.abort(), 2000);
    fetch('./message.txt', { signal: controller.signal, cache: 'no-cache' })
      .then(response => { if (!response.ok) throw new Error('HTTP error'); return response.text(); })
      .then(() => Module._sf_http_complete(token, 0), error => Module._sf_http_complete(token, error.name === 'AbortError' ? 2 : 1))
      .finally(() => { clearTimeout(timer); if (Module.networkController === controller) Module.networkController = null; });
});
EM_JS(void, cancel_message, (), { Module.networkController?.abort(); });

namespace {
ui_app* app = nullptr;
ui_text_source* text = nullptr;
ui_text_source* draft = nullptr;
unsigned count = 0;
ui_dock* workspace = nullptr;
ui_text_source* network_status = nullptr;
ui_command* network_command = nullptr;
bool network_cancelled = false;
std::string text_state_json;
void increment(ui_node*, void*)
{
    char value[64];
    std::snprintf(value, sizeof(value), "Count: %u", ++count);
    ui_text_source_set(text, value);
}
void delayed(void* raw)
{
    auto* completion = static_cast<ui_completion*>(raw);
    // A detached token is safe even when the host has already destroyed App.
    if (ui_completion_complete(completion, UI_OK) == UI_OK) { increment(nullptr, nullptr); }
    ui_completion_release(completion);
}
void begin(void*, ui_completion* completion) { emscripten_async_call(delayed, completion, 500); }
void reset_text(ui_node*, void*) { ui_text_source_set(draft, "Edit me"); }
void float_panel(ui_node*, void*) {
    ui_dock_item* item = nullptr;
    if (ui_dock_find(workspace, "document", &item) == UI_OK) {
        const ui_rect bounds{150, 80, 400, 320}; ui_dock_float(item, &bounds, 0);
    }
}
void reset_dock(ui_node*, void*) { ui_dock_reset(workspace); }
void network_begin(void*, ui_completion* completion) {
    network_cancelled = false; ui_text_source_set(network_status, "Network: loading"); fetch_message(completion);
}
void network_cancel(void*) { network_cancelled = true; cancel_message(); }
void cancel_network(ui_node*, void*) { ui_command_cancel(network_command); ui_text_source_set(network_status, "Network: cancelled"); }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int sf_start(const char* page_override)
{
    if (app) { return UI_ERROR_INVALID_STATE; }
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    auto status = ui_app_create(&options, &app);
    if (status != UI_OK) { return status; }
    count = 0;
    ui_command* sync = nullptr;
    ui_command* async = nullptr;
    ui_command* reset = nullptr;
    ui_command* cancel = nullptr;
    const ui_external_command_options external{sizeof(external), 1, begin, nullptr, nullptr, nullptr};
    const ui_external_command_options network{sizeof(network), 1, network_begin, network_cancel, nullptr, nullptr};
    if ((status = ui_text_source_create(app, "Count: 0", &text)) == UI_OK &&
        (status = ui_text_source_create(app, "Edit me", &draft)) == UI_OK &&
        (status = ui_command_create(app, reset_text, nullptr, &reset)) == UI_OK &&
        (status = ui_command_create(app, increment, nullptr, &sync)) == UI_OK &&
        (status = ui_command_create_external(app, &external, &async)) == UI_OK &&
        (status = ui_text_source_create(app, "Network: idle", &network_status)) == UI_OK &&
        (status = ui_command_create_external(app, &network, &network_command)) == UI_OK &&
        (status = ui_command_create(app, cancel_network, nullptr, &cancel)) == UI_OK) {
        const ui_fleck_text_export texts[]{{"Counter", text}, {"Draft", draft}, {"NetworkStatus", network_status}};
        const ui_fleck_command_export commands[]{{"Increment", sync}, {"Delayed", async}, {"Reset", reset}, {"Network", network_command}, {"CancelNetwork", cancel}};
        const ui_fleck_data_context context{texts, 3, commands, 5};
        constexpr char page[] = R"(<Window xmlns="urn:sunfleck:v1" Title="Sunfleck Web" Width="560" Height="560"><StackPanel Spacing="12" Padding="16"><TextBlock Text="Sunfleck / C++ WebAssembly"/><TextBlock Text="{Binding Counter}"/><Button Text="Increment" Command="{Binding Increment}"/><Button Text="Increment after 500 ms" Command="{Binding Delayed}" CommandMode="Async"/><TextBox Id="editor" Text="{Binding Draft, Mode=TwoWay}"/><TextArea Id="notes" Text="Multiline notes"/><PasswordBox Id="password"/><TextBlock Text="{Binding Draft}"/><Button Text="Reset text" Command="{Binding Reset}"/></StackPanel></Window>)";
        ui_fleck_diagnostic diagnostic{};
        const char* selected = page_override && *page_override ? page_override : page;
        status = ui_app_load_fleck_with_context(app, selected, std::strlen(selected), &context, &diagnostic);
        if (status != UI_OK) { std::fprintf(stderr, "%s\n", diagnostic.message); }
        ui_node* host = nullptr;
        if (status == UI_OK && ui_app_find_fleck_node(app, "workspace", &host) == UI_OK) {
            status = ui_dock_create(host, &workspace);
            ui_node* button = nullptr;
            if (ui_app_find_fleck_node(app, "float", &button) == UI_OK) ui_node_set_action(button, float_panel, nullptr);
            if (ui_app_find_fleck_node(app, "resetDock", &button) == UI_OK) ui_node_set_action(button, reset_dock, nullptr);
        }
    }
    if (status == UI_OK) { status = ui_app_open_window(app); }
    if (status != UI_OK) { ui_app_destroy(app); app = nullptr; text = nullptr; }
    sunfleck_web_debug_bind(app);
    return status;
}
EMSCRIPTEN_KEEPALIVE int sf_tick() { return ui_app_poll(app, 0); }
EMSCRIPTEN_KEEPALIVE int sf_verify_contracts() {
    ui_window* window = nullptr;
    return ui_app_poll(app, 1) == UI_ERROR_UNSUPPORTED && ui_app_run(app) == UI_ERROR_UNSUPPORTED &&
        ui_app_create_window(app, &window) == UI_ERROR_UNSUPPORTED && !window && ui_host_capabilities() == 63 ? 0 : 1;
}
EMSCRIPTEN_KEEPALIVE void sf_http_complete(ui_completion* token, int status) {
    if (ui_completion_complete(token, UI_OK) == UI_OK && !network_cancelled) {
        ui_text_source_set(network_status, status == 0 ? "Network: loaded" : status == 2 ? "Network: timeout" : "Network: failed");
    }
    ui_completion_release(token);
}
EMSCRIPTEN_KEEPALIVE const unsigned char* sf_pixels() { uint32_t w, h; return ui_host_pixels(app, &w, &h); }
EMSCRIPTEN_KEEPALIVE int sf_render_mode(unsigned mode) { return ui_host_set_render_mode(app, mode); }
EMSCRIPTEN_KEEPALIVE const uint8_t* sf_draw_commands(uint32_t* size)
{
    const uint8_t* bytes = nullptr;
    if (ui_host_draw_commands(app, &bytes, size) != UI_OK) { *size = 0; return nullptr; }
    return bytes;
}
EMSCRIPTEN_KEEPALIVE int sf_select_view(unsigned id) { return ui_host_select_view(app, id); }
EMSCRIPTEN_KEEPALIVE int sf_semantic_action(const char* id, unsigned action) { return ui_host_semantic_action(app, std::strtoull(id, nullptr, 10), action); }
EMSCRIPTEN_KEEPALIVE const char* sf_snapshot()
{
    static std::string json;
    size_t size = 0;
    if (ui_host_snapshot(app, nullptr, &size) != UI_OK) { return "{\"views\":[]}"; }
    json.resize(size);
    if (ui_host_snapshot(app, json.data(), &size) != UI_OK) { return "{\"views\":[]}"; }
    return json.c_str();
}
EMSCRIPTEN_KEEPALIVE int sf_resize(unsigned w, unsigned h, unsigned dpi) { return ui_host_resize(app, w, h, dpi); }
EMSCRIPTEN_KEEPALIVE int sf_pointer(int action, int x, int y) { return ui_host_pointer(app, ui_pointer_action(action), x, y); }
EMSCRIPTEN_KEEPALIVE int sf_key(int key, int shift) { return ui_host_key_event(app, ui_host_key(key), shift); }
EMSCRIPTEN_KEEPALIVE int sf_focus(int value) { return ui_host_focus(app, value); }
EMSCRIPTEN_KEEPALIVE int sf_scroll(int x, int y, int dy) { return ui_host_scroll(app, x, y, dy); }
EMSCRIPTEN_KEEPALIVE int sf_context_menu(int x, int y) { return ui_host_context_menu(app, x, y); }
EMSCRIPTEN_KEEPALIVE void* sf_alloc(unsigned size) { return std::malloc(size); }
EMSCRIPTEN_KEEPALIVE void sf_free(void* data) { std::free(data); }
EMSCRIPTEN_KEEPALIVE int sf_text_update(unsigned session, unsigned revision, const char* text, unsigned size, unsigned first, unsigned last, unsigned backward)
{ return ui_host_text_update(app, session, revision, text, size, first, last, backward); }
EMSCRIPTEN_KEEPALIVE int sf_text_preview(unsigned session, unsigned revision, const char* text, unsigned size, unsigned first, unsigned last, unsigned backward, unsigned cs, unsigned ce)
{
    const ui_host_composition_range range{sizeof(range), 1, first, last, backward, cs, ce};
    return ui_host_text_preview(app, session, revision, text, size, &range);
}
EMSCRIPTEN_KEEPALIVE int sf_text_hit(unsigned session, unsigned revision, int x, int y)
{
    unsigned offset = 0; const auto result = ui_host_text_hit_test(app, session, revision, x, y, &offset);
    return result == UI_OK ? int(offset) : -int(result);
}
EMSCRIPTEN_KEEPALIVE int sf_text_scroll(unsigned session, unsigned revision, int dx, int dy)
{ return ui_host_text_scroll(app, session, revision, dx, dy); }
EMSCRIPTEN_KEEPALIVE const char* sf_text_read()
{
    ui_host_text_state s{}; s.struct_size = sizeof(s); s.version = 1;
    const auto result = ui_host_text_read(app, &s);
    if (result != UI_OK) { text_state_json = "{\"error\":" + std::to_string(result) + "}"; return text_state_json.c_str(); }
    if (!s.active) { return "null"; }
    char fields[1024];
    std::snprintf(fields, sizeof(fields), "{\"session\":%u,\"revision\":%u,\"multiline\":%u,\"password\":%u,\"left\":%d,\"top\":%d,\"right\":%d,\"bottom\":%d,\"clipLeft\":%d,\"clipTop\":%d,\"clipRight\":%d,\"clipBottom\":%d,\"selectionStart\":%u,\"selectionEnd\":%u,\"selectionBackward\":%u,\"foreground\":%u,\"background\":%u,\"fontSize\":%u,\"textLeft\":%d,\"textTop\":%d,\"textRight\":%d,\"textBottom\":%d,\"lineHeight\":%u,\"caretX\":%d,\"caretY\":%d,\"scrollX\":%d,\"scrollY\":%d,\"text\":\"",
        s.session, s.revision, s.multiline, s.password, s.left, s.top, s.right, s.bottom, s.clip_left, s.clip_top, s.clip_right, s.clip_bottom,
        s.selection_start, s.selection_end, s.selection_backward, s.foreground, s.background, s.font_size, s.text_left, s.text_top, s.text_right, s.text_bottom, s.line_height, s.caret_x, s.caret_y, s.scroll_x, s.scroll_y);
    text_state_json = fields;
    for (unsigned i = 0; i < s.text_size; ++i) {
        const auto c = static_cast<unsigned char>(s.text[i]);
        if (c == '"' || c == '\\') { text_state_json += '\\'; text_state_json += char(c); }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); text_state_json += escaped; }
        else { text_state_json += char(c); }
    }
    text_state_json += "\"}";
    return text_state_json.c_str();
}
EMSCRIPTEN_KEEPALIVE unsigned sf_count() { return count; }
EMSCRIPTEN_KEEPALIVE void sf_stop()
{
    if (app) {
        sunfleck_web_debug_bind(nullptr);
        ui_app_destroy(app);
        app = nullptr;
        text = nullptr;
    }
}
}
