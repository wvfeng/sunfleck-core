#include "raster.h"
#include "theme.h"
#include <cmath>

namespace drawing {
namespace {
using Rect = layout::Rect;
using Color = std::array<std::uint8_t, 4>;
void span(std::vector<FillRectCommand>& commands, Rect r, Rect clip, Color color)
{
    r = {std::max(r.left, clip.left), std::max(r.top, clip.top), std::min(r.right, clip.right), std::min(r.bottom, clip.bottom)};
    if (color[3] && r.right > r.left && r.bottom > r.top) { commands.push_back({r.left, r.top, r.right, r.bottom, color}); }
}
double edge_at(Rect r, int radius, double y, bool square_bottom)
{
    const double distance = square_bottom ? y - r.top : std::min(y - r.top, r.bottom - y);
    if (distance >= radius || radius <= 0) { return 0; }
    const double dy = radius - std::max(0.0, distance);
    return radius - std::sqrt(std::max(0.0, radius * static_cast<double>(radius) - dy * dy));
}
void interval(std::vector<FillRectCommand>& commands, int y, Rect clip, Color color,
              const std::array<double, 4>& left, const std::array<double, 4>& right)
{
    const int first = std::max(clip.left, static_cast<int>(std::floor(*std::min_element(left.begin(), left.end()))));
    const int last = std::min(clip.right, static_cast<int>(std::ceil(*std::max_element(right.begin(), right.end()))));
    const int solid_left = std::clamp(static_cast<int>(std::ceil(*std::max_element(left.begin(), left.end()))), first, std::max(first, last));
    const int solid_right = std::clamp(static_cast<int>(std::floor(*std::min_element(right.begin(), right.end()))), solid_left, std::max(solid_left, last));
    const auto pixel = [&](int x) {
        double coverage = 0;
        for (unsigned sample = 0; sample < left.size(); ++sample) {
            coverage += std::max(0.0, std::min(x + 1.0, right[sample]) - std::max(static_cast<double>(x), left[sample]));
        }
        auto fringe = color; fringe[3] = static_cast<uint8_t>(std::lround(color[3] * coverage / left.size()));
        span(commands, {x, y, x + 1, y + 1}, clip, fringe);
    };
    for (int x = first; x < solid_left; ++x) { pixel(x); }
    span(commands, {solid_left, y, solid_right, y + 1}, clip, color);
    for (int x = solid_right; x < last; ++x) { pixel(x); }
}
void blur(std::vector<uint8_t>& pixels, int width, int height, int radius)
{
    std::vector<uint8_t> temporary(pixels.size());
    // Three separable box passes approximate a Gaussian without reading platform surfaces.
    for (int pass = 0; pass < 3; ++pass) {
        for (int axis = 0; axis < 2; ++axis) {
            const int lines = axis ? width : height, extent = axis ? height : width;
            const auto index = [=](int line, int position) { return static_cast<size_t>(axis ? position * width + line : line * width + position) * 4; };
            for (int line = 0; line < lines; ++line) {
                unsigned sum[4]{};
                for (int offset = -radius; offset <= radius; ++offset) {
                    const auto source = index(line, std::clamp(offset, 0, extent - 1));
                    for (int channel = 0; channel < 4; ++channel) { sum[channel] += pixels[source + channel]; }
                }
                for (int position = 0; position < extent; ++position) {
                    const auto destination = index(line, position);
                    for (int channel = 0; channel < 4; ++channel) { temporary[destination + channel] = static_cast<uint8_t>((sum[channel] + radius) / (2 * radius + 1)); }
                    const auto removed = index(line, std::clamp(position - radius, 0, extent - 1));
                    const auto added = index(line, std::clamp(position + radius + 1, 0, extent - 1));
                    for (int channel = 0; channel < 4; ++channel) { sum[channel] = sum[channel] - pixels[removed + channel] + pixels[added + channel]; }
                }
            }
            pixels.swap(temporary);
        }
    }
}
}
void rounded(std::vector<FillRectCommand>& commands, Rect area, Rect clip, Color color, int radius, int stroke, bool square_bottom)
{
    if (area.right <= area.left || area.bottom <= area.top || !color[3]) { return; }
    radius = std::clamp(radius, 0, std::min(area.right - area.left, area.bottom - area.top) / 2);
    stroke = std::clamp(stroke, 0, std::min(area.right - area.left, area.bottom - area.top) / 2);
    const int cap = std::max(radius, stroke), bottom_cap = square_bottom ? stroke : cap;
    const Rect middle{area.left, area.top + cap, area.right, area.bottom - bottom_cap};
    if (!stroke) { span(commands, middle, clip, color); }
    else {
        span(commands, {middle.left, middle.top, middle.left + stroke, middle.bottom}, clip, color);
        span(commands, {middle.right - stroke, middle.top, middle.right, middle.bottom}, clip, color);
    }
    const auto row = [&](int y) {
        std::array<double, 4> outer_left{}, outer_right{}, inner_left{}, inner_right{};
        const Rect inner{area.left + stroke, area.top + stroke, area.right - stroke, area.bottom - stroke};
        bool has_inner = stroke && y >= inner.top && y < inner.bottom;
        for (unsigned sample = 0; sample < outer_left.size(); ++sample) {
            const double sy = y + (sample + 0.5) / outer_left.size();
            const auto outer = edge_at(area, radius, sy, square_bottom);
            outer_left[sample] = area.left + outer; outer_right[sample] = area.right - outer;
            if (has_inner) {
                const auto inset = edge_at(inner, std::max(0, radius - stroke), sy, square_bottom);
                inner_left[sample] = inner.left + inset; inner_right[sample] = inner.right - inset;
            }
        }
        if (!has_inner) { interval(commands, y, clip, color, outer_left, outer_right); }
        else {
            interval(commands, y, clip, color, outer_left, inner_left);
            interval(commands, y, clip, color, inner_right, outer_right);
        }
    };
    for (int y = std::max(area.top, clip.top); y < std::min(area.top + cap, clip.bottom); ++y) { row(y); }
    for (int y = std::max(area.bottom - bottom_cap, clip.top); y < std::min(area.bottom, clip.bottom); ++y) { row(y); }
}
void clip_rounded(std::vector<FillRectCommand>& commands, size_t first, Rect area, int radius)
{
    if (area.right <= area.left || area.bottom <= area.top) { return; }
    radius = std::clamp(radius, 0, std::min(area.right - area.left, area.bottom - area.top) / 2);
    if (!radius || first >= commands.size()) { return; }
    std::vector<FillRectCommand> surface(commands.begin() + first, commands.end());
    commands.resize(first);
    // Preserve the straight sections as commands; only the small corner tiles
    // need compositing before applying coverage to avoid dark overlapping edges.
    for (const auto& command : surface) {
        span(commands, {command.left, command.top, command.right, command.bottom},
            {area.left + radius, area.top, area.right - radius, area.bottom}, command.color);
        for (const auto side : {Rect{area.left, area.top + radius, area.left + radius, area.bottom - radius},
                                Rect{area.right - radius, area.top + radius, area.right, area.bottom - radius}}) {
            span(commands, {command.left, command.top, command.right, command.bottom}, side, command.color);
        }
    }
    for (const int y : {area.top, area.bottom - radius}) {
        for (const int x : {area.left, area.right - radius}) {
            const Rect corner{x, y, x + radius, y + radius};
            std::vector<FillRectCommand> tile, mask;
            for (const auto& command : surface) {
                const Rect r{std::max(x, command.left), std::max(y, command.top),
                    std::min(corner.right, command.right), std::min(corner.bottom, command.bottom)};
                if (r.right > r.left && r.bottom > r.top) {
                    tile.push_back({r.left - x, r.top - y, r.right - x, r.bottom - y, command.color});
                }
            }
            rounded(mask, {area.left - x, area.top - y, area.right - x, area.bottom - y},
                {0, 0, radius, radius}, {255, 255, 255, 255}, radius);
            std::vector<uint8_t> pixels, coverage;
            rasterize(pixels, radius, radius, tile); rasterize(coverage, radius, radius, mask);
            for (int row = 0; row < radius; ++row) {
                for (int column = 0; column < radius; ++column) {
                    const size_t offset = (static_cast<size_t>(row) * radius + column) * 4;
                    const unsigned alpha = pixels[offset + 3];
                    if (!alpha || !coverage[offset + 3]) { continue; }
                    Color color{};
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        color[channel] = static_cast<uint8_t>(std::min(255u, (pixels[offset + channel] * 255u + alpha / 2) / alpha));
                    }
                    color[3] = static_cast<uint8_t>((alpha * coverage[offset + 3] + 127) / 255);
                    span(commands, {x + column, y + row, x + column + 1, y + row + 1}, corner, color);
                }
            }
        }
    }
}
void glass(std::vector<FillRectCommand>& commands, Rect area, Rect clip, Color tint, Color edge, unsigned dpi, int radius)
{
    const Rect visible{std::max(area.left, clip.left), std::max(area.top, clip.top), std::min(area.right, clip.right), std::min(area.bottom, clip.bottom)};
    if (visible.right <= visible.left || visible.bottom <= visible.top) { return; }
    const int filter = std::clamp(static_cast<int>((3 * dpi + 48) / 96), 1, 8), margin = filter * 3;
    const Rect sample{visible.left - margin, visible.top - margin, visible.right + margin, visible.bottom + margin};
    const int width = sample.right - sample.left, height = sample.bottom - sample.top;
    // Clip the retained background commands to this surface's sampling region.
    std::vector<FillRectCommand> background;
    for (const auto& command : commands) {
        const Rect r{std::max(sample.left, command.left), std::max(sample.top, command.top), std::min(sample.right, command.right), std::min(sample.bottom, command.bottom)};
        if (r.right > r.left && r.bottom > r.top) { background.push_back({r.left - sample.left, r.top - sample.top, r.right - sample.left, r.bottom - sample.top, command.color}); }
    }
    std::vector<uint8_t> pixels;
    rasterize(pixels, width, height, background); blur(pixels, width, height, filter);
    const bool dark = tint[0] + tint[1] + tint[2] < 384;
    // A restrained shadow stays within the reserved outer bounds.
    rounded(commands, area, clip, {0, 0, 0, static_cast<uint8_t>(dark ? 30 : 18)}, radius);
    const Rect body{area.left + 1, area.top + 1, area.right - 1, area.bottom - 2};
    if (body.right <= body.left || body.bottom <= body.top) { return; }
    const int curve = std::clamp(radius - 1, 0, std::min(body.right - body.left, body.bottom - body.top) / 2);
    for (int y = std::max(body.top, visible.top); y < std::min(body.bottom, visible.bottom); ++y) {
        const double progress = (y - body.top + 0.5) / (body.bottom - body.top);
        const double sheen = dark ? 0.065 * (1 - progress) : 0.18 * (1 - progress);
        const int left = std::max(body.left, visible.left), right = std::min(body.right, visible.right);
        const auto material = [&](int x) {
            const auto offset = (static_cast<size_t>(y - sample.top) * width + x - sample.left) * 4;
            Color value{};
            for (unsigned channel = 0; channel < 3; ++channel) {
                const unsigned back = std::min(255, pixels[offset + channel] + (tint[channel] * (255 - pixels[offset + 3]) + 127) / 255);
                const double base = (tint[channel] * glass_opacity + back * (255 - glass_opacity) + 127) / 255;
                value[channel] = static_cast<uint8_t>(std::clamp(std::lround(base * (1 - sheen) + 255 * sheen), 0L, 255L));
            }
            double coverage = 0;
            for (unsigned s = 0; s < 4; ++s) {
                const auto inset = edge_at(body, curve, y + (s + 0.5) / 4, false);
                coverage += std::max(0.0, std::min(x + 1.0, body.right - inset) - std::max(static_cast<double>(x), body.left + inset));
            }
            value[3] = static_cast<uint8_t>(std::lround(255 * coverage / 4));
            return value;
        };
        // Merge equal pixels into spans, keeping flat backdrops inexpensive on GPU.
        for (int x = left; x < right;) {
            const auto value = material(x); int end = x + 1;
            while (end < right && material(end) == value) { ++end; }
            span(commands, {x, y, end, y + 1}, clip, value); x = end;
        }
    }
    edge[3] = dark ? 110 : 95;
    rounded(commands, body, clip, edge, curve, std::max(1u, (dpi + 48) / 96));
    // Complete specular rim, brightest above and fading towards the lower edge.
    Color highlight{255, 255, 255, static_cast<uint8_t>(dark ? 105 : 210)};
    const Rect inner{body.left + 1, body.top + 1, body.right - 1, body.bottom - 1};
    for (int y = inner.top; y < inner.bottom; ++y) {
        highlight[3] = static_cast<uint8_t>((dark ? 100 : 205) * (1.0 - 0.8 * (y - inner.top) / std::max(1, inner.bottom - inner.top)));
        rounded(commands, inner, {visible.left, y, visible.right, y + 1}, highlight, std::max(0, curve - 1), 1);
    }
}
}
