#pragma once
#include <sunfleck/ui.h>

/* These declarations are shared by runtime registration and the metadata
   exporter. Neither path needs to instantiate business objects. */
#ifdef __cplusplus
#include <sunfleck/components.hpp>
namespace Example {
class UserCard : public sunfleck::UserControl {
  public:
    using sunfleck::UserControl::UserControl;
};
inline constexpr int user_card_line = __LINE__ - 4;
class Rating : public sunfleck::Control {
  public:
    using sunfleck::Control::Control;
    void onPropertyChanged(const char *name) {
        if (std::string_view(name) == "Value") { raise("Changed", get<int>("Value")); }
    }
};
inline constexpr int rating_line = __LINE__ - 7;
}
#else
/* C expresses inheritance by embedding the base handle as its first member. */
typedef struct Example_UserCard { ui_user_control base; } Example_UserCard;
typedef struct Example_Rating { ui_control base; } Example_Rating;
#endif

static const int example_value_line = __LINE__ + 1;
static ui_component_property example_rating_value(void) {
    static const ui_component_property_metadata metadata = {
        sizeof(ui_component_property_metadata), 1,
        UI_PROPERTY_BINDABLE | UI_PROPERTY_TWO_WAY | UI_PROPERTY_ANIMATABLE | UI_PROPERTY_AFFECTS_RENDER,
        NULL, NULL, NULL};
    ui_component_property value = {0};
    value.name = "Value";
    value.default_value.kind = UI_VALUE_INT;
    value.default_mode = UI_BIND_TWO_WAY;
    value.metadata = &metadata;
    return value;
}
static ui_component_type example_rating_type(void) {
    static ui_component_property properties[1];
    static const char *events[] = {"Changed"};
    static const ui_template_part parts[] = {{"PART_Value", UI_SLIDER, 1}};
    ui_component_type type = {0};
    properties[0] = example_rating_value();
    type.struct_size = sizeof(type);
    type.version = UI_COMPONENT_TYPE_VERSION;
    type.namespace_uri = "urn:example:controls";
    type.name = "Rating";
    type.template_namespace = "urn:example:controls";
    type.template_name = "RatingTemplate";
    type.properties = properties;
    type.property_count = 1;
    type.events = events;
    type.event_count = 1;
    type.parts = parts;
    type.part_count = 1;
    return type;
}
static ui_component_type example_user_card_type(void) {
    ui_component_type type = {0};
    type.struct_size = sizeof(type);
    type.version = UI_COMPONENT_TYPE_VERSION;
    type.namespace_uri = "urn:example:controls";
    type.name = "UserCard";
    return type;
}
