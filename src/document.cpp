#include "document.h"
#include "ui_tree.h"
#include "dock_json.h"
#include "input/hosted_text.h"
#include "dwrite_text.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace document {
using layout::Rect;
using Color = std::array<uint8_t, 4>;
static Rect intersect(Rect a, Rect b) { return {std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)}; }
static bool inside(Rect r,int x,int y) { return x>=r.left && x<r.right && y>=r.top && y<r.bottom; }
static void fill(std::vector<FillRectCommand>& out, Rect r, Rect clip, Color c) {
    r=intersect(r,clip); if(r.right>r.left && r.bottom>r.top) out.push_back({r.left,r.top,r.right,r.bottom,c});
}
std::shared_ptr<State> parse(std::string_view json) {
    if(json.size()>16*1024*1024) throw std::range_error("Document too large");
    auto root=docking::json::Reader(json, 1024*1024).parse();
    if(root.at("version").number(1,1)!=1) throw std::range_error("Document version");
    auto result=std::make_shared<State>();
    std::set<std::string> ids;
    for(const auto& item:root.at("blocks").array()) {
        Block b; b.kind=item.at("kind").string();
        if(b.kind!="heading" && b.kind!="paragraph" && b.kind!="code" && b.kind!="quote" && b.kind!="list" && b.kind!="table" && b.kind!="rule" && b.kind!="image") throw std::range_error("Document block");
        if(item.has("id")) { b.id=item.at("id").string(); if(!b.id.empty()&&!ids.insert(b.id).second) throw std::range_error("Duplicate heading"); }
        if(item.has("level")) b.level=item.at("level").number(0,6);
        if(item.has("indent")) b.indent=item.at("indent").number(0,32);
        if(item.has("language")) b.language=item.at("language").string();
        if(item.has("raw")) b.raw=item.at("raw").string();
        if(item.has("spans")) for(const auto& entry:item.at("spans").array()) {
            Span s; s.text=hosted_text::decode(entry.at("text").string());
            if(entry.has("href")) s.href=entry.at("href").string();
            if(entry.has("color")) s.color=entry.at("color").string();
            if(entry.has("flags")) s.flags=entry.at("flags").number(0,7);
            b.spans.push_back(std::move(s));
        }
        if(item.has("image")) {
            const auto& im=item.at("image"); b.image.width=im.at("width").number(1,1024); b.image.height=im.at("height").number(1,1024);
            const auto& rows=im.at("rows").array();
            for(const auto& row:rows) {
                const auto& hex=row.string();
                if(hex.size()!=b.image.width*6) throw std::range_error("Image row");
                for(size_t i=0;i<hex.size();i+=2) {
                    auto nibble=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; throw std::range_error("Image encoding"); };
                    b.image.rgb.push_back(uint8_t(nibble(hex[i])*16+nibble(hex[i+1])));
                }
            }
            if(b.image.rgb.size()!=size_t(b.image.width)*b.image.height*3) throw std::range_error("Image size");
        }
        result->blocks.push_back(std::move(b));
    }
    return result;
}
static void font(unsigned flags) { document_text_style(flags); }
static void layout(State& s,int width,unsigned dpi) {
    s.format=active_text_format(); s.font_revision=text_resource_revision;
    s.width=width; s.dpi=dpi; s.glyphs.clear(); s.boxes.clear(); s.paragraphs.clear(); s.anchors.clear(); s.plain.clear();
    const auto px=[&](int v){return std::max(1,int(v*dpi/96));};
    std::map<std::tuple<std::wstring,unsigned,unsigned>,int> advances;
    int y=px(24);
    for(size_t bi=0;bi<s.blocks.size();++bi) {
        const auto& b=s.blocks[bi];
        const bool wide=b.kind=="code"||b.kind=="table";
        const int left=px(24+int(b.indent)*18), right=std::max(left+px(32),width-px(30));
        if(!b.id.empty()) s.anchors[b.id]=y-px(16);
        const unsigned size=b.kind=="heading" ? (b.level==1?30:b.level==2?23:19): b.kind=="code"?14:16;
        const unsigned td=std::max(1u,dpi*size/14);
        font(b.kind=="code"||b.kind=="table"?4:0);
        const int line=std::max(px(int(size)*3/2),text_line_height(td)+px(3));
        const int top=y;
        if(b.kind=="code") y+=px(32);
        if(b.kind=="rule") { y+=px(16); s.boxes.push_back({{left,top,right,y},bi}); continue; }
        if(b.kind=="image" && b.image.width) {
            const int w=std::min(right-left,px(b.image.width));
            y+=int(int64_t(w)*b.image.height/b.image.width);
            s.boxes.push_back({{left,top,left+w,y},bi}); y+=px(18); continue;
        }
#ifdef _WIN32
        {
            Paragraph p; p.format=active_text_format(); p.dpi=td; p.start=s.plain.size();
            p.format.trimming_set=true; p.format.trimming=UI_TEXT_TRIM_NONE;
            if (wide) { p.format.wrapping=UI_TEXT_NO_WRAP; p.format.alignment=UI_TEXT_ALIGN_LEFT; }
            struct Link { size_t start,end; const Span* span; };
            std::vector<Link> links;
            for (const auto& span:b.spans) {
                const auto start=p.text.size();
                for (auto ch:span.text) if (ch!=L'\r') p.text+=ch;
                auto f=p.format;
                const unsigned flags=span.flags|(b.kind=="heading"?1:0)|(wide?4:0);
                if (flags&1) f.weight=UI_FONT_BOLD;
                if (flags&2) f.style=UI_FONT_ITALIC;
                if (flags&4) f.family="Consolas";
                if (!span.href.empty()) f.decorations|=UI_TEXT_UNDERLINE;
                p.runs.push_back({uint32_t(start),uint32_t(p.text.size()-start),std::move(f)});
                p.colors.push_back(!span.color.empty() ? span.color.find("comment")!=std::string::npos?2:span.color.find("string")!=std::string::npos?3:1 : !span.href.empty()?1:0);
                links.push_back({start,p.text.size(),&span});
            }
            int maxx=right;
            auto place=[&](Paragraph paragraph,int x,int row_y,int content_width) {
                TextFormatScope format_scope(paragraph.format); InlineTextScope inline_scope(paragraph.runs);
                const int content_height=windows_text::height(paragraph.text,td,content_width);
                int paragraph_right=x+content_width;
                for (const auto& fragment:windows_text::fragments(paragraph.text,td,content_width)) {
                    auto r=fragment.bounds; r.left+=x; r.right+=x; r.top+=row_y; r.bottom+=row_y;
                    maxx=std::max(maxx,r.right);
                    if (wide) paragraph_right=std::max(paragraph_right,r.right);
                    const auto index=paragraph.start-p.start+fragment.start;
                    const auto link=std::find_if(links.begin(),links.end(),[&](const auto& item){return index>=item.start && index<item.end;});
                    const auto* span=link!=links.end()?link->span:nullptr;
                    s.glyphs.push_back({r,paragraph.text.substr(fragment.start,fragment.length),td,span?span->flags:0,paragraph.start+fragment.start,bi,span?span->href:"",span?span->color:"",fragment.rtl});
                }
                paragraph.bounds={x,row_y,paragraph_right,row_y+content_height};
                s.paragraphs.emplace(bi,std::move(paragraph));
                return content_height;
            };
            s.plain+=p.text; s.plain+=L'\n';
            if (b.kind=="table") {
                struct Cell { Paragraph paragraph; size_t row,column; };
                std::vector<Cell> cells;
                std::vector<int> columns;
                size_t row=0,column=0,start=0;
                // Delimiters remain in plain text, but each cell is a separate
                // bidi paragraph. Run ranges are intersected with the cell so
                // shaping and combining clusters can cross style boundaries.
                for (size_t end=0;end<=p.text.size();++end) {
                    if (end<p.text.size() && p.text[end]!=L'\t' && p.text[end]!=L'\n') continue;
                    if (end==p.text.size() && start==end && end && p.text.back()==L'\n') break;
                    Paragraph cell; cell.text=p.text.substr(start,end-start); cell.format=p.format;
                    cell.dpi=td; cell.start=p.start+start;
                    for (size_t i=0;i<p.runs.size();++i) {
                        const auto& run=p.runs[i];
                        const auto first=std::max(start,size_t(run.start)), last=std::min(end,size_t(run.start)+run.length);
                        if (last>first) {
                            cell.runs.push_back({uint32_t(first-start),uint32_t(last-first),run.format});
                            cell.colors.push_back(p.colors[i]);
                        }
                    }
                    TextFormatScope format_scope(cell.format); InlineTextScope inline_scope(cell.runs);
                    if (columns.size()<=column) columns.resize(column+1);
                    columns[column]=std::max(columns[column],windows_text::width(cell.text,td)+px(24));
                    cells.push_back({std::move(cell),row,column});
                    if (end<p.text.size() && p.text[end]==L'\n') { ++row; column=0; } else ++column;
                    start=end+1;
                }
                size_t current_row=0; int row_height=line,x=left;
                for (auto& cell:cells) {
                    if (cell.row!=current_row) { y+=row_height; row_height=line; x=left; current_row=cell.row; }
                    row_height=std::max(row_height,place(std::move(cell.paragraph),x,y,columns[cell.column]-px(12)));
                    x+=columns[cell.column]; maxx=std::max(maxx,x);
                }
                y+=row_height;
            } else {
                y+=place(p,left,y,right-left);
            }
            y+=px(12);
            s.boxes.push_back({{left-px(8),top,maxx+px(8),y},bi});
            y+=px(b.kind=="heading"?8:12);
            continue;
        }
#endif
        std::vector<int> columns;
        if(b.kind=="table") {
            std::wstring all;for(const auto& span:b.spans)all+=span.text;
            std::wstring cell;size_t column=0;
            for(auto ch:all) {
                if(ch==L'\t'||ch==L'\n') {
                    if(columns.size()<=column)columns.resize(column+1);
                    columns[column]=std::max(columns[column],text_width(cell.c_str(),td)+px(24));
                    cell.clear();column=ch==L'\n'?0:column+1;
                }else cell+=ch;
            }
        }
        size_t column=0;
        int x=left, maxx=right;
        for(const auto& span:b.spans) {
            const unsigned flags=span.flags|(b.kind=="heading"?1:0)|(b.kind=="code"?4:0);
            font(flags);
            for(size_t ci=0;ci<span.text.size();++ci) {
                const auto ch=span.text[ci];
                if(ch=='\r') continue;
                const size_t index=s.plain.size(); s.plain+=ch;
                if(ch=='\n') { x=left;column=0;y+=line;continue; }
                std::wstring text(1,ch=='\t'?L' ':ch);
                if(sizeof(wchar_t)==2 && ch>=0xd800 && ch<=0xdbff && ci+1<span.text.size()) {text+=span.text[++ci];s.plain+=text.back();}
                const auto key=std::tuple{text,td,flags};
                auto found=advances.find(key);
                int advance=(found!=advances.end()?found->second:advances.emplace(key,std::max(1,text_width(text.c_str(),td))).first->second)*(ch=='\t'?4:1);
                if(ch=='\t' && b.kind=="table") {int edge=left;for(size_t j=0;j<=column&&j<columns.size();++j)edge+=columns[j];advance=std::max(px(12),edge-x);++column;}
                if(!wide && x+advance>right && x>left) { x=left;y+=line; }
                s.glyphs.push_back({{x,y,x+advance,y+line},std::move(text),td,flags,index,bi,span.href,span.color});
                x+=advance; maxx=std::max(maxx,x);
            }
        }
        s.plain+=L'\n'; y+=line+px(12);
        s.boxes.push_back({{left-px(8),top,maxx+px(8),y},bi});
        y+=px(b.kind=="heading"?8:12);
    }
    font(0); s.height=y+px(24); s.first=std::min(s.first,s.plain.size()); s.last=std::min(s.last,s.plain.size());
}
static Rect screen(const ui_node& n,Rect r,size_t block) {
    const auto& s=*n.document;
    const int x=n.bounds.left-(s.horizontal.contains(block)?s.horizontal.at(block):0);
    return layout::Box{r.left+x,r.top+n.bounds.top-n.offset,r.right+x,r.bottom+n.bounds.top-n.offset};
}
void paint(ui_node& node,Rect area,Rect clip,GalleryStyle style,unsigned dpi,std::vector<FillRectCommand>& out) {
    if(!node.document) node.document=std::make_shared<State>();
    auto& s=*node.document;
    const int width=area.right-area.left;
    if(s.width!=width||s.dpi!=dpi||s.format!=active_text_format()||s.font_revision!=text_resource_revision) layout(s,width,dpi);
    node.scroll_limit=std::max(0,s.height-(area.bottom-area.top));
    if(!s.pending_anchor.empty()) { if(s.anchors.contains(s.pending_anchor)) node.offset=s.anchors.at(s.pending_anchor); s.pending_anchor.clear(); }
    node.offset=std::clamp(node.offset,0.0,node.scroll_limit);
    const auto ink=text_foreground(theme_rgb(style.colors[UI_THEME_TEXT])), blue=theme_rgb(style.colors[UI_THEME_PRIMARY]);
    const auto panel=theme_rgb(style.colors[UI_THEME_CONTROL]), divider=theme_rgb(style.colors[UI_THEME_DIVIDER]);
    fill(out,area,clip,theme_rgb(style.colors[UI_THEME_BACKGROUND]));
    for(const auto& box:s.boxes) {
        auto r=screen(node,box.bounds,box.block); const auto& b=s.blocks[box.block];
        if(r.bottom<clip.top||r.top>clip.bottom) continue;
        if(b.kind=="code"||b.kind=="table") fill(out,r,clip,panel);
        if(b.kind=="quote") fill(out,{r.left,r.top,r.left+3,r.bottom},clip,blue);
        if(b.kind=="rule") fill(out,{r.left,r.top,r.right,r.top+1},clip,divider);
        if(b.kind=="code") {
            const auto label=hosted_text::decode(b.language+"     复制代码");
            Rect header{area.left+24,r.top,area.right-24,r.top+int(28*dpi/96)};
            append_text(out,header,clip,blue,dpi,label.c_str(),true,nullptr,false);
        }
        if(b.kind=="image" && b.image.width) for(int y=std::max(r.top,clip.top);y<std::min(r.bottom,clip.bottom);++y) for(int x=std::max(r.left,clip.left);x<std::min(r.right,clip.right);++x) {
            const size_t index=(size_t((y-r.top)*b.image.height/std::max(1,r.bottom-r.top))*b.image.width+(x-r.left)*b.image.width/std::max(1,r.right-r.left))*3;
            fill(out,{x,y,x+1,y+1},clip,{b.image.rgb[index],b.image.rgb[index+1],b.image.rgb[index+2],255});
        }
    }
    const auto low=std::min(s.first,s.last),high=std::max(s.first,s.last);
#ifdef _WIN32
    for (const auto& [block,p]:s.paragraphs) {
        const layout::Box paragraph_bounds=screen(node,p.bounds,block);
        const auto bounds=intersect(paragraph_bounds,clip);
        if (bounds.bottom<=bounds.top || bounds.right<=bounds.left) continue;
        auto runs=p.runs;
        const Color colors[]={ink,blue,{105,130,120,255},{38,150,100,255}};
        for (size_t i=0;i<runs.size();++i) {
            const auto c=colors[p.colors[i]];
            runs[i].format.foreground_set=true;
            runs[i].format.foreground=(uint32_t(c[3])<<24)|(uint32_t(c[0])<<16)|(uint32_t(c[1])<<8)|c[2];
        }
        TextFormatScope format_scope(p.format); InlineTextScope inline_scope(runs);
        HostedEditor hosted; hosted.active=true; hosted.reveal=false;
        hosted.first=std::clamp(low,p.start,p.start+p.text.size())-p.start;
        hosted.last=std::clamp(high,p.start,p.start+p.text.size())-p.start;
        std::vector<TextStop> stops;
        EditorGeometry selection{stops,0,false,&hosted,theme_rgb(style.colors[UI_THEME_SELECTED]),theme_rgb(style.colors[UI_THEME_ON_PRIMARY])};
        windows_text::append(out,bounds,clip,ink,p.dpi,p.text.c_str(),false,&selection,0,&paragraph_bounds);
    }
#endif
    for(const auto& g:s.glyphs) {
        if (s.paragraphs.contains(g.block)) continue;
        auto r=screen(node,g.bounds,g.block);
        if(r.bottom<=clip.top||r.top>=clip.bottom||r.right<=clip.left||r.left>=clip.right) continue;
        auto c=g.href.empty()?ink:blue;
        if(!g.color.empty()) c= g.color.find("comment")!=std::string::npos?Color{105,130,120,255}:g.color.find("string")!=std::string::npos?Color{38,150,100,255}:blue;
        // An opaque local backdrop keeps glyph compositing independent of all
        // preceding text commands, avoiding quadratic work on article pages.
        auto background=(s.blocks[g.block].kind=="code"||s.blocks[g.block].kind=="table"||(g.flags&4))?panel:theme_rgb(style.colors[UI_THEME_BACKGROUND]);
        if(g.index>=low && g.index<high) background=theme_rgb(style.colors[UI_THEME_SELECTED]);
        fill(out,r,clip,background);
        font(g.flags); append_text(out,r,clip,c,g.dpi,g.text.c_str(),true,nullptr,false);
        if((g.flags&1)!=0) { ++r.left;++r.right;append_text(out,r,clip,c,g.dpi,g.text.c_str(),true,nullptr,false); }
        if(!g.href.empty()) fill(out,{r.left,r.bottom-3,r.right,r.bottom-2},clip,c);
    }
    font(0);
    node.scrollbar={};node.scrollbar_thumb={};
    if(node.scroll_limit) {
        const int h=area.bottom-area.top,th=std::max(24,int(int64_t(h)*h/s.height));
        const int top=area.top+int(int64_t(h-th)*node.offset/node.scroll_limit);
        node.scrollbar={area.right-8,area.top,area.right,area.bottom}; node.scrollbar_thumb={area.right-8,top,area.right,top+th};
    }
}
void pointer(ui_node& node,int action,int x,int y) {
    if(!node.document) return;
    auto& s=*node.document;
    if(action==3) {s.selecting=false;return;}
    const Glyph* nearest=nullptr;
    auto distance=std::tuple{INT64_MAX,INT64_MAX,INT64_MAX};
    for(const auto& g:s.glyphs) {
        const auto r=screen(node,g.bounds,g.block);
        // Compare distance to the actual hit rectangles before their centers:
        // adjacent styled runs may have different line heights/baselines.
        const int64_t dy=std::max({int64_t(r.top)-y,int64_t(y)-r.bottom, int64_t(0)});
        const int64_t dx=std::max({int64_t(r.left)-x,int64_t(x)-r.right, int64_t(0)});
        const auto d=std::tuple{dy,dx,std::abs(int64_t(x)*2-r.left-r.right)};
        if(d<distance) {distance=d;nearest=&g;}
    }
    const size_t position=nearest?nearest->index+((x>screen(node,nearest->bounds,nearest->block).left+(nearest->bounds.right-nearest->bounds.left)/2)!=nearest->rtl?nearest->text.size():0):0;
    if(action==0) {s.first=s.last=position;s.selecting=true;s.down_x=x;s.down_y=y;}
    if(action==1 && s.selecting) s.last=position;
    if(action==2 && s.selecting) {
        s.last=position; s.selecting=false;
        if(std::abs(x-s.down_x)+std::abs(y-s.down_y)<5) {
            for(const auto& box:s.boxes) {
                auto r=screen(node,box.bounds,box.block);
                if(s.blocks[box.block].kind=="code" && y>=r.top&&y<r.top+int(28*s.dpi/96)) {
                    s.first=s.plain.size();s.last=0;
                    for(const auto& g:s.glyphs) if(g.block==box.block){s.first=std::min(s.first,g.index);s.last=std::max(s.last,g.index+g.text.size());}
                    if(s.first>s.last)s.first=s.last=0;
                    s.event=UI_DOCUMENT_COPY;s.event_value=s.blocks[box.block].raw;return;
                }
            }
            if(nearest&&!nearest->href.empty()&&inside(screen(node,nearest->bounds,nearest->block),x,y)) {s.event=UI_DOCUMENT_LINK;s.event_value=nearest->href;}
        }
    }
}
bool horizontal(ui_node& node,int x,int y,int delta) {
    if(!node.document) return false;
    auto& s=*node.document;
    for(const auto& b:s.boxes) {
        if(s.blocks[b.block].kind!="code"&&s.blocks[b.block].kind!="table") continue;
        auto r=screen(node,b.bounds,b.block);
        if(y<r.top||y>=r.bottom||x<node.bounds.left||x>=node.bounds.right) continue;
        const int limit=std::max(0,b.bounds.right-s.width+24);
        s.horizontal[b.block]=std::clamp(s.horizontal[b.block]+delta,0,limit);return true;
    }
    return false;
}
std::string selected(const State& s) {return hosted_text::encode(std::wstring_view(s.plain).substr(std::min(s.first,s.last),std::max(s.first,s.last)-std::min(s.first,s.last)));}
}
