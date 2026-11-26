#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../controls.h"
ui_result load_reusable_with_model(ui_app *, ui_model *,
                                   const ui_fleck_data_context *,
                                   ui_fleck_diagnostic *);
typedef struct State {
    const char *name;
    int score;
    ui_model *primary;
    ui_model *secondary;
} State;
static ui_result name_get(void *context, ui_binding_value *value) {
    value->kind = UI_VALUE_STRING;
    value->string = ((State *)context)->name;
    return UI_OK;
}
static ui_result score_get(void *context, ui_binding_value *value) {
    value->kind = UI_VALUE_INT;
    value->integer = ((State *)context)->score;
    return UI_OK;
}
static ui_result score_set(void *context, const ui_binding_value *value) {
    ((State *)context)->score = (int)value->integer;
    return UI_OK;
}
static ui_result model_get(void *context, ui_binding_value *value) {
    value->kind = UI_VALUE_MODEL;
    value->model = *(ui_model **)context;
    return UI_OK;
}
#define OK(call)                                                               \
    do {                                                                       \
        if ((call) != UI_OK) {                                                 \
            fprintf(stderr, "Failed at line %d\n", __LINE__);                  \
            return 1;                                                          \
        }                                                                      \
    } while (0)
static void rating_changed(ui_component *component, const char *name,
                           void *context) {
    (void)context;
    if (strcmp(name, "Value") == 0) {
        ui_binding_value value;
        if (ui_component_get_property(component, name, &value) == UI_OK) {
            (void)ui_component_raise(component, "Changed", &value);
        }
    }
}
ui_result register_rating(ui_app *app) {
    uint32_t registered = 0;
    ui_result status = ui_app_has_component_type(app, "urn:example:controls",
                                                 "Rating", &registered);
    if (status != UI_OK || registered) {
        return status;
    }
    ui_component_type type = example_rating_type();
    type.changed = rating_changed;
    return ui_app_register_component_type(app, &type);
}
static char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    const long length = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    *size = (size_t)length;
    char *bytes = (char *)malloc(*size);
    if (bytes && fread(bytes, 1, *size, file) != *size) {
        free(bytes);
        bytes = NULL;
    }
    fclose(file);
    return bytes;
}
int main(int argc, char **argv) {
    int test = argc > 1 && strcmp(argv[1], "--test") == 0;
    ui_app *app = NULL;
    ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE,
                              test ? UI_HEADLESS : 0};
    OK(ui_app_create(&options, &app));
    State first = {"Alice", 0, NULL, NULL}, second = {"Bob", 0, NULL, NULL},
          model = {NULL, 40, NULL, NULL};
    ui_model *properties = NULL;
    OK(ui_model_create(app, &model.primary));
    OK(ui_model_create(app, &model.secondary));
    OK(ui_model_create(app, &properties));
    ui_model_property p = {sizeof(p), 1,    "Name", UI_VALUE_STRING,
                           name_get,  NULL, &first};
    OK(ui_model_add_property(model.primary, &p));
    p.context = &second;
    OK(ui_model_add_property(model.secondary, &p));
    p.path = "PrimaryUser";
    p.kind = UI_VALUE_MODEL;
    p.get = model_get;
    p.context = &model.primary;
    OK(ui_model_add_property(properties, &p));
    p.path = "SecondaryUser";
    p.context = &model.secondary;
    OK(ui_model_add_property(properties, &p));
    p.path = "Score";
    p.kind = UI_VALUE_INT;
    p.get = score_get;
    p.set = score_set;
    p.context = &model;
    OK(ui_model_add_property(properties, &p));
    ui_fleck_diagnostic diagnostic;
    ui_result loaded =
        load_reusable_with_model(app, properties, NULL, &diagnostic);
    if (loaded != UI_OK) {
        fprintf(stderr, "%s\n", diagnostic.message);
        return 1;
    }
    if (test && argc > 2) {
        size_t size;
        if (argc > 3) {
            char *resource = read_file(argv[2], &size);
            if (!resource) {
                return 3;
            }
            loaded = ui_app_load_fleck_resource_with_model(
                app, resource, size, properties, NULL, &diagnostic);
            free(resource);
            if (loaded != UI_OK) {
                fprintf(stderr, "Resource: %s\n", diagnostic.message);
                return 1;
            }
        }
        char *map = read_file(argv[argc > 3 ? 3 : 2], &size);
        if (!map) {
            return 3;
        }
        ui_result map_status = ui_app_attach_fleck_source_map(
            app, map, size, &diagnostic);
        free(map);
        if (map_status != UI_OK) {
            fprintf(stderr, "Source map: %s\n", diagnostic.message);
            return 4;
        }
    }
    ui_node *node = NULL;
    ui_component *rating = NULL;
    OK(ui_app_find_fleck_node(app, "rating", &node));
    OK(ui_node_get_component(node, &rating));
    OK(ui_component_apply_template(rating, "urn:example:controls",
                                   "CompactRatingTemplate", &diagnostic));
    ui_binding_value value = {0};
    value.kind = UI_VALUE_INT;
    value.integer = 70;
    OK(ui_component_set_property(rating, "Value", &value));
    if (model.score != 70) {
        return 2;
    }
    OK(ui_app_open_window(app));
    if (test) {
        OK(ui_app_request_close(app));
    } else {
        while (ui_app_poll(app, 100) != UI_EXITED) {
        }
    }
    ui_app_destroy(app);
    return 0;
}
