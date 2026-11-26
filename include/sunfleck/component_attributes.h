#pragma once
#include <sunfleck/ui.h>
/* Framework-owned Fleck attributes, not custom registered properties. */
typedef struct ui_component_attribute { const char *name; ui_value_kind kind; uint32_t bindable; } ui_component_attribute;
static const ui_component_attribute ui_framework_element_DataContextProperty = {"DataContext", UI_VALUE_MODEL, 1};
static const ui_component_attribute ui_control_TemplateProperty = {"Template", UI_VALUE_STRING, 0};
