#include "gallery.h"
#include "inline_text.h"
#include "harfbuzz_text.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#ifdef SUNFLECK_FRIBIDI
#include <fribidi.h>
#elif defined(SUNFLECK_SHEENBIDI)
#include <SheenBidi/SheenBidi.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <string_view>

namespace {
using Color = std::array<std::uint8_t, 4>;

struct Fonts {
    FT_Library library = nullptr;
    FT_Face latin = nullptr;
    FT_Face cjk = nullptr;
    FT_Face mono = nullptr;
    FT_Face symbols = nullptr;
    std::vector<uint8_t> symbols_data;
    unsigned style = 0;
    FT_F26Dot6 current_size = 0;
    std::vector<std::uint8_t> mono_data;
    std::vector<std::uint8_t> data;

    Fonts()
    {
        if (!FT_Init_FreeType(&library)) {
#ifdef __EMSCRIPTEN__
            FT_New_Face(library, "/sunfleck/font.ttf", 0, &latin);
#else
            FT_New_Face(library, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &latin);
            FT_New_Face(library, "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 2, &cjk);
#endif
        }
    }

    ~Fonts()
    {
        if (symbols) { FT_Done_Face(symbols); }
        if (mono) { FT_Done_Face(mono); }
        if (cjk) {
            FT_Done_Face(cjk);
        }
        if (latin) {
            FT_Done_Face(latin);
        }
        if (library) {
            FT_Done_FreeType(library);
        }
    }

    void set_size(const TextFormat& format, unsigned dpi)
    {
        const auto size = std::max<FT_F26Dot6>(64, std::lround(format.size * dpi * 64 / 96));
        if (current_size == size) {
            return;
        }
        current_size = size;
        for (auto face : {mono, latin, cjk, symbols}) {
            if (face) {
                FT_Set_Char_Size(face, 0, size, 72, 72);
            }
        }
    }

    FT_Face base() const
    {
        return latin ? latin : cjk;
    }

    int line_height(const TextFormat& format, unsigned dpi)
    {
        set_size(format, dpi);
        if (format.line_height > 0) {
            return std::max(1, int(std::lround(format.line_height * dpi / 96)));
        }
        const auto face = base();
        return face ? std::max(1, int(face->size->metrics.height >> 6)) : int((18 * dpi + 48) / 96);
    }

    FT_Face face_for(wchar_t character, const TextFormat& format) const
    {
        const bool monospace = (style & 4) || format.family == "Consolas" || format.family == "monospace";
        for (auto face : {mono, latin, cjk, symbols}) {
            if (face && (face != mono || monospace) && FT_Get_Char_Index(face, character)) {
                return face;
            }
        }
        return nullptr;
    }

    bool load_index(FT_Face face, FT_UInt index, const TextFormat& format, bool render)
    {
        if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT)) { return false; }
        if ((style & 1) || format.weight >= UI_FONT_BOLD) { FT_GlyphSlot_Embolden(face->glyph); }
        if ((style & 2) || format.style != UI_FONT_NORMAL_STYLE) { FT_GlyphSlot_Oblique(face->glyph); }
        return !render || !FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    }

    FT_Face load(wchar_t character, const TextFormat& format, bool render)
    {
        const auto face = face_for(character, format);
        if (face && load_index(face, FT_Get_Char_Index(face, character), format, render)) { return face; }
        const auto fallback = base();
        return fallback && !FT_Load_Char(fallback, 0x25a1, render ? FT_LOAD_RENDER : FT_LOAD_DEFAULT) ? fallback : nullptr;
    }
};

thread_local Fonts fonts;

struct Glyph {
    wchar_t character;
    int x;
    size_t line;
    const TextFormat* format;
    FT_Face face = nullptr;
    FT_UInt index = 0;
    int y_offset = 0;
};
struct Line {
    int width = 0;
    int y = 0;
    int height = 0;
    int ascent = 0;
    int offset = 0;
};
struct Paragraph {
    std::vector<Glyph> glyphs;
    std::vector<TextStop> stops;
    std::vector<size_t> stop_lines;
    std::vector<TextStop> trailing_stops;
    std::vector<Line> lines;
    int width = 0, height = 0;
};

#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
struct BidiParagraph {
#ifdef SUNFLECK_FRIBIDI
    std::vector<FriBidiCharType> types;
    std::vector<FriBidiLevel> levels;
    std::vector<FriBidiParType> directions;
#else
    using Algorithm = std::unique_ptr<const _SBAlgorithm, decltype(&SBAlgorithmRelease)>;
    using Paragraph = std::unique_ptr<const _SBParagraph, decltype(&SBParagraphRelease)>;
    std::vector<std::uint32_t> characters;
    Algorithm algorithm{nullptr, SBAlgorithmRelease};
    std::vector<Paragraph> paragraphs;
    std::vector<SBLevel> levels;
    std::vector<size_t> paragraph_indices;
#endif
    bool reorder = false;
};

BidiParagraph bidi_paragraphs(std::wstring_view text, bool rtl)
{
    BidiParagraph result;
    if (!rtl && std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch < 0x80; })) {
        return result;
    }
#ifdef SUNFLECK_FRIBIDI
    result.types.resize(text.size());
    result.levels.resize(text.size());
    result.directions.resize(text.size());
    std::vector<FriBidiChar> chars(text.begin(), text.end());
    std::vector<FriBidiBracketType> brackets(text.size());
    for (size_t first = 0; first < text.size();) {
        const auto newline = text.find(L'\n', first);
        const auto end = newline == text.npos ? text.size() : newline;
        const auto count = static_cast<FriBidiStrIndex>(end - first);
        FriBidiParType direction = rtl ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;
        if (count) {
            fribidi_get_bidi_types(chars.data() + first, count, result.types.data() + first);
            fribidi_get_bracket_types(chars.data() + first, count, result.types.data() + first, brackets.data() + first);
            if (!fribidi_get_par_embedding_levels_ex(result.types.data() + first, brackets.data() + first,
                    count, &direction, result.levels.data() + first)) {
                return {};
            }
            result.reorder |= std::any_of(result.levels.begin() + first, result.levels.begin() + end,
                [](FriBidiLevel level) { return level != 0; });
        }
        std::fill(result.directions.begin() + first, result.directions.begin() + end, direction);
        first = newline == text.npos ? end : end + 1;
    }
#else
    result.characters.assign(text.begin(), text.end());
    const SBCodepointSequence sequence{SBStringEncodingUTF32, result.characters.data(), result.characters.size()};
    result.algorithm.reset(SBAlgorithmCreate(&sequence));
    if (!result.algorithm) { return {}; }
    result.levels.resize(text.size());
    result.paragraph_indices.resize(text.size());
    for (size_t first = 0; first < text.size();) {
        BidiParagraph::Paragraph paragraph(SBAlgorithmCreateParagraph(result.algorithm.get(), first,
            text.size() - first, rtl ? 1 : 0), SBParagraphRelease);
        if (!paragraph) { return {}; }
        const auto count = SBParagraphGetLength(paragraph.get());
        if (!count || count > text.size() - first) { return {}; }
        const auto* levels = SBParagraphGetLevelsPtr(paragraph.get());
        std::copy_n(levels, count, result.levels.begin() + first);
        std::fill_n(result.paragraph_indices.begin() + first, count, result.paragraphs.size());
        result.reorder |= std::any_of(levels, levels + count, [](SBLevel level) { return level != 0; });
        result.paragraphs.push_back(std::move(paragraph));
        first += count;
    }
#endif
    return result;
}

struct BidiCluster {
    size_t first, end, glyph_first, glyph_end, line;
    int left, advance;
};

void reorder_bidi(Paragraph& result, const std::vector<BidiCluster>& clusters, BidiParagraph& bidi)
{
    result.trailing_stops = result.stops;
    for (size_t first = 0; first < clusters.size();) {
        const auto line_index = clusters[first].line;
        size_t end = first + 1;
        while (end < clusters.size() && clusters[end].line == line_index) { ++end; }
#ifdef SUNFLECK_FRIBIDI
        std::vector<FriBidiStrIndex> order(end - first);
        std::iota(order.begin(), order.end(), 0);
        std::vector<FriBidiCharType> types;
        std::vector<FriBidiLevel> levels;
        for (size_t i = first; i < end; ++i) {
            types.push_back(bidi.types[clusters[i].first]);
            levels.push_back(bidi.levels[clusters[i].first]);
        }
        const auto direction = bidi.directions[clusters[first].first];
        if (!fribidi_reorder_line(0, types.data(), static_cast<FriBidiStrIndex>(order.size()), 0,
                direction, levels.data(), nullptr, order.data())) {
            first = end;
            continue;
        }
#else
        std::vector<size_t> order;
        std::vector<SBLevel> levels(end - first);
        const auto offset = clusters[first].first;
        std::unique_ptr<const _SBLine, decltype(&SBLineRelease)> line(
            SBParagraphCreateLine(bidi.paragraphs[bidi.paragraph_indices[offset]].get(), offset,
                clusters[end - 1].end - offset), SBLineRelease);
        if (line) {
            const auto* runs = SBLineGetRunsPtr(line.get());
            for (SBUInteger run_index = 0; run_index < SBLineGetRunCount(line.get()); ++run_index) {
                const auto& run = runs[run_index];
                const auto start = order.size();
                for (size_t index = first; index < end; ++index) {
                    if (clusters[index].first >= run.offset && clusters[index].first - run.offset < run.length) {
                        order.push_back(index - first);
                        levels[index - first] = run.level;
                    }
                }
                if (run.level & 1) { std::reverse(order.begin() + start, order.end()); }
            }
        }
        if (order.size() != end - first) {
            first = end;
            continue;
        }
#endif
        int pen = 0;
        for (auto mapped : order) {
            const auto& cluster = clusters[first + mapped];
            const bool rtl = levels[mapped] & 1;
            for (size_t i = cluster.glyph_first; i < cluster.glyph_end; ++i) {
                result.glyphs[i].x += pen - cluster.left;
            }
            const auto y = result.lines[line_index].y;
            for (size_t i = cluster.first; i < cluster.end; ++i) {
                const auto edge = [&](size_t index) {
                    const int delta = int(cluster.advance * (index - cluster.first) / (cluster.end - cluster.first));
                    return pen + (rtl ? cluster.advance - delta : delta);
                };
                result.stops[i] = {edge(i), y, i, false, rtl};
                result.trailing_stops[i + 1] = {edge(i + 1), y, i + 1, true, rtl};
            }
            pen += cluster.advance;
        }
        first = end;
    }
    if (!clusters.empty() && clusters.back().end == result.stops.size() - 1) {
        result.stops.back() = result.trailing_stops.back();
    }
}
#endif

Paragraph layout_text(std::wstring_view text, unsigned dpi, int width, bool single_line, bool record)
{
    const auto& format = active_text_format();
    const bool wrap = !single_line && format.wrapping == UI_TEXT_WRAP;
    const int base_height = fonts.line_height(format, dpi);
    const auto base = fonts.base();
    const int base_ascent = base ? int(base->size->metrics.ascender >> 6) : base_height;
    Paragraph result;
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
    auto bidi = bidi_paragraphs(text, format.flow == UI_FLOW_RIGHT_TO_LEFT);
    std::vector<BidiCluster> bidi_clusters;
#endif
    auto& lines = result.lines;
    lines.push_back({0, 0, base_height, base_ascent});
    const auto new_line = [&] {
        const auto& line = lines.back();
        lines.push_back({0, line.y + line.height, base_height, base_ascent});
    };
    const auto stop = [&](size_t index) {
        if (record) {
            result.stops.push_back({lines.back().width, lines.back().y, index});
            result.stop_lines.push_back(lines.size() - 1);
        }
    };
    auto run = current_text_runs.begin();
    const auto end = current_text_runs.end();
#ifdef SUNFLECK_HARFBUZZ
    size_t unshaped_end = 0;
#endif
    for (size_t i = 0; i < text.size(); ++i) {
        const auto ch = text[i];
        if (!single_line && ch == L'\n') {
            stop(i);
            new_line();
            continue;
        }
        if (ch == L'\r') {
            stop(i);
            continue;
        }
        while (run != end && i >= size_t(run->start) + run->length) {
            ++run;
        }
        const auto& glyph_format = run != end && i >= run->start ? run->format : format;
        const int height = fonts.line_height(glyph_format, dpi);
#ifdef SUNFLECK_HARFBUZZ
        if (const auto face = fonts.face_for(ch, glyph_format); i >= unshaped_end && face && ch != L'\n') {
            bool rtl = false;
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
            if (bidi.reorder) { rtl = (bidi.levels[i] & 1) != 0; }
#endif
            const auto format_end = run == end ? text.size() :
                i < run->start ? size_t(run->start) : std::min(text.size(), size_t(run->start) + run->length);
            auto span_end = i + 1;
            while (span_end < format_end && text[span_end] != L'\n' && text[span_end] != L'\r' &&
                   fonts.face_for(text[span_end], glyph_format) == face) {
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
                if (bidi.reorder && bidi.levels[span_end] != bidi.levels[i]) { break; }
#endif
                ++span_end;
            }
            const auto shaped = harfbuzz_shape(face, text.substr(i, span_end - i), rtl);
            if (!shaped.empty() && shaped.front().cluster == 0) {
                const auto pixels = [](int position) { return int(std::lround(position / 64.0)); };
                int pen = 0;
                for (size_t first = 0; first < shaped.size();) {
                    const auto cluster = shaped[first].cluster;
                    auto last = first + 1;
                    int end_pen = pen + shaped[first].advance;
                    while (last < shaped.size() && shaped[last].cluster == cluster) {
                        end_pen += shaped[last++].advance;
                    }
                    const auto cluster_end = last < shaped.size() ? shaped[last].cluster : span_end - i;
                    const int advance = std::max(0, pixels(end_pen) - pixels(pen));
                    if (wrap && lines.back().width && advance > width - lines.back().width) { new_line(); }
                    auto& line = lines.back();
                    const int left = line.width;
                    for (size_t index = cluster; index < cluster_end; ++index) {
                        line.width = left + int(advance * (index - cluster) / (cluster_end - cluster));
                        stop(i + index);
                    }
                    if (record) {
                        [[maybe_unused]] const auto glyph_first = result.glyphs.size();
                        int glyph_pen = pen;
                        for (size_t index = first; index < last; ++index) {
                            const auto& glyph = shaped[index];
                            result.glyphs.push_back({0, left + pixels(glyph_pen + glyph.x_offset) - pixels(pen),
                                lines.size() - 1, &glyph_format, face, glyph.index, pixels(glyph.y_offset)});
                            glyph_pen += glyph.advance;
                        }
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
                        if (bidi.reorder) {
                            bidi_clusters.push_back({i + cluster, i + cluster_end, glyph_first,
                                result.glyphs.size(), lines.size() - 1, left, advance});
                        }
#endif
                    }
                    line.height = std::max(line.height, height);
                    line.ascent = std::max(line.ascent, int(face->size->metrics.ascender >> 6));
                    line.width = left + advance;
                    result.width = std::max(result.width, line.width);
                    pen = end_pen;
                    first = last;
                }
                i = span_end - 1;
                continue;
            }
            unshaped_end = span_end;
        }
#endif
        const auto face = fonts.load(ch, glyph_format, false);
        const int advance = face ? std::max(0, int((face->glyph->advance.x + 32) >> 6)) : 0;
        if (wrap && lines.back().width && advance > width - lines.back().width) {
            new_line();
        }
        auto& line = lines.back();
        stop(i);
        if (record) {
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
            if (bidi.reorder) {
                bidi_clusters.push_back({i, i + 1, result.glyphs.size(), result.glyphs.size() + 1,
                    lines.size() - 1, line.width, advance});
            }
#endif
            result.glyphs.push_back({ch, line.width, lines.size() - 1, &glyph_format});
        }
        line.height = std::max(line.height, height);
        if (face) {
            line.ascent = std::max(line.ascent, int(face->size->metrics.ascender >> 6));
        }
        line.width += advance;
        result.width = std::max(result.width, line.width);
    }
    stop(text.size());
    result.height = lines.back().y + lines.back().height;
    if (record) {
#if defined(SUNFLECK_FRIBIDI) || defined(SUNFLECK_SHEENBIDI)
        if (bidi.reorder) { reorder_bidi(result, bidi_clusters, bidi); }
#endif
        for (auto& line : lines) {
            const int slack = std::max(0, width - line.width);
            if (format.alignment == UI_TEXT_ALIGN_CENTER) {
                line.offset = slack / 2;
            } else if (format.alignment == UI_TEXT_ALIGN_RIGHT) {
                line.offset = slack;
            }
        }
        for (size_t i = 0; i < result.stops.size(); ++i) {
            result.stops[i].x += lines[result.stop_lines[i]].offset;
        }
        for (size_t i = 1; i < result.trailing_stops.size(); ++i) {
            if (result.trailing_stops[i].trailing) {
                result.trailing_stops[i].x += lines[result.stop_lines[i - 1]].offset;
            }
        }
    }
    return result;
}
}

ui_result freetype_set_font(const void* bytes, size_t size)
{
    if (!bytes || !size || size > 64 * 1024 * 1024 || !fonts.library) { return UI_ERROR_INVALID_ARGUMENT; }
    try {
        const auto* first = static_cast<const std::uint8_t*>(bytes);
        std::vector<std::uint8_t> data(first, first + size);
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(fonts.library, data.data(), static_cast<FT_Long>(size), 0, &face)) { return UI_ERROR_INVALID_ARGUMENT; }
        if (fonts.latin) { FT_Done_Face(fonts.latin); }
        fonts.latin = face;
        fonts.data = std::move(data);
        fonts.current_size = 0;
        ++text_resource_revision;
        return UI_OK;
    } catch (const std::bad_alloc&) { return UI_ERROR_OUT_OF_MEMORY; }
}

void freetype_document_style(unsigned flags) { fonts.style = flags; }
ui_result freetype_document_font(unsigned role, const void* bytes, size_t size) {
    if(role == 0) return freetype_set_font(bytes,size);
    if((role != 1 && role != 2) || !bytes || !size || size > 64*1024*1024 || !fonts.library) return UI_ERROR_INVALID_ARGUMENT;
    try {
        const auto* first=static_cast<const uint8_t*>(bytes);
        std::vector<uint8_t> data(first,first+size); FT_Face face=nullptr;
        if(FT_New_Memory_Face(fonts.library,data.data(),static_cast<FT_Long>(size),0,&face)) return UI_ERROR_INVALID_ARGUMENT;
        auto& target=role==1?fonts.mono:fonts.symbols;
        auto& storage=role==1?fonts.mono_data:fonts.symbols_data;
        if(target) FT_Done_Face(target);
        target=face;storage=std::move(data);fonts.current_size=0;++text_resource_revision;return UI_OK;
    } catch(const std::bad_alloc&) {return UI_ERROR_OUT_OF_MEMORY;}
}
int freetype_text_height(unsigned dpi)
{
    return fonts.line_height(active_text_format(), dpi);
}
int freetype_text_width(const wchar_t* text, unsigned dpi)
{
    return layout_text(text, dpi, std::numeric_limits<int>::max(), false, false).width;
}

int freetype_paragraph_height(const wchar_t* text, unsigned dpi, int width)
{
    return layout_text(text, dpi, std::max(1, width), false, false).height;
}

void append_freetype_text(std::vector<FillRectCommand>& commands, layout::Rect bounds,
                          layout::Rect clip, Color color, unsigned dpi, const wchar_t* value,
                          bool single_line, EditorGeometry* editor)
{
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0 || (!*value && !editor)) {
        return;
    }
    if (!fonts.base()) {
        return;
    }
    const std::wstring_view text(value);
    const auto paragraph = layout_text(text, dpi, width, single_line, true);
    const auto& stops = paragraph.stops;
    const auto& trailing = paragraph.trailing_stops;
    const auto caret_index = editor ? std::min(editor->index, text.size()) : 0;
    const bool previous = editor && editor->trailing && caret_index && !trailing.empty() && trailing[caret_index].trailing;
    const auto caret = previous ? trailing[caret_index] : stops[caret_index];
    const int caret_height = paragraph.lines[paragraph.stop_lines[previous ? caret_index - 1 : caret_index]].height;
    auto* hosted = editor ? editor->hosted : nullptr;
    int horizontal = single_line && editor ? std::max(0, caret.x - width + 1) : 0;
    int vertical = editor ? std::max(0, caret.y + caret_height - height) : 0;
    if (hosted) {
        horizontal = hosted->scroll_x; vertical = hosted->scroll_y;
        if (hosted->reveal) {
            if (caret.x < horizontal) { horizontal = caret.x; }
            if (caret.x >= horizontal + width) { horizontal = caret.x - width + 1; }
            if (caret.y < vertical) { vertical = caret.y; }
            if (caret.y + caret_height > vertical + height) { vertical = caret.y + caret_height - height; }
        }
        horizontal = std::clamp(horizontal, 0, single_line ? std::max(0, paragraph.width - width + 1) : 0);
        vertical = std::clamp(vertical, 0, std::max(0, paragraph.height - height));
        hosted->scroll_x = horizontal; hosted->scroll_y = vertical; hosted->reveal = false;
        hosted->caret_x = bounds.left + caret.x - horizontal;
        hosted->caret_y = bounds.top + caret.y - vertical;
    }
    const int left = std::max(bounds.left, clip.left);
    const int top = std::max(bounds.top, clip.top);
    const int right = std::min(bounds.right, clip.right);
    const int bottom = std::min(bounds.bottom, clip.bottom);
    if (editor) {
        editor->stops.clear();
        const auto append_stop = [&](const TextStop& stop, size_t line) {
            const int sx = bounds.left + stop.x - horizontal;
            const int sy = bounds.top + stop.y - vertical;
            const int line_height = paragraph.lines[line].height;
            if (hosted || (sx >= left && sx <= right && sy + line_height > top && sy < bottom)) {
                editor->stops.push_back({sx, sy, stop.index, stop.trailing, stop.rtl});
            }
        };
        for (size_t i = 0; i < stops.size(); ++i) {
            append_stop(stops[i], paragraph.stop_lines[i]);
            if (i && !trailing.empty() && trailing[i].trailing && !stops[i].trailing &&
                (trailing[i].x != stops[i].x || trailing[i].y != stops[i].y || trailing[i].rtl != stops[i].rtl)) {
                append_stop(trailing[i], paragraph.stop_lines[i - 1]);
            }
        }
    }
    if (left >= right || top >= bottom) {
        return;
    }
    std::vector<layout::Rect> selection;
    // Stops use exactly the same integer advances and wrapping as the glyphs.
    const auto range_rects = [&](std::size_t first, std::size_t last, auto draw) {
        for (std::size_t i = first; i < std::min(last, stops.size() - 1); ++i) {
            const auto& a = stops[i];
            const auto& b = !trailing.empty() && trailing[i + 1].trailing ? trailing[i + 1] : stops[i + 1];
            const int sx = bounds.left + a.x - horizontal, sy = bounds.top + a.y - vertical;
            const int ex = b.y == a.y ? bounds.left + b.x - horizontal : bounds.right;
            const int line_height = paragraph.lines[paragraph.stop_lines[i]].height;
            const layout::Rect r{std::max(left, std::min(sx, ex)), std::max(top, sy),
                std::min(right, std::max(sx, ex)), std::min(bottom, sy + line_height)};
            if (r.left < r.right && r.top < r.bottom) { draw(r); }
        }
    };
    if (hosted) {
        range_rects(hosted->first, hosted->last, [&](layout::Rect r) {
            if (!selection.empty() && selection.back().top == r.top && selection.back().bottom == r.bottom && selection.back().right == r.left) {
                selection.back().right = r.right;
            } else { selection.push_back(r); }
        });
        for (const auto& r : selection) { commands.push_back({r.left, r.top, r.right, r.bottom, editor->selection}); }
    }
    const int mask_width = right - left;
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(mask_width) * (bottom - top));
    std::vector<Color> inks(current_text_runs.empty() ? 0 : mask.size(), color);
    for (const auto& glyph : paragraph.glyphs) {
        const auto& line = paragraph.lines[glyph.line];
        if (line.y + line.height <= vertical || line.y >= vertical + height) {
            continue;
        }
        fonts.set_size(*glyph.format, dpi);
        const auto face = glyph.face ? glyph.face : fonts.load(glyph.character, *glyph.format, true);
        if (!face) {
            continue;
        }
        if (glyph.face && !fonts.load_index(face, glyph.index, *glyph.format, true)) { continue; }
        const auto slot = face->glyph;
        const auto& bitmap = slot->bitmap;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.pitch <= 0) {
            continue;
        }
        const int gx = bounds.left + line.offset + glyph.x - horizontal + slot->bitmap_left;
        const int gy = bounds.top + line.y - vertical + line.ascent - slot->bitmap_top - glyph.y_offset;
        const auto argb = glyph.format->foreground;
        const auto ink = glyph.format->foreground_set
            ? Color{uint8_t(argb >> 16), uint8_t(argb >> 8), uint8_t(argb), uint8_t(argb >> 24)} : color;
        for (int row = std::max(0, top - gy); row < std::min(static_cast<int>(bitmap.rows), bottom - gy); ++row) {
            for (int col = std::max(0, left - gx); col < std::min(static_cast<int>(bitmap.width), right - gx); ++col) {
                const auto index = static_cast<std::size_t>(gy + row - top) * mask_width + gx + col - left;
                const auto coverage = bitmap.buffer[row * bitmap.pitch + col];
                if (coverage > mask[index]) {
                    mask[index] = coverage;
                    if (!inks.empty()) {
                        inks[index] = ink;
                    }
                }
            }
        }
    }
    const auto backdrop = commands.size();
    // Text coverage samples the topmost earlier rectangle. Earlier commands
    // outside this clip, or behind a rectangle covering it, cannot contribute.
    std::vector<std::size_t> backdrop_candidates;
    for (std::size_t i = backdrop; i > 0; --i) {
        const auto& below = commands[i - 1];
        if (below.right <= left || below.left >= right || below.bottom <= top || below.top >= bottom) {
            continue;
        }
        backdrop_candidates.push_back(i - 1);
        if (below.left <= left && below.right >= right && below.top <= top && below.bottom >= bottom) {
            break;
        }
    }
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            const auto index = static_cast<std::size_t>(py - top) * mask_width + px - left;
            const unsigned coverage = mask[index];
            if (!coverage) {
                continue;
            }
            const bool selected = std::any_of(selection.begin(), selection.end(), [&](layout::Rect r) {
                return px >= r.left && px < r.right && py >= r.top && py < r.bottom;
            });
            const Color ink = selected ? editor->selected_text : (inks.empty() ? color : inks[index]);
            Color resolved = ink;
            if (coverage != 255) {
                for (auto index : backdrop_candidates) {
                    const auto& below = commands[index];
                    if (px >= below.left && px < below.right && py >= below.top && py < below.bottom) {
                        for (int channel = 0; channel < 3; ++channel) {
                            resolved[channel] = (ink[channel] * coverage +
                                below.color[channel] * (255 - coverage) + 127) / 255;
                        }
                        break;
                    }
                }
            }
            if (commands.size() > backdrop && commands.back().right == px &&
                commands.back().top == py && commands.back().color == resolved) {
                commands.back().right = px + 1;
            } else {
                commands.push_back({px, py, px + 1, py + 1, resolved});
            }
        }
    }
    if (hosted && hosted->composing) {
        range_rects(hosted->composition_first, hosted->composition_last, [&](layout::Rect r) {
            commands.push_back({r.left, r.bottom - 1, r.right, r.bottom, color});
        });
    }
    if (editor && editor->show && (!hosted || (caret.x >= horizontal && caret.x < horizontal + width))) {
        const int px = std::clamp(bounds.left + caret.x - horizontal, left, right - 1);
        const int py = bounds.top + caret.y - vertical;
        if (std::max(top, py) < std::min(bottom, py + caret_height)) {
            commands.push_back({px, std::max(top, py), px + 1, std::min(bottom, py + caret_height), color});
        }
    }
}
