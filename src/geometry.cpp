#include "geometry.h"
#include "numeric_literal.h"
#include <cctype>

namespace drawing {
namespace {
class PathReader {
    std::string_view text;
    size_t cursor=0;
    void spaces() {
        while(cursor<text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) ++cursor;
    }
    bool number(double& value) {
        spaces();
        if (cursor<text.size() && text[cursor]==',') { ++cursor; spaces(); }
        const size_t start=cursor;
        if (cursor<text.size() && (text[cursor]=='-' || text[cursor]=='+')) ++cursor;
        const auto digits=[&] { while(cursor<text.size() && text[cursor]>='0' && text[cursor]<='9') ++cursor; };
        digits();
        if(cursor<text.size() && text[cursor]=='.') { ++cursor; digits(); }
        if(cursor<text.size() && (text[cursor]=='E' || text[cursor]=='e')) {
            ++cursor;
            if(cursor<text.size() && (text[cursor]=='-' || text[cursor]=='+')) ++cursor;
            digits();
        }
        auto part=text.substr(start,cursor-start);
        if(part.starts_with('+')) part.remove_prefix(1);
        return !part.empty() && numeric_literal::real_literal(part,value) && std::abs(value)<=10000000;
    }
public:
    explicit PathReader(std::string_view input): text(input) {}
    std::optional<Geometry> read() {
        Geometry path;
        char op=0;
        ui_point pos{};
        size_t segments=0;
        for(;;) {
            spaces();
            if(cursor==text.size()) break;
            if(++segments>4096) return {};
            if (std::isalpha(static_cast<unsigned char>(text[cursor]))) op=text[cursor++];
            const char command=char(std::toupper(static_cast<unsigned char>(op)));
            const bool relative=op!=command;
            auto point=[&](ui_point& p) {
                if (!number(p.x) || !number(p.y)) return false;
                if (relative) { p.x+=pos.x;p.y+=pos.y; }
                return std::abs(p.x)<=10000000 && std::abs(p.y)<=10000000;
            };
            if (command=='M') {
                ui_point next;
                if(!point(next)) return {};
                path.figures.push_back({{next},false});
                pos=next;op=relative?'l':'L';continue;
            }
            if(path.figures.empty()) return {};
            auto& figure=path.figures.back();
            if(command=='Z') {
                figure.closed=true;pos=figure.points.front();op=0;continue;
            }
            if(figure.closed) { path.figures.push_back({{pos},false}); }
            auto& pts=path.figures.back().points;
            if(command=='L') {
                ui_point next;
                if(!point(next)) return {};
                pos=next;
            } else if(command=='H' || command=='V') {
                double next;
                if(!number(next)) return {};
                auto& axis=command=='H'?pos.x:pos.y;
                axis=next+(relative?axis:0);
            } else if(command=='Q' || command=='C') {
                ui_point a,b,end;
                if(!point(a) || (command=='C' && !point(b)) || !point(end)) return {};
                const double length=std::hypot(a.x-pos.x,a.y-pos.y)+std::hypot(end.x-a.x,end.y-a.y)+
                    (command=='C' ? std::hypot(b.x-a.x,b.y-a.y) : 0.);
                const int steps=std::clamp(int(std::ceil(std::sqrt(length)*2)),2,128);
                for(int i=1;i<steps;++i) {
                    const double t=double(i)/steps,u=1-t;
                    if(command=='Q') pts.push_back({u*u*pos.x+2*u*t*a.x+t*t*end.x,u*u*pos.y+2*u*t*a.y+t*t*end.y});
                    else pts.push_back({u*u*u*pos.x+3*u*u*t*a.x+3*u*t*t*b.x+t*t*t*end.x,
                                        u*u*u*pos.y+3*u*u*t*a.y+3*u*t*t*b.y+t*t*t*end.y});
                }
                pos=end;
            } else return {};
            pts.push_back(pos);
        }
        for(const auto& f:path.figures) for(auto p:f.points) {
            path.size.width=std::max(path.size.width,p.x); path.size.height=std::max(path.size.height,p.y);
        }
        path.data=text;
        return path;
    }
};
double distance(ui_point p,ui_point a,ui_point b) {
    const double dx=b.x-a.x,dy=b.y-a.y,len=dx*dx+dy*dy;
    const double t=len ? std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/len,0.,1.) : 0;
    return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
}
std::optional<Geometry> parse_path(std::string_view data) { return PathReader(data).read(); }

void paint_shape(ui_node_kind kind,const Geometry* path,const Brush* fill,const Brush* stroke,double thickness,
                 layout::Rect r,layout::Rect clip,double scale,std::vector<FillRectCommand>& out) {
    if (!fill && !stroke) return;
    const auto first=out.size();
    const double w=(double(r.right)-r.left)/scale,h=(double(r.bottom)-r.top)/scale;
    if(w<=0 || h<=0) return;
    for(int y=std::max(r.top,clip.top);y<std::min(r.bottom,clip.bottom);++y)
        for(int x=std::max(r.left,clip.left);x<std::min(r.right,clip.right);++x) {
            unsigned fills=0,strokes=0;
            for(double oy:{.25,.75}) for(double ox:{.25,.75}) {
                const ui_point p{(x+ox-r.left)/scale,(y+oy-r.top)/scale};
                bool inside=false,edge=false;
                if(kind==UI_RECTANGLE) {
                    inside=true;edge=std::min({p.x,p.y,w-p.x,h-p.y})<thickness;
                } else if(kind==UI_ELLIPSE) {
                    inside=std::hypot(2*p.x/w-1,2*p.y/h-1)<=1;
                    const double rx=w/2-thickness,ry=h/2-thickness;
                    edge=inside && (rx<=0 || ry<=0 || std::hypot((p.x-w/2)/rx,(p.y-h/2)/ry)>=1);
                } else if(kind==UI_LINE) edge=distance(p,{0,0},{w,h})<=thickness/2;
                else if(path) for(const auto& f:path->figures) {
                    for(size_t i=0,j=f.points.size()-1;i<f.points.size();j=i++) {
                        auto a=f.points[j],b=f.points[i];
                        if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
                        if(stroke && (i || f.closed) && distance(p,a,b)<=thickness/2) edge=true;
                    }
                }
                if(stroke && thickness>0 && edge) ++strokes;
                else if(fill && inside) ++fills;
            }
            auto f=fill ? fill->sample(r,x+.5,y+.5,scale) : std::array<uint8_t,4>{};
            auto s=stroke ? stroke->sample(r,x+.5,y+.5,scale) : std::array<uint8_t,4>{};
            const unsigned af=fills*f[3],as=strokes*s[3],alpha=af+as;
            if(!alpha) continue;
            for(unsigned c=0;c<3;++c) f[c]=uint8_t((af*f[c]+as*s[c]+alpha/2)/alpha);
            f[3]=uint8_t((alpha+2)/4);
            if(out.size()>first && out.back().software_layer && out.back().right==x && out.back().top==y &&
               out.back().bottom==y+1 && out.back().color==f) ++out.back().right;
            else out.push_back({x,y,x+1,y+1,f,true});
        }
}
}
