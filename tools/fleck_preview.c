#include <sunfleck/ui.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SUNFLECK_PREVIEW_SHOWCASE
#include "showcase.h"
#endif

int main(int argc, char** argv)
{
    static const char* const renderers[] = {"auto", "gles3", "software", "d3d11", "metal", "angle"};
    ui_renderer renderer = UI_AUTO;
    if (argc > 1 && strncmp(argv[1], "--renderer=", 11) == 0) {
        const char* name = argv[1] + 11;
        size_t index = 0;
        while (index < sizeof(renderers) / sizeof(renderers[0]) && strcmp(name, renderers[index]) != 0) {
            ++index;
        }
        if (index == sizeof(renderers) / sizeof(renderers[0])) {
            fprintf(stderr, "Unknown renderer: %s\n", name);
            return 1;
        }
        renderer = (ui_renderer)index;
        --argc;
        ++argv;
    }
    const int test = argc == 3 && strcmp(argv[1], "--test") == 0;
#ifdef SUNFLECK_PREVIEW_SHOWCASE
    const int showcase_test_mode = argc == 3 && strcmp(argv[1], "--showcase-test") == 0;
    const int showcase = showcase_test_mode || (argc == 3 && strcmp(argv[1], "--showcase") == 0);
    const int headless = test || showcase_test_mode;
    if (argc != 2 && !headless && !showcase) {
        fputs("Usage: fleck_preview [--renderer=auto|d3d11|angle|gles3|metal|software] [--showcase | --test | --showcase-test] page.fleck\n", stderr);
#else
    const int headless = test;
    if (argc != 2 && !headless) {
        fputs("Usage: fleck_preview [--renderer=auto|d3d11|angle|gles3|metal|software] [--test] page.fleck\n", stderr);
#endif
        return 1;
    }
    setlocale(LC_CTYPE, "");
    FILE* file = fopen(argv[argc - 1], "rb");
    if (!file) {
        perror(argv[argc - 1]);
        return 1;
    }
    const size_t limit = 1024 * 1024 + 1;
    char* text = malloc(limit);
    if (!text) {
        fclose(file);
        return 1;
    }
    const size_t length = fread(text, 1, limit, file);
    const int failed = ferror(file);
    fclose(file);
    if (failed) {
        free(text);
        return 1;
    }
    const ui_app_options options = {sizeof(options), UI_API_VERSION,
        renderer, headless ? UI_HEADLESS : UI_SHOW_RENDERER};
    ui_app* app = NULL;
    ui_result status = ui_app_create(&options, &app);
    if (status == UI_OK) {
        ui_fleck_diagnostic diagnostic;
        status = ui_app_load_fleck(app, text, length, &diagnostic);
        if (status != UI_OK) {
            fprintf(stderr, "%s:%u:%u: Fleck %d: %s\n", argv[argc - 1],
                diagnostic.line, diagnostic.column, diagnostic.code, diagnostic.message);
        }
    }
    free(text);
#ifdef SUNFLECK_PREVIEW_SHOWCASE
    if (status == UI_OK && showcase) {
        showcase_nodes nodes = {0};
        const int result = showcase_fleck_handles(app, &nodes) == UI_OK
            ? showcase_run(app, &nodes, headless) : 1;
        ui_app_destroy(app);
        return result;
    }
#endif
    if (status == UI_OK) {
        status = ui_app_open_window(app);
    }
    if (status == UI_OK) {
        status = headless ? ui_app_poll(app, 0) : ui_app_run(app);
    }
    if (headless && status == UI_OK) {
        uint32_t width = 0, height = 0;
        if (!ui_app_pixels(app, &width, &height) || width != 400 || height != 240) {
            status = UI_ERROR_PLATFORM;
        }
    }
    ui_app_destroy(app);
    return status == (headless ? UI_OK : UI_EXITED) ? 0 : 1;
}
