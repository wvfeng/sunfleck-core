#include <sunfleck/ui.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include "../src/dwrite_text.h"
#include <iostream>
#include <limits>

static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static ui_binding_value number(double n) { ui_binding_value v{}; v.kind=UI_VALUE_DOUBLE; v.number=n; return v; }
int main() {
    try {
        sunfleck::App app(UI_SOFTWARE,true);
        const std::string xml="<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root' FontFamily='Consolas' FontSize='24' FontWeight='Bold' FontStyle='Italic' FontStretch='Normal'><TextBlock Id='text' Text='MMMM' HorizontalAlignment='Left'/><TextBox Id='edit' Text='Combining e&#x301;'/></StackPanel></Window>";
        app.loadFleck(xml); app.openWindow(); app.poll(0);
        auto root=app.findFleckNode("root"), text=app.findFleckNode("text"), edit=app.findFleckNode("edit");
        check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==24,"Inherited font size");
        check(std::string(text.propertyValue(UI_PROPERTY_FONT_FAMILY).string)=="Consolas","Inherited font family");
        check(text.propertySource(UI_PROPERTY_FONT_SIZE)==UI_PROPERTY_SOURCE_INHERITED,"Inherited source");
        check(text.nativeHandle()->resolved_font.weight==700 && text.nativeHandle()->resolved_font.style==2,"Inherited font styling reaches layout");
        // Explicit equal values must establish a local value, not expose a stale field default.
        text.setPropertyValue(UI_PROPERTY_FONT_SIZE,number(24));
        root.setPropertyValue(UI_PROPERTY_FONT_SIZE,number(32));
        check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==24 && text.propertySource(UI_PROPERTY_FONT_SIZE)==UI_PROPERTY_SOURCE_LOCAL,"Equal local value interrupts inheritance");
        text.clearProperty(UI_PROPERTY_FONT_SIZE);
        check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==32,"Clear restores current inherited value");
        text.setPropertyLayer(UI_PROPERTY_FONT_SIZE,UI_PRECEDENCE_STYLE,number(18));
        text.setPropertyLayer(UI_PROPERTY_FONT_SIZE,UI_PRECEDENCE_ANIMATION,number(36));
        check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==36 && text.basePropertyValue(UI_PROPERTY_FONT_SIZE).number==18,"Animation and base are distinct");
        text.clearPropertyLayer(UI_PROPERTY_FONT_SIZE,UI_PRECEDENCE_STYLE);
        root.setPropertyValue(UI_PROPERTY_FONT_SIZE,number(40));
        check(text.basePropertyValue(UI_PROPERTY_FONT_SIZE).number==40,"Animation base tracks inheritance");
        text.clearPropertyLayer(UI_PROPERTY_FONT_SIZE,UI_PRECEDENCE_ANIMATION);
        check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==40,"Removing animation restores current inheritance");
        double size=20; sunfleck::PropertyModel model(app); model.property("Size",[&]{return size;});
        model.bind(text,UI_PROPERTY_FONT_SIZE,"Size",UI_BIND_ONE_WAY);
        check(text.propertySource(UI_PROPERTY_FONT_SIZE)==UI_PROPERTY_SOURCE_BINDING,"Font binding source");
        size=30; model.notify("Size"); app.poll(0);
        check(text.nativeHandle()->resolved_font.size==30,"Font binding updates layout");
        for(double invalid:{0.,-1.,4097.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            auto value=number(invalid);
            check(ui_node_set_property(text.nativeHandle(),UI_PROPERTY_FONT_SIZE,&value)!=UI_OK && text.propertyValue(UI_PROPERTY_FONT_SIZE).number==30,"Invalid size preserves effective value");
        }
        size=22; model.notify("Size"); check(text.propertyValue(UI_PROPERTY_FONT_SIZE).number==22,"Invalid set preserves binding");
        text.clearProperty(UI_PROPERTY_FONT_SIZE);
        for(unsigned dpi:{96u,144u,192u}) {
            check(ui_host_resize(app.nativeHandle(),900,700,dpi)==UI_OK,"Font DPI resize");
            root.setPropertyValue(UI_PROPERTY_FONT_SIZE,number(14)); app.poll(0);
            const auto small=(text.nativeHandle()->bounds.right-text.nativeHandle()->bounds.left);
            root.setPropertyValue(UI_PROPERTY_FONT_SIZE,number(42)); app.poll(0);
            check((text.nativeHandle()->bounds.right-text.nativeHandle()->bounds.left)>small*2,"Inherited font invalidates child width cache");
            check(edit.nativeHandle()->bounds.bottom-edit.nativeHandle()->bounds.top>=int(42*dpi/96),"Editor expands for large font");
        }
        check(ui_host_focus(app.nativeHandle(),1)==UI_OK && ui_host_key_event(app.nativeHandle(),UI_HOST_TAB,0)==UI_OK,"Focus font editor");
        ui_host_text_state state{}; state.struct_size=sizeof(state); state.version=1;
        check(ui_host_text_read(app.nativeHandle(),&state)==UI_OK && state.active && state.font_size==84 && state.line_height>=84,"Hosted input exposes effective font geometry");
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        check(fleck::parse(xml,page,diagnostic),"Font Fleck parse"); auto resource=fleck::compile_resource(page);
        sunfleck::App compiled(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(compiled.nativeHandle(),resource.data(),resource.size(),&diagnostic)==UI_OK,"Font compiled resource");
        check(compiled.findFleckNode("text").propertyValue(UI_PROPERTY_FONT_SIZE).number==24,"Resource preserves font inheritance");
        const auto retained=root.nativeHandle();
        try { app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock FontSize='nan'/></Window>"); check(false,"Invalid font accepted"); } catch(const sunfleck::Error&) {}
        check(app.findFleckNode("root").nativeHandle()==retained,"Invalid font refresh retains old page");
#ifdef _WIN32
        TextFormat font; font.family="Segoe UI";
        TextFormatScope scope(font);
        const auto normal=windows_text::width(L"WiiiWiii",96);
        font.size=28;
        check(std::abs(windows_text::width(L"WiiiWiii",96)-normal*2)<=1,"DirectWrite cache keys size");
        font.size=14; font.family="Consolas";
        check(windows_text::width(L"WiiiWiii",96)!=normal,"DirectWrite cache keys family");
        for(auto weight:{400,700}) for(auto style:{0,2}) for(auto stretch:{3,5,7}) {
            font.weight=weight; font.style=style; font.stretch=stretch;
            check(windows_text::width(L"abc",96)>0,"Font faces resolve");
        }
#endif
        std::cout<<"Font inheritance, binding, precedence, invalid updates, resources and DPI layout passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
