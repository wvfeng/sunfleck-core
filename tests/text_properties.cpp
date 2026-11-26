#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include "../src/dwrite_text.h"
#include "../src/document.h"
#include "../src/fleck.h"
#include <iostream>
static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static ui_binding_value scalar(ui_value_kind kind,int64_t n) { ui_binding_value v{}; v.kind=kind; v.integer=n; return v; }
int main() {
    try {
        sunfleck::App app(UI_SOFTWARE,true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root' Foreground='#80224466' FontSize='22' FlowDirection='RightToLeft'><TextBlock Id='text' Text='Text' TextAlignment='Center' TextWrapping='NoWrap' TextTrimming='CharacterEllipsis' TextDecorations='Underline' LineHeight='34'/><DocumentView Id='doc'/></StackPanel></Window>");
        app.openWindow(); app.poll(0);
        auto root=app.findFleckNode("root"),text=app.findFleckNode("text"),doc=app.findFleckNode("doc");
        check(text.propertyValue(UI_PROPERTY_FOREGROUND).integer==0x80224466,"Inherited foreground");
        check(text.propertyValue(UI_PROPERTY_FLOW_DIRECTION).integer==UI_FLOW_RIGHT_TO_LEFT,"Inherited flow direction");
        check(text.propertySource(UI_PROPERTY_FOREGROUND)==UI_PROPERTY_SOURCE_INHERITED,"Foreground source");
        text.setPropertyValue(UI_PROPERTY_FOREGROUND,scalar(UI_VALUE_COLOR,0xffabcdef));
        text.clearProperty(UI_PROPERTY_FOREGROUND);
        check(text.propertyValue(UI_PROPERTY_FOREGROUND).integer==0x80224466,"Foreground clear");
        const auto invalid=scalar(UI_VALUE_ENUM,99);
        check(ui_node_set_property(text.nativeHandle(),UI_PROPERTY_TEXT_ALIGNMENT,&invalid)!=UI_OK && text.propertyValue(UI_PROPERTY_TEXT_ALIGNMENT).integer==UI_TEXT_ALIGN_CENTER,"Invalid enum retains value");
        root.setPropertyValue(UI_PROPERTY_TEXT_WRAPPING,scalar(UI_VALUE_ENUM,UI_TEXT_NO_WRAP));
        check(doc.propertyValue(UI_PROPERTY_TEXT_WRAPPING).integer==UI_TEXT_WRAP,"Wrapping is not inherited");
        const auto retained=root.nativeHandle();
        try { app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock LineHeight='-1'/></Window>"); check(false,"Invalid line height accepted"); } catch(const sunfleck::Error&) {}
        check(app.findFleckNode("root").nativeHandle()==retained,"Invalid text refresh retains old tree");
        const std::string xml="<Window xmlns='urn:sunfleck:v1'><TextBlock Id='t' Foreground='#80402010' LineHeight='28' TextAlignment='Right' TextWrapping='NoWrap' TextTrimming='WordEllipsis' TextDecorations='Underline,Strikethrough' FlowDirection='RightToLeft'/></Window>";
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        check(fleck::parse(xml,page,diagnostic),"Text properties parse");
        const auto bytes=fleck::compile_resource(page);
        sunfleck::App compiled(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(compiled.nativeHandle(),bytes.data(),bytes.size(),&diagnostic)==UI_OK,"Text resource load");
        check(compiled.findFleckNode("t").propertyValue(UI_PROPERTY_TEXT_DECORATIONS).integer==3 && compiled.findFleckNode("t").propertyValue(UI_PROPERTY_LINE_HEIGHT).number==28,"Resource preserves high property bits");
        const std::string document=R"({"version":1,"blocks":[{"kind":"paragraph","spans":[{"text":"A long paragraph to remeasure after changing the inherited font size."}]}]})";
        check(ui_document_load(doc.nativeHandle(),document.data(),document.size())==UI_OK,"Document load");
        app.poll(0);
        const auto oldHeight=doc.nativeHandle()->document->height;
        ui_binding_value size{}; size.kind=UI_VALUE_DOUBLE; size.number=48;
        root.setPropertyValue(UI_PROPERTY_FONT_SIZE,size); app.poll(0);
        check(doc.nativeHandle()->document->height>oldHeight,"Inherited font change invalidates document layout");
#ifdef _WIN32
        TextFormat format; format.size=18; format.line_height=32; format.alignment=UI_TEXT_ALIGN_CENTER; format.wrapping=UI_TEXT_NO_WRAP;
        TextFormatScope scope(format);
        std::vector<FillRectCommand> commands; std::vector<TextStop> stops;
        EditorGeometry editor{stops,0,true};
        windows_text::append(commands,{0,0,300,200},{0,0,300,200},{0,0,0,255},96,L"abc",false,&editor,0);
        check(!stops.empty() && stops.front().x>100,"Center alignment changes caret geometry");
        check(windows_text::line_height(96)==32,"Explicit line height");
        format.alignment=UI_TEXT_ALIGN_LEFT; format.wrapping=UI_TEXT_WRAP;
        commands.clear();
        windows_text::append(commands,{0,0,50,200},{0,0,50,200},{0,0,0,255},96,L"one two three",false,&editor,0);
        check(std::any_of(stops.begin(),stops.end(),[](const auto& s){return s.y>=32;}),"Wrap uses configured line height");
        format.wrapping=UI_TEXT_NO_WRAP;
        commands.clear();
        windows_text::append(commands,{0,0,50,200},{0,0,50,200},{0,0,0,255},96,L"one two three",false,&editor,0);
        check(!stops.empty() && std::all_of(stops.begin(),stops.end(),[&](const auto& s){return s.y==stops.front().y;}),"NoWrap remains on one line");
        format.flow=UI_FLOW_RIGHT_TO_LEFT; format.alignment=UI_TEXT_ALIGN_RIGHT;
        commands.clear();
        windows_text::append(commands,{0,0,300,200},{0,0,300,200},{0,0,0,255},96,L"\u05d0\u05d1\u05d2",false,&editor,0);
        check(stops.front().x>stops.back().x && stops.back().x>200,"RTL caret ordering and absolute right alignment");
        for (unsigned dpi:{96u,144u,192u}) check(windows_text::line_height(dpi)==int(32*dpi/96),"Line height DPI scaling");
        format.flow=UI_FLOW_LEFT_TO_RIGHT; format.alignment=UI_TEXT_ALIGN_LEFT;
        format.foreground_set=true; format.foreground=0x80224466;
        commands.clear(); append_text(commands,{0,0,300,100},{0,0,300,100},{0,0,0,255},96,L"Text",true);
        check(!commands.empty() && std::all_of(commands.begin(),commands.end(),[](auto c){return c.color[0]==0x22 && c.color[1]==0x44 && c.color[2]==0x66 && c.color[3]<=0x80;}),"Foreground keeps ARGB alpha");
        const auto coverage=[&] { uint64_t total=0; for(auto c:commands) total+=uint64_t(c.right-c.left)*(c.bottom-c.top)*c.color[3]; return total; };
        const auto plain=coverage();
        format.decorations=UI_TEXT_UNDERLINE_STRIKETHROUGH;
        commands.clear(); append_text(commands,{0,0,300,100},{0,0,300,100},{0,0,0,255},96,L"Text",true);
        check(coverage()>plain,"Text decorations add glyph coverage");
        format.decorations=0; format.foreground_set=false; format.trimming_set=true; format.trimming=UI_TEXT_TRIM_NONE;
        commands.clear(); append_text(commands,{0,0,50,100},{0,0,50,100},{0,0,0,255},96,L"one two three",true);
        const auto clipped=coverage();
        format.trimming=UI_TEXT_TRIM_CHARACTER_ELLIPSIS;
        commands.clear(); append_text(commands,{0,0,50,100},{0,0,50,100},{0,0,0,255},96,L"one two three",true);
        check(coverage()!=clipped,"Explicit trimming changes rendered overflow");
#endif
        std::cout<<"Text appearance and layout semantics passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
