#include "../src/skia_d3d12_surface.h"
#include "../src/ui_tree.h"
#include <sunfleck/ui.hpp>
#ifdef SUNFLECK_TEST_D3D11
#include "../src/d3d11_surface.h"
#endif
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Window {
    HWND handle=CreateWindowExW(0,L"STATIC",L"Skia surface regression",WS_OVERLAPPEDWINDOW,
        0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    ~Window() { if(handle) DestroyWindow(handle); }
};
static void compare_scene(SkiaD3d12Surface& skia) {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Canvas Id='scene'><Canvas Opacity='0.5' Width='40' Height='20' RenderTransform='0,1,-1,0,45,5'>"
                  "<Rectangle Width='28' Height='15' Fill='#DDEEFF'/><Ellipse Canvas.Left='10' Width='25' Height='15' Fill='#1133DD'/>"
                  "</Canvas><TextBlock Canvas.Left='45' Width='68' Height='28' Text='GPU'/><Path Canvas.Top='36' Width='20' Height='15' Data='M0,0 L18,0 L0,14 Z' Fill='#AA323C'/>"
                  "</Canvas></Window>");
    drawing::CommandList list;
    std::vector<FillRectCommand> reference;
    std::vector<ui_node*> hits,scroll;
    render_tree_with_pick_order(*app.findFleckNode("scene").nativeHandle(),{0,0,120,60},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
        builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,reference,hits,scroll,nullptr,nullptr,&list);
    list.flush(reference);
    for(auto kind:{drawing::CommandKind::layer,drawing::CommandKind::transform,drawing::CommandKind::clip,
                   drawing::CommandKind::glyph_coverage,drawing::CommandKind::image})
        require(std::any_of(list.commands.begin(),list.commands.end(),[&](const auto& c){return c.kind==kind;}),"Scene missing recorded command");
    std::vector<uint8_t> pixels; rasterize(pixels,120,60,reference);
    const auto compare=[&](const std::vector<uint8_t>& actual,const char* name) {
        require(actual.size()==pixels.size(),"Scene capture size");
        int delta=0;
        for(size_t i=0;i<pixels.size();++i) delta=std::max(delta,std::abs(int(actual[i])-int(pixels[i])));
        std::printf("CPU/%s composed scene max channel delta %d\n",name,delta);
        require(delta<=3,"Composed scene tolerance exceeded");
    };
    require(skia.present_frame(120,60,reference,list),"Replay stateful scene");
    compare(skia.captured_pixels(),"Skia");
#ifdef SUNFLECK_TEST_D3D11
    // WARP-only systems may have no D3D11 hardware. Exercise D3D11 on the hardware run.
    if (!std::getenv("SUNFLECK_TEST_SKIA_D3D12_WARP")) {
        Window window;
        D3d11Surface gpu;
        _putenv_s("SUNFLECK_TEST_D3D11_CAPTURE","1");
        require(gpu.open(window.handle) && gpu.present(120,60,reference),"D3D11 reference texture upload");
        compare(gpu.captured,"D3D11");
        _putenv_s("SUNFLECK_TEST_D3D11_CAPTURE","");
    }
#endif
}
int main() {
    try {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        require(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"D3D12 debug layer required");
        debug->EnableDebugLayer();
        _putenv_s("SUNFLECK_TEST_SKIA_D3D12_CAPTURE","1");
        Window first,second;
        require(first.handle && second.handle,"create two windows");
        {
            SkiaD3d12Surface a,b;
            require(a.open(first.handle) && b.open(second.handle),"open two independent devices");
            require(a.present(0,0,{}),"minimized window");
            compare_scene(a);
            int max_error=0;
            for (int frame=0;frame<12;++frame) {
                // Reuse both swapchain buffers and resize each independent window.
                if (frame==6) require(a.present(0,0,{}),"minimize after active presentation");
                const int scale=1+frame/4;
                const int width=96*scale,height=64*scale;
                std::vector<FillRectCommand> commands{
                    {-10,-10,width+10,height+10,{29,47,91,255}},
                    {4,5,80,40,{230,11,73,127}},
                    {12,17,90,54,{11,230,103,83}},
                    {-8,30,20,height+4,{255,250,5,210}},
                    {5,5,2,2,{255,0,0,255}},
                    {width+2,0,width+4,5,{255,0,0,255}}};
                if (frame%2) commands.erase(commands.begin()); // transparent destination too
                if (frame % 3 != 0) {
                    const auto first = commands.size();
                    commands.push_back({8,11,30,26,{43,87,242,192}});
                    commands.push_back({17,13,40,29,{202,33,67,215}});
                    drawing::composite_opacity(commands,first,0.65);
                    drawing::transform(commands,first,{0.85,0.25,-0.25,0.85,10,3},{0,0,width,height});
                }
                require(a.present(width,height,commands),"render first window");
                require(b.present(width+3,height+7,commands),"render second window");
                std::vector<std::uint8_t> expected;
                for (int window=0;window<2;++window) {
                    rasterize(expected,width+window*3,height+window*7,commands);
                    const auto& actual=(window?b:a).captured_pixels();
                    require(actual.size()==expected.size(),"capture dimensions");
                    for(size_t i=0;i<actual.size();++i)
                        max_error=std::max(max_error,std::abs(int(actual[i])-int(expected[i])));
                }
            }
            std::printf("CPU/Skia maximum channel delta: %d\n",max_error);
            require(max_error<=3,"CPU/Skia alpha and clipping tolerance exceeded");
            // Readback synchronizes Skia internally; also exercise resize with
            // genuinely asynchronous outstanding frames and no capture path.
            _putenv_s("SUNFLECK_TEST_SKIA_D3D12_CAPTURE","");
            for(int frame=0;frame<6;++frame)
                require(a.present(71+frame,59+frame,{}),"resize without readback synchronization");
            require(a.debug_clean() && b.debug_clean(),"D3D12 error/corruption messages");
            std::printf("Two windows, resize, transparent blending: max channel delta %d; debug errors 0\n",max_error);
            _putenv_s("SUNFLECK_TEST_SKIA_D3D12_FAILURE","present");
            require(!a.present(96,64,{}),"present injection must propagate failure");
            _putenv_s("SUNFLECK_TEST_SKIA_D3D12_FAILURE","");
        }
        {
            SkiaD3d12Surface reopened;
            require(reopened.open(first.handle) && reopened.present(32,32,{}),"reopen after teardown");
            require(reopened.debug_clean(),"reopened device validation");
        }
        _putenv_s("SUNFLECK_TEST_SKIA_D3D12_FAILURE","initialize");
        SkiaD3d12Surface failed;
        require(!failed.open(first.handle),"initialize injection must propagate failure");
        _putenv_s("SUNFLECK_TEST_SKIA_D3D12_FAILURE","");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());return 1;
    }
}
