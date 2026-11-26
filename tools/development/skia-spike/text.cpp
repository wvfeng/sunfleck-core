#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <wrl/client.h>
#include "scene.h"
#include "text.h"
#include "include/core/SkData.h"
#ifndef SKIA_EVALUATION_DWRITE_ONLY
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#endif
#include <map>
#include <set>
#include <vector>
using Microsoft::WRL::ComPtr;
#ifndef SKIA_EVALUATION_DWRITE_ONLY
using namespace skia::textlayout;
#endif
namespace {
void checked(HRESULT hr){require(SUCCEEDED(hr),"DirectWrite operation");}
ComPtr<IDWriteFactory> factory() {
    ComPtr<IDWriteFactory> result;
    checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(result.GetAddressOf())));
    return result;
}
const std::vector<std::u16string> samples={
    u"office ffi AV Latin", u"中文排版与换行测试", u"abc אבג xyz", u"العربية نص مختلط 123",
    u"e\u0301 A\u030a 🌱 👍🏽 👩‍👩‍👧‍👦", u"one two three four five six seven eight nine"};
ComPtr<IDWriteTextLayout> layout(IDWriteFactory* factory,const std::u16string& text,float scale) {
    ComPtr<IDWriteTextFormat> format;
    checked(factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,24*scale,L"en-us",&format));
    ComPtr<IDWriteTextLayout> result;
    checked(factory->CreateTextLayout(reinterpret_cast<const wchar_t*>(text.data()),UINT32(text.size()),format.Get(),420*scale,240*scale,&result));
    return result;
}
// Public font-file APIs preserve the exact fallback face and TTC index selected
// by DirectWrite. No dependency on Skia's private DirectWrite typeface classes.
class Renderer final:public IDWriteTextRenderer {
    ULONG refs=1;
    SkCanvas* canvas;
    sk_sp<SkFontMgr> manager=SkFontMgr_New_DirectWrite();
    std::map<IDWriteFontFace*,sk_sp<SkTypeface>> faces;
    std::vector<ComPtr<IDWriteFontFace>> retainedFaces;
    sk_sp<SkTypeface> face(IDWriteFontFace* source) {
        if(auto found=faces.find(source);found!=faces.end()) return found->second;
        UINT32 count=0; checked(source->GetFiles(&count,nullptr)); require(count==1,"Single-file DirectWrite face");
        ComPtr<IDWriteFontFile> file; checked(source->GetFiles(&count,file.GetAddressOf()));
        const void* key=nullptr; UINT32 keySize=0; checked(file->GetReferenceKey(&key,&keySize));
        ComPtr<IDWriteFontFileLoader> loader; checked(file->GetLoader(&loader));
        ComPtr<IDWriteFontFileStream> stream; checked(loader->CreateStreamFromKey(key,keySize,&stream));
        UINT64 size=0; checked(stream->GetFileSize(&size)); require(size<=64*1024*1024,"Font file size limit");
        const void* bytes=nullptr; void* context=nullptr;
        checked(stream->ReadFileFragment(&bytes,0,size,&context));
        auto data=SkData::MakeWithCopy(bytes,size); stream->ReleaseFileFragment(context);
        auto result=manager->makeFromData(data,source->GetIndex()); require(bool(result),"Import exact DirectWrite font file");
        retainedFaces.emplace_back(source);
        faces.emplace(source,result); return result;
    }
public:
    unsigned glyphs=0,runs=0;
    explicit Renderer(SkCanvas* value):canvas(value){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** output) override {
        if(!output)return E_POINTER; *output=nullptr;
        if(id!=__uuidof(IUnknown)&&id!=__uuidof(IDWritePixelSnapping)&&id!=__uuidof(IDWriteTextRenderer))return E_NOINTERFACE;
        *output=this; AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{const auto count=--refs;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* output) override{*output=TRUE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* output) override{*output={1,0,0,1,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* output) override{*output=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN* run,
        const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*) override {
        try {
            require(!run->isSideways,"Vertical text is outside this probe");
            SkFont font(face(run->fontFace),run->fontEmSize); font.setEdging(SkFont::Edging::kAntiAlias);
            const auto simulations=run->fontFace->GetSimulations();
            font.setEmbolden((simulations&DWRITE_FONT_SIMULATIONS_BOLD)!=0);
            if(simulations&DWRITE_FONT_SIMULATIONS_OBLIQUE)font.setSkewX(-.25f);
            std::vector<SkPoint> positions(run->glyphCount); float cursor=0;
            require(run->glyphAdvances!=nullptr,"DirectWrite supplied glyph advances");
            const bool rtl=(run->bidiLevel&1)!=0;
            for(UINT32 i=0;i<run->glyphCount;++i) {
                if(rtl)cursor-=run->glyphAdvances[i];
                const DWRITE_GLYPH_OFFSET offset=run->glyphOffsets?run->glyphOffsets[i]:DWRITE_GLYPH_OFFSET{};
                positions[i]={cursor+(rtl?-1.f:1.f)*offset.advanceOffset,-offset.ascenderOffset};
                if(!rtl)cursor+=run->glyphAdvances[i];
            }
            SkPaint paint; paint.setColor(SK_ColorBLACK);
            canvas->drawGlyphs({run->glyphIndices,run->glyphCount},positions,{x,y},font,paint);
            glyphs+=run->glyphCount; ++runs; return S_OK;
        }catch(...){return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT x,FLOAT y,const DWRITE_UNDERLINE* line,IUnknown*) override {
        SkPaint paint;paint.setColor(SK_ColorBLACK);canvas->drawRect(SkRect::MakeXYWH(x,y+line->offset,line->width,line->thickness),paint);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT x,FLOAT y,const DWRITE_STRIKETHROUGH* line,IUnknown*) override {
        SkPaint paint;paint.setColor(SK_ColorBLACK);canvas->drawRect(SkRect::MakeXYWH(x,y+line->offset,line->width,line->thickness),paint);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override{return E_NOTIMPL;}
};
}
void dwriteScene(SkCanvas* canvas,float scale) {
    canvas->clear(SK_ColorWHITE);auto engine=factory();Renderer renderer(canvas);
    float y=5*scale;
    for(const auto& sample:samples) {
        auto text=layout(engine.Get(),sample,scale);DWRITE_TEXT_METRICS metrics{};checked(text->GetMetrics(&metrics));
        checked(text->Draw(nullptr,&renderer,10*scale,y));y+=metrics.height+8*scale;
    }
    require(renderer.glyphs>0 && renderer.runs>=samples.size(),"DirectWrite shaped glyphs reached Skia");
}
#ifndef SKIA_EVALUATION_DWRITE_ONLY
void evaluateParagraphs(const std::string& directory) {
    size_t rawHitFailures=0;
    auto unicode=SkUnicodes::ICU::Make();require(bool(unicode),"Bundled ICU initialized");
    auto fonts=sk_make_sp<FontCollection>();fonts->setDefaultFontManager(SkFontMgr_New_DirectWrite());
    auto engine=factory();
    for(float scale:{1.f,1.5f,2.f}) {
        auto image=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(int(480*scale),int(520*scale)));
        image->getCanvas()->clear(SK_ColorWHITE);float y=5*scale;
        for(size_t index=0;index<samples.size();++index) {
            const auto& sample=samples[index];
            auto breaks=unicode->makeBreakIterator("en",SkUnicode::BreakType::kGraphemes);
            require(breaks && breaks->setText(sample.data(),int(sample.size())),"UTF16 grapheme iterator");
            std::set<size_t> boundaries;
            for(auto at=breaks->first();!breaks->isDone();at=breaks->next())boundaries.insert(size_t(at));
            boundaries.insert(sample.size());
            if(index==4)require(boundaries==std::set<size_t>{0,2,3,5,6,8,9,13,14,25},"Known combining, skin-tone and family-emoji boundaries");
            TextStyle textStyle;textStyle.setFontFamilies({SkString("Segoe UI")});textStyle.setFontSize(24*scale);textStyle.setColor(SK_ColorBLACK);
            ParagraphStyle style;style.setTextStyle(textStyle);
            auto builder=ParagraphBuilder::make(style,fonts,unicode);require(bool(builder),"Create SkParagraph");
            builder->addText(sample);auto paragraph=builder->Build();paragraph->layout(420*scale);
            std::cout<<"checking sample="<<index<<" dpi="<<96*scale<<" unresolved="<<paragraph->unresolvedGlyphs()<<std::endl;
            require(paragraph->getHeight()>0 && paragraph->lineNumber()>0,"Paragraph layout");
            require(paragraph->unresolvedGlyphs()==0,"System font fallback resolves sample glyphs");
            for(size_t position=0;position<sample.size();++position) {
                Paragraph::GlyphInfo info{};require(paragraph->getGlyphInfoAtUTF16Offset(position,&info),"UTF16 glyph lookup");
                if(!boundaries.contains(info.fGraphemeClusterTextRange.start) || !boundaries.contains(info.fGraphemeClusterTextRange.end))
                    std::cerr<<"invalid grapheme position="<<position<<" range="<<info.fGraphemeClusterTextRange.start<<','<<info.fGraphemeClusterTextRange.end<<'\n';
                require(boundaries.contains(info.fGraphemeClusterTextRange.start) && boundaries.contains(info.fGraphemeClusterTextRange.end),"Grapheme range cannot split combining/emoji sequence");
                const auto hit=paragraph->getGlyphPositionAtCoordinate(info.fGraphemeLayoutBounds.centerX(),info.fGraphemeLayoutBounds.centerY());
                if(!boundaries.contains(size_t(hit.position))) {
                    ++rawHitFailures;
                    std::cerr<<"invalid raw hit sample="<<index<<" dpi="<<96*scale<<" position="<<position<<" hit="<<hit.position<<'\n';
                }
            }
            const auto rectangles=paragraph->getRectsForRange(0,unsigned(sample.size()),RectHeightStyle::kTight,RectWidthStyle::kTight);
            require(!rectangles.empty(),"Full-range selection rectangles");
            if(index==4) {
                require(!paragraph->getRectsForRange(0,2,RectHeightStyle::kTight,RectWidthStyle::kTight).empty(),"Combining-sequence selection");
                require(!paragraph->getRectsForRange(14,25,RectHeightStyle::kTight,RectWidthStyle::kTight).empty(),"Family-emoji selection");
            }
            auto dw=layout(engine.Get(),sample,scale);DWRITE_TEXT_METRICS metrics{};checked(dw->GetMetrics(&metrics));
            std::cout<<"text sample="<<index<<" dpi="<<96*scale<<" graphemes="<<boundaries.size()-1
                <<" dwriteHeight="<<metrics.height<<" skParagraphHeight="<<paragraph->getHeight()
                <<" dwriteLines="<<metrics.lineCount<<" skParagraphLines="<<paragraph->lineNumber()<<'\n';
            paragraph->paint(image->getCanvas(),10*scale,y);y+=paragraph->getHeight()+8*scale;
        }
        save(readback(image.get()),directory+"/paragraph-"+std::to_string(int(96*scale))+".ppm");
    }
    std::cout<<"SkParagraph raw hit violations="<<rawHitFailures<<'\n';
    require(rawHitFailures==0,"SkParagraph raw hit testing splits grapheme clusters; Sunfleck normalization is required");
    std::cout<<"PASS: SkParagraph fallback, UTF16/cluster mapping, hit-testing and selection geometry\n";
}
#endif
