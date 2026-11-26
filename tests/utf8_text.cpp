#include "../src/utf8_text.h"
#include <iostream>
#include <initializer_list>

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Unicode codec assertion failed");
}
template<class F> static void rejects(F action) {
    try { action(); } catch (const std::range_error&) { return; }
    throw std::runtime_error("Malformed Unicode was accepted");
}
int main() {
    try {
        const std::string text = "ASCII \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x8c\xb1 \xf4\x8f\xbf\xbf";
        require(utf8_text::encode(utf8_text::decode(text)) == text);
        require(utf8_text::decode("\xf0\x9f\x8c\xb1").size() == (sizeof(wchar_t) == 2 ? 2u : 1u));
        require(utf8_text::encode(utf8_text::decode(std::string("a\0b", 3))) == std::string("a\0b", 3));
        for (const auto* invalid : {"\xc0\x80", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x9f", "\x80"}) {
            rejects([&] { (void)utf8_text::decode(invalid); });
        }
        rejects([] { (void)utf8_text::encode(std::wstring(1, wchar_t(0xd800))); });
        rejects([] { (void)utf8_text::encode(std::wstring(1, wchar_t(0xdc00))); });
        rejects([] { (void)utf8_text::encode(std::wstring{wchar_t(0xd800), L'A'}); });
        if constexpr (sizeof(wchar_t) > 2) {
            rejects([] { (void)utf8_text::encode(std::wstring(1, wchar_t(0x110000))); });
        }
        std::cout << "Unicode scalar/UTF-16 round trips passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
