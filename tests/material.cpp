#include "../src/raster.h"
#include "../src/theme.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#define CHECK(value) do { if (!(value)) { throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #value); } } while (false)

namespace {
double luminance(std::array<uint8_t, 4> rgb)
{
    const auto linear = [](uint8_t value) {
        const double channel = value / 255.0;
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(rgb[0]) + 0.7152 * linear(rgb[1]) + 0.0722 * linear(rgb[2]);
}
void curves(unsigned dpi)
{
    const auto px = [=](int value) { return static_cast<int>((value * dpi + 48) / 96); };
    const int width = px(100), height = px(70);
    const layout::Rect area{px(10), px(10), px(90), px(60)}, clip{0, 0, width, height};
    for (int thickness : {0, px(1), px(3)}) {
        std::vector<FillRectCommand> commands{{0, 0, width, height, {0, 0, 0, 255}}};
        drawing::rounded(commands, area, clip, {255, 255, 255, 255}, px(12), thickness);
        std::vector<uint8_t> pixels; rasterize(pixels, width, height, commands);
        const auto sample = [&](int x, int y) { return pixels[(static_cast<size_t>(y) * width + x) * 4]; };
        unsigned fringe = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                CHECK(sample(x, y) == sample(width - x - 1, y));
                CHECK(sample(x, y) == sample(x, height - y - 1));
                fringe += sample(x, y) > 0 && sample(x, y) < 255;
            }
        }
        CHECK(fringe > 8); CHECK(sample(area.left, area.top) == 0);
        CHECK(sample(width / 2, area.top) == 255);
        CHECK(sample(area.left, height / 2) == 255);
        CHECK(sample(width / 2, height / 2) == (thickness ? 0 : 255));
        for (const auto& command : commands) { CHECK(command.left >= clip.left && command.right <= clip.right); }
    }
}
void material(bool dark)
{
    const auto palette = builtin_palette(UI_BUILTIN_LIQUID_GLASS, dark, true);
    const layout::Rect bounds{20, 20, 180, 76}, clip{0, 0, 200, 100};
    const auto render = [&](int pattern) {
        std::vector<FillRectCommand> commands;
        for (int x = 0; x < 200; ++x) {
            const auto value = static_cast<uint8_t>(pattern < 2 ? pattern * 255 : (x % 2) * 255);
            commands.push_back({x, 0, x + 1, 100, {value, value, value, 255}});
        }
        drawing::glass(commands, bounds, clip, theme_rgb(palette.colors[UI_THEME_PANEL]), theme_rgb(palette.colors[UI_THEME_DIVIDER]), 96, 20);
        std::vector<uint8_t> pixels; rasterize(pixels, 200, 100, commands); return pixels;
    };
    const auto black = render(0), white = render(1), stripes = render(2);
    const auto pixel = [](const auto& pixels, int x, int y) { return pixels[(y * 200 + x) * 4]; };
    // The surface reacts to its real backdrop, and filters high-frequency detail.
    CHECK(pixel(white, 100, 48) - pixel(black, 100, 48) > 30);
    CHECK(std::abs(pixel(stripes, 100, 48) - pixel(stripes, 101, 48)) < 5);
    CHECK(pixel(stripes, 10, 48) == 0 && pixel(stripes, 11, 48) == 255);
    CHECK(pixel(black, bounds.left, bounds.top) == 0);
    // Complete rounded ends, a vertical sheen, and a visible lower edge.
    CHECK(pixel(white, 21, 48) == pixel(white, 178, 48));
    CHECK(pixel(white, 21, 48) != pixel(white, 25, 48));
    CHECK(pixel(white, 100, 25) != pixel(white, 100, 69));
    CHECK(pixel(white, 100, 73) != pixel(white, 100, 69));
    // Test text against the rendered material, including tint, blur and sheen.
    const double foreground = luminance(theme_rgb(palette.colors[UI_THEME_TEXT]));
    for (const auto* image : {&black, &white, &stripes}) {
        for (int y : {28, 48, 65}) {
            const size_t offset = (y * 200 + 100) * 4;
            const double background = luminance({(*image)[offset], (*image)[offset + 1], (*image)[offset + 2], 255});
            CHECK((std::max(foreground, background) + 0.05) / (std::min(foreground, background) + 0.05) >= 4.5);
        }
    }
}
void clipped_surface(unsigned dpi)
{
    const int radius = 8 * dpi / 96;
    const layout::Rect area{10, 10, 100, 70};
    const FillRectCommand backdrop{0, 0, 110, 80, {20, 30, 40, 255}};
    std::vector<FillRectCommand> commands{backdrop, {10, 10, 100, 70, {80, 90, 100, 255}},
        {10, 10, 100, 35, {150, 160, 170, 255}}};
    drawing::clip_rounded(commands, 1, area, radius);
    CHECK(commands.front().color == backdrop.color);
    std::vector<uint8_t> pixels; rasterize(pixels, 110, 80, commands);
    const auto value = [&](int x, int y) { return pixels[(y * 110 + x) * 4]; };
    CHECK(value(10, 10) == 20); CHECK(value(99, 10) == 20);
    CHECK(value(10, 69) == 20); CHECK(value(99, 69) == 20);
    CHECK(value(11, 40) == 80); CHECK(value(50, 11) == 150);
    // Overlapping opaque children must receive coverage once, not one mask each.
    std::vector<FillRectCommand> expected{backdrop};
    drawing::rounded(expected, area, area, {150, 160, 170, 255}, radius);
    std::vector<uint8_t> reference; rasterize(reference, 110, 80, expected);
    for (int y = 10; y < 10 + radius; ++y) {
        for (int x = 10; x < 100; ++x) { CHECK(value(x, y) == reference[(y * 110 + x) * 4]); }
    }
}
}
int main()
{
    try {
        for (unsigned dpi : {96u, 144u, 192u}) { curves(dpi); clipped_surface(dpi); }
        material(false); material(true); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
