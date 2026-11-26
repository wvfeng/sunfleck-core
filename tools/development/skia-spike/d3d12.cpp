#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "scene.h"
#include "text.h"
#include "d3d12-adapter.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendContext.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DDirectContext.h"
#include <d3d12sdklayers.h>
#include <filesystem>
#include <sstream>
#include <vector>

static void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) {
        std::ostringstream error;error<<operation<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr);
        throw std::runtime_error(error.str());
    }
}
struct Window {
    HWND handle=nullptr;
    explicit Window(int index) {
        WNDCLASSW type{};type.lpfnWndProc=DefWindowProcW;type.hInstance=GetModuleHandleW(nullptr);
        type.lpszClassName=L"SunfleckSkiaD3D12Evaluation";RegisterClassW(&type);
        handle=CreateWindowW(type.lpszClassName,L"Sunfleck Skia D3D12 evaluation",WS_OVERLAPPEDWINDOW,
            40+index*60,40+index*60,520,380,nullptr,nullptr,type.hInstance,nullptr);
        require(handle!=nullptr,"Create D3D12 probe window");ShowWindow(handle,SW_SHOWNOACTIVATE);
    }
    void size(int width,int height) {
        RECT bounds{0,0,width,height};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
        require(SetWindowPos(handle,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,
            SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE),"Resize native window");
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        RECT client{};require(GetClientRect(handle,&client)&&client.right==width&&client.bottom==height,"Actual native client dimensions");
    }
    ~Window(){if(handle)DestroyWindow(handle);}
};
struct Context {
    gr_cp<IDXGIFactory4> factory;
    GrD3DBackendContext backend;
    sk_sp<GrDirectContext> skia;
    gr_cp<IDXGISwapChain3> swapchain;
    gr_cp<ID3D12Fence> fence;
    gr_cp<ID3D12InfoQueue> debugMessages;
    gr_cp<ID3D12Resource> buffers[2];
    sk_sp<SkSurface> surfaces[2];
    HANDLE event=nullptr;
    UINT64 nextFence=0;
    Context(HWND window,bool warp,bool requireDebug) {
        check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");
        if(warp) {
            check(factory->EnumWarpAdapter(IID_PPV_ARGS(&backend.fAdapter)),"Select explicit WARP adapter");
            check(D3D12CreateDevice(backend.fAdapter.get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&backend.fDevice)),"Create WARP D3D12 device");
        } else {
            for(UINT index=0;;++index) {
                gr_cp<IDXGIAdapter1> adapter;const auto hr=factory->EnumAdapters1(index,&adapter);
                if(hr==DXGI_ERROR_NOT_FOUND)break;check(hr,"Enumerate hardware adapter");
                DXGI_ADAPTER_DESC1 desc{};check(adapter->GetDesc1(&desc),"Adapter description");
                if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
                if(SUCCEEDED(D3D12CreateDevice(adapter.get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&backend.fDevice)))) {
                    backend.fAdapter=adapter;break;
                }
            }
            require(backend.fDevice.get()!=nullptr,"No hardware D3D12 adapter; use --warp explicitly for software validation");
        }
        backend.fDevice->QueryInterface(IID_PPV_ARGS(&debugMessages));
        require(!requireDebug||bool(debugMessages),"Required D3D12 debug info queue is unavailable");
        DXGI_ADAPTER_DESC1 desc{};check(backend.fAdapter->GetDesc1(&desc),"Selected adapter");
        std::wcout<<L"D3D12 ";printD3D12Adapter(backend.fAdapter.get(),desc);std::wcout<<L'\n';
        D3D12_COMMAND_QUEUE_DESC queue{};queue.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(backend.fDevice->CreateCommandQueue(&queue,IID_PPV_ARGS(&backend.fQueue)),"Create D3D12 queue");
        skia=GrDirectContexts::MakeD3D(backend);require(bool(skia),"Create Skia D3D12 context");
        DXGI_SWAP_CHAIN_DESC1 descSwap{};descSwap.Width=480;descSwap.Height=320;descSwap.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        descSwap.BufferCount=2;descSwap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        descSwap.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;descSwap.SampleDesc.Count=1;
        gr_cp<IDXGISwapChain1> initial;
        check(factory->CreateSwapChainForHwnd(backend.fQueue.get(),window,&descSwap,nullptr,nullptr,&initial),"Create D3D12 swapchain");
        check(initial->QueryInterface(IID_PPV_ARGS(&swapchain)),"Swapchain interface");
        check(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"Window association");
        check(backend.fDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create fence");
        setup(480,320);
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"Create fence event");
    }
    void wait() {
        check(backend.fQueue->Signal(fence.get(),++nextFence),"Signal completion fence");
        if(fence->GetCompletedValue()<nextFence) {
            check(fence->SetEventOnCompletion(nextFence,event),"Set fence event");
            require(WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"D3D12 fence completion within 30 seconds");
        }
        check(backend.fDevice->GetDeviceRemovedReason(),"D3D12 device health");
    }
    void setup(int width,int height) {
        for(UINT index=0;index<2;++index) {
            check(swapchain->GetBuffer(index,IID_PPV_ARGS(&buffers[index])),"Acquire swapchain buffer");
            const auto desc=buffers[index]->GetDesc();
            require(desc.Width==UINT64(width)&&desc.Height==UINT(height),"Actual D3D12 buffer dimensions");
            GrD3DTextureResourceInfo info;
            info.fResource=buffers[index];info.fResourceState=D3D12_RESOURCE_STATE_PRESENT;
            info.fFormat=DXGI_FORMAT_R8G8B8A8_UNORM;info.fSampleCount=1;info.fLevelCount=1;info.fSampleQualityPattern=0;
            auto target=GrBackendRenderTargets::MakeD3D(width,height,info);
            surfaces[index]=SkSurfaces::WrapBackendRenderTarget(skia.get(),target,kTopLeft_GrSurfaceOrigin,kRGBA_8888_SkColorType,nullptr,nullptr);
            require(bool(surfaces[index]),"Wrap D3D12 buffer in Skia");
        }
    }
    void resize(int width,int height) {
        require(skia->flushAndSubmit(GrSyncCpu::kYes).fSuccess,"Flush before resize");wait();
        for(auto& surface:surfaces)surface.reset();for(auto& buffer:buffers)buffer.reset();
        check(swapchain->ResizeBuffers(2,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0),"ResizeBuffers");setup(width,height);
    }
    void resourceSemantics() {
        require(skia->maxSurfaceSampleCountForColorType(kN32_SkColorType)>=4,"4x MSAA support");
        for(int samples:{0,4}) {
            auto surface=SkSurfaces::RenderTarget(skia.get(),skgpu::Budgeted::kNo,
                SkImageInfo::MakeN32Premul(64,64),samples,nullptr);
            require(bool(surface),"Discard/load regression surface");
            const SkPoint points[]={{0,0},{64,64}};
            const SkColor4f colors[]={SkColors::kBlue,SkColors::kGreen};
            SkPaint gradient;
            gradient.setShader(SkShaders::LinearGradient(points,
                SkGradient(SkGradient::Colors(colors,SkTileMode::kClamp),{})));
            surface->getCanvas()->discard();
            surface->getCanvas()->drawPaint(gradient);
            const auto before=readback(surface.get());
            require(SkColorGetB(before.getColor(8,8))>SkColorGetG(before.getColor(8,8))
                &&SkColorGetG(before.getColor(55,55))>SkColorGetB(before.getColor(55,55)),
                "Discard followed by full gradient draw");
            // A later load pass must preserve pixels outside the partial update.
            SkPaint red;red.setColor(SK_ColorRED);
            surface->getCanvas()->drawRect(SkRect::MakeXYWH(4,4,16,16),red);
            const auto after=readback(surface.get());
            for(int y=0;y<64;++y) {
                for(int x=0;x<64;++x) {
                    const bool changed=x>=4&&x<20&&y>=4&&y<20;
                    require(after.getColor(x,y)==(changed?SK_ColorRED:before.getColor(x,y)),
                        "Load must preserve pixels outside the partial update");
                }
            }
            std::cout<<"PASS resource semantics: requestedSamples="<<samples<<" discard/full draw, load/partial draw\n";
        }
    }
    void run(float scale,const std::string& directory,int index) {
        const int width=int(480*scale),height=int(320*scale);resize(width,height);
        auto cpu=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        auto gpu=SkSurfaces::RenderTarget(skia.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(width,height));
        require(cpu&&gpu,"D3D12 comparison surfaces");scene(cpu->getCanvas(),scale);scene(gpu->getCanvas(),scale);
        const auto a=readback(cpu.get()),b=readback(gpu.get());semantics(a,scale);semantics(b,scale);compare(a,b,scale);
        const auto stem=directory+"/window-"+std::to_string(index)+"-dpi-"+std::to_string(int(scale*96));
        save(a,stem+"-cpu.ppm");save(b,stem+"-d3d12.ppm");
        // Exercise both swapchain images and repeated PRESENT transitions.
        for(int frame=0;frame<3;++frame) {
            auto& surface=surfaces[swapchain->GetCurrentBackBufferIndex()];scene(surface->getCanvas(),scale);
            semantics(readback(surface.get()),scale);
            require(skia->flush(surface.get(),SkSurfaces::BackendSurfaceAccess::kPresent,{}).fSuccess,"Transition to present");
            require(skia->submit(),"Submit D3D12 drawing");check(swapchain->Present(1,0),"Present D3D12 frame");wait();
        }
        auto textInfo=SkImageInfo::MakeN32Premul(width,int(520*scale));
        auto textCpu=SkSurfaces::Raster(textInfo);
        auto textGpu=SkSurfaces::RenderTarget(skia.get(),skgpu::Budgeted::kNo,textInfo);
        require(textCpu&&textGpu,"D3D12 text comparison surfaces");
        dwriteScene(textCpu->getCanvas(),scale);dwriteScene(textGpu->getCanvas(),scale);
        const auto c=readback(textCpu.get()),d=readback(textGpu.get());std::cout<<"DirectWrite glyph bridge ";compare(c,d,scale);
        save(c,stem+"-text-cpu.ppm");save(d,stem+"-text-d3d12.ppm");
    }
    void close() {
        if(!skia)return;
        require(skia->flushAndSubmit(GrSyncCpu::kYes).fSuccess,"Flush before teardown");wait();
        for(auto& surface:surfaces)surface.reset();for(auto& buffer:buffers)buffer.reset();
        skia->abandonContext();skia.reset();swapchain.reset();
        if(event){CloseHandle(event);event=nullptr;}
        UINT64 debugErrors=0,debugWarnings=0;
        if(debugMessages) {
            require(debugMessages->GetNumMessagesDiscardedByMessageCountLimit()==0,"D3D12 debug info queue overflow");
            for(UINT64 index=0;index<debugMessages->GetNumStoredMessagesAllowedByRetrievalFilter();++index) {
                SIZE_T size=0;check(debugMessages->GetMessage(index,nullptr,&size),"Debug message size");
                std::vector<char> data(size);auto message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
                check(debugMessages->GetMessage(index,message,&size),"Debug message");
                if(message->Severity<=D3D12_MESSAGE_SEVERITY_WARNING)std::cerr<<"D3D12 validation: id="<<message->ID
                    <<" severity="<<message->Severity<<" "<<message->pDescription<<'\n';
                debugErrors+=message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR;
                debugWarnings+=message->Severity==D3D12_MESSAGE_SEVERITY_WARNING;
            }
            std::cout<<"D3D12 validation summary: errors="<<debugErrors<<" warnings="<<debugWarnings<<'\n';
        }
        debugMessages.reset();fence.reset();backend.fMemoryAllocator.reset();backend.fQueue.reset();backend.fDevice.reset();backend.fAdapter.reset();factory.reset();
        require(debugErrors==0,"D3D12 debug layer reported errors");
    }
    ~Context(){try{close();}catch(...){if(skia)skia->abandonContext();if(event)CloseHandle(event);}}
};
int main(int argc,char** argv) try {
    require(argc==3||argc==4,"Usage: d3d12_probe --hardware|--warp EVIDENCE_DIRECTORY [--require-debug-layer]");
    const bool requireDebug=argc==4;
    require(!requireDebug||std::string(argv[3])=="--require-debug-layer","Unknown D3D12 validation option");
    const std::string mode=argv[1];require(mode=="--hardware"||mode=="--warp","Explicit D3D12 adapter mode");
    gr_cp<ID3D12Debug> debug;const auto debugStatus=D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if(SUCCEEDED(debugStatus)){debug->EnableDebugLayer();std::cout<<"D3D12 debug layer enabled\n";}
    else std::cout<<"D3D12 debug layer unavailable HRESULT=0x"<<std::hex<<static_cast<unsigned long>(debugStatus)<<std::dec<<'\n';
    require(!requireDebug||SUCCEEDED(debugStatus),"Required D3D12 debug layer is unavailable");
    std::filesystem::create_directories(argv[2]);SetProcessDPIAware();
    Window first(0),second(1);Context a(first.handle,mode=="--warp",requireDebug),b(second.handle,mode=="--warp",requireDebug);
    a.resourceSemantics();b.resourceSemantics();
    std::cout<<"PASS stage: render-target discard/load and MSAA\n";
    for(float scale:{1.f,1.5f,2.f}) {
        first.size(int(480*scale),int(320*scale));second.size(int(480*scale),int(320*scale));
        a.run(scale,argv[2],0);b.run(scale,argv[2],1);
    }
    ShowWindow(first.handle,SW_MINIMIZE);require(IsIconic(first.handle),"Minimized native window");b.run(2,argv[2],1);
    ShowWindow(first.handle,SW_RESTORE);a.run(2,argv[2],0);
    std::cout<<"PASS stage: two windows, 96/144/192 DPI, resize, minimize/restore\n";
    a.close();b.close();
    std::cout<<"PASS stage: original contexts teardown\n";
    {Window restored(2);restored.size(480,320);Context recreated(restored.handle,mode=="--warp",requireDebug);
        recreated.resourceSemantics();recreated.run(1,argv[2],2);recreated.close();}
    std::cout<<"PASS stage: explicit device recreation\n";
    require(!GetModuleHandleW(L"libEGL.dll")&&!GetModuleHandleW(L"libGLESv2.dll"),"No ANGLE modules loaded");
    for(const wchar_t* name:{L"d3d12.dll",L"D3D12Core.dll",L"dxgi.dll",L"d3dcompiler_47.dll"}) {
        const auto module=GetModuleHandleW(name);if(!module)continue;wchar_t path[32768]{};
        require(GetModuleFileNameW(module,path,32768)>0,"Module identity");std::wcout<<L"module="<<path<<L'\n';
    }
    std::cout<<"PASS: D3D12 "<<mode<<" CPU comparison, text, two windows, resize, minimize/restore, explicit device recreation, no ANGLE\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
