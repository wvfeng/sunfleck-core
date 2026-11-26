#include <sunfleck/elements.h>
#include <stdio.h>
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "Check failed at line %d\n", __LINE__); return 1; } } while (0)
static int score = 40;
static ui_result get(void *context, ui_binding_value *value) {
    (void)context; value->kind = UI_VALUE_INT; value->integer = score; return UI_OK;
}
static ui_result set(void *context, const ui_binding_value *value) {
    (void)context; score = (int)value->integer; return UI_OK;
}
int main(void) {
    ui_binding_value parsed = {0};
    double coordinates[6] = {0};
    CHECK(ui_value_parse(UI_VALUE_MATRIX, "1,0,0,1,3.5,-2", &parsed, coordinates, 6) == UI_OK);
    CHECK(parsed.geometry == coordinates && coordinates[4] == 3.5 && coordinates[5] == -2);
    CHECK(ui_value_parse(UI_VALUE_MATRIX, "1,0,0,1,NaN,0", &parsed, coordinates, 6) == UI_ERROR_INVALID_ARGUMENT);
    CHECK(coordinates[4] == 3.5 && parsed.kind == UI_VALUE_MATRIX);
    CHECK(ui_value_parse(UI_VALUE_POINT, "1,2", &parsed, coordinates, 1) == UI_ERROR_INVALID_ARGUMENT);
    ui_app *app = NULL;
    ui_app_options options = {sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
    CHECK(ui_app_create(&options, &app) == UI_OK);
    CHECK(&ui_TextBlock_IdProperty == &ui_Image_IdProperty);
    CHECK(&ui_StackPanel_PaddingProperty == &ui_Grid_PaddingProperty);
    ui_Border border;
    CHECK(ui_Border_create(app, &border) == UI_OK);
    CHECK(ui_Border_BackgroundProperty.kind == UI_VALUE_COLOR && ui_Border_BorderThicknessProperty.kind == UI_VALUE_THICKNESS);
    CHECK(ui_value_parse(UI_VALUE_THICKNESS, "1,2,3,4", &parsed, coordinates, 6) == UI_OK);
    CHECK(ui_node_set_property(ui_Border_node(&border), ui_Border_BorderThicknessProperty.target, &parsed) == UI_OK);
    CHECK(ui_node_get_property(ui_Border_node(&border), UI_PROPERTY_BORDER_THICKNESS, &parsed) == UI_OK && parsed.geometry[2] == 3);
    parsed.kind = UI_VALUE_ENUM; parsed.integer = UI_HIDDEN;
    CHECK(ui_node_set_property(ui_Border_node(&border), ui_Border_VisibilityProperty.target, &parsed) == UI_OK);
    CHECK(ui_node_get_property(ui_Border_node(&border), UI_PROPERTY_VISIBILITY, &parsed) == UI_OK && parsed.integer == UI_HIDDEN);
    CHECK(ui_Border_FontSizeProperty.kind == UI_VALUE_DOUBLE && ui_Border_FontWeightProperty.kind == UI_VALUE_ENUM);
    parsed.kind = UI_VALUE_ENUM; parsed.integer = UI_FONT_BOLD;
    CHECK(ui_node_set_property(ui_Border_node(&border), ui_Border_FontWeightProperty.target, &parsed) == UI_OK);
    CHECK(ui_node_get_property(ui_Border_node(&border), UI_PROPERTY_FONT_WEIGHT, &parsed) == UI_OK && parsed.integer == UI_FONT_BOLD);
    ui_Slider slider;
    CHECK(ui_Slider_create(app, &slider) == UI_OK);
    ui_model *model = NULL;
    CHECK(ui_model_create(app, &model) == UI_OK);
    ui_model_property property = {sizeof(property), 1, "Score", UI_VALUE_INT, get, set, NULL};
    CHECK(ui_model_add_property(model, &property) == UI_OK);
    CHECK(ui_Slider_ValueProperty.kind == UI_VALUE_INT && ui_Slider_ValueProperty.two_way);
    CHECK(ui_element_bind(ui_Slider_node(&slider), &ui_Slider_ValueProperty, model, "Score", UI_BIND_DEFAULT) == UI_OK);
    uint32_t value = 0;
    CHECK(ui_node_get_value(ui_Slider_node(&slider), &value) == UI_OK && value == 40);
    CHECK(ui_node_update_source(ui_Slider_node(&slider), UI_PROPERTY_VALUE) == UI_OK);
    score = 75;
    CHECK(ui_model_notify_property_changed(model, "Score") == UI_OK);
    CHECK(ui_node_get_value(ui_Slider_node(&slider), &value) == UI_OK && value == 75);
    CHECK(ui_element_bind(ui_Slider_node(&slider), &ui_Slider_HeightProperty, model, "Score", UI_BIND_DEFAULT) == UI_OK);
    int32_t height = -1;
    CHECK(ui_node_get_height(ui_Slider_node(&slider), &height) == UI_OK && height == 75);
    score = 24;
    CHECK(ui_model_notify_property_changed(model, "Score") == UI_OK);
    CHECK(ui_node_get_height(ui_Slider_node(&slider), &height) == UI_OK && height == 24);
    CHECK(ui_element_bind(ui_Slider_node(&slider), &ui_StackPanel_PaddingProperty, model, "Score", UI_BIND_DEFAULT) == UI_ERROR_INVALID_ARGUMENT);
    ui_property_descriptor descriptor = {0};
    descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
    CHECK(ui_property_describe(UI_SLIDER, UI_PROPERTY_HEIGHT, &descriptor) == UI_OK);
    CHECK(descriptor.default_value.kind == UI_VALUE_LENGTH && descriptor.default_value.integer == UI_LENGTH_AUTO && (descriptor.flags & UI_PROPERTY_AFFECTS_MEASURE));
    ui_property_source source;
    CHECK(ui_node_get_property_source(ui_Slider_node(&slider), UI_PROPERTY_HEIGHT, &source) == UI_OK && source == UI_PROPERTY_SOURCE_BINDING);
    CHECK(ui_node_clear_property(ui_Slider_node(&slider), UI_PROPERTY_HEIGHT) == UI_OK);
    ui_binding_value current = {0};
    CHECK(ui_node_get_property(ui_Slider_node(&slider), UI_PROPERTY_HEIGHT, &current) == UI_OK && current.kind == UI_VALUE_LENGTH && current.integer == UI_LENGTH_AUTO);
    CHECK(ui_node_get_property_source(ui_Slider_node(&slider), UI_PROPERTY_HEIGHT, &source) == UI_OK && source == UI_PROPERTY_SOURCE_DEFAULT);
    ui_app_destroy(app);
    return 0;
}
