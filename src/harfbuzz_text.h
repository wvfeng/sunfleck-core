#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string_view>
#include <vector>

#ifdef SUNFLECK_HARFBUZZ
struct ShapedGlyph {
    FT_UInt index;
    std::size_t cluster;
    int advance, x_offset, y_offset;
};
// Returns clusters in logical order, preserving the glyph order within each cluster.
std::vector<ShapedGlyph> harfbuzz_shape(FT_Face face, std::wstring_view text, bool rtl);
#endif
