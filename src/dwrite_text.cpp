#include "dwrite_text.h"
#include "text_format.h"
#include "inline_text.h"
#include "utf8_text.h"
#ifdef _WIN32
#define NOMINMAX
#include <dwrite.h>
#include <icu.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <list>
#include <limits>
#include <stdexcept>

namespace windows_text {
namespace {
using Microsoft::WRL::ComPtr;
using Color = std::array<uint8_t,4>;
void checked(HRESULT result) { if (FAILED(result)) throw std::runtime_error("DirectWrite operation failed"); }
IDWriteFactory* factory() {
    thread_local ComPtr<IDWriteFactory> value;
    if (!value) checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(value.GetAddressOf())));
    return value.Get();
}
struct ColorEffect final : IUnknown {
    ULONG references=1;
    Color color;
    explicit ColorEffect(uint32_t value):color{uint8_t(value>>16),uint8_t(value>>8),uint8_t(value),uint8_t(value>>24)}{}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if (!out) return E_POINTER;
        *out=nullptr;
        if (id!=__uuidof(IUnknown)) return E_NOINTERFACE;
        *out=static_cast<IUnknown*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { const auto count=--references; if (!count) delete this; return count; }
};
struct Layout {
    std::wstring text;
    TextFormat font;
    std::vector<StyledRun> runs;
    unsigned dpi=96, flags=0;
    double width=0;
    bool single=false, editor=false;
    ComPtr<IDWriteTextLayout> object;
    DWRITE_TEXT_METRICS metrics{};
    std::vector<size_t> boundaries;
    std::vector<uint8_t> mask;
    std::vector<Color> pixels;
    Color raster_color{};
    int mask_width=0,mask_height=0;
    double scroll_x=-1,scroll_y=-1;
};
Layout make(std::wstring_view text, unsigned dpi, unsigned flags, double width, bool single, bool editor) {
    Layout result; result.text=text; result.dpi=dpi; result.flags=flags; result.width=width; result.single=single; result.editor=editor;
    result.font=active_text_format();
    result.runs.assign(current_text_runs.begin(),current_text_runs.end());
    const auto family=utf8_text::decode(result.font.family);
    ComPtr<IDWriteTextFormat> format;
    const auto* selected_family=flags&4 ? L"Consolas" : family.c_str();
    checked(factory()->CreateTextFormat(selected_family,collection_for(result.font.resources,selected_family),
        flags&1 ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT(result.font.weight),
        flags&2 ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE(result.font.style),DWRITE_FONT_STRETCH(result.font.stretch),
        float(result.font.size*dpi/96.0),L"",&format));
    checked(format->SetWordWrapping(single || result.font.wrapping==UI_TEXT_NO_WRAP ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP));
    const bool rtl=result.font.flow==UI_FLOW_RIGHT_TO_LEFT;
    checked(format->SetReadingDirection(rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
    const auto alignment=result.font.alignment==UI_TEXT_ALIGN_CENTER ? DWRITE_TEXT_ALIGNMENT_CENTER :
        result.font.alignment==UI_TEXT_ALIGN_JUSTIFY ? DWRITE_TEXT_ALIGNMENT_JUSTIFIED :
        (result.font.alignment==UI_TEXT_ALIGN_RIGHT)!=rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING;
    checked(format->SetTextAlignment(alignment));
    const auto trimming=result.font.trimming_set ? result.font.trimming : single ? UI_TEXT_TRIM_CHARACTER_ELLIPSIS : UI_TEXT_TRIM_NONE;
    if (trimming!=UI_TEXT_TRIM_NONE && !editor) {
        ComPtr<IDWriteInlineObject> ellipsis;
        checked(factory()->CreateEllipsisTrimmingSign(format.Get(),&ellipsis));
        const DWRITE_TRIMMING trim{trimming==UI_TEXT_TRIM_WORD_ELLIPSIS ? DWRITE_TRIMMING_GRANULARITY_WORD : DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        checked(format->SetTrimming(&trim,ellipsis.Get()));
    }
    checked(factory()->CreateTextLayout(result.text.data(),UINT32(result.text.size()),format.Get(),
        float(std::min(std::max(0.0,width),double(std::numeric_limits<float>::max()))),std::numeric_limits<float>::max(),&result.object));
    const DWRITE_TEXT_RANGE range{0,UINT32(result.text.size())};
    checked(result.object->SetUnderline((result.font.decorations&UI_TEXT_UNDERLINE)!=0,range));
    checked(result.object->SetStrikethrough((result.font.decorations&UI_TEXT_STRIKETHROUGH)!=0,range));
    ComPtr<IDWriteFontCollection> system_fonts;
    for (const auto& run:result.runs) {
        if (run.start>=result.text.size()) continue;
        const DWRITE_TEXT_RANGE part{run.start,std::min(run.length,UINT32(result.text.size()-run.start))};
        const auto& f=run.format;
        const auto family=utf8_text::decode(f.family);
        auto* collection=collection_for(f.resources,family.c_str());
        if (!collection) {
            if (!system_fonts) checked(factory()->GetSystemFontCollection(&system_fonts));
            collection=system_fonts.Get();
        }
        checked(result.object->SetFontCollection(collection,part));
        checked(result.object->SetFontFamilyName(family.c_str(),part));
        checked(result.object->SetFontSize(float(f.size*dpi/96.0),part));
        checked(result.object->SetFontWeight(DWRITE_FONT_WEIGHT(f.weight),part));
        checked(result.object->SetFontStyle(DWRITE_FONT_STYLE(f.style),part));
        checked(result.object->SetFontStretch(DWRITE_FONT_STRETCH(f.stretch),part));
        checked(result.object->SetUnderline((f.decorations&UI_TEXT_UNDERLINE)!=0,part));
        checked(result.object->SetStrikethrough((f.decorations&UI_TEXT_STRIKETHROUGH)!=0,part));
        if (f.foreground_set) {
            ComPtr<ColorEffect> effect; effect.Attach(new ColorEffect(f.foreground));
            checked(result.object->SetDrawingEffect(effect.Get(),part));
        }
    }
    if (result.font.line_height>0) {
        UINT32 line_count=0;
        result.object->GetLineMetrics(nullptr,0,&line_count);
        std::vector<DWRITE_LINE_METRICS> lines(line_count);
        if (line_count) checked(result.object->GetLineMetrics(lines.data(),line_count,&line_count));
        const float height=float(result.font.line_height*dpi/96.0);
        const float baseline=line_count ? lines[0].baseline+(height-lines[0].height)/2 : height*.8f;
        checked(result.object->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,height,std::max(0.f,baseline)));
    }
    checked(result.object->GetMetrics(&result.metrics));
    UINT32 count=0; result.object->GetClusterMetrics(nullptr,0,&count);
    std::vector<DWRITE_CLUSTER_METRICS> metrics(count);
    if (count) checked(result.object->GetClusterMetrics(metrics.data(),count,&count));
    size_t position=0; result.boundaries.push_back(0);
    for (const auto& cluster : metrics) { position+=cluster.length; result.boundaries.push_back(position); }
    // Shaping clusters and user-perceived characters are different boundaries.
    // A fallback run can split a combining sequence; never expose that split to editing.
    UErrorCode error=U_ZERO_ERROR;
    auto* breaks=ubrk_open(UBRK_CHARACTER,"",reinterpret_cast<const UChar*>(result.text.data()),int32_t(result.text.size()),&error);
    if (U_FAILURE(error) || !breaks) throw std::runtime_error("Windows ICU character segmentation failed");
    struct CloseBreaks { UBreakIterator* value; ~CloseBreaks() { ubrk_close(value); } } close{breaks};
    std::erase_if(result.boundaries,[&](size_t boundary){return !ubrk_isBoundary(breaks,int32_t(boundary));});
    return result;
}
thread_local std::list<Layout> layouts;
Layout& cached(std::wstring_view text,unsigned dpi,unsigned flags,double width,bool single,bool editor) {
    auto found=std::find_if(layouts.begin(),layouts.end(),[&](const auto& l) {
        return l.font==active_text_format() && std::equal(l.runs.begin(),l.runs.end(),current_text_runs.begin(),current_text_runs.end()) && l.text==text && l.dpi==dpi && l.flags==flags && l.width==width && l.single==single && l.editor==editor;
    });
    if (found==layouts.end()) layouts.push_front(make(text,dpi,flags,width,single,editor));
    else layouts.splice(layouts.begin(),layouts,found);
    size_t bytes=0;
    for (auto it=layouts.begin();it!=layouts.end();) {
        bytes+=it->mask.size()+it->pixels.size()*sizeof(Color);
        if (it!=layouts.begin() && bytes>8*1024*1024) it=layouts.erase(it); else ++it;
    }
    if (layouts.size()>64) layouts.pop_back();
    return layouts.front();
}
struct Hit { float x=0,y=0; DWRITE_HIT_TEST_METRICS metrics{}; };
Hit hit(Layout& layout,size_t position,bool trailing=false) {
    Hit result;
    const bool end=position>=layout.text.size() && !layout.text.empty();
    position=std::min(position,layout.text.size());
    if ((trailing || end) && position) {
        // A grapheme can span several fallback shaping clusters (e.g. ZWJ
        // emoji). Its trailing edge belongs to the final shaping cluster.
        --position;
        trailing=true;
    } else trailing=false;
    checked(layout.object->HitTestTextPosition(UINT32(position),trailing,&result.x,&result.y,&result.metrics));
    return result;
}
void fill(std::vector<FillRectCommand>& out,layout::Rect r,layout::Rect clip,Color color) {
    r={std::max(r.left,clip.left),std::max(r.top,clip.top),std::min(r.right,clip.right),std::min(r.bottom,clip.bottom)};
    if (r.right>r.left && r.bottom>r.top) out.push_back({r.left,r.top,r.right,r.bottom,color});
}
class Renderer final : public IDWriteTextRenderer {
    ULONG references=1;
    int width,height;
    std::vector<uint8_t>& mask;
    std::vector<Color>& colors;
    Color fallback;
    Color ink(IUnknown* effect) const { return effect ? static_cast<ColorEffect*>(effect)->color : fallback; }
    void pixel(size_t index,unsigned coverage,Color color) {
        auto& destination=mask[index];
        destination=uint8_t(coverage+(unsigned(destination)*(255-coverage)+127)/255);
        if (colors.empty()) return;
        const unsigned alpha=(coverage*color[3]+127)/255;
        auto& old=colors[index];
        const unsigned retained=(unsigned(old[3])*(255-alpha)+127)/255;
        const unsigned total=alpha+retained;
        if (total) for (unsigned c=0;c<3;++c) old[c]=uint8_t((unsigned(color[c])*alpha+unsigned(old[c])*retained+total/2)/total);
        old[3]=uint8_t(total);
    }
    void rectangle(float x,float y,float w,float h,IUnknown* effect) {
        for (int row=std::max(0,int(std::floor(y)));row<std::min(height,int(std::ceil(y+h)));++row)
            for (int col=std::max(0,int(std::floor(x)));col<std::min(width,int(std::ceil(x+w)));++col) pixel(size_t(row)*width+col,255,ink(effect));
    }
public:
    Renderer(int w,int h,std::vector<uint8_t>& pixels,std::vector<Color>& canvas,Color color):width(w),height(h),mask(pixels),colors(canvas),fallback(color){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** object) override {
        if (!object) return E_POINTER;
        *object=nullptr;
        if (id==__uuidof(IUnknown) || id==__uuidof(IDWritePixelSnapping) || id==__uuidof(IDWriteTextRenderer)) { *object=static_cast<IDWriteTextRenderer*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* disabled) override { *disabled=TRUE; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* transform) override { *transform={1,0,0,1,0,0}; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* scale) override { *scale=1; return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE mode,const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown* effect) override {
        try {
            ComPtr<IDWriteGlyphRunAnalysis> analysis;
            checked(factory()->CreateGlyphRunAnalysis(run,1,nullptr,DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,mode,x,y,&analysis));
            RECT r{}; checked(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1,&r));
            r.left=std::max(0L,r.left); r.top=std::max(0L,r.top); r.right=std::min(LONG(width),r.right); r.bottom=std::min(LONG(height),r.bottom);
            if (r.right<=r.left || r.bottom<=r.top) return S_OK;
            std::vector<uint8_t> pixels(size_t(r.right-r.left)*(r.bottom-r.top)*3);
            checked(analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1,&r,pixels.data(),UINT32(pixels.size())));
            size_t i=0;
            for (int row=r.top;row<r.bottom;++row) for (int col=r.left;col<r.right;++col,i+=3) {
                const unsigned alpha=(unsigned(pixels[i])+pixels[i+1]+pixels[i+2]+1)/3;
                pixel(size_t(row)*width+col,alpha,ink(effect));
            }
            return S_OK;
        } catch(const std::bad_alloc&) { return E_OUTOFMEMORY; } catch(...) { return E_FAIL; }
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT x,FLOAT y,const DWRITE_UNDERLINE* line,IUnknown* effect) override { rectangle(x,y+line->offset,line->width,line->thickness,effect); return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT x,FLOAT y,const DWRITE_STRIKETHROUGH* line,IUnknown* effect) override { rectangle(x,y+line->offset,line->width,line->thickness,effect); return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void* context,FLOAT x,FLOAT y,IDWriteInlineObject* object,BOOL sideways,BOOL rtl,IUnknown* effect) override { return object->Draw(context,this,x,y,sideways,rtl,effect); }
};
}
void discard_font_layouts(const FontResources& resources) {
    if (resources) std::erase_if(layouts,[&](const auto& layout){return layout.font.resources==resources;});
}
int width(std::wstring_view text,unsigned dpi,unsigned flags) {
    return int(std::ceil(cached(text,dpi,flags,std::numeric_limits<double>::infinity(),true,true).metrics.widthIncludingTrailingWhitespace));
}
int line_height(unsigned dpi,unsigned flags) {
    InlineTextScope plain({});
    return int(std::ceil(cached(L"M",dpi,flags,std::numeric_limits<double>::infinity(),true,true).metrics.height));
}
int height(std::wstring_view text,unsigned dpi,int width,unsigned flags) {
    return int(std::ceil(cached(text,dpi,flags,std::max(1,width),false,false).metrics.height));
}
double measure_width(std::wstring_view text,unsigned dpi) {
    return cached(text,dpi,0,std::numeric_limits<double>::infinity(),true,true).metrics.widthIncludingTrailingWhitespace;
}
double measure_height(std::wstring_view text,unsigned dpi,double width) {
    return cached(text,dpi,0,std::max(0.0,width),false,false).metrics.height;
}
double measure_line_height(unsigned dpi) {
    InlineTextScope plain({});
    return cached(L"M",dpi,0,std::numeric_limits<double>::infinity(),true,true).metrics.height;
}
std::vector<Fragment> fragments(std::wstring_view text,unsigned dpi,int width) {
    auto& layout=cached(text,dpi,0,std::max(1,width),false,true);
    std::vector<Fragment> result;
    for (size_t i=1;i<layout.boundaries.size();++i) {
        const auto start=UINT32(layout.boundaries[i-1]),length=UINT32(layout.boundaries[i]-start);
        UINT32 count=0;
        layout.object->HitTestTextRange(start,length,0,0,nullptr,0,&count);
        std::vector<DWRITE_HIT_TEST_METRICS> hits(count);
        if (count) checked(layout.object->HitTestTextRange(start,length,0,0,hits.data(),count,&count));
        for (const auto& hit:hits) result.push_back({{int(std::floor(hit.left)),int(std::floor(hit.top)),int(std::ceil(hit.left+hit.width)),int(std::ceil(hit.top+hit.height))},start,length,(hit.bidiLevel&1)!=0});
    }
    return result;
}
std::vector<size_t> clusters(std::wstring_view text) { return cached(text,96,0,std::numeric_limits<double>::infinity(),true,true).boundaries; }
void append(std::vector<FillRectCommand>& commands,layout::Rect bounds,layout::Rect clip,Color color,
    unsigned dpi,const wchar_t* text,bool single,EditorGeometry* editor,unsigned flags,const layout::Box* paragraph_bounds,bool viewport_editor) {
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    if (width<=0 || height<=0 || (!*text && !editor)) return;
    if (uint64_t(width)*height>16777216) throw std::range_error("DirectWrite surface limit");
    auto& layout=cached(text,dpi,flags,paragraph_bounds?paragraph_bounds->right-paragraph_bounds->left:width,single,editor!=nullptr);
    auto* hosted=editor?editor->hosted:nullptr;
    auto caret=hit(layout,editor?editor->index:0,editor && editor->trailing);
    int sx=editor && single?std::max(0,int(std::ceil(caret.x))-width+1):0;
    int sy=editor?std::max(0,int(std::ceil(caret.y+caret.metrics.height))-height):0;
    if (hosted && (!paragraph_bounds || viewport_editor)) {
        sx=hosted->scroll_x; sy=hosted->scroll_y;
        if (hosted->reveal) {
            if (caret.x<sx) sx=int(std::floor(caret.x));
            if (caret.x>=sx+width) sx=int(std::ceil(caret.x))-width+1;
            if (caret.y<sy) sy=int(std::floor(caret.y));
            if (caret.y+caret.metrics.height>sy+height) sy=int(std::ceil(caret.y+caret.metrics.height))-height;
        }
        // NoWrap multiline editors also need horizontal reveal. Alignment may
        // place overflowing text to the left, so retain the layout's origin.
        const int min_x=std::min(0,int(std::floor(layout.metrics.left)));
        const int max_x=std::max(0,int(std::ceil(layout.metrics.left+layout.metrics.widthIncludingTrailingWhitespace))-width+1);
        sx=std::clamp(sx,min_x,max_x);
        sy=std::clamp(sy,0,std::max(0,int(std::ceil(layout.metrics.height))-height));
        hosted->scroll_x=sx; hosted->scroll_y=sy; hosted->reveal=false;
        hosted->caret_x=int(std::lround((paragraph_bounds?paragraph_bounds->left:bounds.left)+caret.x-sx));
        hosted->caret_y=int(std::lround((paragraph_bounds?paragraph_bounds->top:bounds.top)+caret.y-sy));
    }
    // Retain the fractional shaping origin while rasterizing only the visible
    // device rectangle; ordinary editors keep their pixel scroll contract.
    const double raster_x=paragraph_bounds?bounds.left-paragraph_bounds->left+(viewport_editor?sx:0):sx;
    const double raster_y=paragraph_bounds?bounds.top-paragraph_bounds->top+(viewport_editor?sy:0):sy;
    if (editor) {
        editor->stops.clear();
        for (auto index:layout.boundaries) {
            const auto stop=hit(layout,index);
            editor->stops.push_back({int(std::lround(bounds.left+stop.x-raster_x)),int(std::lround(bounds.top+stop.y-raster_y)),index,false,(stop.metrics.bidiLevel & 1) != 0});
            if (index) {
                const auto previous=hit(layout,index,true);
                editor->stops.push_back({int(std::lround(bounds.left+previous.x-raster_x)),int(std::lround(bounds.top+previous.y-raster_y)),index,true,(previous.metrics.bidiLevel & 1) != 0});
            }
        }
    }
    clip={std::max(bounds.left,clip.left),std::max(bounds.top,clip.top),std::min(bounds.right,clip.right),std::min(bounds.bottom,clip.bottom)};
    if (clip.right<=clip.left || clip.bottom<=clip.top) return;
    const auto ranges=[&](size_t first,size_t last) {
        std::vector<layout::Rect> output;
        first=std::min(first,layout.text.size()); last=std::clamp(last,first,layout.text.size());
        if (first==last) return output;
        UINT32 count=0; layout.object->HitTestTextRange(UINT32(first),UINT32(last-first),float(bounds.left-raster_x),float(bounds.top-raster_y),nullptr,0,&count);
        std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
        if (count) checked(layout.object->HitTestTextRange(UINT32(first),UINT32(last-first),float(bounds.left-raster_x),float(bounds.top-raster_y),metrics.data(),count,&count));
        for (const auto& m:metrics) output.push_back({int(std::floor(m.left)),int(std::floor(m.top)),int(std::ceil(m.left+m.width)),int(std::ceil(m.top+m.height))});
        return output;
    };
    std::vector<layout::Rect> selection;
    if (hosted) {
        selection=ranges(hosted->first,hosted->last);
        for (auto r:selection) fill(commands,r,clip,editor->selection);
    }
    if (layout.mask_width!=width || layout.mask_height!=height || layout.scroll_x!=raster_x || layout.scroll_y!=raster_y || (!layout.runs.empty() && layout.raster_color!=color)) {
        std::vector<uint8_t> mask(size_t(width)*height);
        std::vector<Color> pixels(layout.runs.empty()?0:size_t(width)*height);
        Renderer renderer(width,height,mask,pixels,color);
        checked(layout.object->Draw(nullptr,&renderer,float(-raster_x),float(-raster_y)));
        layout.mask=std::move(mask); layout.mask_width=width; layout.mask_height=height; layout.scroll_x=raster_x; layout.scroll_y=raster_y;
        layout.pixels=std::move(pixels); layout.raster_color=color;
    }
    const auto first_command=commands.size();
    for (int y=clip.top;y<clip.bottom;++y) for (int x=clip.left;x<clip.right;++x) {
        const unsigned coverage=layout.mask[size_t(y-bounds.top)*width+x-bounds.left];
        if (!coverage) continue;
        const bool selected=std::any_of(selection.begin(),selection.end(),[&](auto r){return x>=r.left && x<r.right && y>=r.top && y<r.bottom;});
        auto ink=selected?editor->selected_text:layout.pixels.empty()?color:layout.pixels[size_t(y-bounds.top)*width+x-bounds.left];
        if (selected || layout.pixels.empty()) ink[3]=uint8_t((unsigned(ink[3])*coverage+127)/255);
        if (commands.size()>first_command && commands.back().top==y && commands.back().right==x && commands.back().color==ink) commands.back().right=x+1;
        else commands.push_back({x,y,x+1,y+1,ink});
    }
    if (hosted && hosted->composing) for (auto r:ranges(hosted->composition_first,hosted->composition_last)) fill(commands,{r.left,r.bottom-1,r.right,r.bottom},clip,color);
    if (editor && editor->show) {
        const int x=int(std::lround(bounds.left+caret.x-raster_x)),y=int(std::lround(bounds.top+caret.y-raster_y));
        fill(commands,{x,y,x+std::max(1,int(dpi/96)),y+int(std::ceil(caret.metrics.height))},clip,color);
    }
}
}
#endif
