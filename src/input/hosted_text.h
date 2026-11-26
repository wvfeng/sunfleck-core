#pragma once
#include <cstdint>
#include <string>
#include <string_view>

struct ui_node;
namespace hosted_text {
// DOM offsets count UTF-16 code units; native strings use wchar_t (16 or 32 bits).
std::wstring decode(std::string_view utf8);
std::string encode(std::wstring_view text);
uint32_t utf16_offset(std::wstring_view text, size_t native_offset);
size_t native_offset(std::wstring_view text, uint32_t utf16_offset);

struct Session {
    uint32_t serial = 0, id = 0, revision = 0;
    uint64_t node_id = 0;
    std::wstring baseline;
    std::string utf8;
    size_t caret = 0;
    bool select_all = false;
    uint32_t start = 0, end = 0, backward = 0;

    void observe(ui_node* node);
    void accept(ui_node& node, uint32_t first, uint32_t last, uint32_t reverse);
};
}
