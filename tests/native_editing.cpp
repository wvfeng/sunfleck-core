#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include "../src/input/text_navigation.h"
#include "../src/input/hosted_text.h"
#include <iostream>

static void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
static void key(HWND window,unsigned code,bool shift=false,bool control=false) {
    BYTE saved[256],state[256]; GetKeyboardState(saved); std::copy(std::begin(saved),std::end(saved),state);
    state[VK_SHIFT]=shift?0x80:0; state[VK_CONTROL]=control?0x80:0; SetKeyboardState(state);
    SendMessageW(window,WM_KEYDOWN,code,0); SetKeyboardState(saved);
}
int main(int argc,char** argv) {
    try {
        const bool skia=argc>1 && std::string(argv[1])=="skia";
        const ui_app_options options{sizeof(options),UI_API_VERSION,static_cast<uint32_t>(argc>1?(skia?UI_AUTO:UI_D3D11):UI_SOFTWARE),
            skia?UI_SHOW_RENDERER:0u};
        sunfleck::App app(options);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1' Title='Sunfleck M2 editing' Width='600' Height='440'><StackPanel><TextBox Id='single' FontFamily='Segoe UI' FontSize='24' Height='50' Text='abcdef'/><TextArea Id='multi' Width='180' Height='230' FontSize='24' Text='one two three four five six seven eight nine'/></StackPanel></Window>");
        app.openWindow(); app.poll(0);
        HWND window=FindWindowW(L"SunfleckPreview",skia?L"Sunfleck M2 editing [GPU]":L"Sunfleck M2 editing"); check(window!=nullptr,"Native editor window");
        SetFocus(window); SendMessageW(window,WM_SETFOCUS,0,0);
        auto single=app.findFleckNode("single"); auto* node=single.nativeHandle();
        auto click=[&](ui_node& target,size_t index) {
            const auto* at=text_navigation::locate(target.text_stops,{index,false}); check(at!=nullptr,"Caret geometry exists");
            auto point=MAKELPARAM(at->x,at->y+3);
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point); SendMessageW(window,WM_LBUTTONUP,0,point);
            app.poll(0);
        };
        for (unsigned dpi:{96u,144u,192u}) {
            RECT bounds; GetWindowRect(window,&bounds);
            SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),LPARAM(&bounds)); app.poll(0);
            single.setText("abcdef"); app.poll(0); click(*node,0);
            key(window,VK_HOME,false,true); key(window,VK_RIGHT,true); key(window,VK_RIGHT,true); app.poll(0);
            check(node->selection()==std::pair<size_t,size_t>{0,2},"Shift arrow creates partial selection");
            hosted_text::Session state; state.observe(node);
            check(state.start==0 && state.end==2,"Native selection reaches shared host session");
            SendMessageW(window,WM_CHAR,L'Z',0); app.poll(0);
            check(single.text()=="Zcdef" && node->selection().first==node->selection().second,"Typing replaces only selected range");
            key(window,VK_END,false,true); key(window,VK_LEFT,true); key(window,VK_LEFT,true); key(window,VK_DELETE); app.poll(0);
            check(single.text()=="Zcd","Delete removes backward selection");
            single.setText("e\xcc\x81\xf0\x9f\x8c\xb1x"); app.poll(0); key(window,VK_HOME,false,true);
            key(window,VK_DELETE); app.poll(0); check(single.text()=="\xf0\x9f\x8c\xb1x","Delete preserves combining cluster boundary");
            key(window,VK_DELETE); app.poll(0); check(single.text()=="x","Delete preserves emoji surrogate pair");
            single.setText("abcdef"); app.poll(0);
            const auto a=*text_navigation::locate(node->text_stops,{1,false}),b=*text_navigation::locate(node->text_stops,{4,false});
            SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(a.x,a.y+3));
            SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(b.x,b.y+3));
            SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(b.x,b.y+3)); app.poll(0);
            check(node->selection()==std::pair<size_t,size_t>{1,4},"Mouse drag selects a partial range");
            SendMessageW(window,WM_CHAR,8,0); app.poll(0); check(single.text()=="aef","Backspace removes only dragged selection");
            single.setText("abc \xd7\x90\xd7\x91\xd7\x92 xyz"); app.poll(0);
            // At the left edge of the Hebrew run the preceding space (offset 4)
            // and the last RTL letter (offset 7) have the same screen coordinate.
            const auto boundary = *text_navigation::locate(node->text_stops,{4,true});
            auto click_boundary = [&](int side) {
                const auto point = MAKELPARAM(boundary.x + side, boundary.y + 3);
                SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
                SendMessageW(window,WM_LBUTTONUP,0,point);
                app.poll(0);
            };
            click_boundary(-1);
            check(node->caret==4 && node->caret_trailing,"Click from LTR side selects preceding space");
            click_boundary(1);
            check(node->caret==7 && node->caret_trailing,"Click from RTL side selects final Hebrew letter");
            key(window,VK_RIGHT,true); app.poll(0);
            check(node->selection().first!=node->selection().second,"Shift Right selects into RTL run");
            key(window,VK_LEFT); app.poll(0);
            check(node->caret==7 && node->caret_trailing,"Selection collapses to the original visual anchor");
            const TextStop* split=nullptr;
            for (const auto& stop:node->text_stops) {
                const auto* other=text_navigation::locate(node->text_stops,{stop.index,!stop.trailing});
                if (other && (stop.x!=other->x || stop.y!=other->y)) { split=&stop; break; }
            }
            check(split!=nullptr,"Mixed bidi text has distinct caret affinities");
            node->caret=split->index; node->caret_trailing=split->trailing; node->clear_selection();
            key(window,VK_LEFT); app.poll(0);
            const auto* caret=text_navigation::locate(node->text_stops,{node->caret,node->caret_trailing});
            check(caret && node->hosted_editor.caret_x==caret->x && node->hosted_editor.caret_y==caret->y,"Painted caret follows stored bidi affinity");
            POINT native{}; check(GetCaretPos(&native) && native.x==caret->x && native.y==caret->y,"Native IME caret agrees with drawn bidi caret");
        }
        // Use the node coordinate API to target shaped caret stops after a
        // rotation. Native mouse capture, selection, typing and IME share it.
        single.setRenderTransform({0.8,0.3,-0.3,0.8,15,3});
        single.setText("abcdef"); app.poll(0);
        auto at_stop=[&](size_t index) {
            const auto* stop=text_navigation::locate(node->text_stops,{index,false});
            check(stop!=nullptr,"Transformed caret stop exists");
            const auto p=affine::map(node->world_transform,stop->x,stop->y+3);
            return MAKELPARAM(int(std::round(p.x)),int(std::round(p.y)));
        };
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,at_stop(1));
        SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,at_stop(4));
        SendMessageW(window,WM_LBUTTONUP,0,at_stop(4)); app.poll(0);
        check(node->selection()==std::pair<size_t,size_t>{1,4},"Transformed pointer drag selects the shaped range");
        SendMessageW(window,WM_CHAR,L'Z',0); app.poll(0);
        check(single.text()=="aZef","Rotated editor replaces selected range");
        const auto* stop=text_navigation::locate(node->text_stops,{node->caret,node->caret_trailing});
        check(stop,"Rotated IME stop exists");
        const auto ime=affine::map(node->world_transform,stop->x,stop->y);
        POINT native{};
        check(GetCaretPos(&native) && native.x==std::lround(ime.x) && native.y==std::lround(ime.y),"Rotated editor places native IME caret");
        single.setRenderTransform(affine::identity); app.poll(0);
        auto* multi=app.findFleckNode("multi").nativeHandle(); click(*multi,0);
        key(window,VK_HOME,false,true); key(window,VK_END); app.poll(0);
        check(multi->caret>0 && multi->caret<multi->text.size() && multi->caret_trailing,"End selects trailing edge of current wrapped line");
        const auto firstLine=*text_navigation::locate(multi->text_stops,{multi->caret,multi->caret_trailing});
        key(window,VK_DOWN,true); app.poll(0);
        const auto* nextLine=text_navigation::locate(multi->text_stops,{multi->caret,multi->caret_trailing});
        check(nextLine && nextLine->y>firstLine.y && multi->selection().first!=multi->selection().second,"Shift Down extends selection across shaped lines");
        key(window,VK_END,false,true); check(multi->caret==multi->text.size(),"Ctrl End reaches document boundary");
        SendMessageW(window,WM_CHAR,13,0); app.poll(0);
        check(!multi->caret_trailing && multi->text.back()==L'\n',"Enter selects leading edge of new line");
        const auto beforeComposition=multi->caret;
        SendMessageW(window,WM_IME_STARTCOMPOSITION,0,0);
        key(window,VK_LEFT); key(window,VK_DELETE);
        check(multi->caret==beforeComposition && multi->text.back()==L'\n',"IME candidate keys preserve editor state");
        SendMessageW(window,WM_IME_ENDCOMPOSITION,0,0);
        for (unsigned dpi:{96u,144u,192u}) {
            RECT bounds; GetWindowRect(window,&bounds);
            SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),LPARAM(&bounds)); app.poll(0);
            app.findFleckNode("multi").setText("abcdefghij\nx\nabcdefghij"); app.poll(0);
            key(window,VK_HOME,false,true); app.poll(0);
            for(int i=0;i<6;++i) { key(window,VK_RIGHT); app.poll(0); }
            check(multi->caret==6,"Vertical navigation fixture starts in the sixth column");
            key(window,VK_DOWN);app.poll(0);
            check(multi->caret==12,"Short middle line clamps to its end");
            key(window,VK_DOWN);app.poll(0);
            check(multi->caret==19,"Down through a short line restores the original column");
            key(window,VK_UP);app.poll(0);key(window,VK_UP);app.poll(0);
            check(multi->caret==6,"Up through a short line restores the original column");
            key(window,VK_DOWN,true);app.poll(0);key(window,VK_DOWN,true);app.poll(0);
            check(multi->selection()==std::pair<size_t,size_t>{6,19},"Shift Down preserves preferred column and selection anchor");
            key(window,VK_LEFT);app.poll(0); // Collapse selection, then establish a new column.
            key(window,VK_LEFT);app.poll(0);
            check(multi->caret==5,"Horizontal movement resets preferred column");
            key(window,VK_DOWN);app.poll(0);key(window,VK_DOWN);app.poll(0);
            check(multi->caret==18,"Vertical movement uses the new horizontal column");
            click(*multi,2);key(window,VK_DOWN);app.poll(0);key(window,VK_DOWN);app.poll(0);
            check(multi->caret==15,"Mouse placement resets preferred column");
            check(multi->caret_column.has_value(),"Vertical navigation retains a preferred column");
            SendMessageW(window,WM_CHAR,L'Z',0); app.poll(0);
            check(!multi->caret_column,"Typing clears the preferred column");
            key(window,VK_UP);app.poll(0);key(window,VK_UP);app.poll(0);
            check(multi->caret==3,"Navigation after typing uses the edited caret column");
            app.findFleckNode("multi").setText("abcdefghij\nx\nabcdefghij"); app.poll(0);
            check(!multi->caret_column,"Programmatic text replacement clears the preferred column");
        }

        if (skia) {
            wchar_t title[128]{};
            check(GetWindowTextW(window,title,128) && std::wstring(title)==L"Sunfleck M2 editing [GPU]",
                  "Editing must remain on Skia GPU rendering");
        }
        std::cout<<"Native partial selection, cluster editing, bidi affinity and IME geometry passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
