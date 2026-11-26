#pragma once
#include "raster.h"
#include "layout.h"
#include "theme.h"
#include <string>

struct TextStop {
    int x, y;
    std::size_t index;
    bool trailing = false; // Previous cluster's edge at a bidi/soft-wrap boundary.
    bool rtl = false; // Direction of the shaping cluster owning this edge.
};

// Transient hosted editing state. Drafts never replace the node's bound value.
struct HostedEditor {
    bool active = false, composing = false, reveal = true;
    std::wstring draft;
    std::size_t first = 0, last = 0, caret = 0, composition_first = 0, composition_last = 0;
    int scroll_x = 0, scroll_y = 0, caret_x = 0, caret_y = 0;
};

struct EditorGeometry {
    std::vector<TextStop>& stops;
    std::size_t index;
    bool show;
    HostedEditor* hosted = nullptr;
    std::array<std::uint8_t, 4> selection{}, selected_text{};
    bool trailing = false;
};

#ifdef SUNFLECK_FREETYPE
ui_result freetype_set_font(const void* bytes, size_t size);
void freetype_document_style(unsigned flags);
ui_result freetype_document_font(unsigned role, const void* bytes, size_t size);
int freetype_text_width(const wchar_t* text, unsigned dpi);
int freetype_text_height(unsigned dpi);
int freetype_paragraph_height(const wchar_t* text, unsigned dpi, int width);
void append_freetype_text(std::vector<FillRectCommand>& commands, layout::Rect bounds,
                          layout::Rect clip, std::array<std::uint8_t, 4> color, unsigned dpi,
                          const wchar_t* text, bool single_line, EditorGeometry* editor);
#endif

// Font resources are thread-local, as are layout and text rendering.
inline thread_local uint64_t text_resource_revision = 0;
void document_text_style(unsigned flags);

using GalleryStyle = ui_theme;

struct GalleryImage {
    std::vector<std::uint8_t> rgb;
    unsigned width = 0;
    unsigned height = 0;
    unsigned channels = 3;
};

FillRectCommand gallery_button(GalleryStyle style, bool active, unsigned dpi);
void append_gallery(std::vector<FillRectCommand>& commands, GalleryStyle style,
                    int width, int height, unsigned dpi, int scroll, const GalleryImage& image);
int gallery_scroll_limit(GalleryStyle style, int height, unsigned dpi);
int text_width(const wchar_t* text, unsigned dpi);
int text_line_height(unsigned dpi);
int text_height(const wchar_t* text, unsigned dpi, int width);
std::array<uint8_t,4> text_foreground(std::array<uint8_t,4> fallback);
void append_centered_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                          std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text);
void append_rotated_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                         std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text, bool clockwise);
void append_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                 std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text,
                 bool single_line = false, EditorGeometry* editor = nullptr, bool use_foreground = true);
