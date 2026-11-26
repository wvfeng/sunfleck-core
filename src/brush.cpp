#include "brush.h"
#include <cmath>

namespace drawing {
namespace {
using Pixel = std::array<uint8_t,4>;
Pixel unpack(uint32_t argb) {
    return {uint8_t(argb>>16),uint8_t(argb>>8),uint8_t(argb),uint8_t(argb>>24)};
}
void emit(std::vector<FillRectCommand>& out,size_t first,int x,int y,Pixel color) {
    if (!color[3]) return;
    if (out.size()>first) {
        auto& last = out.back();
        if (last.top == y && last.bottom == y+1 && last.right == x && last.color == color && last.software_layer) {
            ++last.right;
            return;
        }
    }
    out.push_back({x,y,x+1,y+1,color,true});
}
}
std::optional<Brush> Brush::copy(const ui_brush_desc& d) {
    Brush b;
    b.kind = d.kind;
    b.color = d.color.argb;
    if (d.kind == UI_BRUSH_SOLID) return b;
    if (d.kind == UI_BRUSH_IMAGE) {
        if (!d.rgba || !d.width || !d.height || d.width > 4096 || d.height > 4096 ||
            d.stretch < UI_STRETCH_NONE || d.stretch > UI_STRETCH_UNIFORM_TO_FILL) return {};
        b.width = d.width; b.height = d.height; b.stretch = d.stretch;
        b.rgba.assign(d.rgba,d.rgba+size_t(d.width)*d.height*4);
        return b;
    }
    if ((d.kind != UI_BRUSH_LINEAR && d.kind != UI_BRUSH_RADIAL) || !d.stops || !d.stop_count || d.stop_count > 256) return {};
    for (double n : {d.start.x,d.start.y,d.end.x,d.end.y})
        if (!std::isfinite(n) || std::abs(n)>10000000) return {};
    if (d.kind == UI_BRUSH_RADIAL && (d.end.x<=0 || d.end.y<=0)) return {};
    b.start=d.start; b.end=d.end;
    for (unsigned i=0;i<d.stop_count;++i) {
        if (!(d.stops[i].offset>=0 && d.stops[i].offset<=1)) return {};
        b.stops.push_back(d.stops[i]);
    }
    std::stable_sort(b.stops.begin(),b.stops.end(),[](auto a,auto c){return a.offset<c.offset;});
    return b;
}
Pixel Brush::sample(layout::Rect r,double x,double y,double scale) const {
    if (kind == UI_BRUSH_SOLID) return unpack(color);
    const double w=double(r.right)-r.left,h=double(r.bottom)-r.top;
    if (w<=0 || h<=0) return {};
    if (kind == UI_BRUSH_IMAGE) {
        double sx=w/width,sy=h/height;
        if (stretch == UI_STRETCH_NONE) sx=sy=scale;
        if (stretch == UI_STRETCH_UNIFORM) sx=sy=std::min(sx,sy);
        if (stretch == UI_STRETCH_UNIFORM_TO_FILL) sx=sy=std::max(sx,sy);
        const double u=(x-r.left-(w-width*sx)/2)/sx,v=(y-r.top-(h-height*sy)/2)/sy;
        if (u<0 || v<0 || u>=width || v>=height) return {};
        const auto* pixel=rgba.data()+(size_t(v)*width+size_t(u))*4;
        return {pixel[0],pixel[1],pixel[2],pixel[3]};
    }
    const double u=(x-r.left)/w-start.x,v=(y-r.top)/h-start.y;
    const double dx=end.x-start.x,dy=end.y-start.y;
    const double length=dx*dx+dy*dy;
    const double t=kind==UI_BRUSH_RADIAL ? std::hypot(u/end.x,v/end.y) : length>0 ? (u*dx+v*dy)/length : 1;
    auto after=std::upper_bound(stops.begin(),stops.end(),t,[](double pos,const auto& stop){return pos<stop.offset;});
    if (after==stops.begin()) return unpack(after->color.argb);
    const auto& before=*(after-1);
    Pixel lhs=unpack(before.color.argb);
    if (after==stops.end()) return lhs;
    const Pixel rhs=unpack(after->color.argb);
    const double f=(t-before.offset)/(after->offset-before.offset);
    for (size_t i=0;i<4;++i) lhs[i]=uint8_t(std::lround(lhs[i]+(rhs[i]-lhs[i])*f));
    return lhs;
}
void shade(std::vector<FillRectCommand>& commands,size_t first,const Brush& brush,layout::Rect bounds,double scale) {
    std::vector<FillRectCommand> source(commands.begin()+first,commands.end());
    commands.resize(first);
    for (auto& run:source)
        for (int y=run.top;y<run.bottom;++y) for(int x=run.left;x<run.right;++x) {
            auto color=brush.sample(bounds,x+.5,y+.5,scale);
            color[3]=uint8_t((unsigned(color[3])*run.color[3]+127)/255);
            emit(commands,first,x,y,color);
        }
}
bool valid_effect(const ui_effect_desc& e) {
    return e.kind>=UI_EFFECT_NONE && e.kind<=UI_EFFECT_DROP_SHADOW && e.radius>=0 && e.radius<=128 &&
           std::abs(e.offset.x)<=4096 && std::abs(e.offset.y)<=4096;
}
void effect_layer(std::vector<FillRectCommand>& commands,size_t first,const ui_effect_desc& effect,
                  const Brush* mask,layout::Rect bounds,layout::Rect clip,double opacity,double scale) {
    if (commands.size()==first) return;
    if (!mask && effect.kind==UI_EFFECT_NONE) { composite_opacity(commands,first,opacity); return; }
    layout::Rect extent{clip.right,clip.bottom,clip.left,clip.top};
    for (size_t i=first;i<commands.size();++i) {
        const auto& c=commands[i];
        extent.left=std::min(extent.left,c.left); extent.right=std::max(extent.right,c.right);
        extent.top=std::min(extent.top,c.top); extent.bottom=std::max(extent.bottom,c.bottom);
    }
    const int radius=effect.kind==UI_EFFECT_NONE ? 0 : int(std::ceil(effect.radius*scale));
    const int dx=effect.kind==UI_EFFECT_DROP_SHADOW ? int(std::round(effect.offset.x*scale)) : 0;
    const int dy=effect.kind==UI_EFFECT_DROP_SHADOW ? int(std::round(effect.offset.y*scale)) : 0;
    const int left=std::max(clip.left,extent.left-radius+std::min(dx,0)), right=std::min(clip.right,extent.right+radius+std::max(dx,0));
    const int top=std::max(clip.top,extent.top-radius+std::min(dy,0)), bottom=std::min(clip.bottom,extent.bottom+radius+std::max(dy,0));
    if (left>=right || top>=bottom) { commands.resize(first); return; }
    const int w=right-left,h=bottom-top;
    std::span layer(commands.data()+first,commands.size()-first);
    for(auto& c:layer) { c.left-=left;c.right-=left;c.top-=top;c.bottom-=top; }
    std::vector<uint8_t> original;
    rasterize(original,w,h,layer);
    commands.resize(first);
    auto blurred=original;
    if (radius>0) {
        const double sigma=std::max(.5,radius/3.);
        std::vector<double> kernel(2*radius+1);
        double sum=0;
        for(int i=-radius;i<=radius;++i) sum+=(kernel[i+radius]=std::exp(-double(i)*i/(2*sigma*sigma)));
        for(auto& v:kernel) v/=sum;
        auto scratch=original;
        for(bool vertical:{false,true}) {
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                std::array<double,4> value{};
                for(int i=-radius;i<=radius;++i) {
                    const int sx=x+(vertical?0:i), sy=y+(vertical?i:0);
                    if(sx<0 || sy<0 || sx>=w || sy>=h) continue;
                    const auto* sample=blurred.data()+4*(size_t(sy)*w+sx);
                    for(unsigned c=0;c<4;++c) value[c]+=sample[c]*kernel[i+radius];
                }
                for(unsigned c=0;c<4;++c) scratch[4*(size_t(y)*w+x)+c]=uint8_t(std::lround(value[c]));
            }
            scratch.swap(blurred);
        }
    }
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const size_t i=4*(size_t(y)*w+x);
        Pixel p{};
        if (effect.kind==UI_EFFECT_DROP_SHADOW) {
            const int sx=x-dx,sy=y-dy;
            const auto shadow=unpack(effect.color.argb);
            const unsigned alpha=sx>=0 && sy>=0 && sx<w && sy<h ?
                unsigned(blurred[4*(size_t(sy)*w+sx)+3])*shadow[3]/255 : 0;
            const unsigned behind=(alpha*(255-original[i+3])+127)/255;
            for(unsigned c=0;c<3;++c) p[c]=uint8_t(std::min(255u,original[i+c]+(shadow[c]*behind+127)/255));
            p[3]=uint8_t(original[i+3]+behind);
        } else std::copy_n(blurred.data()+i,4,p.begin());
        if (!p[3]) continue;
        for(unsigned c=0;c<3;++c) p[c]=uint8_t(std::min(255u,(p[c]*255u+p[3]/2)/p[3]));
        const double coverage=mask ? mask->sample(bounds,left+x+.5,top+y+.5,scale)[3]/255. : 1.;
        p[3]=uint8_t(std::round(p[3]*opacity*coverage));
        emit(commands,first,left+x,top+y,p);
    }
}
}
