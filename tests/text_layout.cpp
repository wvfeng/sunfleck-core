#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include "../src/dwrite_text.h"
#include <iostream>

static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
#ifdef SUNFLECK_TEST_HARFBUZZ
static void shaped_ligature()
{
    constexpr layout::Rect bounds{0, 0, 80, 32};
    constexpr std::array<std::uint8_t, 4> ink{0, 0, 0, 255};
    std::vector<FillRectCommand> combined, precomposed;
    append_freetype_text(combined, bounds, bounds, ink, 96, L"fi", true, nullptr);
    append_freetype_text(precomposed, bounds, bounds, ink, 96, L"\ufb01", true, nullptr);
    std::vector<std::uint8_t> combined_pixels, precomposed_pixels;
    rasterize(combined_pixels, 80, 32, combined);
    rasterize(precomposed_pixels, 80, 32, precomposed);
    check(!combined.empty() && combined_pixels == precomposed_pixels, "HarfBuzz ligature glyph reaches the renderer");
    check(freetype_text_width(L"fi", 96) == freetype_text_width(L"\ufb01", 96),
        "HarfBuzz measurement agrees with the rendered ligature");
}
#endif
#ifdef SUNFLECK_TEST_FRIBIDI
static void bidi_geometry()
{
    constexpr layout::Rect bounds{0, 0, 240, 40};
    constexpr std::array<std::uint8_t, 4> ink{0, 0, 0, 255};
    std::vector<TextStop> stops;
    EditorGeometry geometry{stops, 0, false};
    std::vector<FillRectCommand> commands;
    append_freetype_text(commands, bounds, bounds, ink, 96, L"A \u05d0\u05d1\u05d2 B", true, &geometry);
    const auto edge = [&](size_t index, bool trailing) {
        return std::find_if(stops.begin(), stops.end(), [&](const TextStop& stop) {
            return stop.index == index && stop.trailing == trailing;
        });
    };
    const auto before = edge(2, true), hebrew_start = edge(2, false), hebrew_end = edge(5, true);
    check(before != stops.end() && hebrew_start != stops.end() && hebrew_end != stops.end() &&
        !before->rtl && hebrew_start->rtl && hebrew_end->rtl &&
        hebrew_start->x > before->x && hebrew_end->x == before->x,
        "Mixed Hebrew preserves both visual caret edges");
}
#endif
int main() {
    try {
#ifdef SUNFLECK_TEST_HARFBUZZ
        shaped_ligature();
#endif
#ifdef SUNFLECK_TEST_FRIBIDI
        bidi_geometry();
#endif
        sunfleck::App app(UI_SOFTWARE,true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1' Width='640' Height='800'><StackPanel Id='root' Spacing='0'><Border Id='border' Padding='8' BorderThickness='2'><StackPanel><TextBlock Id='text' FontSize='18' LineHeight='28' Text='A long paragraph needs enough room for all its wrapped lines. Changing available width should remeasure the complete paragraph before positioning the next control.'/></StackPanel></Border><Button Id='next' Content='Next'/><Grid Id='row' ColumnDefinitions='*,3*'><TextBlock Id='left' Text='More words in the narrow first column, wrapping across several lines.' FontSize='20' Weight='1'/><TextBlock Grid.Column='1' Id='right' Text='Wide column' Weight='3'/></Grid><ScrollView Id='scroll' Height='100'><TextBlock Id='scrolltext' Text='A long scrolling paragraph must be measured again after the scrollbar takes away some of the available width. This prevents its last line from being clipped.'/></ScrollView></StackPanel></Window>");
        app.openWindow(); app.poll(0);
        auto text=app.findFleckNode("text").nativeHandle(), border=app.findFleckNode("border").nativeHandle();
        auto next=app.findFleckNode("next").nativeHandle(),left=app.findFleckNode("left").nativeHandle();
        for (unsigned dpi:{96u,144u,192u}) {
            check(ui_host_resize(app.nativeHandle(),int(640*dpi/96),int(1000*dpi/96),dpi)==UI_OK,"Wide resize"); app.poll(0);
            const int wide=text->bounds.bottom-text->bounds.top;
            check(ui_host_resize(app.nativeHandle(),int(280*dpi/96),int(1000*dpi/96),dpi)==UI_OK,"Narrow resize"); app.poll(0);
            check(text->bounds.bottom-text->bounds.top>wide,"Auto text height responds to width");
            check(next->bounds.top>=border->bounds.bottom && border->bounds.bottom>=text->bounds.bottom,"Nested containers preserve following sibling position");
#ifdef _WIN32
            for (auto* node:{text,left,app.findFleckNode("scrolltext").nativeHandle()}) {
                TextFormatScope scope(node->resolved_font);
                const auto& arranged=node->layout_cache.arranged;
                const auto expected=windows_text::measure_height(node->display_text(),dpi,arranged.right-arranged.left)+8*dpi/96.0;
                check(arranged.bottom-arranged.top+1e-6>=expected,"Unrounded height contains the same fractional DirectWrite paragraph");
                check(node->bounds.top==int(std::lround(arranged.top)) && node->bounds.bottom==int(std::lround(arranged.bottom)),"Painted text edges round the arranged paragraph once");
            }
#endif
            const auto measures=text->layout_cache.measures;
            ui_binding_value ink{}; ink.kind=UI_VALUE_COLOR; ink.integer=0xff225577;
            check(ui_node_set_property(text,UI_PROPERTY_FOREGROUND,&ink)==UI_OK,"Foreground update"); app.poll(0);
            check(text->layout_cache.measures==measures,"Foreground does not remeasure paragraph");
        }
        ui_binding_value nowrap{}; nowrap.kind=UI_VALUE_ENUM; nowrap.integer=UI_TEXT_NO_WRAP;
        check(ui_node_set_property(text,UI_PROPERTY_TEXT_WRAPPING,&nowrap)==UI_OK,"NoWrap update"); app.poll(0);
        check(text->bounds.bottom-text->bounds.top==96,"NoWrap returns to existing minimum Auto height at 200% DPI");
        std::cout<<"Width-constrained text layout, nested containers, scrolling, DPI and render-only updates passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
