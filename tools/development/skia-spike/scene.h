#pragma once
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#ifdef _WIN32
#include "include/ports/SkTypeface_win.h"
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

inline void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
inline void scene(SkCanvas* c, float scale, SkFontMgr* fonts=nullptr, const char* family="Segoe UI") {
    c->clear(SK_ColorWHITE);
    c->save(); c->scale(scale, scale);
    SkPaint p; p.setColor(SK_ColorRED);
    c->saveLayerAlpha(nullptr,128);
    c->drawRect(SkRect::MakeXYWH(10,10,80,60),p);
    c->drawRect(SkRect::MakeXYWH(50,10,80,60),p);
    c->restore();
    c->save(); c->translate(160,10); c->clipRect(SkRect::MakeWH(60,60));
    c->scale(2,2); c->clipRect(SkRect::MakeXYWH(5,5,15,15));
    p.setColor(SK_ColorBLUE); c->drawRect(SkRect::MakeWH(100,100),p); c->restore();
    c->save(); c->translate(270,40); c->rotate(90);
    p.setColor(SK_ColorGREEN); c->drawRect(SkRect::MakeWH(30,10),p); c->restore();
    const SkPoint endpoints[]={{10,100},{130,100}};
    const SkColor4f colors[]={SkColors::kRed,SkColors::kBlue};
    p.setShader(SkShaders::LinearGradient(endpoints,SkGradient(SkGradient::Colors(colors,SkTileMode::kClamp),{})));
    c->drawRect(SkRect::MakeXYWH(10,90,120,60),p); p.setShader(nullptr);
    p.setColor(SK_ColorBLUE);
    p.setImageFilter(SkImageFilters::DropShadow(8,8,3,3,SK_ColorBLACK,nullptr));
    c->drawRect(SkRect::MakeXYWH(165,95,40,40),p);
    p.setImageFilter(SkImageFilters::Blur(3,3,nullptr));
    c->drawRect(SkRect::MakeXYWH(255,95,40,40),p); p.setImageFilter(nullptr);
    // RGBA texture upload and alpha blending are exercised independently of layers.
    SkBitmap tile; tile.allocN32Pixels(16,16); tile.eraseColor(SkColorSetARGB(128,0,200,0));
    c->drawImage(tile.asImage(),330,20);
    p.setShader(SkShaders::RadialGradient({365,120},30,SkGradient(SkGradient::Colors(colors,SkTileMode::kClamp),{})));
    c->drawCircle(365,120,30,p); p.setShader(nullptr);
#ifdef _WIN32
    auto manager=SkFontMgr_New_DirectWrite();
    if(!fonts) fonts=manager.get();
#endif
    require(fonts!=nullptr,"Scene font manager");
    auto face=fonts->matchFamilyStyle(family,SkFontStyle());
    require(bool(face),"Scene typeface resolves");
    SkFont font(face,24);
    font.setEdging(SkFont::Edging::kAntiAlias); p.setColor(SK_ColorBLACK);
    c->drawString("Sunfleck / Skia GPU probe",10,205,font,p);
    p.setStyle(SkPaint::kStroke_Style); p.setStrokeWidth(3); p.setAntiAlias(true);
    c->drawOval(SkRect::MakeXYWH(20,230,140,45),p);
    c->restore();
}
inline SkBitmap readback(SkSurface* surface) {
    SkBitmap bitmap; bitmap.allocN32Pixels(surface->width(),surface->height());
    require(surface->readPixels(bitmap.pixmap(),0,0),"Surface readback");
    return bitmap;
}
inline void save(const SkBitmap& bitmap,const std::string& path) {
    std::ofstream output(path,std::ios::binary);
    output<<"P6\n"<<bitmap.width()<<' '<<bitmap.height()<<"\n255\n";
    for (int y=0;y<bitmap.height();++y) for (int x=0;x<bitmap.width();++x) {
        const auto color=bitmap.getColor(x,y);
        const char rgb[]={char(SkColorGetR(color)),char(SkColorGetG(color)),char(SkColorGetB(color))};
        output.write(rgb,3);
    }
    require(bool(output),"Write image evidence");
}
inline void semantics(const SkBitmap& bitmap,float scale) {
    auto at=[&](int x,int y){return bitmap.getColor(int(x*scale),int(y*scale));};
    require(at(20,30)==at(60,30),"Subtree opacity must composite once");
    require(SkColorGetR(at(20,30))==255 && std::abs(int(SkColorGetG(at(20,30)))-127)<=1,"Layer alpha");
    require(at(180,30)==SK_ColorBLUE && at(165,30)==SK_ColorWHITE && at(205,30)==SK_ColorWHITE,"Nested clip spaces");
    require(at(265,55)==SK_ColorGREEN && at(280,45)==SK_ColorWHITE,"Rotation");
    require(SkColorGetR(at(20,110))>SkColorGetR(at(120,110)),"Gradient");
    require(at(210,130)!=SK_ColorWHITE && at(252,110)!=SK_ColorWHITE,"Shadow and blur");
    require(SkColorGetG(at(335,25))>SkColorGetR(at(335,25)),"RGBA image alpha");
}
inline void compare(const SkBitmap& cpu,const SkBitmap& gpu,float scale) {
    require(cpu.width()==gpu.width() && cpu.height()==gpu.height(),"Image dimensions");
    uint64_t total=0,large=0; unsigned maximum=0;
    for (int y=0;y<cpu.height();++y) for (int x=0;x<cpu.width();++x) {
        auto a=cpu.getColor(x,y),b=gpu.getColor(x,y); unsigned delta=0;
        for (unsigned shift:{0u,8u,16u,24u}) {
            unsigned d=unsigned(std::abs(int((a>>shift)&255)-int((b>>shift)&255)));
            total+=d; delta=std::max(delta,d); maximum=std::max(maximum,d);
        }
        large+=delta>16;
    }
    const double pixels=double(cpu.width())*cpu.height(),mean=total/(pixels*4),fraction=large/pixels;
    std::cout<<"comparison dpi="<<96*scale<<" meanChannelError="<<mean<<" max="<<maximum<<" fractionAbove16="<<fraction<<'\n';
    // Text/curve antialiasing can differ; semantic samples above are exact.
    require(mean<=3 && fraction<=.02,"CPU/GPU image tolerance (mean <=3, >16 pixels <=2%)");
}
