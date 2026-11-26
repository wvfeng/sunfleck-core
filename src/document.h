#pragma once
#include "gallery.h"
#include "text_format.h"
#include "inline_text.h"
#include <memory>
#include <map>

struct ui_node;
namespace document {
struct Span {
    std::wstring text;
    std::string href, color;
    unsigned flags = 0;
};
struct Block {
    std::string kind, id, language;
    std::vector<Span> spans;
    unsigned level = 0, indent = 0;
    std::string raw;
    GalleryImage image;
};
struct Glyph {
    layout::Rect bounds;
    std::wstring text;
    unsigned dpi, flags;
    std::size_t index, block;
    std::string href, color;
    bool rtl = false;
};
struct Box { layout::Rect bounds; size_t block; };
struct Paragraph {
    std::wstring text;
    TextFormat format;
    std::vector<StyledRun> runs;
    std::vector<unsigned> colors;
    layout::Rect bounds;
    unsigned dpi=96;
    size_t start=0;
};
struct State {
    std::vector<Block> blocks;
    std::vector<Glyph> glyphs;
    std::vector<Box> boxes;
    std::multimap<size_t,Paragraph> paragraphs;
    std::map<std::string, int> anchors;
    std::wstring plain;
    int width = -1, height = 0;
    unsigned dpi = 0;
    TextFormat format;
    uint64_t font_revision = 0;
    size_t first = 0, last = 0;
    int down_x = 0, down_y = 0;
    bool selecting = false;
    std::map<size_t, int> horizontal;
    std::string pending_anchor;
    std::string event_value;
    ui_document_event event = UI_DOCUMENT_NONE;
};
std::shared_ptr<State> parse(std::string_view json);
void paint(ui_node& node, layout::Rect area, layout::Rect clip, GalleryStyle style,
           unsigned dpi, std::vector<FillRectCommand>& commands);
void pointer(ui_node& node, int action, int x, int y);
bool horizontal(ui_node& node, int x, int y, int delta);
std::string selected(const State& state);
}
