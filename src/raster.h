#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "affine.h"

struct FillRectCommand {
    int left, top, right, bottom;
    std::array<std::uint8_t, 4> color;
    bool software_layer = false;
};

enum class PixelOrder { Rgba, Bgra };

void rasterize(std::vector<std::uint8_t>& pixels, int width, int height,
               std::span<const FillRectCommand> commands, PixelOrder order = PixelOrder::Rgba);

namespace drawing {
void transform(std::vector<FillRectCommand>& commands, size_t first, const ui_matrix& matrix, layout::Rect clip);
void composite_opacity(std::vector<FillRectCommand>& commands, size_t first, double opacity);
// Shared coverage geometry: CPU and GPU consume the same antialiased spans.
void rounded(std::vector<FillRectCommand>& commands, layout::Rect area, layout::Rect clip,
             std::array<std::uint8_t, 4> color, int radius, int stroke = 0, bool square_bottom = false);
// Clip a complete surface once, so overlapping children share one antialiased edge.
void clip_rounded(std::vector<FillRectCommand>& commands, size_t first, layout::Rect area, int radius);
void glass(std::vector<FillRectCommand>& commands, layout::Rect area, layout::Rect clip,
           std::array<std::uint8_t, 4> tint, std::array<std::uint8_t, 4> edge,
           unsigned dpi, int radius);
}
