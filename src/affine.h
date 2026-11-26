#pragma once
#include <sunfleck/ui.h>
#include "layout.h"
#include <array>
#include <optional>

namespace affine {
inline constexpr ui_matrix identity{1,0,0,1,0,0};
inline ui_point map(const ui_matrix& m, double x, double y) {
    return {x*m.m11+y*m.m21+m.offset_x,x*m.m12+y*m.m22+m.offset_y};
}
inline ui_matrix compose(const ui_matrix& a,const ui_matrix& b) {
    auto origin=map(b,a.offset_x,a.offset_y);
    return {a.m11*b.m11+a.m12*b.m21,a.m11*b.m12+a.m12*b.m22,
            a.m21*b.m11+a.m22*b.m21,a.m21*b.m12+a.m22*b.m22,origin.x,origin.y};
}
inline std::optional<ui_matrix> inverse(const ui_matrix& m) {
    const double det=m.m11*m.m22-m.m12*m.m21;
    if (det==0 || !std::isfinite(det)) return {};
    return ui_matrix{m.m22/det,-m.m12/det,-m.m21/det,m.m11/det,
        (m.m21*m.offset_y-m.m22*m.offset_x)/det,(m.m12*m.offset_x-m.m11*m.offset_y)/det};
}
inline layout::Box bounds(const ui_matrix& m,layout::Box b) {
    const auto a=map(m,b.left,b.top), c=map(m,b.right,b.top), d=map(m,b.left,b.bottom), e=map(m,b.right,b.bottom);
    return {std::min({a.x,c.x,d.x,e.x}),std::min({a.y,c.y,d.y,e.y}),
            std::max({a.x,c.x,d.x,e.x}),std::max({a.y,c.y,d.y,e.y})};
}
inline bool trivial(const ui_matrix& m) {
    return m.m11==1 && m.m12==0 && m.m21==0 && m.m22==1 && m.offset_x==0 && m.offset_y==0;
}
inline ui_matrix around(ui_matrix m,double x,double y,double dpi_scale) {
    m.offset_x = x + m.offset_x*dpi_scale - x*m.m11 - y*m.m21;
    m.offset_y = y + m.offset_y*dpi_scale - x*m.m12 - y*m.m22;
    return m;
}
}
