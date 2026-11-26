#pragma once

#include "gallery.h"

namespace macos_text {
int width(const wchar_t* text, unsigned dpi, unsigned flags);
int line_height(unsigned dpi, unsigned flags);
int height(const wchar_t* text, unsigned dpi, int width, unsigned flags);
void append(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
            std::array<uint8_t, 4> color, unsigned dpi, const wchar_t* text, bool single_line,
            EditorGeometry* editor, unsigned flags);
}
