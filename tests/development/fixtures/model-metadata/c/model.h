#pragma once
#include <sunfleck/ui.h>

/* This declaration is shared by runtime registration and the metadata exporter. */
typedef struct CounterModel { int64_t count; unsigned reads; } CounterModel;
static ui_result counter_get(void* context, ui_binding_value* value)
{
    CounterModel* model = (CounterModel*)context;
    ++model->reads;
    *value = (ui_binding_value){0};
    value->kind = UI_VALUE_INT;
    value->integer = model->count;
    return UI_OK;
}
static ui_result counter_set(void* context, const ui_binding_value* value)
{
    ((CounterModel*)context)->count = value->integer;
    return UI_OK;
}
static ui_result counter_register(ui_model* model, CounterModel* counter)
{
    const uint32_t property_line = __LINE__ + 2;
    const ui_model_property properties[] = {
        {sizeof(ui_model_property), 1, "Counter.Value", UI_VALUE_INT, counter_get, counter_set, counter}
    };
    ui_result result = ui_model_add_properties(model, properties, sizeof(properties) / sizeof(properties[0]));
    if (result == UI_OK) {
        result = ui_model_set_property_source(model, "Counter.Value", __FILE__, property_line, 1);
    }
    return result;
}
