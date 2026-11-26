#pragma once
#include "visual_layout.h"
#include "layout.h"
#include "text_format.h"
#include "inline_text.h"
#include <string>
#include <vector>

struct ui_node;

// Exact dependency snapshots also cover native input paths which still mutate
// fields directly. No hashes are used to decide whether geometry is reusable.
struct LayoutInputs {
    VisualLayout layout;
    TextFormat font;
    std::vector<StyledRun> runs;
    std::array<double, 4> border_thickness{};
    std::wstring text;
    std::vector<ui_node*> children;
    double height = -1;
    double spacing = -1, padding = 0;
    unsigned weight = 1, value = 0;
    unsigned dpi = 0, gap = 0, control_height = 0;
    int toggle_height = 0;
    ui_visibility visibility = UI_VISIBLE;
    ui_node_kind kind = UI_STACK_PANEL, parent_kind = UI_STACK_PANEL;
    ui_dock_edge dock = UI_DOCK_LEFT;
    bool expanded = false, image = false, component = false, glass = false;
    uint64_t font_revision = 0;
};

struct LayoutCache {
    LayoutInputs inputs;
    bool initialized = false;
    bool width_dependent_height = false;
    mutable bool height_valid = false, width_valid = false;
    mutable bool natural_height_valid = false, natural_width_valid = false, arrange_valid = false;
    mutable double height = 0, width = 0, natural_height = 0, natural_width = 0;
    mutable double height_constraint = 0, natural_height_constraint = 0;
    mutable layout::Box slot{}, arranged{};
    mutable uint64_t measures = 0, arranges = 0, measure_hits = 0, arrange_hits = 0;
    uint64_t invalidations = 0;
    void invalidate_measure() {
        height_valid = width_valid = natural_height_valid = natural_width_valid = arrange_valid = false;
        ++invalidations;
    }
};

// Called for metadata-driven component changes; ordinary built-in fields are
// reconciled before rendering, including input edits and child reordering.
void invalidate_layout(ui_node& node, uint32_t flags);
