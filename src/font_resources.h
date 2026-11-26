#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ui_app;
struct IDWriteFontCollection;
namespace windows_text {
struct FontCollection;
using FontResources = std::shared_ptr<const FontCollection>;
FontResources app_fonts(const ui_app* app);
#ifdef _WIN32
FontResources register_font(FontResources previous,const void* bytes,size_t size,uint64_t id);
FontResources unregister_font(FontResources previous,uint64_t id);
const std::vector<std::string>& font_families(const FontResources& resources,uint64_t id);
IDWriteFontCollection* collection_for(const FontResources& resources,const wchar_t* family);
#endif
}
