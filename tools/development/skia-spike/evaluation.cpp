#include "evaluation.h"
#include "scene.h"
#include "text.h"
#include <filesystem>
namespace {thread_local std::string lastError;}
unsigned skia_evaluation_version(void){return 1;}
const char* skia_evaluation_error(void){return lastError.c_str();}
int skia_evaluation_render(const char* directory,unsigned dpi) {
    try {
        require(directory && *directory && (dpi==96 || dpi==144 || dpi==192),"Invalid evaluation parameters");
        std::filesystem::create_directories(directory);
        const float scale=dpi/96.f;
        auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(int(480*scale),int(520*scale)));
        require(bool(surface),"Evaluation surface");
        scene(surface->getCanvas(),scale);auto bitmap=readback(surface.get());semantics(bitmap,scale);
        save(bitmap,std::string(directory)+"/scene.ppm");
        dwriteScene(surface->getCanvas(),scale);save(readback(surface.get()),std::string(directory)+"/text.ppm");
        // Packaging feasibility is independent of the separate SkParagraph
        // compatibility gate. This ABI uses the retained DirectWrite layout.
        lastError.clear();return 0;
    }catch(const std::exception& error){lastError=error.what();return 1;}
}
