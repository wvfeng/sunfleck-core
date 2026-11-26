#include "ui_tree.h"
#include <numeric>
#include "dwrite_text.h"
#include "dock.h"
#include <cmath>
#include <stdexcept>

void invalidate_layout(ui_node& node, uint32_t flags)
{
    if (flags & UI_PROPERTY_AFFECTS_MEASURE) {
        for (auto* current = &node; current; current = current->parent)
            current->layout_cache.invalidate_measure();
    } else if (flags & UI_PROPERTY_AFFECTS_ARRANGE) {
        node.layout_cache.arrange_valid = false;
    }
}

namespace {
using layout::Rect;
using layout::Box;
using Color = std::array<std::uint8_t, 4>;

double metric_width(const wchar_t* text,unsigned dpi) {
#ifdef _WIN32
    return windows_text::measure_width(text,dpi);
#else
    return text_width(text,dpi);
#endif
}
double metric_height(const wchar_t* text,unsigned dpi,double width) {
#ifdef _WIN32
    return windows_text::measure_height(text,dpi,width);
#else
    return text_height(text,dpi,std::isfinite(width) ? int(std::min(std::ceil(width),double(std::numeric_limits<int>::max()))) : std::numeric_limits<int>::max());
#endif
}
double metric_line_height(unsigned dpi) {
#ifdef _WIN32
    return windows_text::measure_line_height(dpi);
#else
    return text_line_height(dpi);
#endif
}

Rect intersect(Box a, Box b)
{
    return Box{std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};
}

Rect inset(Rect bounds, int padding)
{
    return {bounds.left + padding, bounds.top + padding,
            bounds.right - padding, bounds.bottom - padding};
}

Box inset(Box bounds, double padding)
{
    return {bounds.left+padding,bounds.top+padding,bounds.right-padding,bounds.bottom-padding};
}

bool empty(Rect rect)
{
    return rect.left >= rect.right || rect.top >= rect.bottom;
}

bool is_button(ui_node_kind kind)
{
    return kind == UI_BUTTON || kind == UI_TOGGLE_BUTTON || kind == UI_ICON_BUTTON;
}

void fill(std::vector<FillRectCommand>& commands, Box bounds, Rect clip, Color color)
{
    const auto visible = intersect(bounds, clip);
    if (!empty(visible)) {
        commands.push_back({visible.left, visible.top, visible.right, visible.bottom, color});
    }
}

// All controls and Dock decorations share the same antialiased coverage.
void rounded(std::vector<FillRectCommand>& commands, Rect area, Rect clip, Color color, int radius)
{
    drawing::rounded(commands, area, clip, color, radius);
}

void tree_chevron(std::vector<FillRectCommand>& commands, Rect area, Rect clip, Color color, bool expanded)
{
    const double size = area.right - area.left;
    const std::array<std::array<double, 2>, 3> points = expanded
        ? std::array<std::array<double, 2>, 3>{{{0.25, 0.375}, {0.5, 0.625}, {0.75, 0.375}}}
        : std::array<std::array<double, 2>, 3>{{{0.375, 0.25}, {0.625, 0.5}, {0.375, 0.75}}};
    const double radius = size / 24.0;
    for (int y = area.top; y < area.bottom; ++y) {
        for (int x = area.left; x < area.right; ++x) {
            unsigned covered = 0;
            for (unsigned sample = 0; sample < 16; ++sample) {
                const double sx = x - area.left + (sample % 4 + 0.5) / 4;
                const double sy = y - area.top + (sample / 4 + 0.5) / 4;
                bool inside = false;
                for (unsigned segment = 0; segment < 2; ++segment) {
                    const double ax = points[segment][0] * size, ay = points[segment][1] * size;
                    const double dx = (points[segment + 1][0] - points[segment][0]) * size;
                    const double dy = (points[segment + 1][1] - points[segment][1]) * size;
                    const double t = std::clamp(((sx - ax) * dx + (sy - ay) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
                    inside |= std::hypot(sx - ax - t * dx, sy - ay - t * dy) <= radius;
                }
                covered += inside;
            }
            if (covered) {
                auto pixel = color;
                pixel[3] = static_cast<std::uint8_t>((color[3] * covered + 8) / 16);
                fill(commands, {x, y, x + 1, y + 1}, clip, pixel);
            }
        }
    }
}

struct Painter {
    GalleryStyle style;
    ThemeShape shape;
    unsigned dpi;
    const ui_node* focus;
    bool keyboard_focus;
    bool caret_visible;
    const ui_node* hovered;
    const ui_node* pressed;
    std::vector<FillRectCommand>& commands;
    std::vector<ui_node*>& interactive;
    std::vector<ui_node*>& scrollable;
    std::vector<ui_node*> popups;
    std::vector<UiOverlay>* overlays = nullptr;
    std::vector<ui_node*>* paint_order = nullptr;
    const std::function<void(ui_node&)>* prepare_dock_content = nullptr;
    const ui_node* dock_surface = nullptr;
    drawing::CommandList* list = nullptr;

    std::vector<ui_node*> shared_grids;
    std::map<std::string, double> shared_columns, shared_rows;

    static void collect_runs(const ui_node& node,uint32_t& start,std::vector<StyledRun>& runs) {
        if (node.visibility==UI_COLLAPSED) return;
        const auto& text=node.text_source ? node.text_source->value : node.text;
        if (!text.empty()) {
            auto format=text_format::effective(node);
            if (!node.is_visible()) { format.foreground_set=true; format.foreground=0; }
            runs.push_back({start,uint32_t(text.size()),std::move(format)}); start+=uint32_t(text.size());
        }
        for (const auto* child:node.children) collect_runs(*child,start,runs);
    }

    bool prepare_layout(ui_node& node)
    {
        if (node.kind == UI_GRID) {
            const auto grouped = [](const auto& track) { return !track.group.empty(); };
            if (std::any_of(node.grid.columns.begin(), node.grid.columns.end(), grouped) ||
                std::any_of(node.grid.rows.begin(), node.grid.rows.end(), grouped)) shared_grids.push_back(&node);
        }
        node.resolved_font = text_format::effective(node);
        node.resolved_runs.clear();
        if (node.kind==UI_TEXT_BLOCK && !node.children.empty()) {
            uint32_t position=0; collect_runs(node,position,node.resolved_runs);
        }
        // Geometry of visible content in another Dock window belongs to that surface.
        // Visibility changes can retire geometry globally; normal paint clears only its own surface.
        if (!node.is_visible()) { node.visible = {}; node.bounds = {}; }
        node.layout_children.clear();
        for (auto* child : node.children) if (child->visibility != UI_COLLAPSED) node.layout_children.push_back(child);
        auto& cache = node.layout_cache;
        auto& old = cache.inputs;
        const auto parent = node.parent ? node.parent->kind : UI_STACK_PANEL;
        // Value affects desired geometry only for selectors, splits and trees.
        const unsigned value = node.kind == UI_TAB_VIEW || node.kind == UI_NAVIGATION_VIEW ||
            node.kind == UI_SPLIT_VIEW || node.kind == UI_DOCK_SPLIT ? node.value : 0;
        const bool alignment_changed = old.layout.horizontal != node.layout.horizontal ||
            old.layout.vertical != node.layout.vertical || old.layout.horizontal_set != node.layout.horizontal_set ||
            old.parent_kind != parent;
        const auto& before = old.layout;
        const auto& after = node.layout;
        const bool dimensions_changed = before.width != after.width || before.min_width != after.min_width ||
            before.max_width != after.max_width || before.min_height != after.min_height ||
            before.max_height != after.max_height || before.margin != after.margin;
        const bool runs_changed=old.runs.size()!=node.resolved_runs.size() || !std::equal(old.runs.begin(),old.runs.end(),node.resolved_runs.begin(),[](const auto& a,const auto& b){return a.start==b.start && a.length==b.length && a.format.same_metrics(b.format);});
        bool changed = !cache.initialized || runs_changed || (old.layout.horizontal==0)!=(node.layout.horizontal==0) || !old.font.same_metrics(node.resolved_font) || dimensions_changed || old.text != node.display_text() ||
            old.children != node.children || old.height != node.height || old.spacing != node.spacing ||
            old.border_thickness != node.border.thickness || old.padding != node.padding || old.weight != node.weight ||
            old.value != value || old.expanded != node.expanded || old.image != !node.image.rgb.empty() ||
            old.component != bool(node.component) || old.kind != node.kind || old.visibility != node.visibility ||
            old.dock != node.dock || old.dpi != dpi || old.gap != style.gap || old.control_height != style.control_height ||
            old.toggle_height != shape.toggle.height || old.glass != shape.glass || old.font_revision != text_resource_revision;
        if (changed || alignment_changed) {
            old.font = node.resolved_font;
            old.runs = node.resolved_runs;
            old.border_thickness = node.border.thickness; old.layout = node.layout; old.text = node.display_text(); old.children = node.children;
            old.height = node.height; old.spacing = node.spacing; old.padding = node.padding;
            old.weight = node.weight; old.value = value;
            old.expanded = node.expanded; old.image = !node.image.rgb.empty(); old.component = bool(node.component);
            old.visibility = node.visibility; old.kind = node.kind; old.parent_kind = parent; old.dock = node.dock;
            old.dpi = dpi; old.gap = style.gap; old.control_height = style.control_height;
            old.toggle_height = shape.toggle.height; old.glass = shape.glass; old.font_revision = text_resource_revision;
            cache.initialized = true;
        }
        for (auto* child : node.children) changed = prepare_layout(*child) || changed;
        cache.width_dependent_height=(!affine::trivial(node.layout_transform) && node.layout.width<0) || node.height<0 &&
            (node.kind == UI_VIEWBOX || node.panel_callbacks || (node.kind==UI_TEXT_BLOCK && node.resolved_font.wrapping==UI_TEXT_WRAP) || node.kind==UI_WRAP_PANEL ||
             (is_container(node.kind) && node.kind!=UI_SCROLL_VIEW && node.kind!=UI_LIST_VIEW && node.kind!=UI_DATA_GRID &&
              std::any_of(node.layout_children.begin(),node.layout_children.end(),[](const auto* child){return child->layout_cache.width_dependent_height;})));
        if (changed) cache.invalidate_measure();
        else if (alignment_changed) cache.arrange_valid = false;
        return changed;
    }

    bool dock_editor(const ui_node& node) const
    {
        if (node.kind != UI_TEXT_AREA) { return false; }
        const auto* surface = &node;
        while (surface != dock_surface && surface->parent &&
            (surface->parent->kind == UI_DOCK_DOCUMENT || surface->parent->kind == UI_DOCK_TOOL)) { surface = surface->parent; }
        return surface == dock_surface;
    }

    int px(int size) const
    {
        return size < 0 ? -int((uint64_t(-int64_t(size)) * dpi + 48) / 96) : int((uint64_t(size) * dpi + 48) / 96);
    }

    double scaled(double size) const { return size*dpi/96.0; }

    int gap() const
    {
        return px(style.gap);
    }

    double button_width(const ui_node& node) const
    {
        TextFormatScope font_scope(node.resolved_font);
        if (node.kind == UI_BUTTON && !node.layout_children.empty())
            return std::max(scaled(96), measure_width(*node.layout_children.front()) + 2 * scaled(style.gap + node.padding));
        const double icon = node.kind == UI_ICON_BUTTON && !node.image.rgb.empty() ? scaled(20) : 0;
        return std::max(scaled(96), metric_width(node.text.c_str(), dpi) + icon + 2 * scaled(style.gap));
    }

    Color color(const ui_node& node, ui_theme_color token) const
    {
        const bool overridden = (node.theme_override_mask & (1u << token)) != 0;
        return theme_rgb(overridden ? node.theme_overrides[token] : style.colors[token]);
    }

    static std::pair<double,double> stretch_scale(ui_stretch mode, double sx, double sy) {
        if (mode == UI_STRETCH_NONE) return {1,1};
        if (mode == UI_STRETCH_FILL) return {sx,sy};
        const double scale = mode == UI_STRETCH_UNIFORM_TO_FILL ? std::max(sx,sy) : std::min(sx,sy);
        return {scale,scale};
    }

    void image(const GalleryImage& source, Rect area, Rect clip, int maximum, std::optional<ui_stretch> stretch = {}) {
        if (source.rgb.empty() || empty(area)) return;
        int left = area.left, top = area.top;
        double width, height;
        if (stretch) {
            const auto [sx,sy] = stretch_scale(*stretch,double(area.right-left)/source.width,double(area.bottom-top)/source.height);
            width = *stretch == UI_STRETCH_NONE ? scaled(source.width) : sx*source.width;
            height = *stretch == UI_STRETCH_NONE ? scaled(source.height) : sy*source.height;
            left += int(std::round((area.right-left-width)/2));
            top += int(std::round((area.bottom-top-height)/2));
        } else {
            const int size = std::min({px(maximum),area.right-left,area.bottom-top});
            const auto longest = std::max(source.width,source.height);
            width = std::max(1u,size*source.width/longest);
            height = std::max(1u,size*source.height/longest);
        }
        if (width <= 0 || height <= 0) return;
        const auto output = intersect(intersect(area,clip),Box{left,top,left+width,top+height});
        for (int y=output.top; y<output.bottom; ++y) {
            const auto row = std::min(source.height-1,unsigned((double(y)-top)*source.height/height));
            for (int x=output.left; x<output.right; ++x) {
                const auto col = std::min(source.width-1,unsigned((double(x)-left)*source.width/width));
                const auto i = (row*source.width+col)*source.channels;
                const uint8_t alpha = source.channels == 4 ? source.rgb[i+3] : 255;
                if (alpha) commands.push_back({x,y,x+1,y+1,{source.rgb[i],source.rgb[i+1],source.rgb[i+2],alpha},true});
            }
        }
    }

    double measure(const ui_node& node, double available_width = std::numeric_limits<double>::infinity()) const
    {
        TextFormatScope font_scope(node.resolved_font);
        auto& cache = node.layout_cache;
        const double constraint=cache.width_dependent_height ? available_width : 0;
        if (cache.height_valid && cache.height_constraint==constraint) { ++cache.measure_hits; return cache.height; }
        ++cache.measures;
        cache.height_constraint=constraint;
        cache.height = compute_measure(node,available_width);
        cache.height_valid = true;
        return cache.height;
    }

    ui_size layout_source_size(const ui_node& node,double available_width) const
    {
        const auto& l = node.layout;
        const auto& m = node.layout_transform;
        const double horizontal = std::abs(m.m11);
        const double intrinsic = natural_width(node);
        double width = l.width >= 0 ? scaled(l.width) : intrinsic;
        if (l.width < 0 && horizontal > 0 && std::isfinite(available_width)) {
            const double limit = std::max(0.,available_width-scaled(l.margin[0]+l.margin[2]))/horizontal;
            width = l.horizontal == 0 && m.m21 == 0 ? limit : std::min(intrinsic,limit);
        }
        width = std::clamp(width,scaled(l.min_width),scaled(l.max_width));
        const double height = node.height >= 0 ? scaled(node.height) : natural_height(node,width);
        return {width,std::clamp(height,scaled(l.min_height),scaled(l.max_height))};
    }

    double compute_measure(const ui_node& node, double available_width) const
    {
        if (node.visibility == UI_COLLAPSED) return 0;
        const auto& l = node.layout;
        if (!affine::trivial(node.layout_transform)) {
            const auto size = layout_source_size(node,available_width);
            return std::max(0.,std::abs(node.layout_transform.m12)*size.width +
                    std::abs(node.layout_transform.m22)*size.height + scaled(l.margin[1]+l.margin[3]));
        }
        double width=l.width>=0 ? scaled(l.width) : l.horizontal!=0 ? natural_width(node) :
            std::max(0.0,available_width-scaled(l.margin[0])-scaled(l.margin[2]));
        width=std::clamp(width,scaled(l.min_width),scaled(l.max_width));
        const double height = node.height >= 0 ? scaled(node.height) : natural_height(node,width);
        return std::max(0.0, std::clamp(height, scaled(l.min_height), scaled(l.max_height)) + scaled(l.margin[1]) + scaled(l.margin[3]));
    }

    double dock_extent(const ui_node& node, bool horizontal, double available_width = std::numeric_limits<double>::infinity()) const
    {
        const double spacing = node.spacing < 0 ? scaled(style.gap) : scaled(node.spacing);
        double consumed = 0, extent = 0;
        for (std::size_t i = 0; i < node.layout_children.size(); ++i) {
            const auto& child = *node.layout_children[i];
            const double child_width=measure_width(child);
            const bool side=child.dock==UI_DOCK_LEFT || child.dock==UI_DOCK_RIGHT;
            const double desired = horizontal ? child_width : measure(child,side && i+1<node.layout_children.size() ? child_width : available_width);
            const bool consumes_axis = horizontal
                ? child.dock == UI_DOCK_LEFT || child.dock == UI_DOCK_RIGHT
                : child.dock == UI_DOCK_TOP || child.dock == UI_DOCK_BOTTOM;
            // The final child fills the remainder, regardless of its Dock value.
            if (consumes_axis && i + 1 < node.layout_children.size()) {
                consumed += desired + spacing;
                extent = std::max(extent, consumed);
            } else {
                extent = std::max(extent, consumed + desired);
            }
            if (!horizontal && side && i+1<node.layout_children.size() && std::isfinite(available_width))
                available_width=std::max(0.0,available_width-child_width-spacing);
        }
        return extent + 2 * scaled(node.padding);
    }

    double natural_width(const ui_node& node) const
    {
        TextFormatScope font_scope(node.resolved_font);
        auto& cache = node.layout_cache;
        if (cache.natural_width_valid) { ++cache.measure_hits; return cache.natural_width; }
        ++cache.measures;
        cache.natural_width = compute_natural_width(node);
        cache.natural_width_valid = true;
        return cache.natural_width;
    }

    double border_inset(const ui_node& node, unsigned side) const { return node.border.thickness[side] * dpi / 96.0; }

    grid_layout::Axis resolve_tracks(const std::vector<grid_layout::Track>& tracks, std::vector<grid_layout::Demand> demands,
                                    double extent, double spacing, const std::map<std::string,double>& groups) const {
        if (groups.empty()) return grid_layout::resolve(tracks, std::move(demands), extent, spacing, dpi / 96.0);
        auto effective = tracks;
        for (auto& track : effective) {
            auto it = groups.find(track.group);
            if (it != groups.end()) { track.unit = grid_layout::Unit::pixels; track.value = it->second * 96 / dpi; }
        }
        return grid_layout::resolve(effective, std::move(demands), extent, spacing, dpi / 96.0);
    }

    void prepare_groups() {
        const double inf = std::numeric_limits<double>::infinity();
        const auto collect = [](const auto& tracks, const auto& axis, auto& groups) {
            for (size_t i = 0; i < tracks.size(); ++i)
                if (!tracks[i].group.empty()) {
                    auto& size = groups[tracks[i].group];
                    size = std::max(size, axis.ends[i] - axis.starts[i]);
                }
        };
        for (bool rows : {false, true}) {
            std::map<std::string,double> sizes;
            for (auto* grid : shared_grids) {
                bool collapsed = false;
                for (auto* ancestor = grid; ancestor; ancestor = ancestor->parent)
                    if (ancestor->visibility == UI_COLLAPSED) { collapsed = true; break; }
                if (collapsed) continue;
                invalidate_layout(*grid, UI_PROPERTY_AFFECTS_MEASURE);
                const double gap = scaled(grid->spacing < 0 ? style.gap : grid->spacing);
                const auto columns = grid_columns(*grid, inf, gap);
                if (rows) collect(grid->grid.rows, grid_rows(*grid, columns, inf, gap), sizes);
                else collect(grid->grid.columns, columns, sizes);
            }
            (rows ? shared_rows : shared_columns) = std::move(sizes);
        }
        for (auto* grid : shared_grids) invalidate_layout(*grid, UI_PROPERTY_AFFECTS_MEASURE);
    }

    grid_layout::Axis grid_columns(const ui_node& node, double width, double spacing) const {
        std::vector<grid_layout::Demand> demands;
        for (const auto* child : node.layout_children)
            demands.push_back({child->grid.cell.column, child->grid.cell.columns, measure_width(*child)});
        return resolve_tracks(node.grid.columns, std::move(demands), width, spacing, shared_columns);
    }

    grid_layout::Axis grid_rows(const ui_node& node, const grid_layout::Axis& columns, double height, double spacing) const {
        std::vector<grid_layout::Demand> demands;
        for (const auto* child : node.layout_children) {
            const auto& cell = child->grid.cell;
            const auto [left,right] = columns.range(cell.column, cell.columns);
            demands.push_back({cell.row, cell.rows, measure(*child,right-left)});
        }
        return resolve_tracks(node.grid.rows, std::move(demands), height, spacing, shared_rows);
    }

    double vertical_wrap(const ui_node& node, double height_limit, double spacing,
                         std::vector<Box>* slots) const
    {
        double x = 0, y = 0, column_width = 0;
        bool first_in_column = true;
        for (size_t i = 0; i < node.layout_children.size(); ++i) {
            const auto& child = *node.layout_children[i];
            const double width = measure_width(child);
            const double height = measure(child, width);
            if (!first_in_column && y + height > height_limit) {
                x += column_width + spacing;
                y = 0;
                column_width = 0;
            }
            if (slots) (*slots)[i] = {x, y, x + width, y + height};
            column_width = std::max(column_width, width);
            y += height + spacing;
            first_in_column = false;
        }
        return x + column_width;
    }

    struct PanelCallbackScope {
        ui_app* app;
        bool previous;
        explicit PanelCallbackScope(ui_app* owner) : app(owner), previous(panel_drawing(owner,true)) {}
        ~PanelCallbackScope() { panel_drawing(app,previous); }
    };

    std::vector<ui_size> panel_children(const ui_node& panel, double width) const {
        std::vector<ui_size> sizes;
        sizes.reserve(panel.layout_children.size());
        for (const auto* child : panel.layout_children) {
            const double w = measure_width(*child);
            sizes.push_back({w*96/dpi,measure(*child,std::min(w,width))*96/dpi});
        }
        return sizes;
    }

    ui_size panel_measure(const ui_node& node, double width) const {
        const double inset = 2*scaled(node.padding);
        double available_width = std::max(0.,width-inset);
        auto sizes = panel_children(node,available_width);
        ui_size desired{};
        const ui_size available{available_width*96/dpi, node.height < 0 ? node.layout.max_height : std::max(0.,node.height-2*node.padding)};
        PanelCallbackScope guard(node.owner);
        const auto& cb = *node.panel_callbacks;
        if (cb.measure(cb.context,available,sizes.data(),uint32_t(sizes.size()),&desired) == UI_OK &&
            std::isfinite(desired.width) && desired.width >= 0 && std::isfinite(desired.height) && desired.height >= 0)
            node.panel_desired = desired;
        return {scaled(node.panel_desired.width)+inset,scaled(node.panel_desired.height)+inset};
    }

    double compute_natural_width(const ui_node& node) const
    {
        InlineTextScope runs_scope(node.resolved_runs);
        if (drawing::shape_kind(node.kind)) return node.geometry ? scaled(node.geometry->size.width+node.stroke_thickness) : 0;
        if (node.kind == UI_VIEWBOX) return 2*scaled(node.padding) + (node.layout_children.empty() ? 0 : measure_width(*node.layout_children.front()));
        if (node.panel_callbacks) return panel_measure(node,std::numeric_limits<double>::infinity()).width;
        if (content_host(node.kind)) {
            const double content = node.layout_children.empty() ? metric_width(node.display_text().c_str(), dpi) : measure_width(*node.layout_children.front());
            return content + 2 * scaled(node.padding);
        }
        if (node.kind == UI_SEPARATOR && node.vertical()) return scaled(8);
        if (node.kind == UI_CANVAS) return 2 * scaled(node.padding);
        if (node.kind == UI_BORDER) return (node.layout_children.empty() ? 0 : measure_width(*node.layout_children.front())) +
            2 * scaled(node.padding) + border_inset(node, 0) + border_inset(node, 2);
        if (node.component) return node.layout_children.empty() ? 0 : measure_width(*node.layout_children.front());
        if (node.kind == UI_DOCK_PANEL) return dock_extent(node, true);
        if (is_button(node.kind)) return button_width(node);
        if (!is_container(node.kind)) {
            if (node.kind == UI_IMAGE || node.kind == UI_ICON) return scaled(40);
            if (node.kind == UI_TEXT_BLOCK) return metric_width(node.display_text().c_str(), dpi);
            if (node.kind == UI_LABEL || node.kind == UI_BADGE)
                return metric_width(node.display_text().c_str(), dpi) + scaled(20);
            return std::max(scaled(96), metric_width(node.display_text().c_str(), dpi) + scaled(20));
        }
        const double spacing = node.spacing < 0 ? scaled(style.gap) : scaled(node.spacing);
        if (node.kind == UI_GRID)
            return grid_columns(node,std::numeric_limits<double>::infinity(),spacing).ends.back() + 2*scaled(node.padding);
        if (node.kind == UI_WRAP_PANEL && node.vertical()) {
            const double height = std::clamp(node.height >= 0 ? node.height : node.layout.max_height, node.layout.min_height, node.layout.max_height);
            const double inner_height = std::max(0.0, scaled(height - 2 * node.padding));
            return vertical_wrap(node, inner_height, spacing, nullptr) + 2 * scaled(node.padding);
        }
        const bool row = (node.kind == UI_STACK_PANEL && !node.vertical()) || node.kind == UI_FLEX || node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR;
        double width = 0;
        for (const auto* child : node.layout_children) {
            width = row ? width + measure_width(*child) + spacing : std::max(width, measure_width(*child));
        }
        if (row && !node.layout_children.empty()) width -= spacing;
        if (node.kind == UI_PANEL || node.kind == UI_CARD) width += 2 * spacing;
        return width + 2 * scaled(node.padding);
    }

    double measure_width(const ui_node& node) const
    {
        TextFormatScope font_scope(node.resolved_font);
        auto& cache = node.layout_cache;
        if (cache.width_valid) { ++cache.measure_hits; return cache.width; }
        ++cache.measures;
        cache.width = compute_measure_width(node);
        cache.width_valid = true;
        return cache.width;
    }

    double compute_measure_width(const ui_node& node) const
    {
        if (node.visibility == UI_COLLAPSED) return 0;
        const auto& l = node.layout;
        if (!affine::trivial(node.layout_transform)) {
            const auto size = layout_source_size(node,std::numeric_limits<double>::infinity());
            return std::max(0.,std::abs(node.layout_transform.m11)*size.width +
                    std::abs(node.layout_transform.m21)*size.height + scaled(l.margin[0]+l.margin[2]));
        }
        return std::max(0.0, std::clamp(l.width >= 0 ? scaled(l.width) : natural_width(node), scaled(l.min_width), scaled(l.max_width)) + scaled(l.margin[0]) + scaled(l.margin[2]));
    }

    double natural_height(const ui_node& node, double width = std::numeric_limits<double>::infinity()) const
    {
        TextFormatScope font_scope(node.resolved_font);
        auto& cache = node.layout_cache;
        const double constraint=cache.width_dependent_height ? width : 0;
        if (cache.natural_height_valid && cache.natural_height_constraint==constraint) { ++cache.measure_hits; return cache.natural_height; }
        ++cache.measures;
        cache.natural_height_constraint=constraint;
        cache.natural_height = compute_natural_height(node,width);
        cache.natural_height_valid = true;
        return cache.natural_height;
    }

    double compute_natural_height(const ui_node& node, double width) const
    {
        InlineTextScope runs_scope(node.resolved_runs);
        if (drawing::shape_kind(node.kind)) return node.geometry ? scaled(node.geometry->size.height+node.stroke_thickness) : 0;
        if (node.kind == UI_VIEWBOX) {
            const double padding = 2*scaled(node.padding);
            if (node.layout_children.empty()) return padding;
            const auto& child = *node.layout_children.front();
            const double w = measure_width(child);
            if (w <= 0) return padding;
            const double h = measure(child,w);
            return padding + (std::isfinite(width) ? std::max(0.,width-padding)*h/w : h);
        }
        if (node.panel_callbacks) return panel_measure(node,width).height;
        const auto remaining=[&](double inset) { return std::max(0.0,width-inset); };
        if (node.kind == UI_CANVAS) return 2 * scaled(node.padding);
        if (node.kind == UI_BORDER) return (node.layout_children.empty() ? 0 : measure(*node.layout_children.front(),remaining(2*scaled(node.padding)+border_inset(node,0)+border_inset(node,2)))) +
            2 * scaled(node.padding) + border_inset(node, 1) + border_inset(node, 3);
        if (node.component) return node.layout_children.empty() ? 0 : measure(*node.layout_children.front(),width);
        if (has_content(node.kind)) {
            const double padding = scaled(node.padding + (node.kind == UI_BUTTON ? style.gap : 0));
            if (!node.layout_children.empty()) {
                const double height = measure(*node.layout_children.front(), remaining(2 * padding)) + 2 * padding;
                return node.kind == UI_BUTTON ? std::max(height, scaled(style.control_height)) : height;
            }
            if (node.kind == UI_BUTTON) return std::max(scaled(style.control_height), metric_line_height(dpi) + scaled(8));
            return (node.display_text().empty() ? 0 : metric_height(node.display_text().c_str(), dpi, remaining(2 * padding))) + 2 * padding;
        }
        if (node.kind == UI_DOCK_HOST) { return scaled(480); }
        if (node.kind == UI_DOCK_PANEL) { return dock_extent(node, false,remaining(2*scaled(node.padding))); }
        const double spacing = node.spacing < 0
            ? (node.kind == UI_TREE_VIEW || node.kind == UI_TREE_ITEM ? 0 : scaled(style.gap)) : scaled(node.spacing);
        if (is_popup(node.kind)) {
            return 0;
        }
        if (node.kind == UI_COMBO_BOX) {
            return std::max(scaled(style.control_height), metric_line_height(dpi) + scaled(8));
        }
        if (node.kind == UI_TAB_VIEW || node.kind == UI_NAVIGATION_VIEW) {
            const double header = scaled(style.control_height);
            return node.value < node.children.size() && node.children[node.value]->visibility != UI_COLLAPSED
                ? header + spacing + measure(*node.children[node.value],width) : header;
        }
        if (!is_container(node.kind)) {
            if (node.kind == UI_SWITCH) {
                return scaled(std::max(static_cast<int>(style.control_height), shape.toggle.height + 4));
            }
            if (node.kind == UI_SEPARATOR) {
                return scaled(8);
            }
            if (node.kind == UI_IMAGE || node.kind == UI_ICON) {
                return scaled(40);
            }
            if (node.kind == UI_TEXT_BLOCK) {
                const auto text_height = metric_height(node.display_text().c_str(), dpi, width) + scaled(8);
                return node.layout.min_height > 0 ? text_height : std::max(scaled(48), text_height);
            }
            if (node.kind == UI_TEXT_AREA || node.kind == UI_INFO_BAR) {
                return std::max(scaled(48), metric_line_height(dpi) + scaled(8));
            }
            return std::max(scaled(style.control_height), metric_line_height(dpi) + scaled(8));
        }
        if (node.kind == UI_TREE_VIEW) {
            double content = 0;
            for (const auto* child : node.layout_children) {
                content += measure(*child,remaining(2*scaled(node.padding))) + spacing;
            }
            return std::clamp(content, scaled(120), scaled(320));
        }
        if (node.kind == UI_SCROLL_VIEW || node.kind == UI_LIST_VIEW || node.kind == UI_DATA_GRID) {
            return scaled(120);
        }
        double height = 0;
        const bool row = (node.kind == UI_STACK_PANEL && !node.vertical()) || node.kind == UI_FLEX || node.kind == UI_OVERLAY ||
            node.kind == UI_SPLIT_VIEW || node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR;
        const double padding = shape.glass && (node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR)
            ? std::max(node.padding, 6.0) : node.padding;
        const double inner_width=remaining(2*scaled(padding)+((node.kind==UI_PANEL || node.kind==UI_CARD)?2*spacing:0));
        std::vector<Box> slots(node.layout_children.size());
        const bool tracks=node.kind==UI_FLEX || node.kind==UI_TOOL_BAR || node.kind==UI_MENU_BAR;
        if (node.kind == UI_GRID) {
            const auto columns = grid_columns(node,inner_width,spacing);
            return grid_rows(node,columns,std::numeric_limits<double>::infinity(),spacing).ends.back() + 2*scaled(padding);
        }
        if (tracks) row_slots(node,{0,0,inner_width,0},spacing,slots);
        double wrap_x=0, wrap_y=0, wrap_line=0;
        for (size_t i=0;i<node.layout_children.size();++i) {
            const auto* child=node.layout_children[i];
            double child_width=tracks ? slots[i].right-slots[i].left : inner_width;
            if (node.kind==UI_SPLIT_VIEW && std::isfinite(inner_width) && node.layout_children.size()>=2) {
                const double available=std::max(0.0,inner_width-spacing);
                child_width=i==0 ? available*int(node.value)/100 : available-available*int(node.value)/100;
            }
            if (node.kind==UI_WRAP_PANEL || (node.kind==UI_STACK_PANEL && !node.vertical())) child_width=measure_width(*child);
            const double child_height = measure(*child,child_width);
            height = row ? std::max(height, child_height) : height + child_height + spacing;
            if (node.kind==UI_WRAP_PANEL && !node.vertical()) {
                if (wrap_x && std::isfinite(inner_width) && wrap_x+child_width>inner_width) { wrap_x=0; wrap_y+=wrap_line+spacing; wrap_line=0; }
                wrap_line=std::max(wrap_line,child_height); wrap_x+=child_width+spacing;
            }
        }
        if (!row && !node.layout_children.empty()) {
            height -= spacing;
        }
        if (node.kind == UI_PANEL || node.kind == UI_CARD) {
            height += 2 * spacing;
        }
        if (node.kind == UI_TREE_ITEM) {
            height = scaled(std::min(style.control_height, 32u)) +
                (node.expanded && !node.layout_children.empty() ? height + spacing : 0);
        }
        if (node.kind == UI_WRAP_PANEL && !node.vertical()) {
            height = std::max(wrap_y+wrap_line, scaled(100));
        }
        return height + 2 * scaled(padding);
    }

    void row_slots(const ui_node& node,Box area,double spacing,std::vector<Box>& children) const {
        std::vector<layout::FractionalTrack> tracks;
        for (const auto* child:node.layout_children) {
            const auto& l=child->layout;
            const double basis=l.width>=0 || !std::isfinite(area.right-area.left) ? measure_width(*child) :
                std::max(0.0,scaled(l.min_width)+scaled(l.margin[0])+scaled(l.margin[2]));
            tracks.push_back({basis,l.width>=0 ? 0u : child->weight});
        }
        layout::flex(area,spacing,tracks,children);
    }

    void surface(Rect area, Rect clip, Color background, int radius)
    {
        rounded(commands, area, clip, background, px(radius));
    }

    void paint_scroll_bar(const ui_node& node, Box track, Box thumb, Rect clip, bool horizontal)
    {
        const auto& appearance = shape.scroll_bar;
        const bool pressed = node.dragging && node.dragging_horizontal == horizontal &&
            !(node.document && node.document->selecting) && !(node.table && node.table->resizing_column >= 0);
        const auto thickness = horizontal ? track.bottom-track.top : track.right-track.left;
        const auto padding = std::min(scaled(appearance.inset), thickness / 4);
        auto track_color = color(node, appearance.track);
        track_color[3] = appearance.track_opacity;
        surface(track, clip, track_color, appearance.radius);
        if (horizontal) {
            thumb.top += padding;
            thumb.bottom -= padding;
        } else {
            thumb.left += padding;
            thumb.right -= padding;
        }
        auto thumb_color = color(node, pressed ? appearance.pressed : appearance.thumb);
        if (!pressed) { thumb_color[3] = appearance.thumb_opacity; }
        surface(thumb, clip, thumb_color, appearance.radius);
    }

    void outlined(const ui_node& node, Rect area, Rect clip, Color background, int radius)
    {
        const int stroke = px(1);
        surface(area, clip, color(node, UI_THEME_DIVIDER), radius);
        rounded(commands, inset(area, stroke), clip, background, std::max(0, px(radius) - stroke));
    }

    void glass(const ui_node& node, Rect area, Rect clip)
    {
        drawing::glass(commands, area, clip, color(node, UI_THEME_PANEL), color(node, UI_THEME_DIVIDER), dpi, px(shape.popup_radius));
    }

    void leaf(ui_node& node, Box logical_area, Rect clip)
    {
        const Rect initial_area=logical_area;
        auto area=initial_area;
        TextFormatScope font_scope(node.resolved_font);
        InlineTextScope runs_scope(node.resolved_runs);
        const bool show_focus = keyboard_focus && &node == focus;
        const Color ink = color(node, node.is_enabled() ? UI_THEME_TEXT : UI_THEME_DIVIDER);
        const Color blue = color(node, UI_THEME_PRIMARY);
        const Color track = color(node, UI_THEME_CONTROL);
        const Color selected = color(node, UI_THEME_SELECTED);
        const bool disabled_button = !node.is_enabled() || (node.action_busy() && (is_button(node.kind) || node.kind == UI_HYPERLINK));
        const bool glass_bar = shape.glass && node.parent && (node.parent->kind == UI_TOOL_BAR || node.parent->kind == UI_MENU_BAR);
        const bool glass_navigation_item = shape.glass && node.kind == UI_TAB_ITEM &&
            node.parent && node.parent->kind == UI_NAVIGATION_VIEW;
        auto label = inset(area, gap());
        // Keep a complete text line in single-row controls at either density.
        label.top = area.top + std::max(0, (area.bottom - area.top - px(20)) / 2);
        label.bottom = area.bottom;
        if (node.kind == UI_TEXT_BLOCK || node.kind == UI_TEXT_AREA || node.kind == UI_INFO_BAR) {
            label.top = area.top + px(4);
            label.bottom = area.bottom - px(4);
        }
        if (node.kind == UI_TEXT_BLOCK) { label.left = area.left; label.right = area.right; }
        if (is_interactive(node.kind) || (node.kind == UI_LABEL && node.anchor)) {
            if (node.is_enabled()) { interactive.push_back(&node); }
            if (show_focus && !is_button(node.kind) && node.kind != UI_SWITCH && node.kind != UI_CHECK_BOX &&
                node.kind != UI_RADIO_BUTTON && node.kind != UI_SLIDER && node.kind != UI_TREE_ITEM &&
                !glass_navigation_item && !dock_editor(node)) {
                if (!disabled_button) {
                    surface(area, clip, color(node, UI_THEME_FOCUS), shape.input_radius);
                    area = inset(area, px(2));
                }
            }
        }
        if (node.kind == UI_TEXT_BLOCK) {
            const auto& text = node.display_text();
            const bool selectable = node.text_selectable && node.is_enabled() && !text.empty();
            HostedEditor selection;
            selection.reveal = false;
            if (selectable) {
                interactive.push_back(&node);
                if (&node == focus) {
                    const auto [first, last] = node.selection(text.size());
                    selection.first = first;
                    selection.last = last;
                }
            }
            EditorGeometry geometry{node.text_stops, 0, false, &selection, blue, color(node, UI_THEME_ON_PRIMARY)};
            auto* editor = selectable ? &geometry : nullptr;
#ifdef _WIN32
            const Box paragraph{logical_area.left,logical_area.top+scaled(4),logical_area.right,logical_area.bottom-scaled(4)};
            windows_text::append(commands, intersect(paragraph, clip), clip, text_foreground(ink), dpi, text.c_str(), false, editor, 0, &paragraph);
#else
            if (glass_bar && !selectable) {
                append_centered_text(commands, {label.left, area.top, label.right, area.bottom}, clip, ink, dpi, text.c_str());
            } else {
                append_text(commands, label, clip, ink, dpi, text.c_str(), false, editor);
            }
#endif
            return;
        }
        switch (node.kind) {
        case UI_BUTTON:
        case UI_TOGGLE_BUTTON:
        case UI_ICON_BUTTON: {
            const bool secondary = node.kind == UI_TOGGLE_BUTTON;
            const bool glass_action = glass_bar && !secondary && !disabled_button;
            auto foreground = glass_action ? blue : disabled_button || secondary ? ink : color(node, UI_THEME_ON_PRIMARY);
            auto background = disabled_button ? track : secondary ? (node.value ? selected : track) : blue;
            const int state = disabled_button ? 0 : &node == pressed ? 16 : &node == hovered ? 8 : 0;
            for (unsigned i = 0; i < 3; ++i) {
                background[i] = static_cast<std::uint8_t>((background[i] * (100 - state) +
                    foreground[i] * state + 50) / 100);
            }
            if (glass_action) {
                auto coating = &node == pressed || &node == hovered ? selected : track;
                coating[3] = &node == pressed ? 235 : &node == hovered ? 200 : 65;
                surface(area, clip, coating, shape.button_radius);
            } else if (secondary && !node.value && !disabled_button) {
                outlined(node, area, clip, background, shape.button_radius);
            } else {
                surface(area, clip, background, shape.button_radius);
            }
            if (show_focus && !disabled_button) {
                // Reverse the whole face so keyboard focus is apparent away from the edges.
                const auto face = color(node, UI_THEME_ON_PRIMARY);
                foreground = blue;
                surface(area, clip, face, shape.button_radius);
                drawing::rounded(commands, area, clip, blue, px(shape.button_radius), px(4));
            }
            if (node.kind == UI_ICON_BUTTON && !node.image.rgb.empty()) {
                image(node.image, label, clip, 16);
                label.left += px(20);
            }
            if (node.kind == UI_BUTTON && !node.layout_children.empty()) return;
            if (glass_bar) { append_centered_text(commands, {label.left, area.top, label.right, area.bottom}, clip, foreground, dpi, node.text.c_str()); }
            else { append_text(commands, label, clip, foreground, dpi, node.text.c_str(), true); }
            return;
        }
        case UI_HYPERLINK:
            append_text(commands, label, clip, disabled_button ? ink : blue, dpi, node.text.c_str(), true);
            return;
        case UI_BADGE:
        case UI_INFO_BAR:
            fill(commands, area, clip, node.kind == UI_BADGE ? blue : track);
            append_text(commands, label, clip, node.kind == UI_BADGE ? color(node, UI_THEME_ON_PRIMARY) : ink,
                        dpi, node.text.c_str());
            return;
        case UI_SPINNER: {
            const int cell = px(5);
            for (int i = 0; i < 8; ++i) {
                const int x = label.left + i * (cell + px(2));
                fill(commands, {x, label.top, x + cell, label.top + cell}, clip,
                     static_cast<unsigned>(i) == node.value % 8 ? blue : track);
            }
            return;
        }
        case UI_SPIN_BOX: {
            fill(commands, area, clip, track);
            const auto number = std::to_wstring(node.value) + L"     - / +";
            append_text(commands, label, clip, ink, dpi, number.c_str(), true);
            return;
        }
        case UI_TEXT_BOX:
        case UI_SEARCH_BOX:
        case UI_PASSWORD_BOX:
        case UI_TEXT_AREA: {
            const bool editing = &node == focus;
#ifdef _WIN32
            Box paragraph=inset(logical_area,scaled(style.gap));
            paragraph.top=logical_area.top+std::max(0.0,(logical_area.bottom-logical_area.top-scaled(20))/2);
            paragraph.bottom=logical_area.bottom;
            if (node.kind==UI_TEXT_AREA) { paragraph.top=logical_area.top+scaled(4); paragraph.bottom=logical_area.bottom-scaled(4); }
            label=paragraph;
#endif
            node.text_bounds = label;
            EditorGeometry geometry{node.text_stops, node.caret,
                editing && caret_visible && !node.select_all};
            auto* hosted = editing ? &node.hosted_editor : nullptr;
            geometry.trailing=node.caret_trailing;
            if (hosted && !hosted->composing) {
                const auto [first,last]=node.selection();
                hosted->active=true; hosted->first=first; hosted->last=last; hosted->caret=node.caret;
            }
            const auto& displayed = hosted && hosted->composing ? hosted->draft : node.text;
            if (hosted) {
                geometry.hosted = hosted; geometry.index = hosted->caret;
                geometry.show = (caret_visible || hosted->composing) && hosted->first == hosted->last;
                geometry.selection = blue; geometry.selected_text = color(node, UI_THEME_ON_PRIMARY);
            }
            // A root document editor is the group's surface. The group owns
            // its border and corners; nested editors retain their own outline.
            if (dock_editor(node)) { fill(commands, area, clip, track); }
            else { outlined(node, area, clip, track, shape.input_radius); }
            auto text_color = ink;
            if (&node == focus && node.select_all && !hosted) {
                fill(commands, label, clip, blue);
                text_color = color(node, UI_THEME_ON_PRIMARY);
            }
            if (node.kind == UI_PASSWORD_BOX) {
                const std::wstring mask(displayed.size(), L'\u2022');
#ifdef _WIN32
                windows_text::append(commands,label,clip,text_foreground(text_color),dpi,mask.c_str(),true,&geometry,0,&paragraph,true);
#else
                append_text(commands, label, clip, text_color, dpi, mask.c_str(), true, &geometry);
#endif
#ifdef _WIN32
                const auto boundaries = windows_text::clusters(displayed);
                std::erase_if(node.text_stops, [&](const auto& stop) {
                    return !std::binary_search(boundaries.begin(), boundaries.end(), stop.index);
                });
#endif
            } else if (node.kind == UI_SEARCH_BOX && node.text.empty() && !editing) {
                append_text(commands, label, clip, ink, dpi, L"Search", true);
                node.text_stops = {{label.left, label.top, 0}};
            } else {
#ifdef _WIN32
                windows_text::append(commands,label,clip,text_foreground(text_color),dpi,displayed.c_str(),node.kind!=UI_TEXT_AREA,&geometry,0,&paragraph,true);
#else
                append_text(commands, label, clip, text_color, dpi, displayed.c_str(), node.kind != UI_TEXT_AREA, &geometry);
#endif
            }
            return;
        }
        case UI_TREE_ITEM: {
            if (node.tree_selected || &node == hovered || &node == pressed) {
                surface(area, clip, node.tree_selected || &node == pressed ? selected : track, shape.input_radius);
            }
            if (show_focus) {
                const int stroke = px(2);
                const auto ring = color(node, UI_THEME_FOCUS);
                fill(commands, {area.left, area.top, area.right, area.top + stroke}, clip, ring);
                fill(commands, {area.left, area.bottom - stroke, area.right, area.bottom}, clip, ring);
                fill(commands, {area.left, area.top, area.left + stroke, area.bottom}, clip, ring);
                fill(commands, {area.right - stroke, area.top, area.right, area.bottom}, clip, ring);
            }
            const int side = px(12);
            const int top = (area.top + area.bottom - side) / 2;
            if (node.items_source ? node.item_count() > 0 : !node.layout_children.empty()) {
                tree_chevron(commands, {area.left + px(8), top, area.left + px(8) + side, top + side},
                             clip, ink, node.expanded);
            }
            label.left = area.left + px(28);
            append_text(commands, label, clip, ink, dpi, node.display_text().c_str(), true);
            return;
        }
        case UI_LIST_ITEM:
        case UI_TAB_ITEM: {
            const bool current = node.is_selected_item();
            if (glass_navigation_item) {
                // Leave the glass rim and its highlights visible around the row.
                const int side = std::min(px(6), (area.right - area.left) / 4);
                const int end = px(3);
                const Rect highlight{area.left + side, area.top + end, area.right - side, area.bottom - end};
                const int radius = shape.navigation_item_radius;
                if (current || &node == hovered || &node == pressed) {
                    auto coating = selected;
                    coating[3] = current ? 255 : &node == pressed ? 220 : 145;
                    surface(highlight, clip, coating, radius);
                }
                if (show_focus && node.is_enabled()) {
                    drawing::rounded(commands, highlight, clip, color(node, UI_THEME_FOCUS), px(radius), px(1));
                }
                label.left += side;
                label.right -= side;
            } else {
                surface(area, clip, current ? selected : track, shape.input_radius);
            }
            append_text(commands, label, clip, ink,
                        dpi, node.text.c_str());
            return;
        }
        case UI_COMBO_BOX: {
            outlined(node, area, clip, track, shape.input_radius);
            const int center_x = area.right - gap() - px(5);
            const int center_y = (area.top + area.bottom) / 2;
            const int step = px(1);
            for (int row = 0; row < 4; ++row) {
                const int left = center_x - (4 - row) * step;
                const int right = center_x + (4 - row) * step;
                const int y = center_y + (row - 2) * step;
                fill(commands, {left, y, left + 2 * step, y + step}, clip, ink);
                fill(commands, {right - step, y, right + step, y + step}, clip, ink);
            }
            label.right = center_x - px(8);
            if ((!node.parent || node.parent->kind != UI_MENU_BAR) && node.value < node.children.size() && node.children[node.value]->visibility != UI_COLLAPSED) {
                if (node.kind == UI_BUTTON && !node.layout_children.empty()) return;
            if (glass_bar) { append_centered_text(commands, {label.left, area.top, label.right, area.bottom}, clip, ink, dpi, node.children[node.value]->text.c_str()); }
                else { append_text(commands, label, clip, ink, dpi, node.children[node.value]->text.c_str()); }
            } else {
                if (node.kind == UI_BUTTON && !node.layout_children.empty()) return;
            if (glass_bar) { append_centered_text(commands, {label.left, area.top, label.right, area.bottom}, clip, ink, dpi, node.text.c_str()); }
                else { append_text(commands, label, clip, ink, dpi, node.text.c_str()); }
            }
            return;
        }
        case UI_SWITCH: {
            const auto& toggle = shape.toggle;
            const int height = px(toggle.height);
            const int width = px(toggle.width);
            const int left = label.left;
            const int top = (area.top + area.bottom - height) / 2;
            const Rect bounds{left, top, left + width, top + height};
            const auto outline = color(node, UI_THEME_DIVIDER);
            if (show_focus) {
                rounded(commands, inset(bounds, -px(2)), clip, color(node, UI_THEME_FOCUS), height / 2 + px(2));
            }
            const int stroke = px(toggle.stroke);
            const auto background = node.value ? blue : &node == hovered ? selected : track;
            if (!node.value) {
                rounded(commands, bounds, clip, outline, height / 2);
            }
            rounded(commands, node.value ? bounds : inset(bounds, stroke), clip, background,
                    height / 2 - (node.value ? 0 : stroke));
            const int diameter = px(&node == pressed ? toggle.pressed_thumb :
                                    node.value ? toggle.on_thumb : toggle.off_thumb);
            const int thumb_top = top + (height - diameter) / 2;
            const int margin = std::max(stroke, (height - diameter) / 2);
            const int thumb_left = node.value ? bounds.right - margin - diameter : left + margin;
            const Color thumb = toggle.light_thumb ? Color{255, 255, 255, 255} :
                node.value ? color(node, UI_THEME_ON_PRIMARY) : outline;
            rounded(commands, {thumb_left, thumb_top, thumb_left + diameter, thumb_top + diameter},
                    clip, thumb, diameter / 2);
            label.left += width + gap();
            break;
        }
        case UI_CHECK_BOX:
        case UI_RADIO_BUTTON: {
            const bool radio = node.kind == UI_RADIO_BUTTON;
            const int side = px(radio ? 20 : 18);
            const int top = (area.top + area.bottom - side) / 2;
            const int left = label.left;
            const Rect indicator{left, top, left + side, top + side};
            const int radius = radio ? 10 : 3;
            if (show_focus) {
                surface(inset(indicator, -px(2)), clip, color(node, UI_THEME_FOCUS), radius + 2);
            }
            const auto background = &node == pressed || &node == hovered ? selected : track;
            if (node.value && !radio) {
                surface(indicator, clip, blue, radius);
                const auto mark = color(node, UI_THEME_ON_PRIMARY);
                for (int step = 0; step < 7; ++step) {
                    const int x = left + px(5 + step);
                    const int y = top + px(step < 3 ? 9 + step : 13 - step);
                    fill(commands, {x, y, x + px(2), y + px(2)}, clip, mark);
                }
            } else {
                surface(indicator, clip, node.value ? blue : color(node, UI_THEME_DIVIDER), radius);
                surface(inset(indicator, px(radio ? 2 : 1)), clip, background, radius - 1);
                if (node.value) {
                    surface(inset(indicator, px(5)), clip, blue, 5);
                }
            }
            label.left += side + gap();
            break;
        }
        case UI_SLIDER:
        case UI_PROGRESS_BAR: {
            const bool slider = node.kind == UI_SLIDER;
            const int y = (area.top + area.bottom) / 2;
            const int half_track = px(slider ? 2 : 3);
            const Rect bar{label.left, y - half_track, label.right, y + half_track};
            const int end = bar.left + (bar.right - bar.left) * static_cast<int>(node.value) / 100;
            rounded(commands, bar, clip, track, half_track);
            rounded(commands, {bar.left, bar.top, end, bar.bottom}, clip, blue, half_track);
            if (slider) {
                if (show_focus) {
                    drawing::rounded(commands, area, clip, color(node, UI_THEME_FOCUS),
                                     px(shape.input_radius), px(2));
                }
                const int radius = px(shape.slider_thumb_radius);
                const Rect thumb{end - radius, y - radius, end + radius, y + radius};
                if (show_focus || &node == hovered || node.dragging) {
                    rounded(commands, inset(thumb, -px(2)), clip,
                            color(node, show_focus ? UI_THEME_FOCUS : UI_THEME_SELECTED), radius + px(2));
                }
                rounded(commands, thumb, clip, blue, radius);
                if (shape.slider_ring) {
                    rounded(commands, inset(thumb, px(3)), clip, color(node, UI_THEME_ON_PRIMARY), radius - px(3));
                }
            }
            return;
        }
        case UI_SEPARATOR:
            if (node.vertical()) fill(commands,{area.left+px(3),area.top,area.left+px(4),area.bottom},clip,track);
            else fill(commands, {area.left, area.top + px(3), area.right, area.top + px(4)}, clip, track);
            return;
        case UI_IMAGE:
        case UI_ICON: {
            image(resolved_image(node), area, clip, node.kind == UI_ICON ? 20 : 36, node.stretch);
            return;
        }
        default:
            break;
        }
        if (glass_bar) { append_centered_text(commands, {label.left, area.top, label.right, area.bottom}, clip, ink, dpi, node.display_text().c_str()); }
        else { append_text(commands, label, clip, ink, dpi, node.display_text().c_str()); }
    }

    Box arrange(const ui_node& node, Box area) const
    {
        auto& cache = node.layout_cache;
        const auto& slot = cache.slot;
        if (cache.arrange_valid && slot.left == area.left && slot.top == area.top &&
            slot.right == area.right && slot.bottom == area.bottom) {
            ++cache.arrange_hits; return cache.arranged;
        }
        ++cache.arranges;
        cache.slot = area;
        cache.arranged = compute_arrange(node, area);
        cache.arrange_valid = true;
        return cache.arranged;
    }

    Box compute_arrange(const ui_node& node, Box area) const
    {
        const auto& l = node.layout;
        area.left += scaled(l.margin[0]); area.top += scaled(l.margin[1]);
        area.right = std::max(area.left, area.right - scaled(l.margin[2]));
        area.bottom = std::max(area.top, area.bottom - scaled(l.margin[3]));
        const auto align = [](double& start, double& end, double desired, unsigned alignment) {
            const double extra = std::max(0.0, end - start - desired);
            if (alignment == 2) start += extra / 2;
            if (alignment == 3) start += extra;
            end = start + desired;
        };
        double width = l.width >= 0 ? scaled(l.width) : l.horizontal == 0 ? area.right - area.left : natural_width(node);
        width = std::clamp(width, scaled(l.min_width), scaled(l.max_width));
        double height = node.height >= 0 ? scaled(node.height) : l.vertical == 0 ? area.bottom - area.top : natural_height(node,width);
        height = std::clamp(height, scaled(l.min_height), scaled(l.max_height));
        align(area.left, area.right, width, l.horizontal);
        align(area.top, area.bottom, height, l.vertical);
        if (is_button(node.kind) && l.width < 0 && l.horizontal == 0 && !l.horizontal_set) {
            const auto parent = node.parent ? node.parent->kind : UI_STACK_PANEL;
            if (parent == UI_STACK_PANEL || parent == UI_SCROLL_VIEW || parent == UI_PANEL ||
                parent == UI_CARD || parent == UI_TAB_ITEM || parent == UI_TREE_ITEM ||
                parent == UI_TOOL_BAR || is_popup(parent)) {
                area.right = area.left + std::clamp(std::min(area.right - area.left, button_width(node)), scaled(l.min_width), scaled(l.max_width));
            }
        }
        return area;
    }

    void viewbox(ui_node& node, Box area, Rect clip) {
        if (node.layout_children.empty()) return;
        auto& child = *node.layout_children.front();
        area = inset(area,scaled(node.padding));
        const double width = measure_width(child), height = measure(child,width);
        if (width <= 0 || height <= 0) return;
        const auto [sx,sy] = stretch_scale(node.stretch.value_or(UI_STRETCH_UNIFORM),(area.right-area.left)/width,(area.bottom-area.top)/height);
        if (sx <= 0 || sy <= 0) return;
        const double left = area.left + ((area.right-area.left)-width*sx)/2;
        const double top = area.top + ((area.bottom-area.top)-height*sy)/2;
        transformed({sx,0,0,sy,left,top}, clip, [&](Rect source_clip) {
            draw(child,{0,0,width,height},source_clip);
        });
    }

    template<class Draw>
    void transformed(const ui_matrix& matrix, Rect clip, Draw draw_local) {
        const auto inverse = affine::inverse(matrix);
        if (!inverse) return;
        const auto source = affine::bounds(*inverse,clip);
        const Rect source_clip = Box{std::floor(source.left),std::floor(source.top),
                                     std::ceil(source.right),std::ceil(source.bottom)};
        std::vector<ui_node*> local_order;
        auto* saved_order = paint_order;
        if (!paint_order) paint_order = &local_order;
        const auto first_node = paint_order->size();
        const auto first_command = commands.size();
        if (list) {
            list->flush(commands);
            list->clip(clip);
            list->transform(matrix);
        }
        draw_local(source_clip);
        if (list) { list->flush(commands); list->restore(); list->restore(); }
        drawing::transform(commands,first_command,matrix,clip);
        if (list) list->consumed(commands.size());
        const auto bounds = [&](Rect r) -> Rect {
            return empty(r) ? Rect{} : Rect(affine::bounds(matrix,r));
        };
        for (size_t i = first_node; i < paint_order->size(); ++i) {
            auto& n = *(*paint_order)[i];
            n.world_transform = affine::compose(n.world_transform,matrix);
            n.bounds = bounds(n.bounds);
            n.visible = intersect(bounds(n.visible),clip);
            n.text_bounds = bounds(n.text_bounds);
            n.scrollbar = bounds(n.scrollbar);
            n.scrollbar_thumb = bounds(n.scrollbar_thumb);
            n.horizontal_bar = bounds(n.horizontal_bar);
            n.horizontal_thumb = bounds(n.horizontal_thumb);
            n.viewport = bounds(n.viewport);
            auto caret = affine::map(matrix,n.hosted_editor.caret_x,n.hosted_editor.caret_y);
            n.hosted_editor.caret_x = int(std::round(caret.x));
            n.hosted_editor.caret_y = int(std::round(caret.y));
        }
        paint_order = saved_order;
    }

    void draw(ui_node& node, Box area, Rect clip) {
        node.world_transform = affine::identity;
        const bool layout = !affine::trivial(node.layout_transform);
        const auto paint_layer = [&](Rect source_clip) {
            const auto start = commands.size();
            auto* recorder=list;
            const bool bitmap=node.effect.kind!=UI_EFFECT_NONE || node.brushes[UI_BRUSH_OPACITY_MASK] ||
                node.kind==UI_DOCK_HOST || drawing::shape_kind(node.kind) || node.kind==UI_IMAGE || node.kind==UI_ICON ||
                node.brushes[UI_BRUSH_BACKGROUND] || node.brushes[UI_BRUSH_OUTLINE];
            const bool glyphs=node.kind==UI_TEXT_BLOCK || is_editor(node.kind);
            if (recorder) {
                recorder->flush(commands);
                if (bitmap || glyphs) list=nullptr;
                if (!bitmap && node.opacity<1) recorder->layer(node.opacity);
            }
            draw_content(node,area,source_clip);
            if (recorder && !bitmap) {
                recorder->flush(commands,glyphs ? drawing::CommandKind::glyph_coverage : drawing::CommandKind::rectangles);
                if (node.opacity<1) recorder->restore();
            }
            drawing::effect_layer(commands,start,node.effect,node.brushes[UI_BRUSH_OPACITY_MASK].get(),
                                  node.bounds,source_clip,node.opacity,dpi/96.);
            list=recorder;
            if (recorder) {
                if (bitmap) recorder->flush(commands,drawing::CommandKind::image);
                recorder->consumed(commands.size());
            }
        };
        if (!layout && affine::trivial(node.render_transform)) {
            paint_layer(clip);
        } else {
            auto placement = affine::identity;
            if (layout) {
                const auto size = layout_source_size(node,area.right-area.left);
                const auto& l = node.layout;
                const double left = area.left+scaled(l.margin[0]), top = area.top+scaled(l.margin[1]);
                const auto target = affine::bounds(node.layout_transform,{0,0,size.width,size.height});
                const double spare_x = std::max(0.,area.right-scaled(l.margin[2])-left-target.right+target.left);
                const double spare_y = std::max(0.,area.bottom-scaled(l.margin[3])-top-target.bottom+target.top);
                placement = node.layout_transform;
                placement.offset_x += left-target.left + (l.horizontal==2 ? spare_x/2 : l.horizontal==3 ? spare_x : 0);
                placement.offset_y += top-target.top + (l.vertical==2 ? spare_y/2 : l.vertical==3 ? spare_y : 0);
                area = {-scaled(l.margin[0]),-scaled(l.margin[1]),size.width+scaled(l.margin[2]),size.height+scaled(l.margin[3])};
            }
            const Box box = arrange(node,area);
            const auto render = affine::around(node.render_transform,
                box.left + (box.right-box.left)*node.transform_origin.x,
                box.top + (box.bottom-box.top)*node.transform_origin.y,dpi/96.);
            const auto matrix = affine::compose(render,placement);
            if (!affine::inverse(matrix)) {
                node.world_transform = matrix;
                node.visible = {}; node.bounds = affine::bounds(matrix,box);
                return;
            }
            transformed(matrix,clip,paint_layer);
        }

    }

    void draw_content(ui_node& node, Box area, Rect clip)
    {
        TextFormatScope font_scope(node.resolved_font);
        if (node.visibility == UI_COLLAPSED) return;
        if (!node.is_visible()) { node.bounds = arrange(node,area); return; }
        if (is_popup(node.kind)) {
            node.bounds = area;
            node.visible = intersect(area, clip);
            node.local_hit = node.visible;
            if (node.value && area.top >= clip.top && area.top < clip.bottom) popups.push_back(&node);
            return;
        }
        area = arrange(node, area);
        node.bounds = area;
        node.visible = intersect(area, clip);
        node.local_hit = node.visible;
        node.horizontal_bar = {}; node.horizontal_thumb = {};
        node.scrollbar = {};
        node.scrollbar_thumb = {};
        const bool viewport = node.kind == UI_SCROLL_VIEW || node.kind == UI_LIST_VIEW ||
            node.kind == UI_TREE_VIEW || node.kind == UI_DATA_GRID;
        const bool clip_children = node.clip_to_bounds || viewport || !is_container(node.kind);
        if (clip_children && empty(node.visible)) return;
        if (paint_order && !empty(node.visible)) paint_order->push_back(&node);
        if (clip_children) clip = node.visible;
        // A component boundary contributes scope and behavior, while its visual root owns appearance.
        if (node.component) {
            if (is_interactive(node.kind) && node.is_enabled() && !empty(node.visible)) interactive.push_back(&node);
            for (auto* child : node.layout_children) { draw(*child, area, clip); }
            return;
        }
        if (drawing::shape_kind(node.kind)) {
            drawing::paint_shape(node.kind,node.geometry.get(),node.brushes[UI_BRUSH_BACKGROUND].get(),
                node.brushes[UI_BRUSH_OUTLINE].get(),node.stroke_thickness,area,clip,dpi/96.,commands);
            return;
        }
        if (node.kind == UI_BORDER) {
            if (!node.brushes[UI_BRUSH_BACKGROUND] && !node.brushes[UI_BRUSH_OUTLINE]) {
                border::paint(node.border,area,clip,dpi,commands);
            } else {
                for (auto target : {UI_BRUSH_BACKGROUND, UI_BRUSH_OUTLINE}) {
                    auto style = node.border;
                    const auto& brush = node.brushes[target];
                    if (target == UI_BRUSH_BACKGROUND) {
                        style.brush = 0;
                        if (brush) style.background = 0xffffffff;
                    } else {
                        style.background = 0;
                        if (brush) style.brush = 0xffffffff;
                    }
                    const auto start = commands.size();
                    border::paint(style,area,clip,dpi,commands);
                    if (brush) drawing::shade(commands,start,*brush,area,dpi/96.);
                }
            }
            if (!node.layout_children.empty()) {
                auto content = inset(area, scaled(node.padding));
                content.left += border_inset(node, 0); content.top += border_inset(node, 1);
                content.right = std::max(content.left, content.right - border_inset(node, 2));
                content.bottom = std::max(content.top, content.bottom - border_inset(node, 3));
                draw(*node.layout_children.front(), content, clip);
            }
            return;
        }
        if (has_content(node.kind)) {
            const bool button = node.kind == UI_BUTTON;
            const auto content = inset(area, scaled(node.padding + (button ? style.gap : 0)));
            if (button) leaf(node, area, node.visible);
            if (!node.layout_children.empty()) draw(*node.layout_children.front(), content, clip);
            else if (!button && !node.display_text().empty())
                append_text(commands, content, clip, text_foreground(color(node, UI_THEME_TEXT)), dpi, node.display_text().c_str(), false);
            return;
        }
        if (node.collection_item_id && node.parent && node.parent->item_template && !node.layout_children.empty()) {
            if (node.kind == UI_LIST_ITEM) {
                surface(area,clip,color(node,node.is_selected_item() ? UI_THEME_SELECTED : UI_THEME_CONTROL),shape.input_radius);
                if (node.is_enabled()) interactive.push_back(&node);
            }
            draw(*node.layout_children.front(),area,clip);
            return;
        }
        if (node.kind == UI_VIEWBOX) { viewbox(node,area,clip); return; }
        if (node.kind == UI_DOCUMENT_VIEW) {
            document::paint(node, area, clip, style, dpi, commands);
            if (!empty(node.scrollbar)) {
                paint_scroll_bar(node, node.scrollbar, node.scrollbar_thumb, clip, false);
            }
            if (node.is_enabled()) interactive.push_back(&node);
            scrollable.push_back(&node);
            return;
        }
        if (node.kind == UI_DOCK_HOST && node.workspace) {
            docking::paint(*node.workspace, node.dock_window, area, clip, style, shape, dpi, commands,
                [&](ui_node& content, Rect bounds, Rect content_clip) {
                    const auto* previous = dock_surface; dock_surface = &content;
                    // Floating roots are synthetic hosts and do not own content in children.
                    // Prepare the selected content for this window even before the main window's first paint.
                    if (prepare_dock_content) (*prepare_dock_content)(content);
                    prepare_layout(content);
                    draw(content, bounds, content_clip); dock_surface = previous;
                });
            return;
        }
        if (node.kind == UI_DOCK_DOCUMENT || node.kind == UI_DOCK_TOOL) {
            for (auto* child : node.layout_children) { draw(*child, area, clip); }
            return;
        }
        if (node.kind == UI_COMBO_BOX) {
            leaf(node, area, clip);
            if (node.expanded) {
                popups.push_back(&node);
            }
            return;
        }
        if (node.kind == UI_TAB_VIEW || node.kind == UI_NAVIGATION_VIEW) {
            const double header = scaled(style.control_height);
            const bool vertical = node.kind == UI_NAVIGATION_VIEW;
            std::vector<Box> headers(node.layout_children.size());
            const std::vector<layout::FractionalTrack> tracks(headers.size(), {0, 1});
            const double rail = std::min(scaled(120), (area.right - area.left) / 2);
            if (vertical) {
                if (shape.glass) {
                    glass(node, Box{area.left, area.top, area.left + rail, area.bottom}, clip);
                }
                const std::vector<double> heights(headers.size(), header);
                layout::stack({area.left, area.top, area.left + rail, area.bottom}, scaled(style.gap), heights, headers);
            } else {
                layout::grid({area.left, area.top, area.right, area.top + header}, scaled(style.gap), tracks, headers);
            }
            for (std::size_t i = 0; i < headers.size(); ++i) {
                auto& tab = *node.layout_children[i];
                tab.bounds = arrange(tab, headers[i]);
                tab.visible = intersect(tab.bounds, clip);
                if (tab.visibility != UI_VISIBLE) tab.visible = {};
                if (!empty(tab.visible)) {
                    leaf(tab, tab.bounds, tab.visible);
                }
            }
            if (node.value < node.children.size() && node.children[node.value]->visibility == UI_VISIBLE) {
                double top = vertical ? area.top : area.top + header + scaled(style.gap);
                const double left = vertical ? area.left + rail + scaled(style.gap) : area.left;
                for (auto* child : node.children[node.value]->layout_children) {
                    const double height = child->kind == UI_SCROLL_VIEW && child->height < 0
                        ? std::max(0.0, area.bottom - top) : measure(*child,area.right-left);
                    draw(*child, {left, top, area.right, top + height}, clip);
                    top += height + scaled(style.gap);
                }
            }
            return;
        }
        if (node.kind == UI_TREE_ITEM) {
            const double height = scaled(std::min(style.control_height, 32u));
            node.visible = intersect({area.left, area.top, area.right, area.top + height}, clip);
            if (!empty(node.visible)) {
                leaf(node, Box{area.left, area.top, area.right, area.top + height}, node.visible);
            }
            if (node.expanded) {
                const double spacing = node.spacing < 0 ? 0 : scaled(node.spacing);
                double top = area.top + height + spacing;
                for (auto* child : node.layout_children) {
                    const double child_height = measure(*child,std::max(0.0,area.right-area.left-scaled(24)));
                    draw(*child, {area.left + scaled(24), top, area.right, top + child_height}, clip);
                    top += child_height + spacing;
                }
            }
            return;
        }
        if (!is_container(node.kind)) {
            leaf(node, area, clip);
            return;
        }
        const double spacing = node.spacing < 0 ? (node.kind == UI_TREE_VIEW ? 0 : scaled(style.gap)) : scaled(node.spacing);
        if (shape.glass && (node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR)) {
            glass(node, area, clip);
        }
        if (node.kind == UI_PANEL || node.kind == UI_CARD) {
            const Color panel = color(node, UI_THEME_PANEL);
            const Color card = color(node, UI_THEME_CARD);
            if (node.kind == UI_CARD) {
                outlined(node, area, clip, card, shape.card_radius);
            } else {
                fill(commands, area, clip, panel);
            }
            if (!node.panel_callbacks) area = inset(area, spacing);
        }
        const double padding = shape.glass && (node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR)
            ? std::max(node.padding, 6.0) : node.padding;
        area = inset(area, scaled(padding));
        if (empty(area) && clip_children) return;
        const bool virtual_list = (node.kind == UI_LIST_VIEW || node.kind == UI_DATA_GRID) && node.items_source;
        double column_origin=area.left;
        double row_height = 0, row_step = 0;
        if (virtual_list) {
            row_height = std::max(scaled(style.control_height),metric_line_height(dpi)+scaled(8));
            row_step = std::max(1.0,row_height+spacing);
            if (node.kind==UI_DATA_GRID && node.table) {
                auto& table=*node.table;
                const auto bounds=area;
                double width = 0;
                for (const auto& col : table.columns) width+=scaled(col.width);
                const double body_width=std::max(0.0,area.right-area.left-scaled(14));
                node.horizontal_limit=std::max(0.0,width-body_width);
                node.horizontal_offset=std::clamp(node.horizontal_offset,0.0,node.horizontal_limit);
                if (node.horizontal_limit) {
                    area.bottom=std::max(area.top,area.bottom-scaled(12));
                    node.horizontal_bar=Box{area.left,area.bottom,area.right-scaled(14),bounds.bottom};
                }
                const double header_bottom=std::min(area.bottom,area.top+row_height);
                table.header=Box{area.left,area.top,area.right,header_bottom};
                uint32_t begin=0, end=0;
                double x=area.left-node.horizontal_offset;
                while (begin<table.columns.size() && x+scaled(table.columns[begin].width)<=area.left) {
                    x+=scaled(table.columns[begin++].width);
                }
                column_origin=x;
                table.left=x;
                end=begin;
                while (end<table.columns.size() && x<area.right) {
                    auto& col=table.columns[end++];
                    double right=x+scaled(col.width);
                    Box head{x,area.top,right,header_bottom};
                    fill(commands,head,clip,color(node,UI_THEME_CONTROL));
                    append_text(commands,inset(head,scaled(3)),intersect(clip,head),color(node,UI_THEME_TEXT),dpi,col.header.c_str());
                    x=right;
                }
                if (!table.columns.empty()) prepare_grid_columns(node,begin,end-begin);
                area.top=header_bottom;
            }
            const double height = std::max(0.0,area.bottom-area.top);
            const auto total = static_cast<uint32_t>(node.item_count());
            node.scroll_limit = std::max(0.0, total*row_step - spacing - height);
            node.offset = std::clamp(node.offset,0.0,node.scroll_limit);
            uint32_t first = static_cast<uint32_t>(std::min<double>(total,node.offset/row_step));
            if (first) --first;
            const uint32_t count = static_cast<uint32_t>(std::min<double>(total-first,std::ceil(height/row_step)+3));
            if (realize_list_range(node,first,count) != UI_OK) return;
            node.layout_children = node.children;
            for (auto* child : node.layout_children) prepare_layout(*child);
            scrollable.push_back(&node);
            if (node.scroll_limit) {
                node.scrollbar = Box{std::max(area.left,area.right-scaled(10)),area.top,area.right,area.bottom};
                area.right = std::max(area.left,node.scrollbar.left-scaled(4));
            }
        }
        std::vector<Box> children(node.layout_children.size());
        Rect child_clip = clip;
        if (virtual_list) {
            child_clip=intersect(child_clip,area);
            for (size_t i=0;i<children.size();++i) {
                const double top = area.top+(node.first_realized+i)*row_step-node.offset;
                children[i] = {column_origin,top,area.right,top+row_height};
            }
        } else if (node.panel_callbacks) {
            auto sizes = panel_children(node,area.right-area.left);
            std::vector<ui_layout_rect> slots(children.size());
            const auto& cb = *node.panel_callbacks;
            const ui_size final_size{(area.right-area.left)*96/dpi,(area.bottom-area.top)*96/dpi};
            {
                PanelCallbackScope guard(node.owner);
                if (cb.arrange(cb.context,final_size,sizes.data(),uint32_t(sizes.size()),slots.data()) == UI_OK &&
                    std::all_of(slots.begin(),slots.end(),[](const auto& r) {
                        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) && r.width >= 0 && r.height >= 0;
                    })) node.panel_slots.swap(slots);
            }
            if (node.panel_slots.size() != children.size()) return;
            for (size_t i=0; i<children.size(); ++i) {
                const auto& r = node.panel_slots[i];
                children[i] = {area.left+scaled(r.x),area.top+scaled(r.y),area.left+scaled(r.x+r.width),area.top+scaled(r.y+r.height)};
            }
        } else if (node.kind == UI_SCROLL_VIEW) {
            const auto bounds = area;
            const double bar_size = scaled(10), reserve = bar_size + scaled(4);
            double desired_width = 0;
            if (node.horizontal_scroll != UI_SCROLL_DISABLED)
                for (auto* child : node.layout_children) desired_width = std::max(desired_width, measure_width(*child));
            bool horizontal = node.horizontal_scroll == UI_SCROLL_VISIBLE;
            bool vertical = node.vertical_scroll == UI_SCROLL_VISIBLE;
            std::vector<double> heights(children.size());
            double content_width = 0, content_height = 0;
            for (int pass = 0; pass < 3; ++pass) {
                area = bounds;
                if (vertical) area.right = std::max(area.left, area.right-reserve);
                if (horizontal) area.bottom = std::max(area.top, area.bottom-reserve);
                content_width = std::max(area.right-area.left, desired_width);
                content_height = scaled(style.gap);
                for (size_t i = 0; i < children.size(); ++i) {
                    heights[i] = measure(*node.layout_children[i],content_width);
                    content_height += heights[i] + (i ? spacing : 0);
                }
                bool show_x = horizontal || (node.horizontal_scroll == UI_SCROLL_AUTO && desired_width > area.right-area.left);
                bool show_y = vertical || (node.vertical_scroll == UI_SCROLL_AUTO && content_height > area.bottom-area.top);
                if (show_x == horizontal && show_y == vertical) break;
                horizontal = show_x; vertical = show_y;
            }
            node.viewport = area;
            child_clip = intersect(area, clip);
            node.horizontal_limit = std::max(0., content_width-(area.right-area.left));
            node.scroll_limit = node.vertical_scroll == UI_SCROLL_DISABLED ? 0 : std::max(0., content_height-(area.bottom-area.top));
            node.offset = std::clamp(node.offset, 0., node.scroll_limit);
            node.horizontal_offset = std::clamp(node.horizontal_offset, 0., node.horizontal_limit);
            if (horizontal) node.horizontal_bar = Box{area.left,std::max(bounds.top,bounds.bottom-bar_size),area.right,bounds.bottom};
            if (vertical) node.scrollbar = Box{std::max(bounds.left,bounds.right-bar_size),area.top,bounds.right,area.bottom};
            scrollable.push_back(&node);
            area.left -= node.horizontal_offset;
            area.right = area.left + content_width;
            area.top -= node.offset;
            layout::stack(area,spacing,heights,children);
        } else if (node.kind == UI_CANVAS) {
            for (size_t i = 0; i < children.size(); ++i) {
                const auto& child = *node.layout_children[i];
                const double x = area.left + scaled(child.canvas_left);
                const double y = area.top + scaled(child.canvas_top);
                const double width = measure_width(child);
                children[i] = {x, y, x + width, y + measure(child, width)};
            }
        } else if (node.kind == UI_OVERLAY) {
            std::fill(children.begin(), children.end(), area);
        } else if (node.kind == UI_STACK_PANEL && !node.vertical()) {
            double x = area.left;
            for (size_t i=0;i<children.size();++i) {
                const double width = measure_width(*node.layout_children[i]);
                children[i] = {x,area.top,x+width,area.bottom};
                x += width+spacing;
            }
        } else if (node.kind == UI_WRAP_PANEL && node.vertical()) {
            vertical_wrap(node, area.bottom - area.top, spacing, &children);
            for (auto& slot : children) {
                slot.left += area.left;
                slot.right += area.left;
                slot.top += area.top;
                slot.bottom += area.top;
            }
        } else if (node.kind == UI_WRAP_PANEL) {
            double x = area.left, y = area.top, line_height = 0;
            for (std::size_t i = 0; i < children.size(); ++i) {
                const auto& child = *node.layout_children[i];
                const double width = measure_width(child);
                const double height = measure(child,width);
                if (x != area.left && x + width > area.right) {
                    x = area.left;
                    y += line_height + spacing;
                    line_height = 0;
                }
                children[i] = {x, y, x + width, y + height};
                x += width + spacing;
                line_height = std::max(line_height, height);
            }
        } else if (node.kind == UI_DOCK_PANEL) {
            auto available = area;
            for (std::size_t i = 0; i < children.size(); ++i) {
                const auto& child = *node.layout_children[i];
                auto& rect = children[i];
                rect = available;
                if (i + 1 == children.size()) {
                    break;
                }
                const double width = measure_width(child);
                const double height = std::min(available.bottom - available.top, measure(child,available.right-available.left));
                switch (child.dock) {
                case UI_DOCK_LEFT:
                    rect.right = rect.left + width;
                    available.left = std::min(available.right, rect.right + spacing);
                    break;
                case UI_DOCK_RIGHT:
                    rect.left = rect.right - width;
                    available.right = std::max(available.left, rect.left - spacing);
                    break;
                case UI_DOCK_TOP:
                    rect.bottom = rect.top + height;
                    available.top = std::min(available.bottom, rect.bottom + spacing);
                    break;
                case UI_DOCK_BOTTOM:
                    rect.top = rect.bottom - height;
                    available.bottom = std::max(available.top, rect.top - spacing);
                    break;
                }
            }
        } else if (node.kind == UI_SPLIT_VIEW && children.size() < 2) {
            if (!children.empty()) children[0] = area;
        } else if (node.kind == UI_SPLIT_VIEW) {
            const double split = area.left + std::max(0.0, area.right - area.left - spacing) *
                static_cast<int>(node.value) / 100;
            const Box divider{split, area.top, split + spacing, area.bottom};
            fill(commands, divider, clip, color(node, node.dragging ? UI_THEME_SELECTED : UI_THEME_DIVIDER));
            const double center = (divider.top + divider.bottom) / 2;
            const double inset_x = std::max(0.0, (divider.right - divider.left - scaled(2)) / 2);
            fill(commands, {divider.left + inset_x, std::max(divider.top, center - scaled(10)),
                            divider.right - inset_x, std::min(divider.bottom, center + scaled(10))},
                 clip, color(node, node.dragging ? UI_THEME_ON_PRIMARY : UI_THEME_PRIMARY));
            node.visible = intersect(divider, clip);
            if (node.is_enabled()) { interactive.push_back(&node); }
            if (!children.empty()) {
                children[0] = {area.left, area.top, split, area.bottom};
            }
            if (children.size() > 1) {
                children[1] = {split + spacing, area.top, area.right, area.bottom};
            }
        } else if (node.kind == UI_GRID) {
            const auto columns = grid_columns(node,area.right-area.left,spacing);
            const auto rows = grid_rows(node,columns,area.bottom-area.top,spacing);
            for (size_t i=0; i<children.size(); ++i) {
                const auto& cell = node.layout_children[i]->grid.cell;
                const auto [left,right] = columns.range(cell.column,cell.columns);
                const auto [top,bottom] = rows.range(cell.row,cell.rows);
                children[i] = {area.left+left,area.top+top,area.left+right,area.top+bottom};
            }
        } else if (node.kind == UI_FLEX ||
                   node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR) {
            row_slots(node,area,spacing,children);
        } else {
            std::vector<double> heights;
            double content = scaled(style.gap);
            for (const auto* child : node.layout_children) {
                heights.push_back(measure(*child,area.right-area.left));
                content += heights.back();
            }
            if (!heights.empty()) {
                content += spacing * static_cast<int>(heights.size() - 1);
            }
            if (node.kind == UI_SCROLL_VIEW || node.kind == UI_LIST_VIEW ||
                node.kind == UI_TREE_VIEW || node.kind == UI_DATA_GRID) {
                node.scroll_limit = std::max(0.0,content-(area.bottom-area.top));
                node.offset = std::clamp(node.offset, 0.0, node.scroll_limit);
                scrollable.push_back(&node);
                if (node.scroll_limit && area.right > area.left && area.bottom > area.top) {
                    const double track_width = std::min(scaled(10), area.right - area.left);
                    node.scrollbar = Box{area.right - track_width, area.top, area.right, area.bottom};
                    area.right = std::max(area.left, area.right-track_width-scaled(4));
                    // A scrollbar narrows the paragraph. Recompute the same
                    // constraints that will be used to draw the content.
                    content=scaled(style.gap);
                    for (size_t i=0;i<heights.size();++i) {
                        heights[i]=measure(*node.layout_children[i],area.right-area.left);
                        content+=heights[i];
                    }
                    if (!heights.empty()) content+=spacing*int(heights.size()-1);
                    node.scroll_limit=std::max(0.0,content-(area.bottom-area.top));
                    node.offset=std::clamp(node.offset,0.0,node.scroll_limit);
                }
                area.top-=node.offset; area.bottom=area.top+content;
            }
            layout::stack(area, spacing, heights, children);
        }
        const auto lower_z = [](const ui_node* a,const ui_node* b) { return a->z_index < b->z_index; };
        std::vector<size_t> paint_indices;
        if (!std::is_sorted(node.layout_children.begin(),node.layout_children.end(),lower_z)) {
            paint_indices.resize(children.size());
            std::iota(paint_indices.begin(),paint_indices.end(),size_t{0});
            std::stable_sort(paint_indices.begin(),paint_indices.end(),[&](size_t a,size_t b) {
                return lower_z(node.layout_children[a],node.layout_children[b]);
            });
        }
        for (size_t step = 0; step < children.size(); ++step) {
            const size_t i = paint_indices.empty() ? step : paint_indices[step];
            if (node.kind == UI_TOOL_BAR || node.kind == UI_MENU_BAR) {
                const double height = std::min(children[i].bottom - children[i].top, measure(*node.layout_children[i],children[i].right-children[i].left));
                children[i].top += std::max(0.0, (children[i].bottom - children[i].top - height) / 2);
                children[i].bottom = children[i].top + height;
            }
            draw(*node.layout_children[i], children[i], child_clip);
        }
        if (!empty(node.horizontal_bar)) {
            const auto bar = node.horizontal_bar;
            const double width = bar.right - bar.left;
            const double thumb = std::clamp(width*width/(width+node.horizontal_limit),std::min(width,scaled(16)),width);
            const double left = bar.left + (node.horizontal_limit ? (width-thumb)*node.horizontal_offset/node.horizontal_limit : 0);
            node.horizontal_thumb = Box{left,bar.top,left+thumb,bar.bottom};
            paint_scroll_bar(node, bar, node.horizontal_thumb, clip, true);
        }
        if (!empty(node.scrollbar)) {
            const auto bar = node.scrollbar;
            const double height = bar.bottom - bar.top;
            const double thumb = std::clamp(static_cast<double>(height * height /
                                         (height + node.scroll_limit)),
                                         std::min(height, scaled(16)), height);
            const double top = bar.top + (node.scroll_limit ? (height - thumb) * node.offset / node.scroll_limit : 0);
            node.scrollbar_thumb = Box{double(bar.left), top, double(bar.right), top + thumb};
            paint_scroll_bar(node, bar, node.scrollbar_thumb, clip, false);
        }
    }

    void draw_popups(Rect viewport)
    {
        for (std::size_t i = 0; i < popups.size(); ++i) {
            auto& popup = *popups[i];
            TextFormatScope font_scope(popup.resolved_font);
            if (popup.anchor && empty(popup.anchor->visible)) {
                continue;
            }
            Box anchor = popup.anchor ? popup.anchor->visible : popup.bounds;
            if (popup.anchor && is_button(popup.anchor->kind)) {
                // Keep room for popup content when the anchor button uses its intrinsic width.
                anchor.right = double(std::min(viewport.right, popup.anchor->bounds.right));
            }
            const bool dialog = popup.kind == UI_DIALOG;
            const double margin = dialog ? (viewport.right - viewport.left) / 8 : 0;
            const double popup_width=std::max(0.0,anchor.right-anchor.left-2*margin-scaled(style.gap));
            double content = scaled(style.gap);
            for (const auto* child : popup.layout_children) {
                content += measure(*child,popup_width) + scaled(style.gap);
            }
            const double top = dialog ? std::max(double(viewport.top), (viewport.top + viewport.bottom - content) / 2)
                : std::max(double(viewport.top), std::min(anchor.bottom, viewport.bottom - content));
            Box area{anchor.left + margin, top, anchor.right - margin,
                            std::min(double(viewport.bottom), top + content)};
            area = arrange(popup, area);
            popup.bounds = area;
            if (overlays) {
                overlays->push_back({&popup, dialog ? viewport : Rect(area)});
            }
            popup.visible = intersect(area, viewport);
            if (paint_order) { paint_order->push_back(&popup); }
            if (dialog) {
                interactive.clear();
                scrollable.clear();
                fill(commands, viewport, viewport, color(popup, UI_THEME_SCRIM));
            }
            if (shape.glass) {
                glass(popup, area, viewport);
            } else {
                outlined(popup, area, viewport, color(popup, UI_THEME_PANEL), shape.popup_radius);
            }
            const auto content_area = inset(area, scaled(style.gap) / 2);
            double y = content_area.top;
            for (auto* child : popup.layout_children) {
                const double height = measure(*child,content_area.right-content_area.left);
                draw(*child, {content_area.left, y, content_area.right, y + height}, intersect(content_area, viewport));
                y += height + scaled(style.gap);
            }
        }
    }
};
}

bool is_container(ui_node_kind kind)
{
    return kind == UI_VIEWBOX || has_content(kind) || kind == UI_CANVAS || kind == UI_BORDER || (kind >= UI_STACK_PANEL && kind <= UI_CARD) || is_popup(kind) ||
        kind == UI_COMBO_BOX || kind == UI_ITEMS_CONTROL || kind == UI_LIST_VIEW ||
        kind == UI_TAB_VIEW || kind == UI_TAB_ITEM || (kind >= UI_OVERLAY && kind <= UI_SPLIT_VIEW) ||
        (kind >= UI_TREE_VIEW && kind <= UI_NAVIGATION_VIEW) || kind == UI_TREE_ITEM ||
        (kind >= UI_DOCK_HOST && kind <= UI_DOCK_TOOL);
}

bool is_interactive(ui_node_kind kind)
{
    return kind == UI_BUTTON || (kind >= UI_CHECK_BOX && kind <= UI_SLIDER) ||
        kind == UI_COMBO_BOX || is_editor(kind) || kind == UI_LIST_ITEM || kind == UI_TAB_ITEM ||
        (kind >= UI_ICON_BUTTON && kind <= UI_SPIN_BOX) || kind == UI_TREE_ITEM ||
        kind == UI_SPLIT_VIEW || kind == UI_DOCUMENT_VIEW;
}

bool is_editor(ui_node_kind kind)
{
    return kind == UI_TEXT_BOX || kind == UI_PASSWORD_BOX || kind == UI_TEXT_AREA || kind == UI_SEARCH_BOX;
}

bool is_popup(ui_node_kind kind)
{
    return kind == UI_POPUP || kind == UI_DIALOG || kind == UI_TOOLTIP || kind == UI_CONTEXT_MENU;
}

bool is_selector(ui_node_kind kind)
{
    return kind == UI_COMBO_BOX || kind == UI_LIST_VIEW || kind == UI_TAB_VIEW ||
        kind == UI_NAVIGATION_VIEW;
}

double measure_tree_height(ui_node& root, double available_width, GalleryStyle style, ThemeShape shape, unsigned dpi)
{
    if (std::isnan(available_width) || available_width < 0) throw std::invalid_argument("Invalid measurement width");
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> interactive, scrollable;
    Painter painter{style, shape, dpi, nullptr, false, false, nullptr, nullptr, commands, interactive, scrollable};
    painter.prepare_layout(root);
    painter.prepare_groups();
    return painter.measure(root, available_width);
}

void render_tree_with_pick_order(ui_node& root, layout::Rect area, GalleryStyle style, ThemeShape shape, unsigned dpi,
                 const ui_node* focus, bool keyboard_focus, bool caret_visible,
                 const ui_node* hovered, const ui_node* pressed,
                 std::vector<FillRectCommand>& commands,
                 std::vector<ui_node*>& interactive, std::vector<ui_node*>& scrollable,
                 std::vector<UiOverlay>* overlays, std::vector<ui_node*>* paint_order,drawing::CommandList* list,
                 const std::function<void(ui_node&)>& prepare_dock_content)
{
    interactive.clear();
    scrollable.clear();
    if (overlays) { overlays->clear(); }
    if (paint_order) { paint_order->clear(); }
    Painter painter{style, shape, dpi, focus, keyboard_focus, caret_visible, hovered, pressed,
                    commands, interactive, scrollable};
    if (prepare_dock_content) painter.prepare_dock_content = &prepare_dock_content;
    painter.list = list;
    painter.overlays = overlays;
    painter.paint_order = paint_order;
    painter.prepare_layout(root);
    painter.prepare_groups();
    painter.draw(root, area, area);
    std::stable_partition(painter.popups.begin(), painter.popups.end(),
        [](const ui_node* node) { return node->kind != UI_DIALOG; });
    painter.draw_popups(area);
}

void render_tree(ui_node& root, layout::Rect area, GalleryStyle style, ThemeShape shape, unsigned dpi,
                 const ui_node* focus, bool keyboard_focus, bool caret_visible,
                 const ui_node* hovered, const ui_node* pressed,
                 std::vector<FillRectCommand>& commands,
                 std::vector<ui_node*>& interactive, std::vector<ui_node*>& scrollable,
                 std::vector<UiOverlay>* overlays)
{
    render_tree_with_pick_order(root, area, style, shape, dpi, focus, keyboard_focus, caret_visible,
        hovered, pressed, commands, interactive, scrollable, overlays, nullptr);
}
