// Isolated experiment: consumes Sunfleck's existing v1 rectangle stream.
// It does not implement the future M4 command protocol.
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include <cstdint>
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../scene.h"
#include "include/core/SkData.h"
#include "include/core/SkString.h"
#include "include/ports/SkFontMgr_data.h"

namespace {
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle=0;
sk_sp<GrDirectContext> context;
bool lost=false;
bool makeSkia() {
    if(emscripten_webgl_make_context_current(handle)!=EMSCRIPTEN_RESULT_SUCCESS)return false;
    context=GrDirectContexts::MakeGL(GrGLInterfaces::MakeWebGL());
    return bool(context);
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE int skia_web_font_version() {
    FT_Library library=nullptr;
    if(FT_Init_FreeType(&library))return 0;
    FT_Int major=0,minor=0,patch=0;FT_Library_Version(library,&major,&minor,&patch);
    FT_Done_FreeType(library);return major*10000+minor*100+patch;
}
EMSCRIPTEN_KEEPALIVE int skia_web_init() {
    if(handle)return -1;
    if(skia_web_font_version()!=FREETYPE_MAJOR*10000+FREETYPE_MINOR*100+FREETYPE_PATCH)return -5;
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion=2;attributes.minorVersion=0;
    attributes.alpha=true;attributes.depth=false;attributes.stencil=true;
    attributes.antialias=false;attributes.preserveDrawingBuffer=true;
    handle=emscripten_webgl_create_context("#surface",&attributes);
    if(handle<=0){handle=0;return -2;}
    lost=false;return makeSkia()?0:-3;
}
EMSCRIPTEN_KEEPALIVE int skia_web_features(const uint8_t* fontBytes,int size) {
    try {
        require(context && !lost && fontBytes && size>0,"Web feature probe inputs");
        require(emscripten_webgl_make_context_current(handle)==EMSCRIPTEN_RESULT_SUCCESS,"Web feature context");
        sk_sp<SkData> data=SkData::MakeWithCopy(fontBytes,size);
        auto fonts=SkFontMgr_New_Custom_Data({&data,1});
        require(fonts && fonts->countFamilies()>0,"Web font bytes reach Skia");
        SkString family;fonts->getFamilyName(0,&family);
        for(float scale:{1.f,1.5f,2.f}) {
            const auto info=SkImageInfo::MakeN32Premul(int(480*scale),int(320*scale));
            auto cpu=SkSurfaces::Raster(info);
            auto gpu=SkSurfaces::RenderTarget(context.get(),skgpu::Budgeted::kNo,info);
            require(cpu && gpu,"Web CPU/GPU feature surfaces");
            scene(cpu->getCanvas(),scale,fonts.get(),family.c_str());
            scene(gpu->getCanvas(),scale,fonts.get(),family.c_str());
            const auto a=readback(cpu.get()),b=readback(gpu.get());
            semantics(a,scale);semantics(b,scale);compare(a,b,scale);
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return -1;}
}
EMSCRIPTEN_KEEPALIVE int skia_web_present(const uint8_t* commands,int size,int width,int height) {
    if(!context || lost || !commands || size<0 || size%20 || width<=0 || height<=0)return -1;
    if(emscripten_webgl_make_context_current(handle)!=EMSCRIPTEN_RESULT_SUCCESS)return -2;
    GrGLFramebufferInfo info{0,GL_RGBA8};
    auto backend=GrBackendRenderTargets::MakeGL(width,height,0,8,info);
    auto surface=SkSurfaces::WrapBackendRenderTarget(context.get(),backend,kBottomLeft_GrSurfaceOrigin,kRGBA_8888_SkColorType,nullptr,nullptr);
    if(!surface)return -3;
    auto canvas=surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
    SkPaint paint;paint.setAntiAlias(false);
    for(int offset=0;offset<size;offset+=20) {
        int32_t bounds[4];std::memcpy(bounds,commands+offset,sizeof(bounds));
        if(bounds[2]<=bounds[0] || bounds[3]<=bounds[1])continue;
        const auto rgba=commands+offset+16;
        paint.setColor(SkColorSetARGB(rgba[3],rgba[0],rgba[1],rgba[2]));
        canvas->drawRect(SkRect::MakeLTRB(float(bounds[0]),float(bounds[1]),float(bounds[2]),float(bounds[3])),paint);
    }
    return context->flushAndSubmit(surface.get()).fSuccess?0:-4;
}
EMSCRIPTEN_KEEPALIVE void skia_web_lost() {
    lost=true;if(context)context->abandonContext();context.reset();
}
EMSCRIPTEN_KEEPALIVE int skia_web_restore() {
    if(!handle || !lost)return -1;
    lost=false;return makeSkia()?0:-2;
}
EMSCRIPTEN_KEEPALIVE void skia_web_close() {
    if(!handle)return;
    if(emscripten_webgl_make_context_current(handle)!=EMSCRIPTEN_RESULT_SUCCESS && context)context->abandonContext();
    context.reset();emscripten_webgl_destroy_context(handle);handle=0;lost=false;
}
}
