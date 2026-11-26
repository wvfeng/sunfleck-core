#include <sunfleck/ui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int reject(ui_app* app, const char* input, ui_fleck_error expected, unsigned line)
{
    ui_fleck_diagnostic diagnostic;
    const ui_result status = ui_app_load_fleck(app, input, strlen(input), &diagnostic);
    const int rejected = status == UI_ERROR_INVALID_ARGUMENT &&
        diagnostic.code == expected && diagnostic.line == line && diagnostic.column > 0 &&
        diagnostic.message[0] != 0;
    if (!rejected) fprintf(stderr, "Reject failed: result=%d diagnostic=%d at %u:%u expected=%d at %u: %s\nInput: %s\n",
        status, diagnostic.code, diagnostic.line, diagnostic.column, expected, line, diagnostic.message, input);
    return rejected;
}

static int verify(ui_app* app)
{
    if (!reject(app, "<!DOCTYPE Window SYSTEM 'https://example.org/x'><Window/>",
                UI_FLECK_FORBIDDEN, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'>\n<Button Content='&external;'/></Window>",
                UI_FLECK_FORBIDDEN, 2) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'>\n<Missing/></Window>", UI_FLECK_ELEMENT, 2) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'>\n<Button Unknown='a'/></Window>",
                UI_FLECK_ATTRIBUTE, 2) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1' Width='wide'><Button/></Window>",
                UI_FLECK_VALUE, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'><StackPanel></Grid></Window>",
                UI_FLECK_SYNTAX, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'><Button Content='\xff'/></Window>",
                UI_FLECK_SYNTAX, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'><ListView><Button/></ListView></Window>",
                UI_FLECK_ELEMENT, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'><Panel><Label Anchor='absent'/></Panel></Window>",
                UI_FLECK_VALUE, 1) ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1'><Panel><Button Id='same'/><Button Id='same'/></Panel></Window>",
                UI_FLECK_VALUE, 1)) {
        return 1;
    }
    char deep[4096] = "<Window xmlns='urn:sunfleck:v1'>";
    for (int i = 0; i < 64; ++i) {
        strcat(deep, "<StackPanel>");
    }
    if (!reject(app, deep, UI_FLECK_LIMIT, 1)) {
        return 2;
    }
    const size_t limit = 1024 * 1024 + 1;
    char* large = malloc(limit);
    ui_fleck_diagnostic diagnostic;
    if (!large) {
        return 3;
    }
    const ui_result status = ui_app_load_fleck(app, large, limit, &diagnostic);
    free(large);
    if (status != UI_ERROR_INVALID_ARGUMENT || diagnostic.code != UI_FLECK_LIMIT) {
        return 3;
    }
    const char valid[] = "\xef\xbb\xbf<?xml version='1.0' encoding='UTF-8'?>"
        "<!-- example --><Window xmlns='urn:sunfleck:v1' Width='400' Height='240'>"
        "<StackPanel Spacing='12' Padding='16'><TextBlock Text='A &amp; &#x4e2d;'/>"
        "<Button Content='Continue'/></StackPanel></Window>";
    if (ui_app_load_fleck(app, valid, sizeof(valid) - 1, &diagnostic) != UI_OK ||
        diagnostic.code != UI_FLECK_NONE ||
        !reject(app, "<Window xmlns='urn:sunfleck:v1' Width='wide'><Button/></Window>",
                UI_FLECK_VALUE, 1) ||
        ui_app_open_window(app) != UI_OK || ui_app_poll(app, 0) != UI_OK) {
        return 4;
    }
    uint32_t width = 0, height = 0;
    const uint8_t* pixels = ui_app_pixels(app, &width, &height);
    if (!pixels || width != 400 || height != 240 ||
        pixels[(100 * width + 35) * 4] != 136 ||
        ui_app_load_fleck(app, valid, sizeof(valid) - 1, &diagnostic) != UI_ERROR_INVALID_STATE) {
        return 5;
    }
    return 0;
}

static void count_action(ui_node* node, void* context)
{
    (void)node;
    ++*(unsigned*)context;
}

static int text_is(ui_node* node, const char* expected)
{
    char text[32];
    uint32_t size = sizeof(text);
    return ui_node_get_text(node, text, &size) == UI_OK && strcmp(text, expected) == 0;
}

static ui_result load_with_context(ui_app* app, const void* page, size_t size, int binary,
                                   const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    return binary ? ui_app_load_fleck_resource_with_context(app, page, size, context, diagnostic)
        : ui_app_load_fleck_with_context(app, page, size, context, diagnostic);
}

static int verify_bindings(ui_app* app, const void* page, size_t size, int binary)
{
    ui_fleck_diagnostic diagnostic;
    ui_text_source* source = NULL;
    ui_command* command = NULL;
    ui_node *label = NULL, *button = NULL;
    const char* text_name = binary ? "count" : "CountText";
    const char* command_name = binary ? "increment" : "IncrementCount";
    unsigned calls = 0, override_calls = 0, first_calls = 0;
    ui_result status = binary ? ui_app_load_fleck_resource(app, page, size, &diagnostic)
        : ui_app_load_fleck(app, page, size, &diagnostic);
    if (status != UI_ERROR_INVALID_ARGUMENT || diagnostic.code != UI_FLECK_VALUE ||
        ui_text_source_create(app, "Initial", &source) != UI_OK ||
        ui_command_create(app, count_action, &first_calls, &command) != UI_OK ||
        0) {
        return 6;
    }
    ui_node* first_label = NULL;
    ui_text_source* first_source = source;
    const ui_fleck_text_export first_text_exports[] = {{text_name, first_source}};
    const ui_fleck_command_export first_command_exports[] = {{command_name, command}};
    const ui_fleck_data_context first_context = {first_text_exports, 1, first_command_exports, 1};
    if (load_with_context(app, page, size, binary, &first_context, &diagnostic) != UI_OK ||
        ui_app_find_fleck_node(app, "count", &first_label) != UI_OK || !text_is(first_label, "Initial")) {
        return 9;
    }

    if (ui_text_source_create(app, "Second", &source) != UI_OK ||
        ui_command_create(app, count_action, &calls, &command) != UI_OK) {
        return 9;
    }
    const ui_fleck_text_export text_exports[] = {{text_name, source}};
    const ui_fleck_command_export command_exports[] = {{command_name, command}};
    const ui_fleck_data_context context = {text_exports, 1, command_exports, 1};
    const ui_fleck_data_context empty = {0};
    ui_node* current = NULL;
    if (load_with_context(app, page, size, binary, &context, &diagnostic) != UI_OK ||
        ui_app_find_fleck_node(app, "count", &label) != UI_OK ||
        ui_app_find_fleck_node(app, "increment", &button) != UI_OK ||
        !text_is(label, "Second") || !text_is(first_label, "Initial") ||
        load_with_context(app, page, size, binary, &empty, &diagnostic) != UI_ERROR_INVALID_ARGUMENT ||
        diagnostic.code != UI_FLECK_VALUE ||
        ui_app_find_fleck_node(app, "count", &current) != UI_OK || current != label ||
        ui_text_source_set(first_source, "First changed") != UI_OK ||
        !text_is(first_label, "First changed") || !text_is(label, "Second") ||
        ui_text_source_set(source, "Updated") != UI_OK || !text_is(label, "Updated") ||
        ui_app_open_window(app) != UI_OK || ui_app_poll(app, 0) != UI_OK ||
        ui_app_activate_button(app) != UI_OK || calls != 1 || first_calls != 0 ||
        ui_node_set_action(button, count_action, &override_calls) != UI_OK ||
        ui_app_activate_button(app) != UI_OK || calls != 1 || first_calls != 0 || override_calls != 1 ||
        ui_node_set_text(label, "Literal") != UI_OK ||
        ui_text_source_set(source, "Ignored") != UI_OK || !text_is(label, "Literal")) {
        return 7;
    }
    return 0;
}

static int verify_resource(ui_app* app, const char* path)
{
    unsigned char page[4096];
    FILE* file = fopen(path, "rb");
    if (!file) {
        return 8;
    }
    const size_t size = fread(page, 1, sizeof(page), file);
    const int failed = ferror(file);
    fclose(file);
    return failed || size == sizeof(page) ? 8 : verify_bindings(app, page, size, 1);
}

int main(int argc, char** argv)
{
    const ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* app = NULL;
    if (ui_app_create(&options, &app) != UI_OK) {
        return 1;
    }
    int result = argc == 2 ? verify_resource(app, argv[1]) : verify(app);
    ui_app_destroy(app);
    if (result || argc == 2) {
        return result;
    }

    const char page[] = "<Window xmlns='urn:sunfleck:v1'><StackPanel>"
        "<TextBlock Id='count' Text='{Binding CountText, Mode=OneWay}'/>"
        "<Button Id='increment' Command='{Binding IncrementCount}'/></StackPanel></Window>";
    if (ui_app_create(&options, &app) != UI_OK) {
        return 1;
    }
    result = verify_bindings(app, page, sizeof(page) - 1, 0);
    ui_app_destroy(app);
    return result;
}
