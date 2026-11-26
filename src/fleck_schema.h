#pragma once

#include "theme.h"
#include <span>
#include <string_view>

// Shared declarative values used by the native parser and language-service schema.
namespace fleck::schema {
inline constexpr std::string_view booleans[] = {"true", "false"};
inline constexpr std::string_view bases[] = {"Fluent2", "Material3", "Radix", "LiquidGlass", "SunfleckLite"};
inline constexpr std::string_view parts[] = {"Title", "Tab", "Close", "Pin", "Float", "Splitter", "Indicator", "Preview",
    "AutoHide", "Menu", "MenuItem", "WindowTitle", "WindowMinimize", "WindowMaximize", "WindowClose"};
inline constexpr std::string_view states[] = {"Normal", "Active", "Hover", "Pressed", "Disabled"};
inline constexpr unsigned state_flags[] = {0, UI_PART_ACTIVE, UI_PART_HOVER, UI_PART_PRESSED, UI_PART_DISABLED};
inline constexpr std::string_view template_names[] = {"Fill", "Border", "Text", "Button", "Stack"};
inline constexpr std::string_view roles[] = {"Close", "Pin", "Float", "Menu", "Drag"};
inline constexpr std::string_view orientations[] = {"Horizontal", "Vertical"};
inline constexpr std::string_view namespaces[] = {"urn:sunfleck:v1"};
inline constexpr std::string_view versions[] = {"1"};
inline constexpr std::string_view colors[] = {"background", "panel", "card", "text", "primary", "on_primary",
    "selected", "control", "focus", "divider", "scrim"};
static_assert(std::size(colors) == UI_THEME_COLOR_COUNT);
static_assert(std::size(parts) == UI_DOCK_PART_COUNT);
static_assert(std::size(bases) == UI_BUILTIN_THEME_COUNT);

struct Attribute {
    std::string_view name, type = "string";
    std::span<const std::string_view> values{};
    int minimum = -1, maximum = -1;
    bool required = false;
};
inline constexpr Attribute window_attributes[] = {
    {"xmlns", "string", namespaces, -1, -1, true}, {"Model"}, {"Title"},
    {"Width", "integer", {}, 1, 4096}, {"Height", "integer", {}, 1, 4096}
};
inline constexpr Attribute component_attributes[] = {
    {"xmlns", "string", namespaces, -1, -1, true}, {"Class", "string", {}, -1, -1, true},
    {"Namespace", "string", {}, -1, -1, true}, {"Model"}, {"TargetType"}
};
inline constexpr Attribute theme_attributes[] = {
    {"xmlns", "string", namespaces}, {"Version", "integer", versions, 1, 1}, {"Base", "string", bases},
    {"Dark", "bool", booleans}, {"Compact", "bool", booleans}, {"Skin"}
};
inline constexpr Attribute resource_attributes[] = {{"Key", "string", {}, -1, -1, true}, {"Value", "string", {}, -1, -1, true}};
inline constexpr Attribute dock_template_attributes[] = {{"Part", "string", parts, -1, -1, true}, {"State", "string", states}};
inline constexpr Attribute template_attributes[] = {
    {"Background"}, {"Foreground"}, {"Text"}, {"Role", "string", roles}, {"Orientation", "string", orientations},
    {"Radius", "integer", {}, 0, 128}, {"Thickness", "integer", {}, 1, 32},
    {"Width", "integer", {}, 1, 1024}, {"Height", "integer", {}, 1, 1024}, {"Padding", "integer", {}, 0, 64}
};
struct Metric {
    std::string_view name;
    std::uint32_t ui_theme::*field;
    std::uint32_t low, high;
};
inline constexpr Metric palette_metrics[] = {
    {"gap", &ui_theme::gap, 0, 64}, {"control_height", &ui_theme::control_height, 16, 128},
    {"window_padding", &ui_theme::window_padding, 0, 64}, {"gallery_gap", &ui_theme::gallery_gap, 0, 64}
};
inline constexpr Attribute dock_metrics[] = {
    {"dock.title_height", "integer", {}, 1, 1024}, {"dock.tab_height", "integer", {}, 1, 1024},
    {"dock.splitter_width", "integer", {}, 1, 1024}, {"dock.rail_width", "integer", {}, 1, 1024},
    {"dock.radius", "integer", {}, 0, 1024}, {"dock.auto_hide_width", "integer", {}, 1, 1024},
    {"dock.auto_hide_height", "integer", {}, 1, 1024}, {"dock.window_title_height", "integer", {}, 1, 1024},
    {"dock.floating_padding", "integer", {}, 0, 64}
};
inline constexpr Attribute shape_metrics[] = {
    {"button_radius", "integer", {}, 0, 128}, {"input_radius", "integer", {}, 0, 128},
    {"card_radius", "integer", {}, 0, 128}, {"popup_radius", "integer", {}, 0, 128}
};
inline constexpr const Attribute* find(std::span<const Attribute> attributes, std::string_view name)
{
    for (const auto& attribute : attributes) {
        if (attribute.name == name) { return &attribute; }
    }
    return nullptr;
}
}
