#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include "../src/dwrite_text.h"
#include <fstream>
#include <iostream>
#include <thread>

static void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        check(argc==3,"Font fixture path and family required");
        const std::string family = argv[2];
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(file),{}};
        check(!bytes.empty(),"Font fixture readable");
        sunfleck::App app(UI_SOFTWARE,true),other(UI_SOFTWARE,true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='text' FontFamily='" + family +
            "' FontSize='24' Text='iiii WWWW'/></Window>");
        app.openWindow(); app.poll(0);
        auto text=app.findFleckNode("text").nativeHandle();
        check(!text->resolved_font.resources,"No collection before registration");
        const auto id=app.registerFont(bytes.data(),bytes.size());
        const auto names=app.fontFamilies(id);
        check(std::find(names.begin(),names.end(),family)!=names.end(),"Registered family discovery");
        std::fill(bytes.begin(),bytes.end(),0); // Registration must own its font bytes.
        app.poll(0);
        check(bool(text->resolved_font.resources),"Registration invalidates existing text");
        std::weak_ptr<const windows_text::FontCollection> lifetime=text->resolved_font.resources;
        {
            TextFormatScope scope(text->resolved_font);
            check(windows_text::width(L"iiii",96)==windows_text::width(L"WWWW",96),"Custom monospaced font used for shaping");
        }
        ui_font_id unchanged=12345;
        check(ui_app_register_font(app.nativeHandle(),bytes.data(),bytes.size(),&unchanged)==UI_ERROR_INVALID_ARGUMENT && unchanged==12345,"Invalid font registration is atomic");
        uint32_t count=99;
        check(ui_app_font_family_count(other.nativeHandle(),id,&count)==UI_ERROR_INVALID_ARGUMENT && count==99,"Font IDs are scoped to the owning application");
        char sentinel='x'; size_t size=1;
        check(ui_app_font_family(app.nativeHandle(),id,0,&sentinel,&size)==UI_ERROR_INVALID_ARGUMENT && sentinel=='x' && size>1,"Short family buffer does not overwrite output");
        ui_result wrong=UI_OK;
        std::thread worker([&]{wrong=ui_app_unregister_font(app.nativeHandle(),id);}); worker.join();
        check(wrong==UI_ERROR_WRONG_THREAD,"Font registration thread affinity");
        app.unregisterFont(id); app.poll(0);
        check(!text->resolved_font.resources && lifetime.expired(),"Unregister retires cached font layouts and resources");
        check(ui_app_unregister_font(app.nativeHandle(),id)==UI_ERROR_INVALID_ARGUMENT,"Unregistered ID cannot be reused");
        std::cout<<"Application font copying, discovery, shaping, isolation, transactions and retirement passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
