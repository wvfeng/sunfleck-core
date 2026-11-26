#include "showcase.h"

#include <stdio.h>
#include <stdlib.h>

static ui_result load_resource(ui_app* app, const char* path)
{
    const size_t capacity = 9 * 1024 * 1024 + 1;
    char* data = malloc(capacity);
    if (!data) {
        return UI_ERROR_OUT_OF_MEMORY;
    }
    FILE* file = fopen(path, "rb");
    if (!file) {
        free(data);
        return UI_ERROR_INVALID_ARGUMENT;
    }
    const size_t length = fread(data, 1, capacity, file);
    const int failed = ferror(file);
    fclose(file);
    ui_result status = UI_ERROR_INVALID_ARGUMENT;
    if (!failed && length > 0) {
        /* A truncated resource must fail without preventing a subsequent load. */
        if (ui_app_load_fleck_resource(app, data, length - 1, NULL) == UI_ERROR_INVALID_ARGUMENT) {
            status = ui_app_load_fleck_resource(app, data, length, NULL);
        }
    }
    free(data);
    return status;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        return 1;
    }
    const ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    ui_app* app = NULL;
    showcase_nodes nodes = {0};
    int result = 1;
    if (ui_app_create(&options, &app) == UI_OK && load_resource(app, argv[1]) == UI_OK &&
        showcase_fleck_handles(app, &nodes) == UI_OK) {
        result = showcase_run(app, &nodes, 1);
    }
    ui_app_destroy(app);
    return result;
}
