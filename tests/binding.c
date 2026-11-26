#include <sunfleck/ui.h>
#include <stdio.h>
#include <string.h>

typedef struct counter {
    ui_text_source* source;
    unsigned count;
    ui_result status;
} counter;

static void increment(ui_node* button, void* context)
{
    (void)button;
    counter* state = context;
    char text[48];
    snprintf(text, sizeof(text), "Count: %u", ++state->count);
    state->status = ui_text_source_set(state->source, text);
}

static int has_text(ui_node* label, const char* expected)
{
    char text[48];
    uint32_t size = sizeof(text);
    return ui_node_get_text(label, text, &size) == UI_OK && strcmp(text, expected) == 0;
}

static int verify(ui_app* app, ui_node* label, ui_command* command, counter* state)
{
    if (ui_app_poll(app, 0) != UI_OK || !has_text(label, "Count: 0") ||
        ui_app_activate_button(app) != UI_OK || state->status != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !has_text(label, "Count: 1") ||
        ui_text_source_set(state->source, "Count: 1") != UI_OK || ui_app_poll(app, 0) != UI_TIMEOUT ||
        ui_command_destroy(command) != UI_OK || ui_app_activate_button(app) != UI_OK ||
        state->count != 1 || ui_app_poll(app, 0) != UI_OK ||
        ui_node_set_text(label, "Literal") != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        ui_text_source_set(state->source, "Unused") != UI_OK || ui_app_poll(app, 0) != UI_TIMEOUT ||
        !has_text(label, "Literal") || ui_node_bind_text(label, state->source, UI_BIND_ONE_WAY) != UI_OK ||
        !has_text(label, "Unused") || ui_text_source_destroy(state->source) != UI_OK ||
        ui_app_poll(app, 0) != UI_OK || !has_text(label, "")) {
        return 1;
    }
    return 0;
}

static int verify_modes(ui_app* app)
{
    ui_node *snapshot = NULL, *editor = NULL, *mirror = NULL;
    ui_text_source* source = NULL;
    const char* initial = "\xf0\x9f\x8c\xb1";
    return ui_node_create(app, UI_TEXT_BLOCK, &snapshot) != UI_OK ||
        ui_node_create(app, UI_TEXT_BOX, &editor) != UI_OK ||
        ui_node_create(app, UI_TEXT_AREA, &mirror) != UI_OK ||
        ui_text_source_create(app, initial, &source) != UI_OK ||
        ui_node_bind_text(snapshot, source, UI_BIND_ONE_TIME) != UI_OK ||
        ui_node_bind_text(editor, source, UI_BIND_TWO_WAY) != UI_OK ||
        ui_node_bind_text(mirror, source, UI_BIND_TWO_WAY) != UI_OK ||
        !has_text(editor, initial) ||
        ui_text_source_set(source, "Changed") != UI_OK ||
        !has_text(snapshot, initial) || !has_text(editor, "Changed") || !has_text(mirror, "Changed") ||
        ui_text_source_set(source, "Line 1\nLine 2") != UI_ERROR_INVALID_ARGUMENT ||
        !has_text(editor, "Changed") || !has_text(mirror, "Changed") ||
        ui_node_bind_text(editor, NULL, UI_BIND_TWO_WAY) != UI_OK ||
        ui_text_source_set(source, "Line 1\nLine 2") != UI_OK ||
        !has_text(editor, "Changed") || !has_text(mirror, "Line 1\nLine 2") ||
        ui_text_source_destroy(source) != UI_OK ||
        !has_text(snapshot, initial) || !has_text(mirror, "Line 1\nLine 2");
}

int main(int argc, char** argv)
{
    const int headless = argc == 2 && strcmp(argv[1], "--test") == 0;
    const ui_app_options options = {sizeof(options), UI_API_VERSION,
        headless ? UI_SOFTWARE : UI_AUTO, headless ? UI_HEADLESS : 0};
    ui_app* app = NULL;
    ui_node *root = NULL, *label = NULL, *button = NULL;
    ui_command* command = NULL;
    counter state = {0};
    int result = 1;
    if (ui_app_create(&options, &app) == UI_OK &&
        ui_node_create(app, UI_STACK_PANEL, &root) == UI_OK &&
        ui_node_create(app, UI_TEXT_BLOCK, &label) == UI_OK &&
        ui_node_create(app, UI_BUTTON, &button) == UI_OK &&
        ui_node_set_text(button, "Increment") == UI_OK &&
        ui_text_source_create(app, "Count: 0", &state.source) == UI_OK &&
        ui_command_create(app, increment, &state, &command) == UI_OK &&
        ui_node_bind_text(label, state.source, UI_BIND_ONE_WAY) == UI_OK &&
        ui_node_bind_command(button, command) == UI_OK &&
        ui_node_append(root, label) == UI_OK && ui_node_append(root, button) == UI_OK &&
        ui_app_set_root(app, root) == UI_OK && ui_app_open_window(app) == UI_OK) {
        result = headless ? (verify(app, label, command, &state) || verify_modes(app)) : ui_app_run(app) != UI_EXITED;
    }
    ui_app_destroy(app);
    return result;
}
