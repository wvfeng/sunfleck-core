#include "model.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    const ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    CounterModel counter = {0};
    ui_app* app = NULL;
    ui_model* model = NULL;
    char* json = NULL;
    FILE* file = NULL;
    size_t size = 0;
    int status = 1;
    if (argc != 2 || ui_app_create(&options, &app) != UI_OK) { return 1; }
    if (ui_model_create(app, &model) != UI_OK || counter_register(model, &counter) != UI_OK ||
        ui_model_describe(model, "CCounter", NULL, &size) != UI_OK) { goto done; }
    json = (char*)malloc(size);
    if (!json || ui_model_describe(model, "CCounter", json, &size) != UI_OK || counter.reads) { goto done; }
    file = fopen(argv[1], "wb");
    if (!file) { goto done; }
    status = fprintf(file, "{\"version\":1,\"models\":[%s]}\n", json) < 0;
    if (fclose(file) != 0) { status = 1; }
    file = NULL;
done:
    if (file) { fclose(file); }
    free(json);
    if (model) { ui_model_destroy(model); }
    ui_app_destroy(app);
    return status;
}
