#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "scene.h"
#include "text.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include <filesystem>

class Angle {
    HMODULE egl=nullptr,gles=nullptr;
public:
#define EGL_PROC(name,Type) Type name=nullptr
    EGL_PROC(initialize,PFNEGLINITIALIZEPROC);
    EGL_PROC(terminate,PFNEGLTERMINATEPROC);
    EGL_PROC(chooseConfig,PFNEGLCHOOSECONFIGPROC);
    EGL_PROC(bindAPI,PFNEGLBINDAPIPROC);
    EGL_PROC(createContext,PFNEGLCREATECONTEXTPROC);
    EGL_PROC(destroyContext,PFNEGLDESTROYCONTEXTPROC);
    EGL_PROC(createWindowSurface,PFNEGLCREATEWINDOWSURFACEPROC);
    EGL_PROC(destroySurface,PFNEGLDESTROYSURFACEPROC);
    EGL_PROC(makeCurrent,PFNEGLMAKECURRENTPROC);
    EGL_PROC(swapBuffers,PFNEGLSWAPBUFFERSPROC);
    EGL_PROC(querySurface,PFNEGLQUERYSURFACEPROC);
    EGL_PROC(getProcAddress,PFNEGLGETPROCADDRESSPROC);
#undef EGL_PROC
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLConfig config=nullptr;
    explicit Angle(const std::filesystem::path& directory) {
        gles=LoadLibraryExW((directory/L"libGLESv2.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        egl=LoadLibraryExW((directory/L"libEGL.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        require(egl && gles,"Load explicit ANGLE libraries");
#define LOAD(member,name) member=reinterpret_cast<decltype(member)>(GetProcAddress(egl,name)); require(member!=nullptr,name)
        LOAD(initialize,"eglInitialize"); LOAD(terminate,"eglTerminate"); LOAD(chooseConfig,"eglChooseConfig");
        LOAD(bindAPI,"eglBindAPI"); LOAD(createContext,"eglCreateContext"); LOAD(destroyContext,"eglDestroyContext");
        LOAD(createWindowSurface,"eglCreateWindowSurface"); LOAD(destroySurface,"eglDestroySurface");
        LOAD(makeCurrent,"eglMakeCurrent"); LOAD(swapBuffers,"eglSwapBuffers"); LOAD(getProcAddress,"eglGetProcAddress");
        LOAD(querySurface,"eglQuerySurface");
#undef LOAD
        const auto getPlatform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(getProcAddress("eglGetPlatformDisplayEXT"));
        require(getPlatform!=nullptr,"eglGetPlatformDisplayEXT");
        // EGL_PLATFORM_ANGLE_ANGLE, TYPE_ANGLE, TYPE_D3D11_ANGLE.
        const EGLint attributes[]={0x3203,0x3208,EGL_NONE};
        display=getPlatform(0x3202,EGL_DEFAULT_DISPLAY,attributes);
        EGLint major=0,minor=0;
        require(display!=EGL_NO_DISPLAY && initialize(display,&major,&minor),"Initialize ANGLE D3D11 display");
        require(bindAPI(EGL_OPENGL_ES_API),"Bind GLES");
        const EGLint configAttributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_STENCIL_SIZE,8,EGL_NONE};
        EGLint count=0; require(chooseConfig(display,configAttributes,&config,1,&count)&&count==1,"RGBA8 GLES3 window config");
        std::cout<<"EGL "<<major<<'.'<<minor<<'\n';
    }
    GrGLFuncPtr procedure(const char* name) {
        auto value=reinterpret_cast<GrGLFuncPtr>(GetProcAddress(gles,name));
        return value?value:reinterpret_cast<GrGLFuncPtr>(getProcAddress(name));
    }
    ~Angle(){if(display!=EGL_NO_DISPLAY) terminate(display); if(egl) FreeLibrary(egl); if(gles) FreeLibrary(gles);}
};
struct Window {
    HWND handle=nullptr;
    explicit Window(int index) {
        WNDCLASSW cls{}; cls.lpfnWndProc=DefWindowProcW; cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"SunfleckSkiaEvaluation";
        RegisterClassW(&cls);
        handle=CreateWindowW(cls.lpszClassName,L"Sunfleck Skia evaluation",WS_OVERLAPPEDWINDOW,
            40+index*70,40+index*70,520,380,nullptr,nullptr,cls.hInstance,nullptr);
        require(handle!=nullptr,"Create native probe window");
        ShowWindow(handle,SW_SHOWNOACTIVATE);
    }
    void size(int width,int height) {
        RECT rect{0,0,width,height}; AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
        require(SetWindowPos(handle,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE),"Resize window");
    }
    ~Window(){if(handle) DestroyWindow(handle);}
};
struct Context {
    Angle& angle; EGLContext context=EGL_NO_CONTEXT; EGLSurface surface=EGL_NO_SURFACE; sk_sp<GrDirectContext> skia;
    Context(Angle& a,HWND window):angle(a) {
        const EGLint attributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
        context=a.createContext(a.display,a.config,EGL_NO_CONTEXT,attributes);
        surface=a.createWindowSurface(a.display,a.config,window,nullptr);
        require(context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE,"Create EGL context/window surface");
        current();
        auto glInterface=GrGLMakeAssembledGLESInterface(&a,[](void* state,const char* name){return static_cast<Angle*>(state)->procedure(name);});
        require(glInterface && glInterface->validate(),"Assemble GLES interface");
        skia=GrDirectContexts::MakeGL(glInterface); require(bool(skia),"Create Ganesh context");
        auto getString=reinterpret_cast<PFNGLGETSTRINGPROC>(a.procedure("glGetString"));
        const auto renderer=reinterpret_cast<const char*>(getString(GL_RENDERER));
        require(renderer && std::string(renderer).find("Direct3D11")!=std::string::npos,"Verify actual D3D11 renderer");
        std::cout<<"GL_RENDERER="<<renderer<<'\n';
    }
    void current(){require(angle.makeCurrent(angle.display,surface,surface,context),"Make EGL context current");}
    void run(float scale,const std::string& directory,int index) {
        current(); const int width=int(480*scale),height=int(320*scale);
        EGLint actualWidth=0,actualHeight=0;
        require(angle.querySurface(angle.display,surface,EGL_WIDTH,&actualWidth) &&
            angle.querySurface(angle.display,surface,EGL_HEIGHT,&actualHeight),"Query EGL drawable");
        // ANGLE reconciles the native client size at the swap boundary.
        if(actualWidth!=width || actualHeight!=height) {
            require(angle.swapBuffers(angle.display,surface),"Synchronize resized drawable");
            require(angle.querySurface(angle.display,surface,EGL_WIDTH,&actualWidth) &&
                angle.querySurface(angle.display,surface,EGL_HEIGHT,&actualHeight),"Query resized EGL drawable");
        }
        if(actualWidth!=width || actualHeight!=height)
            std::cerr<<"drawable="<<actualWidth<<'x'<<actualHeight<<" requested="<<width<<'x'<<height<<'\n';
        require(actualWidth==width && actualHeight==height,"EGL drawable size after resize");
        auto cpu=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        auto gpu=SkSurfaces::RenderTarget(skia.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(width,height));
        require(cpu && gpu,"CPU/GPU surfaces"); scene(cpu->getCanvas(),scale); scene(gpu->getCanvas(),scale);
        const auto a=readback(cpu.get()),b=readback(gpu.get()); semantics(a,scale); semantics(b,scale); compare(a,b,scale);
        const auto stem=directory+"/window-"+std::to_string(index)+"-dpi-"+std::to_string(int(scale*96));
        save(a,stem+"-cpu.ppm"); save(b,stem+"-gpu.ppm");
        GrGLFramebufferInfo info{0,GL_RGBA8};
        auto backend=GrBackendRenderTargets::MakeGL(width,height,0,8,info);
        auto screen=SkSurfaces::WrapBackendRenderTarget(skia.get(),backend,kBottomLeft_GrSurfaceOrigin,kRGBA_8888_SkColorType,nullptr,nullptr);
        require(bool(screen),"Wrap EGL window framebuffer");
        scene(screen->getCanvas(),scale); require(skia->flushAndSubmit(screen.get()).fSuccess,"Submit window drawing");
        semantics(readback(screen.get()),scale);
        require(angle.swapBuffers(angle.display,surface),"Present native window");
        const auto textInfo=SkImageInfo::MakeN32Premul(width,int(520*scale));
        auto textCpu=SkSurfaces::Raster(textInfo);
        auto textGpu=SkSurfaces::RenderTarget(skia.get(),skgpu::Budgeted::kNo,textInfo);
        require(textCpu && textGpu,"Text comparison surfaces");
        dwriteScene(textCpu->getCanvas(),scale);dwriteScene(textGpu->getCanvas(),scale);
        const auto cpuText=readback(textCpu.get()),gpuText=readback(textGpu.get());
        std::cout<<"DirectWrite glyph bridge ";compare(cpuText,gpuText,scale);
        save(cpuText,stem+"-text-cpu.ppm");save(gpuText,stem+"-text-gpu.ppm");
    }
    ~Context(){
        if(!angle.makeCurrent(angle.display,surface,surface,context) && skia)skia->abandonContext();
        skia.reset(); angle.makeCurrent(angle.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(surface!=EGL_NO_SURFACE) angle.destroySurface(angle.display,surface);
        if(context!=EGL_NO_CONTEXT) angle.destroyContext(angle.display,context);}
};
int main(int argc,char** argv) try {
    require(argc==3,"Arguments: ANGLE directory, evidence directory");
    std::filesystem::create_directories(argv[2]);
    SetProcessDPIAware(); Angle angle(std::filesystem::u8path(argv[1]));
    Window first(0),second(1); Context a(angle,first.handle),b(angle,second.handle);
    for(float scale:{1.f,1.5f,2.f}) {
        first.size(int(480*scale),int(320*scale)); second.size(int(480*scale),int(320*scale));
        a.run(scale,argv[2],0); b.run(scale,argv[2],1);
    }
    ShowWindow(first.handle,SW_MINIMIZE); require(IsIconic(first.handle),"Minimize native surface");
    b.run(2,argv[2],1); ShowWindow(first.handle,SW_RESTORE); a.run(2,argv[2],0);
    // Exercise explicit context retirement. This does not simulate driver/device removal.
    { Window retired(2); Context c(angle,retired.handle); c.skia->abandonContext(); }
    { Window restored(2); restored.size(480,320); Context c(angle,restored.handle); c.run(1,argv[2],2); }
    std::cout<<"PASS: CPU/GPU semantics, DPI scaling, two windows, resize, minimize/restore, context recreation\n";
    return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
