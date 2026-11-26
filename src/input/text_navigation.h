#pragma once
#include "../gallery.h"
#include <algorithm>
#include <optional>
#include <limits>
#include <tuple>

namespace text_navigation {
struct Caret { size_t index=0; bool trailing=false; };
inline const TextStop* nearest(const std::vector<TextStop>& stops,int x,int y,int line_height) {
    const TextStop* best=nullptr;
    // Two different logical positions can share one visual run boundary.
    // Prefer the edge owned by the cluster on the pointer's side.
    auto distance = [&](const TextStop& p) {
        const bool cluster_on_left = p.trailing != p.rtl;
        const bool other_side = x != p.x && (x < p.x) != cluster_on_left;
        return std::tuple{std::abs(p.y + line_height / 2 - y), std::abs(p.x - x), other_side};
    };
    for (const auto& stop:stops) if (!best || distance(stop)<distance(*best)) best=&stop;
    return best;
}
inline const TextStop* locate(const std::vector<TextStop>& stops,Caret caret) {
    const TextStop* fallback=nullptr;
    for (const auto& stop:stops) if (stop.index==caret.index) {
        if (stop.trailing==caret.trailing) return &stop;
        fallback=&stop;
    }
    return fallback;
}
inline std::optional<Caret> move(const std::vector<TextStop>& stops,Caret caret,int dx,int dy,bool rtl,bool edge=false,std::optional<int> preferred_x={}) {
    const auto* current=locate(stops,caret);
    if (!current) return {};
    const TextStop* best=nullptr;
    if (edge) {
        for (const auto& stop:stops) if (stop.y==current->y &&
            (!best || (dx<0?stop.index<best->index:stop.index>best->index) ||
             (stop.index==best->index && stop.trailing==(dx>0)))) best=&stop;
    } else if (dy) {
        auto distance=[&](const TextStop& p){return std::pair{(p.y-current->y)*dy,std::abs(p.x-preferred_x.value_or(current->x))};};
        for (const auto& stop:stops) if ((stop.y-current->y)*dy>0 && (!best || distance(stop)<distance(*best))) best=&stop;
    } else {
        int distance=std::numeric_limits<int>::max();
        for (const auto& stop:stops) {
            const int delta=(stop.x-current->x)*dx;
            if (stop.y==current->y && delta>0 && (delta<distance ||
                (delta==distance && (stop.trailing != stop.rtl)==(dx>0)))) { best=&stop; distance=delta; }
        }
        if (!best) {
            const int line_direction=rtl?-dx:dx;
            int line_distance=std::numeric_limits<int>::max();
            for (const auto& stop:stops) {
                const int delta=(stop.y-current->y)*line_direction;
                if (delta<=0) continue;
                if (delta<line_distance || (delta==line_distance && best && (dx>0?stop.x<best->x:stop.x>best->x))) {
                    best=&stop; line_distance=delta;
                }
            }
        }
    }
    return best?Caret{best->index,best->trailing}:caret;
}
inline std::optional<size_t> horizontal(const std::vector<TextStop>& stops,size_t caret,int direction,bool rtl) {
    auto value=move(stops,{caret,false},direction,0,rtl);
    return value?std::optional<size_t>{value->index}:std::nullopt;
}
}
