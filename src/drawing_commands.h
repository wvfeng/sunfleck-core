#pragma once
#include "raster.h"

namespace drawing {
enum class CommandKind { rectangles, glyph_coverage, image, transform, clip, layer, restore };
struct Command {
    CommandKind kind;
    ui_matrix matrix = affine::identity;
    layout::Rect bounds{};
    double opacity = 1;
    std::vector<FillRectCommand> spans;
};

// The reference painter produces pixel spans. Record unaffected spans between
// state changes, retaining the original coordinate space for GPU replay.
class CommandList {
    size_t cursor_ = 0;
public:
    std::vector<Command> commands;

    void flush(const std::vector<FillRectCommand>& spans, CommandKind kind=CommandKind::rectangles) {
        if (cursor_ < spans.size()) {
            auto& op=commands.emplace_back(Command{kind});
            op.spans.assign(spans.begin()+cursor_,spans.end());
        }
        cursor_=spans.size();
    }
    void consumed(size_t count) { cursor_=count; }
    void transform(ui_matrix matrix) { commands.push_back({CommandKind::transform,matrix}); }
    void clip(layout::Rect bounds) { commands.push_back({CommandKind::clip,affine::identity,bounds}); }
    void layer(double alpha) { commands.push_back({CommandKind::layer,affine::identity,{},alpha}); }
    void restore() { commands.push_back({CommandKind::restore}); }
};
}
