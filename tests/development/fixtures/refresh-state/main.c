#include <sunfleck/ui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Application state is owned here, not serialized by the UI framework. */
typedef struct ApplicationState { char* draft; unsigned restored; } ApplicationState;
static ui_result preserve_state(ui_app* app, ui_fleck_refresh_phase phase, ui_result result, void* context)
{
    ApplicationState* state = (ApplicationState*)context;
    ui_node* editor = NULL;
    ui_result status = ui_app_find_fleck_node(app, "draft", &editor);
    if (status != UI_OK) { return status; }
    if (phase == UI_FLECK_REFRESH_CAPTURE) {
        uint32_t size = 0;
        free(state->draft); state->draft = NULL;
        if ((status = ui_node_get_text(editor, NULL, &size)) != UI_OK) { return status; }
        state->draft = (char*)malloc(size);
        if (!state->draft) { return UI_ERROR_OUT_OF_MEMORY; }
        return ui_node_get_text(editor, state->draft, &size);
    }
    if (result == UI_OK) {
        status = ui_node_set_text(editor, state->draft);
        if (status == UI_OK) { ++state->restored; }
    }
    free(state->draft); state->draft = NULL;
    return status;
}
int main(void)
{
    const ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    const char* before = "<Window xmlns='urn:sunfleck:v1'><TextBox Id='draft'/></Window>";
    const char* after = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='draft'/></Window>";
    ui_app* app = NULL;
    ui_node* editor = NULL;
    ApplicationState state = {0};
    char text[64] = {0}; uint32_t size = sizeof(text);
    int result = 1;
    if (ui_app_create(&options, &app) != UI_OK) { return 1; }
    if (ui_app_load_fleck(app, before, strlen(before), NULL) != UI_OK ||
        ui_app_find_fleck_node(app, "draft", &editor) != UI_OK ||
        ui_node_set_text(editor, "Application-owned draft") != UI_OK || ui_app_open_window(app) != UI_OK ||
        ui_app_set_fleck_refresh_state_callback(app, preserve_state, &state) != UI_OK ||
        ui_app_queue_fleck_refresh(app, after, strlen(after), NULL) != UI_OK) { goto done; }
    {
        const ui_result poll = ui_app_poll(app, 0);
        if (poll != UI_OK && poll != UI_TIMEOUT) { goto done; }
    }
    if (ui_app_find_fleck_node(app, "draft", &editor) != UI_OK || ui_node_get_text(editor, text, &size) != UI_OK ||
        strcmp(text, "Application-owned draft") != 0 || state.restored != 1) { goto done; }
    puts("C restored application state onto the replacement node");
    result = 0;
done:
    free(state.draft);
    ui_app_destroy(app);
    return result;
}
