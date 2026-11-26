#include "../src/metal_surface.h"
#include "../src/raster.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cmath>
#include <cstdio>
#include <cstring>

static int pixel_test()
{
#ifdef SUNFLECK_TEST_METAL
    if (!metal_device_available()) {
        std::puts("No Metal device");
        return 77;
    }
    constexpr int width = 37, height = 23;
    const FillRectCommand frame[] = {
        {-4, 2, 35, 20, {17, 114, 198, 255}},
        {9, 4, 43, 11, {253, 75, 11, 140}},
        {12, 9, 24, 19, {18, 239, 11, 199}, true},
        {7, 8, 15, 17, {211, 18, 251, 59}},
        {32, 17, 36, 19, {246, 44, 26, 255}},
        {8, 12, 8, 13, {255, 255, 255, 255}}
    };
    std::vector<uint8_t> expected, actual;
    rasterize(expected, width, height, frame, PixelOrder::Bgra);
    if (!metal_readback(width, height, frame, actual) || actual.size() != expected.size()) {
        return 1;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (std::abs(int(actual[i]) - expected[i]) > 1) {
            std::fprintf(stderr, "Pixel (%zu, %zu), channel %zu: got %u, expected %u\n",
                         (i / 4) % width, i / (4 * width), i % 4, actual[i], expected[i]);
            return 1;
        }
    }
    return 0;
#else
    return 77;
#endif
}

static bool window_test(ui_renderer renderer, bool gpu)
{
    ui_app* raw = nullptr;
    const ui_app_options options{sizeof(options), UI_API_VERSION, renderer, UI_SHOW_RENDERER};
    if (ui_app_create(&options, &raw) != UI_OK) {
        return false;
    }
    std::unique_ptr<ui_app, decltype(&ui_app_destroy)> app(raw, ui_app_destroy);
    ui_node* text = nullptr;
    if (ui_app_set_window_options(raw, "Sunfleck macOS test", 360, 240) != UI_OK ||
        ui_node_create(raw, UI_TEXT_BOX, &text) != UI_OK ||
        ui_node_set_text(text, "Metal / 中文") != UI_OK || ui_app_set_root(raw, text) != UI_OK ||
        ui_app_open_window(raw) != UI_OK || ui_app_poll(raw, 0) != UI_OK) {
        return false;
    }
    NSWindow* window = nil;
    for (NSWindow* candidate in NSApp.windows) {
        if (candidate.visible && [candidate.title hasPrefix:@"Sunfleck macOS test"]) {
            window = candidate;
            break;
        }
    }
    const auto suffix = gpu ? @" [GPU]" : @" [CPU]";
    if (!window.visible || ![window.title hasSuffix:suffix]) {
        std::fprintf(stderr, "Missing visible window with %s title suffix\n", suffix.UTF8String);
        return false;
    }
    const auto verify_surface = [&] {
        const auto size = [window.contentView convertSizeToBacking:window.contentView.bounds.size];
        CALayer* base = window.contentView.layer;
        if (gpu) {
            for (CALayer* sublayer in base.sublayers) {
                if ([sublayer isKindOfClass:CAMetalLayer.class]) {
                    const auto drawable = ((CAMetalLayer*)sublayer).drawableSize;
                    if (CGSizeEqualToSize(drawable, size)) {
                        return true;
                    }
                    std::fprintf(stderr, "Drawable size %.0f x %.0f, expected %.0f x %.0f\n",
                                 drawable.width, drawable.height, size.width, size.height);
                }
            }
            return false;
        }
        auto image = (__bridge CGImageRef)base.contents;
        return image && CGImageGetWidth(image) == size.width && CGImageGetHeight(image) == size.height;
    };
    if (!verify_surface()) {
        std::fputs("Missing initial window surface\n", stderr);
        return false;
    }
    [window setContentSize:NSMakeSize(485, 321)];
    if (ui_app_poll(raw, 0) != UI_OK || !verify_surface()) {
        std::fputs("Window resize did not update the surface\n", stderr);
        return false;
    }
    [window makeKeyWindow];
    const auto send_key = [&](NSString* input, NSEventModifierFlags flags, unsigned short code) {
        auto* event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:flags
            timestamp:0 windowNumber:window.windowNumber context:nil characters:input
            charactersIgnoringModifiers:input isARepeat:NO keyCode:code];
        [window sendEvent:event];
    };
    send_key(@"\t", 0, 48);
    send_key(@"a", NSEventModifierFlagCommand, 0);
    send_key(@"m", 0, 46);
    char value[64]{};
    uint32_t capacity = sizeof(value);
    if (ui_node_get_text(text, value, &capacity) != UI_OK || std::strcmp(value, "m") != 0) {
        std::fprintf(stderr, "Keyboard editing produced '%s'\n", value);
        return false;
    }
    [window performClose:nil];
    return ui_app_poll(raw, 0) == UI_EXITED && !window.visible;
}

int main(int argc, char** argv)
{
    @autoreleasepool {
        if (argc == 2 && std::strcmp(argv[1], "--pixels") == 0) {
            return pixel_test();
        }
        ui_app* app = nullptr;
        const ui_app_options headless{sizeof(headless), UI_API_VERSION, UI_METAL, UI_HEADLESS};
        if (ui_app_create(&headless, &app) != UI_ERROR_UNSUPPORTED ||
            !window_test(UI_SOFTWARE, false)) {
            std::fputs("AppKit software window failed\n", stderr);
            return 1;
        }
#ifdef SUNFLECK_TEST_METAL
        const bool gpu = metal_device_available();
        if (gpu && !window_test(UI_METAL, true)) {
            std::fputs("AppKit Metal window failed\n", stderr);
            return 1;
        }
#else
        const bool gpu = false;
        const ui_app_options strict{sizeof(strict), UI_API_VERSION, UI_METAL, 0};
        if (ui_app_create(&strict, &app) != UI_ERROR_UNSUPPORTED) {
            return 1;
        }
#endif
        if (!window_test(UI_AUTO, gpu)) {
            std::fputs("AppKit automatic renderer failed\n", stderr);
            return 1;
        }
        return 0;
    }
}
