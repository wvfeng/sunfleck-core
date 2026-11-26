#pragma once

#include <sunfleck/ui.h>
#include <array>
#include <cstdint>

inline constexpr unsigned glass_opacity = 190;

struct SwitchShape {
    int width, height, off_thumb, on_thumb, pressed_thumb, stroke;
    bool light_thumb;
    bool operator==(const SwitchShape&) const = default;
};

struct ScrollBarTemplate {
    ui_theme_color track, thumb, pressed;
    int radius, inset;
    std::uint8_t track_opacity = 255, thumb_opacity = 255;
    bool operator==(const ScrollBarTemplate&) const = default;
};

struct ThemeShape {
    int button_radius, input_radius, card_radius, popup_radius;
    bool glass;
    SwitchShape toggle;
    int slider_thumb_radius;
    bool slider_ring;
    ScrollBarTemplate scroll_bar{};
    int navigation_item_radius = 0;
    bool operator==(const ThemeShape&) const = default;
};

struct BuiltinTheme {
    const char* name;
    std::uint32_t colors[2][UI_THEME_COLOR_COUNT];
    ThemeShape shape;
};

inline constexpr BuiltinTheme builtin_themes[] = {
    {"Fluent 2",
     {{0xfafafaU, 0xf0f0f0U, 0xffffffU, 0x242424U, 0x0e5daaU, 0xffffffU, 0xcde5fbU, 0xf5f5f5U, 0x005a9eU, 0x727272U, 0x8a8a8aU},
      {0x202020U, 0x292929U, 0x333333U, 0xf5f5f5U, 0x91c9ffU, 0x102235U, 0x294762U, 0x303030U, 0x91c9ffU, 0x999999U, 0x101010U}},
     {4, 4, 8, 8, false, {40, 20, 12, 12, 14, 1, false}, 7, true,
      {UI_THEME_CONTROL, UI_THEME_DIVIDER, UI_THEME_TEXT, 3, 2}}},
    {"Material 3",
     {{0xfff7ffU, 0xf0e8f5U, 0xf8effaU, 0x211a25U, 0x67508fU, 0xffffffU, 0xeaddffU, 0xf2e9f5U, 0x563b82U, 0x79717fU, 0x9b919fU},
      {0x17121bU, 0x241e29U, 0x302737U, 0xf3e8f5U, 0xd6baffU, 0x362053U, 0x493360U, 0x302737U, 0xd6baffU, 0xa99fb0U, 0x0e0a11U}},
     {64, 8, 16, 20, false, {52, 32, 16, 24, 28, 2, false}, 10, false,
      {UI_THEME_PANEL, UI_THEME_DIVIDER, UI_THEME_PRIMARY, 8, 1}}},
    {"Radix Themes",
     {{0xfcfcfeU, 0xf2f3f8U, 0xffffffU, 0x202439U, 0x3e52baU, 0xffffffU, 0xe2e6fdU, 0xf5f6faU, 0x394db4U, 0x757b92U, 0x999bacU},
      {0x181a24U, 0x212431U, 0x282c3aU, 0xf0f1faU, 0xb2bfffU, 0x202752U, 0x343e68U, 0x2b2e3cU, 0xb2bfffU, 0x9299b2U, 0x10111aU}},
     {6, 6, 10, 10, false, {35, 20, 18, 18, 18, 1, true}, 7, true,
      {UI_THEME_PANEL, UI_THEME_DIVIDER, UI_THEME_PRIMARY, 6, 2}}},
    {"Apple Liquid Glass",
     {{0xf5f7fcU, 0xeaf1fdU, 0xffffffU, 0x19263aU, 0x075cafU, 0xffffffU, 0xd8e8feU, 0xf0f5ffU, 0x0057a9U, 0x6a7a94U, 0x8997acU},
      {0x161d2cU, 0x26334bU, 0x252e40U, 0xf4f7ffU, 0x9bcaffU, 0x132742U, 0x35496bU, 0x29364cU, 0x9bcaffU, 0x95a6c1U, 0x0f1520U}},
     {20, 10, 12, 20, true, {38, 22, 18, 18, 18, 1, true}, 0, false,
      {UI_THEME_PANEL, UI_THEME_DIVIDER, UI_THEME_PRIMARY, 8, 2, 160, 224}, 8}},
    {"Sunfleck Lite",
     {{0xfbf8f2U, 0xf0eae0U, 0xfffdf8U, 0x2c241dU, 0x884900U, 0xffffffU, 0xf3dfbeU, 0xf6efe4U, 0x854500U, 0x827467U, 0x9e9285U},
      {0x211c17U, 0x2d251dU, 0x352d24U, 0xf9efe1U, 0xf3ba68U, 0x342000U, 0x503921U, 0x3a3025U, 0xf3ba68U, 0xa89580U, 0x110e0bU}},
     {5, 4, 6, 8, false, {32, 16, 10, 10, 12, 1, false}, 0, false,
      {UI_THEME_CONTROL, UI_THEME_DIVIDER, UI_THEME_PRIMARY, 2, 1}}},
};
static_assert(std::size(builtin_themes) == UI_BUILTIN_THEME_COUNT);

constexpr ui_theme builtin_palette(ui_builtin_theme theme, bool dark, bool compact)
{
    ui_theme palette{sizeof(ui_theme), {}, compact ? 6U : 10U,
        compact ? 28U : 36U, 16U, compact ? 8U : 12U};
    for (unsigned i = 0; i < UI_THEME_COLOR_COUNT; ++i) {
        palette.colors[i] = builtin_themes[theme].colors[dark][i];
    }
    return palette;
}

constexpr std::array<std::uint8_t, 4> theme_rgb(std::uint32_t rgb)
{
    return {static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
            static_cast<std::uint8_t>(rgb), 255};
}
