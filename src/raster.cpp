#include "raster.h"

#include <algorithm>
#include <cstring>

void rasterize(std::vector<std::uint8_t>& pixels, int width, int height,
               std::span<const FillRectCommand> commands, PixelOrder order)
{
    const auto stride = static_cast<std::size_t>(width) * 4;
    pixels.assign(stride * height, 0);
    for (const auto& rect : commands) {
        const int left = std::max(rect.left, 0);
        const int right = std::min(rect.right, width);
        const int top = std::max(rect.top, 0);
        const int bottom = std::min(rect.bottom, height);
        if (left >= right || top >= bottom) {
            continue;
        }
        auto color = rect.color;
        if (order == PixelOrder::Bgra) {
            std::swap(color[0], color[2]);
        }
        auto* first = pixels.data() + stride * top + static_cast<std::size_t>(left) * 4;
        const auto row_size = static_cast<std::size_t>(right - left) * 4;
        if (color[3] != 255) {
            const unsigned alpha = color[3];
            for (int y = top; y < bottom; ++y) {
                auto* row = first + stride * (y - top);
                for (std::size_t offset = 0; offset < row_size; offset += 4) {
                    auto* pixel = row + offset;
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        pixel[channel] = static_cast<std::uint8_t>(
                            (color[channel] * alpha + pixel[channel] * (255 - alpha) + 127) / 255);
                    }
                    pixel[3] = static_cast<std::uint8_t>(alpha + (pixel[3] * (255 - alpha) + 127) / 255);
                }
            }
            continue;
        }
        for (std::size_t offset = 0; offset < row_size; offset += 4) {
            std::memcpy(first + offset, color.data(), 4);
        }
        for (int y = top + 1; y < bottom; ++y) {
            std::memcpy(first + stride * (y - top), first, row_size);
        }
    }
}

void drawing::transform(std::vector<FillRectCommand>& commands, size_t first,
                        const ui_matrix& matrix, layout::Rect clip)
{
    const auto inverse = affine::inverse(matrix);
    std::vector<FillRectCommand> source(commands.begin()+first,commands.end());
    commands.resize(first);
    if (!inverse) return;
    for (const auto& rect : source) {
        auto b = affine::bounds(matrix,{rect.left,rect.top,rect.right,rect.bottom});
        layout::Rect dest = layout::Box{std::max(b.left,double(clip.left)),std::max(b.top,double(clip.top)),
                                        std::min(b.right,double(clip.right)),std::min(b.bottom,double(clip.bottom))};
        if (dest.left >= dest.right || dest.top >= dest.bottom) continue;
        if (matrix.m12 == 0 && matrix.m21 == 0) {
            commands.push_back({dest.left,dest.top,dest.right,dest.bottom,rect.color,rect.software_layer});
            continue;
        }
        for (int y=dest.top; y<dest.bottom; ++y) {
            int start = dest.right;
            for (int x=dest.left; x<dest.right; ++x) {
                const auto p = affine::map(*inverse,x+0.5,y+0.5);
                if (p.x >= rect.left && p.x < rect.right && p.y >= rect.top && p.y < rect.bottom) {
                    start = std::min(start,x);
                } else if (start < dest.right) {
                    commands.push_back({start,y,x,y+1,rect.color,true});
                    break;
                }
                if (x == dest.right-1 && start < dest.right)
                    commands.push_back({start,y,dest.right,y+1,rect.color,true});
            }
        }
    }
}

void drawing::composite_opacity(std::vector<FillRectCommand>& commands, size_t first, double opacity)
{
    if (first == commands.size() || opacity >= 1) return;
    if (opacity <= 0) { commands.resize(first); return; }
    std::span layer(commands.data()+first,commands.size()-first);
    int left = layer.front().left, top = layer.front().top, right = left, bottom = top;
    for (const auto& c : layer) {
        left = std::min(left,c.left); top = std::min(top,c.top);
        right = std::max(right,c.right); bottom = std::max(bottom,c.bottom);
    }
    if (right <= left || bottom <= top) { commands.resize(first); return; }
    for (auto& rect : layer) { rect.left -= left; rect.right -= left; rect.top -= top; rect.bottom -= top; }
    std::vector<uint8_t> pixels;
    rasterize(pixels,right-left,bottom-top,layer);
    commands.resize(first);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right;) {
            const size_t i = (size_t(y-top)*(right-left)+(x-left))*4;
            const auto* pixel = pixels.data()+i;
            int end = x+1;
            while (end < right && std::memcmp(pixel,pixel+size_t(end-x)*4,4) == 0) ++end;
            if (unsigned alpha = pixel[3]) {
                std::array<uint8_t,4> color{};
                for (int channel=0; channel<3; ++channel)
                    color[channel] = uint8_t(std::min(255u,(pixel[channel]*255u+alpha/2)/alpha));
                color[3] = uint8_t(std::round(alpha*opacity));
                if (color[3]) commands.push_back({x,y,end,y+1,color,true});
            }
            x = end;
        }
    }
}
