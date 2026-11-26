#include "scene.h"
#include "text.h"
#include <filesystem>
int main(int argc,char** argv) try {
    require(argc==2,"Argument: evidence directory");std::filesystem::create_directories(argv[1]);
    for(float scale:{1.f,1.5f,2.f}) {
        auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(int(480*scale),int(520*scale)));
        dwriteScene(surface->getCanvas(),scale);
        save(readback(surface.get()),std::string(argv[1])+"/dwrite-skia-"+std::to_string(int(96*scale))+".ppm");
    }
    std::cout<<"PASS: DirectWrite layout + Skia glyph bridge at three DPI scales\n";
    evaluateParagraphs(argv[1]);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
