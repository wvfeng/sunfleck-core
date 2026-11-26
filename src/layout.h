#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <cmath>
#include <limits>

namespace layout {
struct Rect {
    int left, top, right, bottom;
};

// Layout keeps subpixel edges until a painter or device-space input API needs
// integer pixels. Adjacent slots round the same shared edge, avoiding drift.
struct Box {
    double left=0, top=0, right=0, bottom=0;
    Box() = default;
    template<class L,class T,class R,class B>
    Box(L l,T t,R r,B b):left(double(l)),top(double(t)),right(double(r)),bottom(double(b)) {}
    Box(Rect rect):Box(rect.left,rect.top,rect.right,rect.bottom) {}
    operator Rect() const {
        const auto pixel=[](double n) {
            if (std::isnan(n)) return 0;
            return int(std::round(std::clamp(n,double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max()))));
        };
        return {pixel(left),pixel(top),pixel(right),pixel(bottom)};
    }
};
struct FractionalTrack { double preferred; unsigned weight=0; };

inline void stack(Box area,double gap,std::span<const double> heights,std::span<Box> children) {
    double top=area.top;
    for (size_t i=0;i<std::min(heights.size(),children.size());++i) {
        children[i]={area.left,top,area.right,top+heights[i]};
        top+=heights[i]+gap;
    }
}
inline void grid(Box area,double gap,std::span<const FractionalTrack> tracks,std::span<Box> cells) {
    const double extent=area.right-area.left;
    double fixed=0; uint64_t weight=0;
    for (const auto& track:tracks) { if (track.weight) weight+=track.weight; else fixed+=track.preferred; }
    const double remaining=std::max(0.0,extent-fixed-gap*std::max(0,int(tracks.size())-1));
    double left=area.left;
    for (size_t i=0;i<std::min(tracks.size(),cells.size());++i) {
        const double size=std::isfinite(extent) && extent>=0 && tracks[i].weight ? remaining*tracks[i].weight/weight : tracks[i].preferred;
        cells[i]={left,area.top,left+size,area.bottom}; left+=size+gap;
    }
}
inline void flex(Box area,double gap,std::span<const FractionalTrack> tracks,std::span<Box> cells) {
    double basis=0; uint64_t weight=0;
    for (const auto& track:tracks) { basis+=track.preferred; weight+=track.weight; }
    const double extra=std::max(0.0,area.right-area.left-basis-gap*std::max(0,int(tracks.size())-1));
    double left=area.left;
    for (size_t i=0;i<std::min(tracks.size(),cells.size());++i) {
        const double size=tracks[i].preferred+(weight && std::isfinite(extra)?extra*tracks[i].weight/weight:0);
        cells[i]={left,area.top,left+size,area.bottom}; left+=size+gap;
    }
}

struct Track {
    int preferred;
    unsigned weight = 0;
};

inline void stack(Rect area, int gap, std::span<const int> heights, std::span<Rect> children)
{
    int top = area.top;
    for (std::size_t i = 0; i < std::min(heights.size(), children.size()); ++i) {
        children[i] = {area.left, top, area.right, top + heights[i]};
        top += heights[i] + gap;
    }
}

// A negative extent represents an unbounded measure axis.
inline void grid(Rect area, int gap, std::span<const Track> tracks, std::span<Rect> cells)
{
    const int extent = area.right - area.left;
    int fixed = 0;
    std::uint64_t total_weight = 0;
    for (const auto& track : tracks) {
        if (track.weight) {
            total_weight += track.weight;
        } else {
            fixed += track.preferred;
        }
    }
    const int remaining = std::max(0, extent - fixed -
        gap * std::max(0, static_cast<int>(tracks.size()) - 1));
    std::uint64_t weight = 0;
    int left = area.left;
    int allocated = 0;
    for (std::size_t i = 0; i < std::min(tracks.size(), cells.size()); ++i) {
        int size = tracks[i].preferred;
        if (extent >= 0 && tracks[i].weight) {
            weight += tracks[i].weight;
            const int next = static_cast<int>(remaining * weight / total_weight);
            size = next - allocated;
            allocated = next;
        }
        cells[i] = {left, area.top, left + size, area.bottom};
        left += size + gap;
    }
}

inline void flex(Rect area, int gap, std::span<const Track> items, std::span<Rect> children)
{
    int basis = 0;
    std::uint64_t total_weight = 0;
    for (const auto& item : items) {
        basis += item.preferred;
        total_weight += item.weight;
    }
    const int extra = std::max(0, area.right - area.left - basis -
        gap * std::max(0, static_cast<int>(items.size()) - 1));
    std::uint64_t weight = 0;
    int allocated = 0;
    int left = area.left;
    for (std::size_t i = 0; i < std::min(items.size(), children.size()); ++i) {
        weight += items[i].weight;
        const int next = total_weight ? static_cast<int>(extra * weight / total_weight) : 0;
        const int right = left + items[i].preferred + next - allocated;
        children[i] = {left, area.top, right, area.bottom};
        allocated = next;
        left = right + gap;
    }
}

inline int scroll_limit(int viewport, int content)
{
    return std::max(0, content - std::max(0, viewport));
}

inline Rect scroll(Rect viewport, int content, int offset)
{
    const int top = viewport.top - std::clamp(offset, 0,
        scroll_limit(viewport.bottom - viewport.top, content));
    return {viewport.left, top, viewport.right, top + content};
}
}
