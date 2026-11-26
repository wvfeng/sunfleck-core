#include "skia_d3d12_surface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkImage.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendContext.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DDirectContext.h"
#ifdef SUNFLECK_TEST_SKIA_D3D12
#include <d3d12sdklayers.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
LRESULT CALLBACK presentation_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if (message==WM_NCHITTEST) return HTTRANSPARENT;
    if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message==WM_ERASEBKGND) return 1;
    if (message==WM_PAINT) {
        PAINTSTRUCT paint{};BeginPaint(window,&paint);EndPaint(window,&paint);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
HWND create_presentation_window(HWND parent) {
    WNDCLASSW type{};type.lpfnWndProc=presentation_proc;
    type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"SunfleckSkiaPresentation";
    if (!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    // Keep flip-model off the application HWND: destroying this disabled child
    // restores the parent's GDI surface on CPU fallback without replacing the
    // input/focus/IME window. Disabled children do not intercept user input.
    return CreateWindowExW(WS_EX_NOACTIVATE,type.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_DISABLED,
        0,0,1,1,parent,nullptr,type.hInstance,nullptr);
}
bool checked(HRESULT result, const char* operation) {
    if (SUCCEEDED(result)) return true;
    std::fprintf(stderr,"Sunfleck: Skia D3D12 %s failed (0x%08lx)\n",operation,static_cast<unsigned long>(result));
    return false;
}
bool inject(const char* stage) {
#ifdef SUNFLECK_TEST_SKIA_D3D12
    const char* value=std::getenv("SUNFLECK_TEST_SKIA_D3D12_FAILURE");
    return value && std::strcmp(value,stage)==0;
#else
    (void)stage;return false;
#endif
}
}

struct SkiaD3d12Surface::State {
    GrD3DBackendContext backend;
    sk_sp<GrDirectContext> context;
    gr_cp<IDXGISwapChain3> swapchain;
    gr_cp<ID3D12Fence> fence;
    gr_cp<ID3D12Resource> buffers[2];
    sk_sp<SkSurface> surfaces[2];
    HANDLE event=nullptr;
    HWND presentation_window=nullptr;
    UINT64 submitted=0;
    UINT64 pending[2]{};
    int width=0,height=0;
    bool presented=false;
#ifdef SUNFLECK_TEST_SKIA_D3D12
    std::vector<std::uint8_t> captured;
#endif

    bool wait(UINT64 value) {
        if (!value) return true;
        if (!checked(backend.fDevice->GetDeviceRemovedReason(),"device health")) return false;
        if (fence->GetCompletedValue()<value) {
            if (!checked(fence->SetEventOnCompletion(value,event),"fence event")) return false;
            if (WaitForSingleObject(event,5000)!=WAIT_OBJECT_0) {
                std::fprintf(stderr,"Sunfleck: D3D12 fence wait failed or timed out\n");return false;
            }
        }
        return checked(backend.fDevice->GetDeviceRemovedReason(),"device health");
    }
    bool drain() {
        if (!context || !fence || !event) return true;
        if (!context->flushAndSubmit().fSuccess) return false;
        const UINT64 value=++submitted;
        if (!checked(backend.fQueue->Signal(fence.get(),value),"drain signal") || !wait(value)) return false;
        // The external fence proves completion, but Skia still owns references in
        // its outstanding command lists until its completion poll recycles them.
        context->checkAsyncWorkCompletion();
        return true;
    }
    bool resize(int w,int h) {
        if (w==width && h==height) return true;
        if (!drain()) return false;
        for (auto& surface:surfaces) surface.reset();
        for (auto& buffer:buffers) buffer.reset();
        if (!checked(swapchain->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,0),"resize")) return false;
        for (UINT i=0;i<2;++i) {
            if (!checked(swapchain->GetBuffer(i,IID_PPV_ARGS(&buffers[i])),"backbuffer")) return false;
            GrD3DTextureResourceInfo info;
            info.fResource=buffers[i];info.fResourceState=D3D12_RESOURCE_STATE_PRESENT;
            info.fFormat=DXGI_FORMAT_R8G8B8A8_UNORM;info.fLevelCount=1;info.fSampleQualityPattern=0;
            auto target=GrBackendRenderTargets::MakeD3D(w,h,info);
            surfaces[i]=SkSurfaces::WrapBackendRenderTarget(context.get(),target,kTopLeft_GrSurfaceOrigin,kRGBA_8888_SkColorType,nullptr,nullptr);
            if (!surfaces[i]) return false;
            pending[i]=0;
        }
        if (!SetWindowPos(presentation_window,nullptr,0,0,w,h,SWP_NOACTIVATE|SWP_NOZORDER)) return false;
        width=w;height=h;return true;
    }
    ~State() {
        // Drain before releasing external backbuffers. On device loss, abandon
        // the context rather than asking Skia to submit more GPU work.
        const bool idle=drain();
        if (context && !idle) context->abandonContext();
        for (auto& surface:surfaces) surface.reset();
        for (auto& buffer:buffers) buffer.reset();
        context.reset();swapchain.reset();
        if (presentation_window) DestroyWindow(presentation_window);
        if (event) CloseHandle(event);
    }
};

SkiaD3d12Surface::SkiaD3d12Surface()=default;
SkiaD3d12Surface::~SkiaD3d12Surface()=default;

bool SkiaD3d12Surface::open(HWND window) {
    if (inject("initialize")) return false;
    auto state=std::make_unique<State>();
    gr_cp<IDXGIFactory4> factory;
    if (!checked(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"DXGI factory")) return false;
#ifdef SUNFLECK_TEST_SKIA_D3D12
    const char* warp_setting=std::getenv("SUNFLECK_TEST_SKIA_D3D12_WARP");
    const bool warp=warp_setting && std::strcmp(warp_setting,"1")==0;
    if (warp) {
        if (!checked(factory->EnumWarpAdapter(IID_PPV_ARGS(&state->backend.fAdapter)),"test WARP adapter") ||
            !checked(D3D12CreateDevice(state->backend.fAdapter.get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&state->backend.fDevice)),"test WARP device")) return false;
    }
#endif
    for (UINT i=0;!state->backend.fDevice;++i) {
        gr_cp<IDXGIAdapter1> adapter;
        const auto result=factory->EnumAdapters1(i,&adapter);
        if (result==DXGI_ERROR_NOT_FOUND) break;
        if (!checked(result,"adapter enumeration")) return false;
        DXGI_ADAPTER_DESC1 description{};
        if (FAILED(adapter->GetDesc1(&description)) || (description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&state->backend.fDevice)))) {
            state->backend.fAdapter=adapter;break;
        }
    }
    if (!state->backend.fDevice) {
        std::fprintf(stderr,"Sunfleck: no hardware D3D12 adapter; using CPU rendering\n");return false;
    }
#ifdef SUNFLECK_TEST_SKIA_D3D12
    DXGI_ADAPTER_DESC1 selected{};
    if (SUCCEEDED(state->backend.fAdapter->GetDesc1(&selected)))
        std::fprintf(stderr,"Sunfleck D3D12 adapter: vendor=%04x device=%04x software=%u LUID=%08lx:%08lx\n",
            selected.VendorId,selected.DeviceId,unsigned((selected.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0),
            static_cast<unsigned long>(selected.AdapterLuid.HighPart),static_cast<unsigned long>(selected.AdapterLuid.LowPart));
#endif
    D3D12_COMMAND_QUEUE_DESC queue{};queue.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (!checked(state->backend.fDevice->CreateCommandQueue(&queue,IID_PPV_ARGS(&state->backend.fQueue)),"command queue")) return false;
    state->context=GrDirectContexts::MakeD3D(state->backend);
    if (!state->context) return false;
    if (!checked(state->backend.fDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&state->fence)),"fence")) return false;
    state->event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!state->event) return false;
    state->presentation_window=create_presentation_window(window);
    if (!state->presentation_window) return false;
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width=1;description.Height=1;description.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferCount=2;description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;description.SampleDesc.Count=1;
    gr_cp<IDXGISwapChain1> swapchain;
    if (!checked(factory->CreateSwapChainForHwnd(state->backend.fQueue.get(),state->presentation_window,&description,nullptr,nullptr,&swapchain),"swapchain") ||
        !checked(swapchain->QueryInterface(IID_PPV_ARGS(&state->swapchain)),"swapchain interface") ||
        !checked(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"window association")) return false;
    state_=std::move(state);return true;
}

namespace {
bool replay(SkCanvas& canvas,const drawing::CommandList& list) {
    SkPaint paint;
    for(const auto& op:list.commands) {
        switch(op.kind) {
        case drawing::CommandKind::clip:
            canvas.save();
            canvas.clipRect(SkRect::MakeLTRB(float(op.bounds.left),float(op.bounds.top),float(op.bounds.right),float(op.bounds.bottom)));
            break;
        case drawing::CommandKind::transform: {
            const auto& m=op.matrix;
            canvas.save();
            canvas.concat(SkMatrix::MakeAll(float(m.m11),float(m.m21),float(m.offset_x),
                                            float(m.m12),float(m.m22),float(m.offset_y),0,0,1));
            break;
        }
        case drawing::CommandKind::layer: canvas.saveLayerAlphaf(nullptr,float(op.opacity)); break;
        case drawing::CommandKind::restore: canvas.restore(); break;
        case drawing::CommandKind::rectangles:
            for(const auto& r:op.spans) {
                if(r.left>=r.right || r.top>=r.bottom) continue;
                paint.setColor(SkColorSetARGB(r.color[3],r.color[0],r.color[1],r.color[2]));
                canvas.drawRect(SkRect::MakeLTRB(float(r.left),float(r.top),float(r.right),float(r.bottom)),paint);
            }
            break;
        case drawing::CommandKind::glyph_coverage:
        case drawing::CommandKind::image: {
            if(op.spans.empty()) break;
            auto spans=op.spans;
            layout::Rect bounds{spans[0].left,spans[0].top,spans[0].right,spans[0].bottom};
            for(const auto& r:spans) {
                bounds.left=std::min(bounds.left,r.left); bounds.right=std::max(bounds.right,r.right);
                bounds.top=std::min(bounds.top,r.top); bounds.bottom=std::max(bounds.bottom,r.bottom);
            }
            const int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
            if(w<=0 || h<=0) break;
            for(auto& r:spans) {r.left-=bounds.left;r.right-=bounds.left;r.top-=bounds.top;r.bottom-=bounds.top;}
            std::vector<uint8_t> pixels; rasterize(pixels,w,h,spans);
            auto info=SkImageInfo::Make(w,h,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
            auto image=SkImages::RasterFromPixmapCopy(SkPixmap(info,pixels.data(),size_t(w)*4));
            if(!image) return false;
            canvas.drawImage(image,float(bounds.left),float(bounds.top));
            break;
        }
        }
    }
    return true;
}
}
bool SkiaD3d12Surface::present_frame(int width,int height,std::span<const FillRectCommand> reference,const drawing::CommandList& list) {
    return draw_frame(width,height,reference,&list);
}
bool SkiaD3d12Surface::present(int width,int height,std::span<const FillRectCommand> commands) {
    return draw_frame(width,height,commands,nullptr);
}
bool SkiaD3d12Surface::draw_frame(int width,int height,std::span<const FillRectCommand> commands,const drawing::CommandList* list) {
    if (!state_) return false;
    if (width<=0 || height<=0) return true;
    auto& state=*state_;
    if (state.presented && inject("present")) return false;
    if (!state.resize(width,height)) return false;
    const UINT index=state.swapchain->GetCurrentBackBufferIndex();
    if (!state.wait(state.pending[index])) return false;
    auto* surface=state.surfaces[index].get();
    auto* canvas=surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
    if (list) {
        SkAutoCanvasRestore restore(canvas,true);
        if(!replay(*canvas,*list)) return false;
    } else if (std::any_of(commands.begin(),commands.end(),[](const auto& c){ return c.software_layer; })) {
        std::vector<uint8_t> pixels;
        rasterize(pixels,width,height,commands);
        const auto info = SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
        auto image = SkImages::RasterFromPixmapCopy(SkPixmap(info,pixels.data(),size_t(width)*4));
        if (!image) return false;
        canvas->drawImage(image,0,0);
    } else {
        SkPaint paint;paint.setAntiAlias(false);
        for (const auto& command:commands) {
            const int left=std::clamp(command.left,0,width),right=std::clamp(command.right,left,width);
            const int top=std::clamp(command.top,0,height),bottom=std::clamp(command.bottom,top,height);
            if (left==right || top==bottom) continue;
            const auto& c=command.color;paint.setColor(SkColorSetARGB(c[3],c[0],c[1],c[2]));
            canvas->drawRect(SkRect::MakeLTRB(float(left),float(top),float(right),float(bottom)),paint);
        }
    }
#ifdef SUNFLECK_TEST_SKIA_D3D12
    if (std::getenv("SUNFLECK_TEST_SKIA_D3D12_CAPTURE")) {
        state.captured.resize(size_t(width)*height*4);
        if (!surface->readPixels(SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType),
            state.captured.data(),size_t(width)*4,0,0)) return false;
    }
#endif
    if (!state.context->flush(surface,SkSurfaces::BackendSurfaceAccess::kPresent,{}).fSuccess || !state.context->submit()) return false;
    if (!checked(state.swapchain->Present(1,0),"present")) return false;
    const UINT64 value=++state.submitted;
    if (!checked(state.backend.fQueue->Signal(state.fence.get(),value),"frame signal")) return false;
    state.pending[index]=value;state.presented=true;return true;
}
#ifdef SUNFLECK_TEST_SKIA_D3D12
bool SkiaD3d12Surface::debug_clean() {
    if (!state_ || !state_->drain()) return false;
    gr_cp<ID3D12InfoQueue> messages;
    if (FAILED(state_->backend.fDevice->QueryInterface(IID_PPV_ARGS(&messages)))) return false;
    bool clean=true;
    for (UINT64 i=0;i<messages->GetNumStoredMessages();++i) {
        SIZE_T size=0;
        if (FAILED(messages->GetMessage(i,nullptr,&size))) return false;
        std::vector<std::uint8_t> storage(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        if (FAILED(messages->GetMessage(i,message,&size))) return false;
        if (message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR) {
            std::fprintf(stderr,"D3D12 validation: %s\n",message->pDescription);clean=false;
        }
    }
    return clean;
}
const std::vector<std::uint8_t>& SkiaD3d12Surface::captured_pixels() const { return state_->captured; }
#endif
