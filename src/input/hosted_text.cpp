#include "hosted_text.h"
#include "../ui_tree.h"
#include <stdexcept>
#include <limits>

namespace hosted_text {
namespace {
uint32_t next(std::wstring_view text, size_t& offset)
{
    uint32_t value = static_cast<uint32_t>(text.at(offset++));
    if constexpr (sizeof(wchar_t) == 2) {
        if (value >= 0xd800 && value <= 0xdbff) {
            if (offset >= text.size()) { throw std::range_error("Unpaired surrogate"); }
            const uint32_t low = text[offset++];
            if (low < 0xdc00 || low > 0xdfff) { throw std::range_error("Unpaired surrogate"); }
            value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
        }
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) || value == 0) {
        throw std::range_error("Invalid scalar");
    }
    return value;
}
void advance(uint32_t& value) { if (++value == 0) { ++value; } }
}
std::wstring decode(std::string_view bytes)
{
    std::wstring result;
    for (size_t i = 0; i < bytes.size();) {
        uint32_t value = static_cast<unsigned char>(bytes[i++]);
        unsigned tails = 0;
        if (value >= 0x80) {
            if (value < 0xc2 || value > 0xf4) { throw std::range_error("Invalid UTF-8"); }
            tails = value < 0xe0 ? 1 : value < 0xf0 ? 2 : 3;
            value &= 0x7f >> (tails + 1);
            for (unsigned j = 0; j < tails; ++j) {
                if (i >= bytes.size() || (static_cast<unsigned char>(bytes[i]) & 0xc0) != 0x80) { throw std::range_error("Invalid UTF-8"); }
                value = (value << 6) | (static_cast<unsigned char>(bytes[i++]) & 0x3f);
            }
        }
        constexpr uint32_t minimum[]{0, 0x80, 0x800, 0x10000};
        if (value < minimum[tails] || !value || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) { throw std::range_error("Invalid scalar"); }
        if constexpr (sizeof(wchar_t) == 2) {
            if (value > 0xffff) {
                result.push_back(static_cast<wchar_t>(0xd800 + ((value - 0x10000) >> 10)));
                result.push_back(static_cast<wchar_t>(0xdc00 + ((value - 0x10000) & 0x3ff)));
                continue;
            }
        }
        result.push_back(static_cast<wchar_t>(value));
    }
    return result;
}
std::string encode(std::wstring_view text)
{
    std::string result;
    for (size_t i = 0; i < text.size();) {
        const auto value = next(text, i);
        if (value < 0x80) { result.push_back(static_cast<char>(value)); continue; }
        const unsigned tails = value < 0x800 ? 1 : value < 0x10000 ? 2 : 3;
        constexpr unsigned prefixes[]{0, 0xc0, 0xe0, 0xf0};
        result.push_back(static_cast<char>(prefixes[tails] | (value >> (tails * 6))));
        for (int shift = (int(tails) - 1) * 6; shift >= 0; shift -= 6) { result.push_back(static_cast<char>(0x80 | ((value >> shift) & 0x3f))); }
    }
    return result;
}
uint32_t utf16_offset(std::wstring_view text, size_t target)
{
    uint32_t result = 0;
    size_t i = 0;
    while (i < target && i < text.size()) { result += next(text, i) > 0xffff ? 2 : 1; }
    if (i != target) { throw std::range_error("Invalid native boundary"); }
    return result;
}
size_t native_offset(std::wstring_view text, uint32_t target)
{
    uint32_t count = 0;
    size_t i = 0;
    while (count < target && i < text.size()) { count += next(text, i) > 0xffff ? 2 : 1; }
    if (count != target) { throw std::range_error("Invalid UTF-16 boundary"); }
    return i;
}
void Session::observe(ui_node* node)
{
    if (!node) { id = 0; node_id = 0; baseline.clear(); utf8.clear(); return; }
    const auto [selection_first,selection_last]=node->selection();
    const auto first=utf16_offset(node->text,selection_first),last=utf16_offset(node->text,selection_last);
    const uint32_t reverse=selection_first!=selection_last && node->caret==selection_first;
    if (node_id != node->debug_id) {
        advance(serial); id = serial; node_id = node->debug_id; revision = 0;
    } else if (baseline == node->text && caret == node->caret && select_all == node->select_all &&
        start==first && end==last && backward==reverse) { return; }
    auto encoded = encode(node->text);
    baseline = node->text; utf8 = std::move(encoded); caret = node->caret; select_all = node->select_all;
    start = first; end = last; backward = reverse; advance(revision);
    const auto scroll_x=node->hosted_editor.scroll_x,scroll_y=node->hosted_editor.scroll_y;
    node->hosted_editor = {};
    auto& view = node->hosted_editor;
    view.active = true; view.first = selection_first; view.last = selection_last;
    view.caret = node->caret; view.scroll_x=scroll_x; view.scroll_y=scroll_y;

}
void Session::accept(ui_node& node, uint32_t first, uint32_t last, uint32_t reverse)
{
    const auto before = revision;
    const auto scroll_x = node.hosted_editor.scroll_x, scroll_y = node.hosted_editor.scroll_y;
    const bool selection_changed = start != first || end != last || backward != reverse;
    observe(&node);
    if (before == revision && selection_changed) { advance(revision); }
    start = first; end = last; backward = reverse;
    auto& view = node.hosted_editor;
    view.active = true; view.composing = false; view.draft.clear(); view.reveal = true;
    view.scroll_x = scroll_x; view.scroll_y = scroll_y;
    view.first = native_offset(node.text, first); view.last = native_offset(node.text, last);
    view.caret = reverse ? view.first : view.last;
}
}
