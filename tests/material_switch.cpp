#include "../src/ui_tree.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// Check the emitted geometry at desktop DPI values, including compact/dark themes.
// Material 3's iconless switch uses a 52x32 track and 16/24/28 handles.
static void verify(unsigned dpi, bool dark, bool compact, unsigned state)
{
    const auto style = builtin_palette(UI_BUILTIN_MATERIAL_3, dark, compact);
    const auto shape = builtin_themes[UI_BUILTIN_MATERIAL_3].shape;
    ui_node node{nullptr, UI_SWITCH};
    node.value = state == 1;
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> interactive, scrollable;
    constexpr int width = 400, height = 160;
    render_tree(node, {0, 0, width, height}, style, shape, dpi, nullptr, false, false,
                nullptr, state == 2 ? &node : nullptr, commands, interactive, scrollable);
    const auto scale = [dpi](int value) { return static_cast<int>((value * dpi + 48) / 96); };
    const auto track = theme_rgb(style.colors[state == 1 ? UI_THEME_PRIMARY : UI_THEME_DIVIDER]);
    layout::Rect bounds{width, height, 0, 0};
    for (const auto& command : commands) {
        if (command.color[0] == track[0] && command.color[1] == track[1] && command.color[2] == track[2]) {
            bounds.left = std::min(bounds.left, command.left);
            bounds.top = std::min(bounds.top, command.top);
            bounds.right = std::max(bounds.right, command.right);
            bounds.bottom = std::max(bounds.bottom, command.bottom);
        }
    }
    if (bounds.right - bounds.left != scale(52) || bounds.bottom - bounds.top != scale(32)) {
        throw std::runtime_error("Material switch track must remain 52x32 logical pixels");
    }
    std::vector<uint8_t> pixels;
    rasterize(pixels, width, height, commands);
    const int radius = (bounds.bottom - bounds.top) / 2;
    const auto* corner = pixels.data() + (bounds.top * width + bounds.left) * 4;
    const auto* edge = corner + radius * 4;
    bool antialiased = false;
    for (const auto* pixel = corner; pixel < edge; pixel += 4) {
        if (!std::equal(corner, corner + 4, pixel) && !std::equal(edge, edge + 4, pixel)) {
            antialiased = true;
            break;
        }
    }
    if (!antialiased) {
        throw std::runtime_error("Material switch track corners must be antialiased");
    }
    const auto thumb = theme_rgb(style.colors[state == 1 ? UI_THEME_ON_PRIMARY : UI_THEME_DIVIDER]);
    const auto background = theme_rgb(style.colors[state == 1 ? UI_THEME_PRIMARY : UI_THEME_CONTROL]);
    const int y = (bounds.top + bounds.bottom) / 2;
    const int center = (bounds.left + bounds.right) / 2;
    int handle_pixels = 0, left_handle_pixels = 0;
    const int stroke = scale(2);
    for (int x = bounds.left + stroke; x < bounds.right - stroke; ++x) {
        const auto* pixel = pixels.data() + (y * width + x) * 4;
        int thumb_distance = 0, background_distance = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            thumb_distance += std::abs(int(pixel[channel]) - int(thumb[channel]));
            background_distance += std::abs(int(pixel[channel]) - int(background[channel]));
        }
        // Curved edges are antialiased; classify by the nearest palette color.
        if (thumb_distance < background_distance) {
            ++handle_pixels;
            left_handle_pixels += x < center;
        }
    }
    if (handle_pixels != scale(state == 2 ? 28 : state == 1 ? 24 : 16)) {
        throw std::runtime_error("Material switch handle diameter differs from the iconless specification");
    }
    if ((left_handle_pixels > handle_pixels / 2) != (state != 1)) {
        throw std::runtime_error("Material switch handle is on the wrong side of the track");
    }
}

int main()
{
    try {
        for (unsigned dpi : {96u, 144u, 192u}) {
            for (bool dark : {false, true}) {
                for (bool compact : {false, true}) {
                    for (unsigned state : {0u, 1u, 2u}) {
                        verify(dpi, dark, compact, state);
                    }
                }
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
