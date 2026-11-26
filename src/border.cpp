#include "border.h"
#include <vector>

namespace border {
namespace {
struct RoundedBox {
    double left, top, right, bottom;
    std::array<double, 4> rx, ry;
    std::pair<double, double> interval(double y) const {
        if (y < top || y >= bottom || right <= left || bottom <= top) return {left, left};
        auto inset = [](double distance, double x, double y) {
            if (x <= 0 || y <= 0 || distance >= y) return 0.0;
            const auto d = 1 - std::max(0.0, distance) / y;
            return x * (1 - std::sqrt(std::max(0.0, 1 - d * d)));
        };
        return {left + std::max(inset(y - top, rx[0], ry[0]), inset(bottom - y, rx[3], ry[3])),
            right - std::max(inset(y - top, rx[1], ry[1]), inset(bottom - y, rx[2], ry[2]))};
    }
};
}
void paint(const BorderStyle& style, layout::Rect area, layout::Rect clip, unsigned dpi, std::vector<FillRectCommand>& commands) {
    if (area.right <= area.left || area.bottom <= area.top ||
        (!(style.background >> 24) && (!(style.brush >> 24) || std::all_of(style.thickness.begin(), style.thickness.end(), [](double x) { return x == 0; })))) return;
    const double scale = dpi / 96.0, width = area.right - area.left, height = area.bottom - area.top;
    auto r = style.radius, t = style.thickness;
    for (auto& v : r) v *= scale;
    for (auto& v : t) v *= scale;
    double factor = 1;
    for (auto pair : {std::pair{width, r[0] + r[1]}, {width, r[3] + r[2]}, {height, r[0] + r[3]}, {height, r[1] + r[2]}})
        if (pair.second > 0) factor = std::min(factor, pair.first / pair.second);
    for (auto& v : r) v *= factor;
    const RoundedBox outer{double(area.left), double(area.top), double(area.right), double(area.bottom), r, r};
    RoundedBox inner{area.left + t[0], area.top + t[1], area.right - t[2], area.bottom - t[3], r, r};
    for (unsigned i = 0; i < 4; ++i) {
        inner.rx[i] = std::max(0.0, r[i] - t[i == 0 || i == 3 ? 0 : 2]);
        inner.ry[i] = std::max(0.0, r[i] - t[i < 2 ? 1 : 3]);
    }
    const int left = std::max(area.left, clip.left), right = std::min(area.right, clip.right);
    if (left >= right) return;
    for (int y = std::max(area.top, clip.top); y < std::min(area.bottom, clip.bottom); ++y) {
        std::array<std::pair<double, double>, 4> outside, inside;
        std::vector<int> cuts{left, right}; cuts.reserve(34);
        for (unsigned sample = 0; sample < 4; ++sample) {
            outside[sample] = outer.interval(y + (sample + .5) / 4);
            inside[sample] = inner.interval(y + (sample + .5) / 4);
            for (double edge : {outside[sample].first, outside[sample].second, inside[sample].first, inside[sample].second}) {
                cuts.push_back(std::clamp(int(std::floor(edge)), left, right));
                cuts.push_back(std::clamp(int(std::ceil(edge)), left, right));
            }
        }
        std::sort(cuts.begin(), cuts.end()); cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        for (size_t segment = 0; segment + 1 < cuts.size(); ++segment) {
            const int x = cuts[segment];
            double background = 0, stroke = 0;
            auto coverage = [x](auto interval) { return std::max(0.0, std::min(double(x + 1), interval.second) - std::max(double(x), interval.first)); };
            for (unsigned sample = 0; sample < 4; ++sample) {
                const double a = coverage(outside[sample]), b = std::min(a, coverage(inside[sample]));
                background += b / 4; stroke += (a - b) / 4;
            }
            const double a = background * (style.background >> 24), b = stroke * (style.brush >> 24), alpha = a + b;
            if (alpha < .5) continue;
            std::array<uint8_t, 4> color{};
            color[3] = uint8_t(std::clamp(std::lround(alpha), 0l, 255l));
            for (unsigned c = 0; c < 3; ++c) {
                const unsigned shift = (2 - c) * 8;
                color[c] = uint8_t(std::lround((((style.background >> shift) & 255) * a + ((style.brush >> shift) & 255) * b) / alpha));
            }
            commands.push_back({x, y, cuts[segment + 1], y + 1, color});
        }
    }
}
}
