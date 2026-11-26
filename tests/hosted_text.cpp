#include <sunfleck/ui.h>
#include "../src/input/hosted_text.h"
#include <cstdio>
#include <cstring>
#include <string>

#define REQUIRE(v) do { if (!(v)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #v); return 1; } } while (false)
struct Model { std::string value = "Initial"; unsigned writes = 0; ui_app* app = nullptr; ui_result reentry = UI_OK; };
static ui_result preview(ui_app* app, uint32_t session, uint32_t revision, const char* text, uint32_t size,
    uint32_t first, uint32_t last, uint32_t backward, uint32_t cs, uint32_t ce)
{
    const ui_host_composition_range range{sizeof(range), 1, first, last, backward, cs, ce};
    return ui_host_text_preview(app, session, revision, text, size, &range);
}
int main()
{
    const std::string unicode = "A\xf0\x9f\x8c\xb1\xe4\xb8\xad"; // A + supplementary scalar + CJK
    const auto native = hosted_text::decode(unicode);
    REQUIRE(hosted_text::encode(native) == unicode);
    REQUIRE(hosted_text::utf16_offset(native, native.size()) == 4);
    REQUIRE(hosted_text::native_offset(native, 3) == (sizeof(wchar_t) == 2 ? 3u : 2u));
    bool rejected = false;
    try { (void)hosted_text::native_offset(native, 2); } catch (...) { rejected = true; }
    REQUIRE(rejected);
    ui_app* app = nullptr;
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    REQUIRE(ui_app_create(&options, &app) == UI_OK);
    ui_node* editor = nullptr;
    REQUIRE(ui_node_create(app, UI_TEXT_BOX, &editor) == UI_OK);
    ui_model* model = nullptr;
    Model data;
    data.app = app;
    REQUIRE(ui_model_create(app, &model) == UI_OK);
    const ui_model_property property{sizeof(property), 1, "Text", UI_VALUE_STRING,
        [](void* raw, ui_binding_value* value) { *value = {}; value->kind = UI_VALUE_STRING; value->string = static_cast<Model*>(raw)->value.c_str(); return UI_OK; },
        [](void* raw, const ui_binding_value* value) {
            auto& m = *static_cast<Model*>(raw); m.value = value->string; ++m.writes;
            ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
            m.reentry = ui_host_text_read(m.app, &state);
            return UI_OK;
        }, &data};
    REQUIRE(ui_model_add_property(model, &property) == UI_OK);
    ui_binding_options binding{sizeof(binding), 1, UI_PROPERTY_TEXT, "Text", UI_BIND_TWO_WAY, UI_UPDATE_PROPERTY_CHANGED, nullptr};
    REQUIRE(ui_node_bind_property(editor, model, &binding) == UI_OK);
    REQUIRE(ui_app_set_root(app, editor) == UI_OK);
    REQUIRE(ui_app_open_window(app) == UI_OK);
    REQUIRE(ui_host_resize(app, 400, 240, 96) == UI_OK);
    REQUIRE(ui_host_focus(app, 1) == UI_OK);
    REQUIRE(ui_host_key_event(app, UI_HOST_TAB, 0) == UI_OK);
    ui_host_text_state state{}; state.struct_size = sizeof(state); state.version = 1;
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.active);
    REQUIRE(std::string(state.text, state.text_size) == "Initial");
    REQUIRE(state.text_left > state.left && state.text_right < state.right);
    REQUIRE(state.text_top >= state.top && state.text_bottom <= state.bottom);
    REQUIRE(state.line_height > 0 && state.line_height <= unsigned(state.bottom - state.text_top));
    const auto session = state.session, revision = state.revision;
#ifdef _WIN32
    const char* combining = "e\xcc\x81";
    REQUIRE(ui_host_text_update(app, session, revision, combining, 3, 1, 1, 0) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(preview(app, session, revision, combining, 3, 2, 2, 0, 1, 2) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.revision == revision);
    REQUIRE(data.value == "Initial" && data.writes == 0);
#endif
    REQUIRE(ui_host_text_preview(app, session, revision, "x", 1, nullptr) == UI_ERROR_INVALID_ARGUMENT);
    const ui_host_composition_range unsupported_range{sizeof(unsupported_range), 2, 0, 1, 0, 0, 1};
    REQUIRE(ui_host_text_preview(app, session, revision, "x", 1, &unsupported_range) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(preview(app, session, revision, "Draft", 5, 5, 5, 0, 0, 5) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK);
    REQUIRE(std::string(state.text, state.text_size) == "Initial" && state.revision == revision);
    REQUIRE(data.value == "Initial" && data.writes == 0);
    REQUIRE(preview(app, session, revision, unicode.data(), unsigned(unicode.size()), 3, 3, 0, 2, 3) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_text_update(app, session, revision, "Initial", 7, 0, 0, 0) == UI_OK);
    REQUIRE(data.writes == 0);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK);
    REQUIRE(ui_host_text_update(app, session, state.revision, unicode.data(), unsigned(unicode.size()), 1, 3, 1) == UI_OK);
    REQUIRE(data.value == unicode && data.writes == 1);
    REQUIRE(data.reentry == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_text_update(app, session, revision, "stale", 5, 0, 0, 0) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK);
    REQUIRE(state.selection_start == 1 && state.selection_end == 3 && state.selection_backward == 1);
    REQUIRE(ui_app_poll(app, 0) == UI_OK); // Reading geometry cannot consume presentation.
    REQUIRE(ui_app_poll(app, 0) == UI_TIMEOUT);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, unicode.data(), unsigned(unicode.size()), 2, 2, 0) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, "bad\nline", 8, 0, 0, 0) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, "\xed\xa0\x80", 3, 0, 0, 0) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, "x\0y", 3, 0, 0, 0) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(data.writes == 1);
    const auto before_external = state.revision;
    REQUIRE(preview(app, session, before_external, "pending", 7, 7, 7, 0, 0, 7) == UI_OK);
    data.value = "External";
    REQUIRE(ui_model_notify_property_changed(model, "Text") == UI_OK);
    REQUIRE(ui_host_text_update(app, state.session, before_external, "late", 4, 0, 0, 0) == UI_ERROR_INVALID_STATE);
    REQUIRE(preview(app, session, before_external, "late", 4, 4, 4, 0, 0, 4) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && std::string(state.text) == "External");
    uint32_t hit = 0;
    REQUIRE(ui_host_text_hit_test(app, state.session, state.revision, state.text_left, state.text_top, &hit) == UI_OK);
    REQUIRE(hit == 0);
    REQUIRE(ui_host_text_scroll(app, state.session, state.revision, 0, 32769) == UI_ERROR_INVALID_ARGUMENT);
    binding.trigger = UI_UPDATE_LOST_FOCUS;
    REQUIRE(ui_node_bind_property(editor, model, &binding) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, "Deferred", 8, 8, 8, 0) == UI_OK);
    REQUIRE(data.value == "External" && data.writes == 1);
    REQUIRE(ui_host_focus(app, 0) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && !state.active);
    REQUIRE(data.value == "Deferred" && data.writes == 2);
    REQUIRE(ui_host_focus(app, 1) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.active && state.session != session);
    REQUIRE(data.writes == 2);
    const auto retired_session = state.session, retired_revision = state.revision;
    ui_node* replacement = nullptr;
    REQUIRE(ui_node_create(app, UI_TEXT_AREA, &replacement) == UI_OK);
    REQUIRE(ui_app_set_root(app, replacement) == UI_OK);
    REQUIRE(ui_host_key_event(app, UI_HOST_TAB, 0) == UI_OK);
    REQUIRE(ui_host_text_update(app, retired_session, retired_revision, "retired", 7, 0, 0, 0) == UI_ERROR_INVALID_STATE);
    REQUIRE(preview(app, retired_session, retired_revision, "retired", 7, 7, 7, 0, 0, 7) == UI_ERROR_INVALID_STATE);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && state.multiline);
    REQUIRE(ui_host_text_update(app, state.session, state.revision, "a\nb", 3, 3, 3, 0) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && std::string(state.text) == "a\nb");
    REQUIRE(ui_node_set_enabled(replacement, 0) == UI_OK);
    REQUIRE(ui_host_text_read(app, &state) == UI_OK && !state.active);
    ui_app_destroy(app);
    return 0;
}
