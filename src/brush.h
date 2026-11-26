#pragma once
#include "raster.h"
#include <optional>
#include <string>

namespace drawing {
struct Brush {
    ui_brush_kind kind = UI_BRUSH_SOLID;
    uint32_t color = 0;
    ui_point start{}, end{1,1};
    std::vector<ui_gradient_stop> stops;
    std::vector<uint8_t> rgba;
    unsigned width = 0, height = 0;
    ui_stretch stretch = UI_STRETCH_FILL;

    static std::optional<Brush> copy(const ui_brush_desc& desc);
    std::array<uint8_t,4> sample(layout::Rect bounds,double x,double y,double scale) const;
};
bool valid_effect(const ui_effect_desc& effect);
void shade(std::vector<FillRectCommand>& commands,size_t first,const Brush& brush,layout::Rect bounds,double scale);
void effect_layer(std::vector<FillRectCommand>& commands,size_t first,const ui_effect_desc& effect,
                  const Brush* mask,layout::Rect bounds,layout::Rect clip,double opacity,double scale);
}
