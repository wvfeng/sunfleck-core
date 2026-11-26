#include "../src/dwrite_text.h"
#include "../src/input/text_navigation.h"
#include "../src/text_format.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <limits>

static void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        check(windows_text::width(L"",96)==0,"Empty width");
        {
            TextFormat format; format.size=512;
            TextFormatScope scope(format);
            const std::wstring text(4000,L'M');
            const double infinity=std::numeric_limits<double>::infinity();
            check(windows_text::measure_width(text,96)>1000000,"Intrinsic text width is not capped at one million pixels");
            const auto unbounded=windows_text::measure_height(text,96,infinity);
            check(unbounded==windows_text::measure_line_height(96),"Unbounded text stays on one line beyond old sentinel");
            check(windows_text::measure_height(text,96,1000000)>unbounded,"Large finite width remains a wrapping constraint");
            check(windows_text::measure_height(L"M\nM",96,infinity)>unbounded,"Unbounded width preserves explicit line breaks");
        }
        {
            TextFormat format; format.flow=UI_FLOW_RIGHT_TO_LEFT; format.alignment=UI_TEXT_ALIGN_RIGHT;
            TextFormatScope scope(format);
            std::vector<FillRectCommand> commands; std::vector<TextStop> stops;
            EditorGeometry editor{stops,0,false};
            windows_text::append(commands,{0,0,200,100},{0,0,200,100},{0,0,0,255},96,L"\u05d0\u05d1\u05d2",true,&editor,0);
            check(text_navigation::horizontal(stops,0,-1,true)==1,"RTL left arrow follows visual glyph geometry");
            check(text_navigation::horizontal(stops,1,1,true)==0,"RTL right arrow reverses visual movement");
        }
        auto combining=windows_text::clusters(L"e\u0301x");
        check(combining.front()==0 && combining.back()==3 && std::find(combining.begin(),combining.end(),1)==combining.end(),"Combining sequence is one editing cluster");
        auto emoji=windows_text::clusters(L"\U0001f331x");
        check(emoji.front()==0 && emoji.back()==3 && std::find(emoji.begin(),emoji.end(),1)==emoji.end(),"Supplementary character has no surrogate-interior caret");
        for (const auto* sequence : {L"\U0001f469\u200d\U0001f4bb",L"\U0001f44d\U0001f3fd",L"\U0001f1e8\U0001f1f3"}) {
            const auto stops=windows_text::clusters(sequence);
            check(stops.size()==2 && stops.front()==0 && stops.back()==std::wstring_view(sequence).size(),"Emoji sequence has only boundary carets");
        }
        check(windows_text::width(L"\u4e2d\u6587",96)>0,"System CJK fallback");
        const int width=windows_text::width(L"Sunfleck",96);
        check(std::abs(windows_text::width(L"Sunfleck",192)-2*width)<=1,"DPI measurement");
        check(windows_text::line_height(192)>=2*windows_text::line_height(96)-1,"DPI line height");
        {
            const auto extent=windows_text::measure_width(L"MMMM MMMM",144);
            const auto tight=windows_text::measure_height(L"MMMM MMMM",144,extent-.125);
            const auto loose=windows_text::measure_height(L"MMMM MMMM",144,extent+.125);
            check(tight>loose,"Fractional width constraints retain distinct wrapping and cache entries");
            std::vector<FillRectCommand> commands;
            std::vector<TextStop> stops;
            HostedEditor hosted; hosted.active=true; hosted.caret=9;
            EditorGeometry editor{stops,9,true,&hosted};
            layout::Box paragraph{.25,.75,.25+extent-.125,150};
            const layout::Rect pixels=paragraph;
            windows_text::append(commands,pixels,pixels,{0,0,0,255},144,L"MMMM MMMM",false,&editor,0,&paragraph,true);
            const int wrapped_y=hosted.caret_y;
            paragraph.right+=.25; hosted.reveal=true;
            windows_text::append(commands,pixels,pixels,{0,0,0,255},144,L"MMMM MMMM",false,&editor,0,&paragraph,true);
            check(hosted.caret_y<wrapped_y,"Editor wrapping retains fractional viewport width before raster rounding");
        }
        {
            TextFormat format; format.wrapping=UI_TEXT_NO_WRAP; format.flow=UI_FLOW_RIGHT_TO_LEFT;
            TextFormatScope scope(format);
            const layout::Box paragraph{-100,-8,300,60};
            const layout::Rect slice{0,0,120,35};
            std::vector<FillRectCommand> full,cropped;
            std::vector<TextStop> full_stops,cropped_stops;
            HostedEditor selection; selection.active=true; selection.first=1; selection.last=7; selection.reveal=false;
            EditorGeometry full_editor{full_stops,0,false,&selection,{0,0,200,255},{255,255,255,255}};
            EditorGeometry cropped_editor{cropped_stops,0,false,&selection,{0,0,200,255},{255,255,255,255}};
            const auto* text=L"Mixed \u05d0\u05d1\u05d2 e\u0301 and long content";
            windows_text::append(full,paragraph,slice,{0,0,0,255},96,text,false,&full_editor,0,&paragraph);
            windows_text::append(cropped,slice,slice,{0,0,0,255},96,text,false,&cropped_editor,0,&paragraph);
            check(!full.empty() && full.size()==cropped.size(),"Visible paragraph slice retains selection and glyphs");
            for (size_t i=0;i<full.size();++i) {
                const auto a=full[i],b=cropped[i];
                check(a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom && a.color==b.color,"Cropping preserves paragraph coordinates");
            }
            const layout::Box very_wide{-100,0,1000000,40};
            cropped.clear();
            windows_text::append(cropped,slice,slice,{0,0,0,255},96,text,false,&cropped_editor,0,&very_wide);
            check(!cropped.empty(),"Large offscreen paragraph rasterizes only its visible slice");
        }
        for (unsigned dpi : {96u,144u,192u}) {
            std::vector<FillRectCommand> commands;
            std::vector<TextStop> stops;
            HostedEditor hosted; hosted.active=true; hosted.first=0; hosted.last=2; hosted.caret=12;
            EditorGeometry editor{stops,12,true,&hosted,{0,0,200,255},{255,255,255,255}};
            const layout::Rect bounds{10,20,110,120};
            windows_text::append(commands,bounds,bounds,{20,40,60,255},dpi,L"e\u0301 ABC long words\n\u05d0\u05d1\u05d2",false,&editor,0);
            check(!commands.empty() && stops.size()>4,"Text, selection and caret commands");
            check(std::any_of(commands.begin(),commands.end(),[](auto c){return c.color[3]>0 && c.color[3]<255;}),"Glyph alpha coverage");
            check(std::none_of(stops.begin(),stops.end(),[](auto p){return p.index==1;}),"Caret stops use shaping clusters");
            check(std::any_of(stops.begin(),stops.end(),[&](auto p){return p.y>stops.front().y;}),"Wrapping uses layout metrics");
            for (auto c:commands) check(c.left>=bounds.left && c.top>=bounds.top && c.right<=bounds.right && c.bottom<=bounds.bottom,"Clipped glyphs and selection");
            auto previous=commands;
            commands.clear(); windows_text::append(commands,bounds,bounds,{20,40,60,255},dpi,L"e\u0301 ABC long words\n\u05d0\u05d1\u05d2",false,&editor,0);
            check(previous.size()==commands.size(),"Cached layout/mask preserves command count");
            for (size_t i=0;i<commands.size();++i) {
                auto a=previous[i],b=commands[i];
                check(a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom && a.color==b.color,"Cached output stable");
            }
        }
        std::cout<<"DirectWrite shaping, clusters, DPI, wrapping, selection and glyph coverage passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
