#include "harfbuzz_text.h"

#ifdef SUNFLECK_HARFBUZZ

#include <hb-ft.h>
#include <hb.h>

#include <algorithm>
#include <memory>

std::vector<ShapedGlyph> harfbuzz_shape(FT_Face face, std::wstring_view text, bool rtl)
{
    if (!face || text.empty()) {
        return {};
    }
    auto* unicode = hb_unicode_funcs_get_default();
    hb_script_t script = HB_SCRIPT_UNKNOWN;
    for (const auto ch : text) {
        const auto current = hb_unicode_script(unicode, ch);
        if (current == HB_SCRIPT_COMMON || current == HB_SCRIPT_INHERITED || current == HB_SCRIPT_UNKNOWN) {
            continue;
        }
        if ((!rtl && hb_script_get_horizontal_direction(current) != HB_DIRECTION_LTR) ||
            (script != HB_SCRIPT_UNKNOWN && current != script)) {
            return {};
        }
        script = current;
    }
    std::unique_ptr<hb_font_t, decltype(&hb_font_destroy)> font(hb_ft_font_create_referenced(face), hb_font_destroy);
    std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer(hb_buffer_create(), hb_buffer_destroy);
    hb_buffer_set_content_type(buffer.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);
    hb_buffer_set_script(buffer.get(), script);
    hb_buffer_set_direction(buffer.get(), rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_language(buffer.get(), hb_language_from_string("und", -1));
    for (std::size_t index = 0; index < text.size(); ++index) {
        hb_buffer_add(buffer.get(), text[index], static_cast<unsigned>(index));
    }
    hb_shape(font.get(), buffer.get(), nullptr, 0);
    unsigned count = 0;
    const auto* positions = hb_buffer_get_glyph_positions(buffer.get(), &count);
    const auto* infos = hb_buffer_get_glyph_infos(buffer.get(), &count);
    std::vector<ShapedGlyph> glyphs;
    glyphs.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        if (!infos[i].codepoint || infos[i].cluster >= text.size()) {
            return {};
        }
        glyphs.push_back({infos[i].codepoint, infos[i].cluster, positions[i].x_advance,
            positions[i].x_offset, positions[i].y_offset});
    }
    if (rtl) {
        std::stable_sort(glyphs.begin(), glyphs.end(), [](const ShapedGlyph& a, const ShapedGlyph& b) {
            return a.cluster < b.cluster;
        });
    }
    return glyphs;
}

#endif
