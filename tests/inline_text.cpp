#include <sunfleck/elements.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include <fstream>
#include <iostream>
extern "C" ui_result load_inlines(ui_app*,ui_fleck_diagnostic*);
static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        check(argc==2,"Inline fixture required");
        std::ifstream file(argv[1]); const std::string xml{std::istreambuf_iterator<char>(file),{}};
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        check(fleck::parse(xml,page,diagnostic),"Parse Run/Span");
        const auto bytes=fleck::compile_resource(page);
        for (int mode=0;mode<3;++mode) {
            sunfleck::App app(UI_SOFTWARE,true);
            if (mode==0) app.loadFleck(xml);
            if (mode==1) check(ui_app_load_fleck_resource(app.nativeHandle(),bytes.data(),bytes.size(),&diagnostic)==UI_OK,"Inline resource");
            if (mode==2) check(load_inlines(app.nativeHandle(),&diagnostic)==UI_OK,"Generated C inline page");
            app.openWindow(); app.poll(0);
            auto paragraph=app.findFleckNode("paragraph"),red=app.findFleckNode("red"),blue=app.findFleckNode("blue");
            check(paragraph.text().starts_with("Large red blue text"),"TextBlock exposes combined text");
            check(blue.propertyValue(UI_PROPERTY_FOREGROUND).integer==0xff0000ff && blue.propertyValue(UI_PROPERTY_FONT_STYLE).integer==UI_FONT_ITALIC,"Span inherits character styling");
            auto* n=paragraph.nativeHandle();
            check(n->resolved_runs.size()==2 && n->bounds.bottom-n->bounds.top>48,"Styled runs share paragraph measurement");
            check(app.findFleckNode("next").nativeHandle()->bounds.top>=n->bounds.bottom,"Inline Auto height positions next control");
#ifdef _WIN32
            uint32_t width=0,height=0; const auto* pixels=ui_app_pixels(app.nativeHandle(),&width,&height);
            bool has_red=false,has_blue=false;
            for (int y=n->visible.top;y<n->visible.bottom;++y) for (int x=n->visible.left;x<n->visible.right;++x) {
                const auto* p=pixels+(size_t(y)*width+x)*4;
                has_red|=p[0]>180 && p[1]<80 && p[2]<80; has_blue|=p[2]>180 && p[0]<80 && p[1]<80;
            }
            check(has_red && has_blue,"Per-run foreground reaches shared renderer");
#endif
            std::string value="Updated run";
            sunfleck::PropertyModel model(app); model.property("Text",[&]{return value;}); model.bind(blue,UI_PROPERTY_TEXT,"Text",UI_BIND_ONE_WAY);
            app.poll(0); check(paragraph.text()=="Large red Updated run","Run binding updates composed text");
            value="Updated again"; model.notify("Text"); app.poll(0); check(paragraph.text().ends_with(value),"Nested run binding refresh");
            const auto old=n;
            try { app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock><Button/></TextBlock></Window>"); check(false,"Visual child accepted as inline"); } catch(const sunfleck::Error&) {}
            check(app.findFleckNode("paragraph").nativeHandle()==old,"Invalid inline refresh preserves tree");
        }
        sunfleck::App code(UI_SOFTWARE,true);
        sunfleck::elements::TextBlock paragraph(code); sunfleck::elements::Span span(code); sunfleck::elements::Run run(code);
        run.node().setText("C++ inline"); span.node().append(run.node()); paragraph.node().append(span.node()); code.setRoot(paragraph.node());
        code.openWindow(); code.poll(0); check(paragraph.node().text()=="C++ inline","C++ typed inline construction");
        std::cout<<"Run/Span styling, paragraph geometry, binding, C/C++/resource parity and rollback passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
