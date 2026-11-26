#include "font_resources.h"
#ifdef _WIN32
#define NOMINMAX
#include <dwrite_3.h>
#include <wrl/client.h>
#include "utf8_text.h"
#include <algorithm>
#include <stdexcept>

namespace windows_text {
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT status) {
    if (status==E_OUTOFMEMORY) throw std::bad_alloc();
    if (FAILED(status)) throw std::runtime_error("DirectWrite font resource operation failed");
}
struct MemoryFont {
    uint64_t id=0;
    size_t size=0;
    ComPtr<IDWriteFactory5> factory;
    ComPtr<IDWriteInMemoryFontFileLoader> loader;
    ComPtr<IDWriteFontFile> file;
    std::vector<std::string> families;
    bool registered=false;
    ~MemoryFont() {
        file.Reset();
        if (registered) factory->UnregisterFontFileLoader(loader.Get());
    }
};
ComPtr<IDWriteFontCollection1> make_collection(IDWriteFactory5* factory,const std::vector<std::shared_ptr<MemoryFont>>& fonts) {
    ComPtr<IDWriteFontSetBuilder1> builder;
    checked(factory->CreateFontSetBuilder(&builder));
    for (const auto& font:fonts) checked(builder->AddFontFile(font->file.Get()));
    ComPtr<IDWriteFontSet> set;
    checked(builder->CreateFontSet(&set));
    ComPtr<IDWriteFontCollection1> result;
    checked(factory->CreateFontCollectionFromFontSet(set.Get(),&result));
    return result;
}
std::vector<std::string> family_names(IDWriteFontCollection* collection) {
    std::vector<std::string> result;
    for (UINT32 i=0;i<collection->GetFontFamilyCount();++i) {
        ComPtr<IDWriteFontFamily> family;
        checked(collection->GetFontFamily(i,&family));
        ComPtr<IDWriteLocalizedStrings> names;
        checked(family->GetFamilyNames(&names));
        UINT32 index=0,length=0; BOOL exists=FALSE;
        checked(names->FindLocaleName(L"en-us",&index,&exists));
        if (!exists) index=0;
        checked(names->GetStringLength(index,&length));
        std::wstring name(length+1,L'\0');
        checked(names->GetString(index,name.data(),length+1));
        name.resize(length); result.push_back(utf8_text::encode(name));
    }
    return result;
}
}
struct FontCollection {
    // Release the collection before unregistering any loaders referenced by it.
    std::vector<std::shared_ptr<MemoryFont>> fonts;
    ComPtr<IDWriteFontCollection1> collection;
};
FontResources register_font(FontResources previous,const void* bytes,size_t size,uint64_t id) {
    if (!bytes || !size || size>64*1024*1024 || !id) throw std::range_error("Invalid font bytes");
    size_t total=size;
    if (previous) {
        if (previous->fonts.size()>=32) throw std::range_error("Too many application fonts");
        for (const auto& font:previous->fonts) total+=font->size;
    }
    if (total>128*1024*1024) throw std::range_error("Application font memory limit");
    auto font=std::make_shared<MemoryFont>(); font->id=id; font->size=size;
    checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory5),reinterpret_cast<IUnknown**>(font->factory.GetAddressOf())));
    checked(font->factory->CreateInMemoryFontFileLoader(&font->loader));
    checked(font->factory->RegisterFontFileLoader(font->loader.Get())); font->registered=true;
    checked(font->loader->CreateInMemoryFontFileReference(font->factory.Get(),bytes,UINT32(size),nullptr,&font->file));
    BOOL supported=FALSE; DWRITE_FONT_FILE_TYPE file_type{}; DWRITE_FONT_FACE_TYPE face_type{}; UINT32 faces=0;
    if (FAILED(font->file->Analyze(&supported,&file_type,&face_type,&faces)) || !supported || !faces)
        throw std::range_error("Invalid or unsupported font file");
    font->families=family_names(make_collection(font->factory.Get(),{font}).Get());
    if (font->families.empty()) throw std::range_error("Font has no families");
    auto result=std::make_shared<FontCollection>();
    if (previous) result->fonts=previous->fonts;
    result->fonts.push_back(font);
    result->collection=make_collection(font->factory.Get(),result->fonts);
    return result;
}
const std::vector<std::string>& font_families(const FontResources& resources,uint64_t id) {
    if (resources) for (const auto& font:resources->fonts) if (font->id==id) return font->families;
    throw std::range_error("Unknown application font");
}
FontResources unregister_font(FontResources previous,uint64_t id) {
    (void)font_families(previous,id);
    auto result=std::make_shared<FontCollection>();
    for (const auto& font:previous->fonts) if (font->id!=id) result->fonts.push_back(font);
    if (result->fonts.empty()) return {};
    result->collection=make_collection(result->fonts.front()->factory.Get(),result->fonts);
    return result;
}
IDWriteFontCollection* collection_for(const FontResources& resources,const wchar_t* family) {
    if (!resources) return nullptr;
    UINT32 index=0; BOOL exists=FALSE;
    checked(resources->collection->FindFamilyName(family,&index,&exists));
    return exists ? resources->collection.Get() : nullptr;
}
}
#endif
