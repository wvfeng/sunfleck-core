#include "macos_text.h"
#include "text_format.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <limits>
#include <memory>

namespace {
template<class T> struct Release {
    void operator()(T* value) const { CFRelease(value); }
};
template<class T> using Owned = std::unique_ptr<T, Release<T>>;

NSFont* font(unsigned dpi, unsigned flags)
{
    const auto& format = active_text_format();
    const auto size = format.size * dpi / 96;
    const auto weight = std::clamp((format.weight - 400) / 500., -0.8, 1.);
    auto* family = [NSString stringWithUTF8String:format.family.c_str()];
    auto* manager = NSFontManager.sharedFontManager;
    NSFont* result = nil;
    if (format.family != default_text_format().family) {
        result = [NSFont fontWithName:family size:size];
    }
    if (!result) {
        result = flags & 4 ? [NSFont monospacedSystemFontOfSize:size weight:weight]
                           : [NSFont systemFontOfSize:size weight:weight];
    }
    if (format.weight >= UI_FONT_BOLD || (flags & 1)) {
        result = [manager convertFont:result toHaveTrait:NSBoldFontMask];
    }
    if (format.style != UI_FONT_NORMAL_STYLE || (flags & 2)) {
        result = [manager convertFont:result toHaveTrait:NSItalicFontMask];
    }
    if (format.stretch != UI_FONT_NORMAL_STRETCH) {
        result = [manager convertFont:result toHaveTrait:format.stretch < UI_FONT_NORMAL_STRETCH
                                                        ? NSCondensedFontMask : NSExpandedFontMask];
    }
    return result;
}

int row_height(NSFont* font, unsigned dpi)
{
    const auto& format = active_text_format();
    return std::max(1, static_cast<int>(std::ceil(format.line_height > 0 ? format.line_height * dpi / 96
                                                   : font.ascender - font.descender + font.leading)));
}

struct Line {
    Owned<const __CTLine> shaped;
    NSUInteger start;
    NSUInteger end;
    double width() const { return CTLineGetTypographicBounds(shaped.get(), nullptr, nullptr, nullptr); }
};

struct Paragraph {
    NSString* text;
    NSFont* face;
    int row;
    std::vector<Line> lines;
};

Paragraph shape(const wchar_t* input, unsigned dpi, unsigned flags, int width, bool single_line, bool editing)
{
    static_assert(sizeof(wchar_t) == 4);
    auto* text = [[NSString alloc] initWithBytes:input length:std::wcslen(input) * sizeof(wchar_t)
                                       encoding:NSUTF32LittleEndianStringEncoding] ?: @"";
    auto* face = font(dpi, flags);
    Paragraph result{text, face, row_height(face, dpi), {}};
    const auto& format = active_text_format();
    auto* style = [NSMutableParagraphStyle new];
    style.baseWritingDirection = format.flow == UI_FLOW_RIGHT_TO_LEFT
        ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    NSDictionary* attributes = @{
        NSFontAttributeName: face,
        NSParagraphStyleAttributeName: style,
        NSUnderlineStyleAttributeName: @((format.decorations & UI_TEXT_UNDERLINE) != 0),
        NSStrikethroughStyleAttributeName: @((format.decorations & UI_TEXT_STRIKETHROUGH) != 0),
        NSForegroundColorAttributeName: NSColor.whiteColor
    };
    auto* attributed = [[NSAttributedString alloc] initWithString:text attributes:attributes];
    auto* newline = NSCharacterSet.newlineCharacterSet;
    Owned<const __CTTypesetter> typesetter(CTTypesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attributed));
    const bool wrap = !single_line && format.wrapping == UI_TEXT_WRAP;
    for (NSUInteger start = 0;;) {
        const auto hard = [text rangeOfCharacterFromSet:newline options:0 range:NSMakeRange(start, text.length - start)];
        const NSUInteger end = hard.location == NSNotFound ? text.length : hard.location;
        do {
            NSUInteger count = end - start;
            if (wrap && count) {
                const auto fit = CTTypesetterSuggestLineBreak(typesetter.get(), start, std::max(1, width));
                count = std::min(count, fit > 0 ? NSUInteger(fit) :
                                 [text rangeOfComposedCharacterSequenceAtIndex:start].length);
            }
            auto* line = count ? CTTypesetterCreateLine(typesetter.get(), CFRangeMake(start, count))
                               : nullptr;
            if (!line) {
                auto* empty = [[NSAttributedString alloc] initWithString:@"" attributes:attributes];
                line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)empty);
            }
            Line shaped{Owned<const __CTLine>(line), start, start + count};
            if (!editing && format.trimming != UI_TEXT_TRIM_NONE && width > 0 && shaped.width() > width) {
                auto* ellipsis = [[NSAttributedString alloc] initWithString:@"\u2026" attributes:attributes];
                Owned<const __CTLine> token(CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)ellipsis));
                shaped.shaped.reset(CTLineCreateTruncatedLine(line, width, kCTLineTruncationEnd, token.get()));
                if (!shaped.shaped) {
                    shaped.shaped = std::move(token);
                }
            } else if (wrap && format.alignment == UI_TEXT_ALIGN_JUSTIFY && start + count < end) {
                if (auto* justified = CTLineCreateJustifiedLine(line, 1, width)) {
                    shaped.shaped.reset(justified);
                }
            }
            result.lines.push_back(std::move(shaped));
            start += count;
        } while (start < end);
        if (single_line || hard.location == NSNotFound) {
            break;
        }
        start = end + 1;
        if ([text characterAtIndex:end] == '\r' && start < text.length && [text characterAtIndex:start] == '\n') {
            ++start;
        }
    }
    return result;
}

int origin(const Line& line, layout::Rect bounds)
{
    const auto& format = active_text_format();
    const auto space = std::max(0., bounds.right - bounds.left - line.width());
    return bounds.left + static_cast<int>(format.alignment == UI_TEXT_ALIGN_CENTER ? space / 2 :
                                          format.alignment == UI_TEXT_ALIGN_RIGHT ? space : 0);
}

void fill(std::vector<FillRectCommand>& commands, layout::Rect rect, layout::Rect clip, std::array<uint8_t, 4> color)
{
    rect = {std::max(rect.left, clip.left), std::max(rect.top, clip.top),
            std::min(rect.right, clip.right), std::min(rect.bottom, clip.bottom)};
    if (rect.left < rect.right && rect.top < rect.bottom && color[3]) {
        commands.push_back({rect.left, rect.top, rect.right, rect.bottom, color});
    }
}
}

namespace macos_text {
int width(const wchar_t* text, unsigned dpi, unsigned flags)
{
    @autoreleasepool {
        const auto paragraph = shape(text, dpi, flags, std::numeric_limits<int>::max(), false, false);
        double longest = 0;
        for (const auto& line : paragraph.lines) {
            longest = std::max(longest, line.width());
        }
        return int(std::ceil(longest));
    }
}

int line_height(unsigned dpi, unsigned flags)
{
    @autoreleasepool {
        return row_height(font(dpi, flags), dpi);
    }
}

int height(const wchar_t* text, unsigned dpi, int width, unsigned flags)
{
    @autoreleasepool {
        const auto paragraph = shape(text, dpi, flags, width, false, false);
        return int(paragraph.lines.size()) * paragraph.row;
    }
}

void append(std::vector<FillRectCommand>& commands, layout::Rect bounds, layout::Rect clip,
            std::array<uint8_t, 4> color, unsigned dpi, const wchar_t* text, bool single_line,
            EditorGeometry* editor, unsigned flags)
{
    @autoreleasepool {
        auto paragraph = shape(text, dpi, flags, bounds.right - bounds.left, single_line, editor != nullptr);
        auto* hosted = editor ? editor->hosted : nullptr;
        const int row = paragraph.row;
        int caret_x = bounds.left;
        int caret_y = bounds.top;
        if (editor) {
            editor->stops.clear();
            std::vector<size_t> indices(paragraph.text.length + 1);
            for (size_t index = 0, offset = 0; offset < paragraph.text.length; ++index) {
                indices[offset++] = index;
                if (text[index] > 0xffff) {
                    indices[offset++] = index;
                }
                indices[offset] = index + 1;
            }
            for (size_t i = 0; i < paragraph.lines.size(); ++i) {
                const auto& line = paragraph.lines[i];
                const int x = origin(line, bounds);
                const int y = bounds.top + int(i) * row;
                for (auto offset = line.start;;) {
                    CGFloat secondary = 0;
                    const auto primary = CTLineGetOffsetForStringIndex(line.shaped.get(), offset, &secondary);
                    const auto index = indices[offset];
                    const int column = x + int(std::lround(primary));
                    const int alternative = x + int(std::lround(secondary));
                    editor->stops.push_back({column, y, index});
                    if (alternative != column) {
                        editor->stops.push_back({alternative, y, index, true});
                    }
                    if (index == editor->index) {
                        caret_x = editor->trailing ? alternative : column;
                        caret_y = y;
                    }
                    if (offset == line.end) {
                        break;
                    }
                    offset = std::min(line.end, NSMaxRange([paragraph.text rangeOfComposedCharacterSequenceAtIndex:offset]));
                }
            }
        }
        if (hosted && hosted->reveal) {
            const int width = std::max(1, bounds.right - bounds.left);
            const int height = std::max(1, bounds.bottom - bounds.top);
            hosted->scroll_x = single_line ? std::clamp(hosted->scroll_x,
                std::max(0, caret_x - bounds.left - width + 2), std::max(0, caret_x - bounds.left)) : 0;
            hosted->scroll_y = single_line ? 0 : std::clamp(hosted->scroll_y,
                std::max(0, caret_y + row - bounds.top - height), std::max(0, caret_y - bounds.top));
        }
        const int sx = hosted ? hosted->scroll_x : 0, sy = hosted ? hosted->scroll_y : 0;
        if (editor) {
            for (auto& stop : editor->stops) {
                stop.x -= sx;
                stop.y -= sy;
            }
        }
        clip = {std::max(clip.left, bounds.left), std::max(clip.top, bounds.top),
                std::min(clip.right, bounds.right), std::min(clip.bottom, bounds.bottom)};
        if (hosted) {
            hosted->caret_x = caret_x - sx;
            hosted->caret_y = caret_y - sy;
            for (size_t i = 1; i < editor->stops.size(); ++i) {
                const auto& a = editor->stops[i - 1];
                const auto& b = editor->stops[i];
                if (a.y == b.y && a.index >= hosted->first && b.index <= hosted->last && a.index < b.index) {
                    fill(commands, {std::min(a.x, b.x), a.y, std::max(a.x, b.x), a.y + row}, clip, editor->selection);
                }
            }
        }
        const auto width = clip.right - clip.left;
        const auto height = clip.bottom - clip.top;
        if (width <= 0 || height <= 0) {
            return;
        }
        std::vector<uint8_t> coverage(size_t(width) * height);
        Owned<CGContext> bitmap(CGBitmapContextCreate(coverage.data(), width, height, 8, width, nullptr, kCGImageAlphaOnly));
        if (!bitmap) {
            throw std::bad_alloc{};
        }
        CGContextSetShouldSmoothFonts(bitmap.get(), false);
        for (size_t i = 0; i < paragraph.lines.size(); ++i) {
            const auto& line = paragraph.lines[i];
            const auto y = bounds.top + int(i) * row - sy;
            if (y + row > clip.top && y < clip.bottom) {
                const auto baseline = y + std::ceil(paragraph.face.ascender);
                CGContextSetTextPosition(bitmap.get(), origin(line, bounds) - sx - clip.left, clip.bottom - baseline);
                CTLineDraw(line.shaped.get(), bitmap.get());
            }
        }
        for (int y = 0; y < height; ++y) {
            const auto* pixels = coverage.data() + size_t(y) * width;
            for (int left = 0, right; left < width; left = right) {
                const auto alpha = pixels[left];
                for (right = left + 1; right < width && pixels[right] == alpha; ++right) {}
                if (alpha) {
                    auto ink = color;
                    ink[3] = uint8_t((ink[3] * alpha + 127) / 255);
                    commands.push_back({clip.left + left, clip.top + y, clip.left + right, clip.top + y + 1, ink});
                }
            }
        }
        if (editor && editor->show) {
            fill(commands, {caret_x - sx, caret_y - sy, caret_x - sx + int(std::max(1u, dpi / 96)), caret_y - sy + row},
                 clip, color);
        }
    }
}
}
