#pragma once
#ifdef _WIN32
#include "gallery.h"
#include "font_resources.h"
#include <string_view>

namespace windows_text {
void discard_font_layouts(const FontResources& resources);
int width(std::wstring_view text, unsigned dpi, unsigned flags = 0);
int line_height(unsigned dpi, unsigned flags = 0);
int height(std::wstring_view text, unsigned dpi, int width, unsigned flags = 0);
double measure_width(std::wstring_view text, unsigned dpi);
double measure_height(std::wstring_view text, unsigned dpi, double width);
double measure_line_height(unsigned dpi);
struct Fragment { layout::Rect bounds; uint32_t start,length; bool rtl; };
std::vector<Fragment> fragments(std::wstring_view text,unsigned dpi,int width);
std::vector<size_t> clusters(std::wstring_view text);
void append(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
    std::array<uint8_t,4> color, unsigned dpi, const wchar_t* text, bool single_line,
    EditorGeometry* editor, unsigned flags, const layout::Box* paragraph_bounds = nullptr, bool viewport_editor = false);
}
#endif
