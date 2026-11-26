#include "macos_window.h"
#ifdef SUNFLECK_METAL
#include "metal_surface.h"
#endif

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <cmath>

using macos::Event;

namespace {
unsigned modifiers(NSEvent* event)
{
    const auto flags = event.modifierFlags;
    return ((flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) ? UI_KEY_CTRL : 0) |
           ((flags & NSEventModifierFlagShift) ? UI_KEY_SHIFT : 0) |
           ((flags & NSEventModifierFlagOption) ? UI_KEY_ALT : 0);
}

int key_code(NSString* characters)
{
    if (!characters.length) {
        return 0;
    }
    const auto key = [characters characterAtIndex:0];
    switch (key) {
    case NSLeftArrowFunctionKey: return macos::left;
    case NSRightArrowFunctionKey: return macos::right;
    case NSUpArrowFunctionKey: return macos::up;
    case NSDownArrowFunctionKey: return macos::down;
    case NSHomeFunctionKey: return macos::home;
    case NSEndFunctionKey: return macos::end;
    case NSDeleteFunctionKey: return macos::forward_delete;
    case 0x19: return '\t'; // Backtab
    default:
        if (key >= NSF1FunctionKey && key <= NSF24FunctionKey) {
            return UI_KEY_F1 + key - NSF1FunctionKey;
        }
        return key < 128 ? key : 0;
    }
}
}

@interface SunfleckView : NSView<NSWindowDelegate> {
@public
    macos::Window::Callback callback;
    void* context;
    NSTrackingArea* tracking;
    double scrollX;
    double scrollY;
}
- (bool)emit:(const Event&)event output:(std::string*)output;
@end

@implementation SunfleckView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { return YES; }

- (bool)emit:(const Event&)event output:(std::string*)output
{
    return callback && callback(context, event, output);
}

- (void)reportSize
{
    const auto size = [self convertSizeToBacking:self.bounds.size];
    [self emit:{Event::resize, int(std::lround(size.width)), int(std::lround(size.height)),
                int(std::lround(self.window.backingScaleFactor * 96))} output:nullptr];
}

- (void)windowDidResize:(NSNotification*)notification { [self reportSize]; }
- (void)windowDidChangeBackingProperties:(NSNotification*)notification { [self reportSize]; }
- (void)windowDidDeminiaturize:(NSNotification*)notification { [self reportSize]; }
- (void)windowDidBecomeKey:(NSNotification*)notification { [self emit:{Event::focus, 0, 0, 1} output:nullptr]; }
- (void)windowDidResignKey:(NSNotification*)notification { [self emit:{Event::focus} output:nullptr]; }
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    [self emit:{Event::close} output:nullptr];
    return NO;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if (tracking) {
        [self removeTrackingArea:tracking];
    }
    tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect
        options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
                NSTrackingInVisibleRect owner:self userInfo:nil];
    [self addTrackingArea:tracking];
}

- (Event)pointerEvent:(NSEvent*)event kind:(Event::Kind)kind value:(int)value
{
    const auto point = [self convertPointToBacking:[self convertPoint:event.locationInWindow fromView:nil]];
    return {kind, int(std::floor(point.x)), int(std::floor(point.y)), value, modifiers(event)};
}

- (void)mouseDown:(NSEvent*)event { [self emit:[self pointerEvent:event kind:Event::pointer value:UI_POINTER_DOWN] output:nullptr]; }
- (void)mouseUp:(NSEvent*)event { [self emit:[self pointerEvent:event kind:Event::pointer value:UI_POINTER_UP] output:nullptr]; }
- (void)mouseMoved:(NSEvent*)event { [self emit:[self pointerEvent:event kind:Event::pointer value:UI_POINTER_MOVE] output:nullptr]; }
- (void)mouseDragged:(NSEvent*)event { [self mouseMoved:event]; }
- (void)mouseEntered:(NSEvent*)event { [self mouseMoved:event]; }
- (void)mouseExited:(NSEvent*)event { [self emit:{Event::pointer, -1, -1, UI_POINTER_MOVE} output:nullptr]; }
- (void)rightMouseDown:(NSEvent*)event { [self emit:[self pointerEvent:event kind:Event::context_menu value:0] output:nullptr]; }
- (void)scrollWheel:(NSEvent*)event
{
    const auto scale = self.window.backingScaleFactor * (event.hasPreciseScrollingDeltas ? 1 : 24);
    scrollX -= event.scrollingDeltaX * scale;
    scrollY -= event.scrollingDeltaY * scale;
    auto location = [self pointerEvent:event kind:Event::scroll value:int(scrollY)];
    location.modifiers = 0;
    if (location.value) {
        scrollY -= location.value;
        [self emit:location output:nullptr];
    }
    location.value = int(scrollX);
    if (location.value) {
        scrollX -= location.value;
        location.modifiers = UI_KEY_SHIFT;
        [self emit:location output:nullptr];
    }
}

- (bool)handleKey:(NSEvent*)event
{
    const auto key = key_code(event.charactersIgnoringModifiers);
    if ([self emit:{Event::key, 0, 0, key, modifiers(event)} output:nullptr]) {
        return true;
    }
    if (!(event.modifierFlags & NSEventModifierFlagCommand)) {
        return false;
    }
    auto* clipboard = NSPasteboard.generalPasteboard;
    switch (key) {
    case 'c': case 'x': {
        std::string output;
        if (![self emit:{key == 'c' ? Event::copy : Event::cut} output:&output]) {
            return false;
        }
        if (auto* string = [NSString stringWithUTF8String:output.c_str()]) {
            [clipboard clearContents];
            [clipboard setString:string forType:NSPasteboardTypeString];
        }
        return true;
    }
    case 'v':
        [self insertText:[clipboard stringForType:NSPasteboardTypeString]];
        return true;
    default:
        return false;
    }
}

- (BOOL)performKeyEquivalent:(NSEvent*)event
{
    return (self.window.firstResponder == self && (event.modifierFlags & NSEventModifierFlagCommand) &&
            [self handleKey:event]) || [super performKeyEquivalent:event];
}

- (void)keyDown:(NSEvent*)event
{
    if ([self handleKey:event]) {
        return;
    }
    if (event.modifierFlags & NSEventModifierFlagCommand) {
        [super keyDown:event];
    } else {
        [self interpretKeyEvents:@[event]];
    }
}

- (void)insertText:(id)value
{
    NSString* string = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
    if (const char* text = string.UTF8String) {
        [self emit:{Event::text, 0, 0, 0, 0, text} output:nullptr];
    }
}

@end

namespace macos {
struct Window::State {
    NSWindow* window;
    SunfleckView* view;
};

Window::Window() : state_(std::make_unique<State>()) {}

Window::~Window()
{
    @autoreleasepool {
        if (state_->view) {
            state_->view->callback = nullptr;
        }
        state_->window.delegate = nil;
        [state_->window close];
    }
}

bool Window::open(const char* title, int width, int height, void* context, Callback callback)
{
    @autoreleasepool {
        if (!NSThread.isMainThread) {
            return false;
        }
        static bool launched = false;
        if (!launched) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
            launched = true;
        }
        const auto rect = NSMakeRect(0, 0, width, height);
        auto* window = [[NSWindow alloc] initWithContentRect:rect
                         styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                   NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                           backing:NSBackingStoreBuffered defer:NO];
        if (!window) {
            return false;
        }
        auto* view = [[SunfleckView alloc] initWithFrame:rect];
        view.wantsLayer = YES;
        view->callback = callback;
        view->context = context;
        window.releasedWhenClosed = NO;
        window.contentView = view;
        window.delegate = view;
        window.acceptsMouseMovedEvents = YES;
        window.title = [NSString stringWithUTF8String:title] ?: @"Sunfleck";
        [window center];
        state_->window = window;
        state_->view = view;
        [window makeFirstResponder:view];
        return true;
    }
}

void Window::show()
{
    @autoreleasepool {
        [state_->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [state_->view reportSize];
    }
}

void Window::title(const char* title)
{
    @autoreleasepool {
        state_->window.title = [NSString stringWithUTF8String:title] ?: @"Sunfleck";
    }
}

bool Window::visible() const { return state_->window.visible && !state_->window.miniaturized; }

void Window::move(int x, int y)
{
    [state_->window setFrameTopLeftPoint:NSMakePoint(x, NSMaxY(NSScreen.screens.firstObject.frame) - y)];
}

ui_rect Window::bounds() const
{
    const auto frame = state_->window.frame;
    const auto client = state_->view.bounds.size;
    return {int(std::lround(frame.origin.x)), int(std::lround(NSMaxY(NSScreen.screens.firstObject.frame) - NSMaxY(frame))),
            int(std::lround(client.width)), int(std::lround(client.height))};
}

void Window::convert_point(double& x, double& y, bool to_screen) const
{
    auto* view = state_->view;
    auto* window = state_->window;
    const auto top = NSMaxY(NSScreen.screens.firstObject.frame);
    NSPoint point;
    if (to_screen) {
        point = [view convertPointFromBacking:NSMakePoint(x, y)];
        point = [window convertPointToScreen:[view convertPoint:point toView:nil]];
        point.y = top - point.y;
    } else {
        point = [window convertPointFromScreen:NSMakePoint(x, top - y)];
        point = [view convertPointToBacking:[view convertPoint:point fromView:nil]];
    }
    x = point.x;
    y = point.y;
}

void Window::present(int width, int height, std::span<const uint8_t> pixels)
{
    @autoreleasepool {
        auto data = CFDataCreate(kCFAllocatorDefault, pixels.data(), pixels.size());
        if (!data) {
            throw std::bad_alloc{};
        }
        auto provider = CGDataProviderCreateWithCFData(data);
        CFRelease(data);
        auto space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        auto image = provider && space ? CGImageCreate(width, height, 8, 32, size_t(width) * 4, space,
                        CGBitmapInfo(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big,
                        provider, nullptr, false, kCGRenderingIntentDefault) : nullptr;
        if (space) {
            CGColorSpaceRelease(space);
        }
        if (provider) {
            CGDataProviderRelease(provider);
        }
        if (!image) {
            throw std::bad_alloc{};
        }
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        state_->view.layer.contents = (__bridge id)image;
        [CATransaction commit];
        CGImageRelease(image);
    }
}

std::unique_ptr<GpuSurface> Window::metal()
{
#ifdef SUNFLECK_METAL
    return open_metal_surface(state_->view.layer);
#else
    return nullptr;
#endif
}

bool Window::poll(unsigned timeout_ms)
{
    @autoreleasepool {
        auto* until = timeout_ms ? [NSDate dateWithTimeIntervalSinceNow:timeout_ms / 1000.0] : NSDate.distantPast;
        bool handled = false;
        for (unsigned count = 0; count < 256; ++count) {
            auto* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
                                               inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!event) {
                break;
            }
            [NSApp sendEvent:event];
            handled = true;
            until = NSDate.distantPast;
        }
        [NSApp updateWindows];
        return handled;
    }
}
}
