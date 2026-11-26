#include "gallery.h"
#include "layout.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <tuple>

#include "dwrite_text.h"
#include "text_format.h"
#ifdef SUNFLECK_MACOS
#include "macos_text.h"
#endif

namespace {
thread_local unsigned document_style = 0;
using Color = std::array<std::uint8_t, 4>;
int px(int logical, unsigned dpi)
{
    return (logical * dpi + 48) / 96;
}

using Viewport = layout::Rect;

void fill(std::vector<FillRectCommand>& commands, Viewport bounds, Viewport clip, Color color)
{
    const int left = std::max(bounds.left, clip.left);
    const int top = std::max(bounds.top, clip.top);
    const int right = std::min(bounds.right, clip.right);
    const int bottom = std::min(bounds.bottom, clip.bottom);
    if (left < right && top < bottom) {
        commands.push_back({left, top, right, bottom, color});
    }
}


}

FillRectCommand gallery_button(GalleryStyle style, bool active, unsigned dpi)
{
    const Color color = theme_rgb(style.colors[active ? UI_THEME_SELECTED : UI_THEME_PRIMARY]);
    return {px(24, dpi), px(24, dpi), px(144, dpi),
            px(24 + style.control_height, dpi), color};
}

int text_width(const wchar_t* text, unsigned dpi)
{
#ifdef _WIN32
    return windows_text::width(text,dpi,document_style);
#elif defined(SUNFLECK_FREETYPE)
    return freetype_text_width(text, dpi);
#elif defined(SUNFLECK_MACOS)
    return macos_text::width(text, dpi, document_style);
#else
    (void)text;
    (void)dpi;
    return 0;
#endif
}

void document_text_style(unsigned flags) {
    document_style = flags;
#ifdef SUNFLECK_FREETYPE
    freetype_document_style(flags);
#endif
}
int text_line_height(unsigned dpi)
{
    thread_local unsigned cached_dpi = 0, cached_style = 0;
    thread_local int cached_height = 0;
    thread_local TextFormat cached_font;
    thread_local uint64_t cached_revision = 0;
    if (cached_dpi == dpi && cached_style == document_style && cached_font == active_text_format() && cached_revision == text_resource_revision && cached_height) { return cached_height; }
    int height = px(18, dpi);
#ifdef _WIN32
    height = windows_text::line_height(dpi,document_style);
#elif defined(SUNFLECK_FREETYPE)
    height = freetype_text_height(dpi);
#elif defined(SUNFLECK_MACOS)
    height = macos_text::line_height(dpi, document_style);
#endif
    cached_revision = text_resource_revision;
    cached_font = active_text_format(); cached_dpi = dpi; cached_style = document_style; cached_height = height; return height;
}
void append_centered_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                          std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text)
{
    bounds.top += std::max(0, (bounds.bottom - bounds.top - text_line_height(dpi)) / 2);
    append_text(commands, bounds, clip, color, dpi, text, true);
}
int text_height(const wchar_t* text,unsigned dpi,int width) {
#ifdef _WIN32
    return windows_text::height(text,dpi,width,document_style);
#elif defined(SUNFLECK_FREETYPE)
    return freetype_paragraph_height(text, dpi, width);
#elif defined(SUNFLECK_MACOS)
    return macos_text::height(text, dpi, width, document_style);
#else
    int lines=1;
    for (auto p=text;*p;++p) if (*p==L'\n') ++lines;
    (void)width;
    return lines*text_line_height(dpi);
#endif
}
void append_rotated_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                         std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text, bool clockwise)
{
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) { return; }
    const auto rotate = [&](layout::Rect r) -> layout::Rect {
        return clockwise ? layout::Rect{bounds.right - r.bottom, bounds.top + r.left, bounds.right - r.top, bounds.top + r.right} :
            layout::Rect{bounds.left + r.top, bounds.bottom - r.right, bounds.left + r.bottom, bounds.bottom - r.left};
    };
    const auto inverse = [&](layout::Rect r) -> layout::Rect {
        return clockwise ? layout::Rect{r.top - bounds.top, bounds.right - r.right, r.bottom - bounds.top, bounds.right - r.left} :
            layout::Rect{bounds.bottom - r.bottom, r.left - bounds.left, bounds.bottom - r.top, r.right - bounds.left};
    };
    clip = {std::max(clip.left, bounds.left), std::max(clip.top, bounds.top), std::min(clip.right, bounds.right), std::min(clip.bottom, bounds.bottom)};
    if (clip.right <= clip.left || clip.bottom <= clip.top) { return; }
    std::vector<FillRectCommand> horizontal;
    // Text spans already contain their antialiased backdrop. Rotate that
    // backdrop into text space too, retaining the same CPU/GPU appearance.
    for (const auto& command : commands) {
        const layout::Rect r{std::max(command.left, clip.left), std::max(command.top, clip.top),
            std::min(command.right, clip.right), std::min(command.bottom, clip.bottom)};
        if (r.right > r.left && r.bottom > r.top) {
            const auto v = inverse(r); horizontal.push_back({v.left, v.top, v.right, v.bottom, command.color});
        }
    }
    std::wstring label(text);
    if (text_width(label.c_str(), dpi) > height) {
        const std::wstring ellipsis = L"\u2026";
        do {
            const auto last = label.back(); label.pop_back();
            if constexpr (sizeof(wchar_t) == 2) {
                if (last >= 0xdc00 && last <= 0xdfff && !label.empty()) { label.pop_back(); }
            }
        } while (!label.empty() && text_width((label + ellipsis).c_str(), dpi) > height);
        label += ellipsis;
    }
    const auto first = horizontal.size();
    const int inset = std::max(0, (height - text_width(label.c_str(), dpi)) / 2);
    append_centered_text(horizontal, {inset, 0, height, width}, inverse(clip), color, dpi, label.c_str());
    for (size_t i = first; i < horizontal.size(); ++i) {
        const auto& command = horizontal[i]; const auto r = rotate({command.left, command.top, command.right, command.bottom});
        commands.push_back({r.left, r.top, r.right, r.bottom, command.color});
    }
}
std::array<uint8_t,4> text_foreground(std::array<uint8_t,4> fallback) {
    const auto& format=active_text_format();
    if (!format.foreground_set) return fallback;
    const auto c=format.foreground;
    return {uint8_t(c>>16),uint8_t(c>>8),uint8_t(c),uint8_t(c>>24)};
}
void append_text(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
                 std::array<std::uint8_t, 4> color, unsigned dpi, const wchar_t* text,
                 bool single_line, EditorGeometry* editor, bool use_foreground)
{
    if (use_foreground) color=text_foreground(color);
#ifdef _WIN32
    windows_text::append(commands, bounds, clip, color, dpi, text, single_line, editor, document_style);
#elif defined(SUNFLECK_FREETYPE)
    append_freetype_text(commands, bounds, clip, color, dpi, text, single_line, editor);
#elif defined(SUNFLECK_MACOS)
    macos_text::append(commands, bounds, clip, color, dpi, text, single_line, editor, document_style);
#else
    (void)commands;
    (void)bounds;
    (void)clip;
    (void)color;
    (void)dpi;
    (void)text;
    (void)single_line;
    (void)editor;
#endif
}

int gallery_scroll_limit(GalleryStyle style, int height, unsigned dpi)
{
    const int gap = px(style.gallery_gap, dpi);
    const int top = gallery_button(style, false, dpi).bottom + gap;
    const int content_height = 2 * px(48, dpi) + 3 * gap;
    return layout::scroll_limit(height - px(24, dpi) - top, content_height);
}

void append_gallery(std::vector<FillRectCommand>& commands, GalleryStyle style,
                    int width, int height, unsigned dpi, int scroll, const GalleryImage& image)
{
    const int padding = px(24, dpi);
    const int gap = px(style.gallery_gap, dpi);
    const int top = gallery_button(style, false, dpi).bottom + gap;
    const int right = width - padding;
    const int bottom = height - padding;
    if (right - padding <= gap || bottom - top <= 2 * gap) {
        return;
    }
    const Color panel = theme_rgb(style.colors[UI_THEME_PANEL]);
    const Color card = theme_rgb(style.colors[UI_THEME_CARD]);
    const Viewport clip{padding, top, right, bottom};
    fill(commands, clip, clip, panel);
    const int inner_left = padding + gap;
    const int inner_right = right - gap;
    if (inner_right - inner_left <= 2 * gap) {
        return;
    }
    const int row_height = px(48, dpi);
    const auto content = layout::scroll(clip, 2 * row_height + 3 * gap, scroll);
    Viewport rows[2];
    const int heights[] = {row_height, row_height};
    layout::stack({inner_left, content.top + gap, inner_right, content.bottom}, gap, heights, rows);
    Viewport cards[2];
    const layout::Track columns[] = {{0, 1}, {0, 1}};
    layout::grid(rows[0], gap, columns, cards);
    fill(commands, cards[0], clip, card);
    fill(commands, cards[1], clip, card);
#if defined(_WIN32) || defined(SUNFLECK_FREETYPE) || defined(SUNFLECK_MACOS)
    const Color ink = theme_rgb(style.colors[UI_THEME_TEXT]);
    append_text(commands, {cards[0].left + gap, cards[0].top + gap, cards[0].right - gap, cards[0].bottom},
                 clip, ink, dpi, L"Sunfleck\n\u521D\u8424");
    append_text(commands, {cards[1].left + gap, cards[1].top + gap, cards[1].right - gap, cards[1].bottom},
                 clip, ink, dpi, L"Sunfleck \u4E3B\u9898\n\u5916\u89C2");
#endif
    const int next_top = rows[1].top;
    fill(commands, rows[1], clip, card);
    Viewport image_row[2];
    const layout::Track items[] = {{px(24, dpi), 0}, {0, 1}};
    layout::flex({inner_left + gap, next_top + gap, inner_right - gap, rows[1].bottom}, gap, items, image_row);

    // An opaque bitmap uses the same fill commands on CPU and GLES.
    constexpr unsigned char bitmap[5][5] = {
        {0, 0, 1, 0, 0}, {0, 1, 1, 1, 0}, {1, 1, 2, 1, 1},
        {0, 1, 1, 1, 0}, {0, 0, 1, 0, 0}
    };
    const Color colors[] = {{}, theme_rgb(style.colors[UI_THEME_PRIMARY]),
                            theme_rgb(style.colors[UI_THEME_SELECTED])};
    const int cell = px(4, dpi);
    const int image_left = image_row[0].left;
    const int image_top = image_row[0].top;
    const Viewport image_clip{inner_left, std::max(next_top, top), inner_right,
                              std::min(next_top + row_height, bottom)};
    if (!image.rgb.empty()) {
        const int size = px(20, dpi);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const auto index = ((y * image.height / size) * image.width +
                    x * image.width / size) * 3;
                const Color color{image.rgb[index], image.rgb[index + 1], image.rgb[index + 2], 255};
                fill(commands, {image_left + x, image_top + y, image_left + x + 1,
                                image_top + y + 1}, image_clip, color);
            }
        }
        return;
    }
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            if (const auto index = bitmap[y][x]) {
                const int left = image_left + x * cell;
                const int upper = image_top + y * cell;
                fill(commands, {left, upper, left + cell, upper + cell}, image_clip, colors[index]);
            }
        }
    }
}
