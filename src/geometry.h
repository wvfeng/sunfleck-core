#pragma once
#include "brush.h"

namespace drawing {
struct Figure { std::vector<ui_point> points; bool closed=false; };
struct Geometry {
    std::vector<Figure> figures;
    ui_size size{};
    std::string data;
};
std::optional<Geometry> parse_path(std::string_view data);
inline bool shape_kind(ui_node_kind kind) { return kind>=UI_RECTANGLE && kind<=UI_PATH; }
void paint_shape(ui_node_kind kind,const Geometry* path,const Brush* fill,const Brush* stroke,double thickness,
                 layout::Rect bounds,layout::Rect clip,double scale,std::vector<FillRectCommand>& commands);
}
