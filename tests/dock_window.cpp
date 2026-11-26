#include <sunfleck/dock.hpp>
#include "../src/dock.h"
#include "../src/ui_tree.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "x11_test_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#endif

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Dock desktop check failed at line %d: %s\n", __LINE__, #value); return 1; } } while (false)

namespace {
#ifdef _WIN32
using NativeWindow = HWND;
HWND named(const char* title)
{
    return FindWindowA("SunfleckPreview", title);
}
void close_window(NativeWindow window) { SendMessageW(window, WM_CLOSE, 0, 0); }
void click_editor(NativeWindow window)
{
    SetFocus(window);
    const int scale = static_cast<int>(GetDpiForWindow(window));
    const LPARAM point = MAKELPARAM(30 * scale / 96, 110 * scale / 96);
    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, point); SendMessageW(window, WM_LBUTTONUP, 0, point);
    SendMessageW(window, WM_CHAR, L'Z', 0);
    SendMessageW(window, WM_IME_STARTCOMPOSITION, 0, 0);
    SendMessageW(window, WM_IME_CHAR, 0x4e2d, 0);
    SendMessageW(window, WM_IME_ENDCOMPOSITION, 0, 0);
}
void pointer(NativeWindow window, ui_pointer_action action, int x, int y)
{
    const UINT message = action == UI_POINTER_DOWN ? WM_LBUTTONDOWN : action == UI_POINTER_MOVE ? WM_MOUSEMOVE : action == UI_POINTER_UP ? WM_LBUTTONUP : WM_CANCELMODE;
    SendMessageW(window, message, action == UI_POINTER_UP ? 0 : MK_LBUTTON, MAKELPARAM(x, y));
}
bool exists(NativeWindow window) { return IsWindow(window) != 0; }
bool above(NativeWindow first, NativeWindow second)
{
    for (auto window = GetTopWindow(nullptr); window; window = GetWindow(window, GW_HWNDNEXT)) {
        if (window == first) { return true; }
        if (window == second) { return false; }
    }
    return false;
}
#else
Display* driver = nullptr;
void* xtst = nullptr;
using NativeWindow = Window;
Window named(const char* title)
{
    return x11_test::find_window(driver, title);
}
void close_window(NativeWindow window)
{
    XEvent event{}; event.xclient.type = ClientMessage; event.xclient.window = window;
    event.xclient.message_type = XInternAtom(driver, "WM_PROTOCOLS", False); event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(XInternAtom(driver, "WM_DELETE_WINDOW", False));
    XSendEvent(driver, window, False, NoEventMask, &event); XSync(driver, False);
}
void pointer(NativeWindow window, ui_pointer_action action, int x, int y)
{
    XEvent event{};
    if (action == UI_POINTER_MOVE) {
        event.xmotion.type = MotionNotify; event.xmotion.window = window; event.xmotion.x = x; event.xmotion.y = y;
        XSendEvent(driver, window, False, PointerMotionMask, &event);
    } else {
        event.xbutton.type = action == UI_POINTER_DOWN ? ButtonPress : ButtonRelease;
        event.xbutton.window = window; event.xbutton.button = Button1; event.xbutton.x = x; event.xbutton.y = y;
        XSendEvent(driver, window, False, action == UI_POINTER_DOWN ? ButtonPressMask : ButtonReleaseMask, &event);
    }
    XFlush(driver);
}
void click_editor(NativeWindow window)
{
    XSetInputFocus(driver, window, RevertToParent, CurrentTime);
    pointer(window, UI_POINTER_DOWN, 30, 110); pointer(window, UI_POINTER_UP, 30, 110);
    XEvent event{}; event.xkey.type = KeyPress; event.xkey.window = window; event.xkey.display = driver;
    event.xkey.root = DefaultRootWindow(driver); event.xkey.keycode = XKeysymToKeycode(driver, XK_Z); event.xkey.state = ShiftMask;
    XSendEvent(driver, window, False, KeyPressMask, &event); XFlush(driver);
}
bool exists(NativeWindow window)
{
    return x11_test::find_window(driver, nullptr, window) != 0;
}
#endif
}

int main(int argc, char** argv)
{
#ifndef SUNFLECK_DOCK_GPU
#define SUNFLECK_DOCK_GPU UI_AUTO
#endif
    const ui_renderer renderer = argc > 1 && std::strcmp(argv[1], "gles3") == 0 ? UI_GLES3 :
        argc > 1 && std::strcmp(argv[1], "gpu") == 0 ? SUNFLECK_DOCK_GPU : UI_SOFTWARE;
#ifndef _WIN32
    // This test asserts literal editor input while exercising native Dock moves.
    if (setenv("XMODIFIERS", "@im=none", 1) != 0) { return 1; }
    driver = XOpenDisplay(nullptr); CHECK(driver);
#endif
    try {
        sunfleck::App app(renderer);
        app.setWindowOptions("Dock desktop test", 900, 640);
        auto host = app.createNode(UI_DOCK_HOST); app.setRoot(host); sunfleck::Dock dock(host);
        auto content = app.createNode(UI_TEXT_AREA); auto source = app.createTextSource("Before");
        sunfleck::detail::check(ui_node_bind_text(content.nativeHandle(), source.nativeHandle(), UI_BIND_TWO_WAY));
        auto editor = dock.add("editor", "Editor", content);
        auto peer = dock.add("peer", "Peer", app.createNode(UI_TEXT_AREA));
        auto tool = dock.add("tool", "Tool", app.createNode(UI_TEXT_AREA), UI_DOCK_TOOL_KIND);
        app.openWindow();
        const auto pump = [&] { for (unsigned i = 0; i < 12; ++i) { (void)app.poll(5); } };
        pump(); const auto main = named("Dock desktop test"); CHECK(main);
#ifdef _WIN32
        {
            const auto original_layout = dock.save();
            tool.move(&editor, UI_DOCK_POSITION_LEFT);
            pump();
            const auto& hits = dock.nativeHandle()->views.at(0).hits;
            const auto divider = std::find_if(hits.begin(), hits.end(), [](const auto& hit) {
                return hit.part == UI_DOCK_PART_SPLITTER;
            });
            CHECK(divider != hits.end());
            const int x = (divider->bounds.left + divider->bounds.right) / 2;
            const int y = (divider->bounds.top + divider->bounds.bottom) / 2;
            const auto layout_before_pick = dock.save();
            app.setDebugPicking(true);
            pointer(main, UI_POINTER_MOVE, x, y);
            pointer(main, UI_POINTER_DOWN, x, y);
            pointer(main, UI_POINTER_UP, x, y);
            pump();
            CHECK(app.debugSnapshot().find("\"selection\":\"" + std::to_string(host.debugId()) + "\"") != std::string::npos);
            CHECK(dock.save() == layout_before_pick);
            dock.restore(original_layout);
            pump();
        }
        // Exercise ordinary stacking; making the main window topmost would mask
        // the owned-window bug and change the floating window's stacking band.
        SetWindowPos(main, HWND_TOP, 16, 16, 0, 0, SWP_NOSIZE);
        const auto hit_at = [&](int x, int y) {
            POINT point{x, y}; ClientToScreen(main, &point);
            return SendMessageW(main, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));
        };
        const auto frame = docking::window_frame(dock_app_theme(app.nativeHandle()), 900, GetDpiForWindow(main));
        CHECK(hit_at(100, frame.height / 2) == HTCAPTION); CHECK(hit_at(1, 1) == HTTOPLEFT);
        CHECK(hit_at((frame.close.left + frame.close.right) / 2, frame.height / 2) == HTCLIENT);
        RECT client{}, outer{}; CHECK(GetClientRect(main, &client)); CHECK(GetWindowRect(main, &outer));
        CHECK(client.right == outer.right - outer.left && client.bottom == outer.bottom - outer.top);
        const int mx = (frame.maximize.left + frame.maximize.right) / 2, my = frame.height / 2;
        pointer(main, UI_POINTER_DOWN, mx, my); pointer(main, UI_POINTER_UP, mx, my); pump(); CHECK(IsZoomed(main));
        RECT maximized_client{}; CHECK(GetClientRect(main, &maximized_client));
        const auto maximized_frame = docking::window_frame(dock_app_theme(app.nativeHandle()), maximized_client.right, GetDpiForWindow(main));
        const int rx = (maximized_frame.maximize.left + maximized_frame.maximize.right) / 2;
        pointer(main, UI_POINTER_DOWN, rx, my); pointer(main, UI_POINTER_UP, rx, my); pump(); CHECK(!IsZoomed(main));
        CHECK(GetWindowRect(main, &outer)); CHECK(outer.right - outer.left == client.right && outer.bottom - outer.top == client.bottom);
#else
        // The WM is asked to omit native decorations; controls stay in the themed client surface.
        Atom actual = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* decorations = nullptr;
        const auto motif = XInternAtom(driver, "_MOTIF_WM_HINTS", False);
        CHECK(XGetWindowProperty(driver, main, motif, 0, 5, False, motif, &actual, &format, &count, &remaining, &decorations) == Success);
        CHECK(format == 32 && count == 5); CHECK(reinterpret_cast<unsigned long*>(decorations)[2] == 0); XFree(decorations);
        XWindowAttributes before_max{}; CHECK(XGetWindowAttributes(driver, main, &before_max));
        auto frame = docking::window_frame(dock_app_theme(app.nativeHandle()), before_max.width, 96);
        pointer(main, UI_POINTER_DOWN, (frame.maximize.left + frame.maximize.right) / 2, frame.height / 2);
        pointer(main, UI_POINTER_UP, (frame.maximize.left + frame.maximize.right) / 2, frame.height / 2); pump();
        XWindowAttributes after_max{}; CHECK(XGetWindowAttributes(driver, main, &after_max)); CHECK(after_max.width > before_max.width);
        frame = docking::window_frame(dock_app_theme(app.nativeHandle()), after_max.width, 96);
        pointer(main, UI_POINTER_DOWN, (frame.maximize.left + frame.maximize.right) / 2, frame.height / 2);
        pointer(main, UI_POINTER_UP, (frame.maximize.left + frame.maximize.right) / 2, frame.height / 2); pump();
        CHECK(XGetWindowAttributes(driver, main, &after_max)); CHECK(before_max.width == after_max.width && before_max.height == after_max.height);
#endif
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto floating = named("Sunfleck Dock"); CHECK(floating); CHECK(exists(floating));
#ifdef _WIN32
        CHECK(!GetWindow(floating, GW_OWNER));
        CHECK(!(GetWindowLongPtrW(floating, GWL_EXSTYLE) & WS_EX_TOPMOST));
        CHECK(BringWindowToTop(main)); pump(); CHECK(above(main, floating));
        CHECK(BringWindowToTop(floating)); pump(); CHECK(above(floating, main));
        {
            // An unrelated top-level window can also cover the floating editor.
            struct Probe { HWND window; ~Probe() { if (window) { DestroyWindow(window); } } } probe{
                CreateWindowExW(0, L"STATIC", L"Dock stacking probe", WS_OVERLAPPEDWINDOW,
                    180, 160, 420, 320, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr)};
            CHECK(probe.window); ShowWindow(probe.window, SW_SHOW);
            CHECK(BringWindowToTop(probe.window)); pump(); CHECK(above(probe.window, floating));
        }
        CHECK(BringWindowToTop(floating)); pump();
        RECT floating_client{}; CHECK(GetClientRect(floating, &floating_client));
        const auto area = dock.nativeHandle()->views.at(dock.windows().front()).area;
        const int padding = MulDiv(4, GetDpiForWindow(floating), 96);
        CHECK(area.left == padding && area.right == floating_client.right - padding);
        CHECK(area.bottom == floating_client.bottom - padding);
        CHECK(area.top == docking::window_frame(dock_app_theme(app.nativeHandle()), floating_client.right, GetDpiForWindow(floating)).height + padding);
        app.setDebugPicking(true);
        pump();
        const auto pick_bounds = content.nativeHandle()->visible;
        CHECK(pick_bounds.right > pick_bounds.left && pick_bounds.bottom > pick_bounds.top);
        const int pick_x = (pick_bounds.left + pick_bounds.right) / 2;
        const int pick_y = (pick_bounds.top + pick_bounds.bottom) / 2;
        pointer(floating, UI_POINTER_MOVE, pick_x, pick_y);
        pump();
        pointer(floating, UI_POINTER_DOWN, pick_x, pick_y);
        pointer(floating, UI_POINTER_UP, pick_x, pick_y);
        pump();
        CHECK(app.debugSnapshot().find("\"selection\":\"" + std::to_string(content.debugId()) + "\"") != std::string::npos);
        CHECK(app.debugSnapshot().find("\"picking\":false") != std::string::npos);
        CHECK(app.debugSnapshot().find("\"object\":\"" + std::to_string(content.debugId()) +
            "\",\"window\":" + std::to_string(dock.windows().front())) != std::string::npos);
        CHECK(content.text() == "Before");
        const auto& hits = dock.nativeHandle()->views.at(dock.windows().front()).hits;
        const auto title = std::find_if(hits.begin(), hits.end(), [](const auto& hit) {
            return hit.part == UI_DOCK_PART_TITLE && hit.item == "editor";
        });
        CHECK(title != hits.end());
        const int title_x = title->bounds.left + 8, title_y = (title->bounds.top + title->bounds.bottom) / 2;
        app.setDebugPicking(true);
        pump();
        pointer(floating, UI_POINTER_UP, title_x, title_y);
        CHECK(app.debugSnapshot().find("\"selection\":\"" + std::to_string(content.debugId()) + "\"") != std::string::npos);
#else
        Window transient = 0; CHECK(!XGetTransientForHint(driver, floating, &transient));
        auto* hints = XGetWMHints(driver, floating); CHECK(hints);
        const bool grouped = (hints->flags & WindowGroupHint) && hints->window_group == main;
        XFree(hints); CHECK(grouped);
#endif
        click_editor(floating); pump(); CHECK(content.text().find('Z') != std::string::npos);
#ifdef _WIN32
        CHECK(content.text().find("\xe4\xb8\xad") != std::string::npos);
        POINT caret{}; CHECK(GetCaretPos(&caret));
        const auto editor_bounds = content.nativeHandle()->visible;
        CHECK(caret.x >= editor_bounds.left && caret.x < editor_bounds.right && caret.y >= editor_bounds.top && caret.y < editor_bounds.bottom);
#endif
        source.set("Changed through binding"); pump(); CHECK(content.text() == "Changed through binding");
        const auto window_id = dock.windows().front();
        const auto view = dock.nativeHandle()->views.at(window_id);
        const auto tab = std::find_if(view.hits.begin(), view.hits.end(), [](const auto& hit) { return hit.part == UI_DOCK_PART_TAB; }); CHECK(tab != view.hits.end());
        // Exercise native drag capture and a return to the primary window.
        const auto saved = dock.save();
        pointer(floating, UI_POINTER_DOWN, tab->bounds.left + 8, tab->bounds.top + 8);
#ifdef _WIN32
        pointer(floating, UI_POINTER_MOVE, -50, -50); pointer(floating, UI_POINTER_CANCEL, -50, -50); pump(); CHECK(dock.save() == saved);
        pointer(floating, UI_POINTER_DOWN, tab->bounds.left + 8, tab->bounds.top + 8);
#endif
        const auto target = dock.nativeHandle()->views.at(0).groups.at(docking::group_of(dock.nativeHandle()->layout, "peer"));
        int x = (target.left + target.right) / 2, y = (target.top + target.bottom) / 2;
#ifdef _WIN32
        SetWindowPos(main, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        POINT point{x, y}; ClientToScreen(main, &point); ScreenToClient(floating, &point); x = point.x; y = point.y;
#else
        Window child = 0; XTranslateCoordinates(driver, main, floating, x, y, &x, &y, &child);
#endif
        pointer(floating, UI_POINTER_MOVE, x, y);
        const auto drag_diagnostic = dock.nativeHandle()->drag;
        pointer(floating, UI_POINTER_UP, x, y); pump();
        if (editor.state() != UI_DOCK_DOCKED) {
            std::fprintf(stderr, "Drag source=(%d,%d), active=%u prepared=%u target_window=%u group=%u target=%s preview=(%d,%d,%d,%d)\n",
                x, y, drag_diagnostic.active, drag_diagnostic.prepared, drag_diagnostic.target_window, drag_diagnostic.group, drag_diagnostic.target.c_str(),
                drag_diagnostic.preview.left, drag_diagnostic.preview.top, drag_diagnostic.preview.right, drag_diagnostic.preview.bottom);
#ifdef _WIN32
            RECT main_rect{}, float_rect{}; GetWindowRect(main, &main_rect); GetWindowRect(floating, &float_rect);
            POINT probe{x, y}; ClientToScreen(floating, &probe);
            const auto hit_window = WindowFromPoint(probe);
            std::fprintf(stderr, "WindowFromPoint=%p main=%p float=%p\n", hit_window, main, floating);
            std::fprintf(stderr, "Main=(%ld,%ld,%ld,%ld), Float=(%ld,%ld,%ld,%ld), captured=%p\n", main_rect.left, main_rect.top, main_rect.right, main_rect.bottom,
                float_rect.left, float_rect.top, float_rect.right, float_rect.bottom, GetCapture());
#endif
        }
        CHECK(editor.state() == UI_DOCK_DOCKED); CHECK(content.text() == "Changed through binding");
#ifdef _WIN32
        // The moving editor keeps focus; an IME commit reaches it without another click.
        SendMessageW(main, WM_IME_CHAR, 0x4e2d, 0); pump();
        CHECK(content.text().find("\xe4\xb8\xad") != std::string::npos);
        // Drag the operating system caption while the floating window covers the target.
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto caption_window = named("Sunfleck Dock"); CHECK(caption_window);
        RECT floating_rect{}; CHECK(GetWindowRect(caption_window, &floating_rect));
        SendMessageW(caption_window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(floating_rect.left + 100, floating_rect.top + 12));
        CHECK(GetCapture() == caption_window);
        const auto destination = dock.nativeHandle()->views.at(0).groups.at(docking::group_of(dock.nativeHandle()->layout, "peer"));
        const auto move_to = [&](int main_x, int main_y) {
            POINT position{main_x, main_y}; ClientToScreen(main, &position); ScreenToClient(caption_window, &position);
            pointer(caption_window, UI_POINTER_MOVE, position.x, position.y);
        };
        move_to((destination.left + destination.right) / 2, (destination.top + destination.bottom) / 2); pump();
        if (dock.nativeHandle()->drag.target_window != 0) {
            POINT probe{(destination.left + destination.right) / 2, (destination.top + destination.bottom) / 2};
            ClientToScreen(main, &probe);
            const auto& drag = dock.nativeHandle()->drag;
            std::fprintf(stderr, "Caption drag active=%u prepared=%u window_move=%u allowed=%u target=%u source=%p main=%p hit=%p capture=%p probe=(%ld,%ld)\n",
                drag.active, drag.prepared, drag.window_move, drag.allowed, drag.target_window, caption_window, main, WindowFromPoint(probe), GetCapture(), probe.x, probe.y);
        }
        CHECK(dock.nativeHandle()->drag.target_window == 0);
        CHECK(dock.nativeHandle()->drag.window_move); CHECK(dock.nativeHandle()->drag.allowed);
        const auto overlay = FindWindowW(L"SunfleckDockOverlay", nullptr); CHECK(overlay); CHECK(IsWindowVisible(overlay));
        const auto directions = dock.nativeHandle()->views.at(0).targets; CHECK(directions.size() == 5);
        for (const auto& direction : directions) {
            // SendMessage updates the hit synchronously. Pumping first can
            // dispatch real cursor motion caused by moving the captured window.
            move_to((direction.bounds.left + direction.bounds.right) / 2, (direction.bounds.top + direction.bounds.bottom) / 2);
            const auto& drag = dock.nativeHandle()->drag;
            if (!drag.allowed || drag.position != direction.position || drag.target_window != 0) {
                std::fprintf(stderr, "Dock indicator expected=%d actual=%d allowed=%u target_window=%u group=%u source=%s target=%s dpi=%u bounds=(%d,%d,%d,%d)\n",
                    direction.position, drag.position, drag.allowed, drag.target_window, drag.group,
                    drag.source.item.c_str(), drag.target.c_str(), GetDpiForWindow(main),
                    direction.bounds.left, direction.bounds.top, direction.bounds.right, direction.bounds.bottom);
            }
            CHECK(drag.target_window == 0);
            CHECK(dock.nativeHandle()->drag.position == direction.position); CHECK(dock.nativeHandle()->drag.allowed);
        }
        move_to((destination.left + destination.right) / 2, (destination.top + destination.bottom) / 2);
        pointer(caption_window, UI_POINTER_UP, 100, -12); pump();
        CHECK(editor.state() == UI_DOCK_DOCKED); CHECK(!IsWindow(caption_window)); CHECK(!IsWindow(overlay));
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto cancelled_window = named("Sunfleck Dock"); CHECK(cancelled_window);
        RECT before_cancel{}; CHECK(GetWindowRect(cancelled_window, &before_cancel));
        SendMessageW(cancelled_window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(before_cancel.left + 100, before_cancel.top + 12));
        pointer(cancelled_window, UI_POINTER_MOVE, 240, 120); pump();
        SendMessageW(cancelled_window, WM_KEYUP, VK_ESCAPE, 0); pump();
        RECT after_cancel{}; CHECK(GetWindowRect(cancelled_window, &after_cancel));
        CHECK(before_cancel.left == after_cancel.left && before_cancel.top == after_cancel.top);
        CHECK(editor.state() == UI_DOCK_FLOATING); CHECK(!dock.nativeHandle()->drag.prepared); CHECK(GetCapture() != cancelled_window);
        editor.move(&peer); pump();
#else
        // Simulate WM caption moves under Xvfb: a held server button and native ConfigureNotify.
        xtst = dlopen("libXtst.so.6", RTLD_NOW); CHECK(xtst);
        using FakeButton = int (*)(Display*, unsigned, Bool, unsigned long);
        using FakeMotion = int (*)(Display*, int, int, int, unsigned long);
        const auto button = reinterpret_cast<FakeButton>(dlsym(xtst, "XTestFakeButtonEvent")); CHECK(button);
        const auto key = reinterpret_cast<FakeButton>(dlsym(xtst, "XTestFakeKeyEvent")); CHECK(key);
        const auto motion = reinterpret_cast<FakeMotion>(dlsym(xtst, "XTestFakeMotionEvent")); CHECK(motion);
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto caption_window = named("Sunfleck Dock"); CHECK(caption_window);
        const auto destination = dock.nativeHandle()->views.at(0).groups.at(docking::group_of(dock.nativeHandle()->layout, "peer"));
        const auto move_to = [&](Window window, int main_x, int main_y) {
            Window child = 0; int sx = 0, sy = 0;
            XTranslateCoordinates(driver, main, DefaultRootWindow(driver), main_x, main_y, &sx, &sy, &child);
            XWarpPointer(driver, None, DefaultRootWindow(driver), 0, 0, 0, 0, sx, sy);
            XMoveWindow(driver, window, sx - 100, sy + 12); XSync(driver, False);
        };
        const auto press_in_main = [&] {
            Window child = 0; int sx = 0, sy = 0;
            XTranslateCoordinates(driver, main, DefaultRootWindow(driver),
                destination.left + 40, destination.top + 40, &sx, &sy, &child);
            motion(driver, DefaultScreen(driver), sx, sy, CurrentTime);
            XSync(driver, False);
            return button(driver, Button1, True, CurrentTime) != 0;
        };
        CHECK(press_in_main());
        move_to(caption_window, (destination.left + destination.right) / 2, (destination.top + destination.bottom) / 2); pump();
        CHECK(dock.nativeHandle()->drag.window_move); CHECK(dock.nativeHandle()->drag.target_window == 0); CHECK(dock.nativeHandle()->drag.allowed);
        const auto overlay = named("Sunfleck Dock Preview"); CHECK(overlay);
        const auto directions = dock.nativeHandle()->views.at(0).targets; CHECK(directions.size() == 5);
        for (const auto& direction : directions) {
            move_to(caption_window, (direction.bounds.left + direction.bounds.right) / 2, (direction.bounds.top + direction.bounds.bottom) / 2); pump();
            CHECK(dock.nativeHandle()->drag.position == direction.position); CHECK(dock.nativeHandle()->drag.allowed);
        }
        move_to(caption_window, (destination.left + destination.right) / 2, (destination.top + destination.bottom) / 2); pump();
        CHECK(button(driver, Button1, False, CurrentTime)); XSync(driver, False); pump();
        CHECK(editor.state() == UI_DOCK_DOCKED); CHECK(!exists(caption_window)); CHECK(!exists(overlay));
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto cancelled_window = named("Sunfleck Dock"); CHECK(cancelled_window);
        XWindowAttributes before_cancel{}; CHECK(XGetWindowAttributes(driver, cancelled_window, &before_cancel));
        CHECK(press_in_main());
        move_to(cancelled_window, destination.left + 40, destination.top + 40); pump();
        // A WM can own the keyboard grab, so cancellation also observes server key state.
        CHECK(key(driver, XKeysymToKeycode(driver, XK_Escape), True, CurrentTime)); XSync(driver, False); pump();
        XWindowAttributes after_cancel{}; CHECK(XGetWindowAttributes(driver, cancelled_window, &after_cancel));
        CHECK(before_cancel.x == after_cancel.x && before_cancel.y == after_cancel.y);
        CHECK(editor.state() == UI_DOCK_FLOATING); CHECK(!dock.nativeHandle()->drag.prepared);
        CHECK(!named("Sunfleck Dock Preview"));
        CHECK(key(driver, XKeysymToKeycode(driver, XK_Escape), False, CurrentTime));
        CHECK(button(driver, Button1, False, CurrentTime)); XSync(driver, False); pump();
        editor.move(&peer); pump();
        // Press the self-drawn caption in the client area and let the fallback move it.
        editor.floatWindow({180, 160, 420, 320}); pump();
        const auto custom_window = named("Sunfleck Dock"); CHECK(custom_window);
        const auto custom_frame = docking::window_frame(dock_app_theme(app.nativeHandle()), 420, 96);
        XWarpPointer(driver, None, custom_window, 0, 0, 0, 0, 100, custom_frame.height / 2);
        CHECK(button(driver, Button1, True, CurrentTime)); XSync(driver, False); pump();
        CHECK(dock.nativeHandle()->drag.window_move);
        const auto custom_destination = dock.nativeHandle()->views.at(0).groups.at(docking::group_of(dock.nativeHandle()->layout, "peer"));
        Window custom_child = 0; int custom_x = 0, custom_y = 0;
        XTranslateCoordinates(driver, main, DefaultRootWindow(driver), (custom_destination.left + custom_destination.right) / 2,
            (custom_destination.top + custom_destination.bottom) / 2, &custom_x, &custom_y, &custom_child);
        XWarpPointer(driver, None, DefaultRootWindow(driver), 0, 0, 0, 0, custom_x, custom_y); XSync(driver, False); pump();
        CHECK(dock.nativeHandle()->drag.allowed); CHECK(dock.nativeHandle()->drag.target_window == 0);
        CHECK(button(driver, Button1, False, CurrentTime)); XSync(driver, False); pump();
        CHECK(editor.state() == UI_DOCK_DOCKED); CHECK(!exists(custom_window));
#endif
        tool.floatWindow({220, 180, 400, 300}); pump();
        const auto tool_window = named("Sunfleck Dock"); CHECK(tool_window);
        bool veto = true;
        dock.setEvent([](ui_dock*, ui_dock_item*, ui_dock_event_kind event, void* context) -> uint32_t { return event == UI_DOCK_CLOSING && *static_cast<bool*>(context); }, &veto);
        close_window(tool_window); pump(); CHECK(tool.state() == UI_DOCK_FLOATING); CHECK(exists(tool_window));
        veto = false;
        const auto tool_surface = dock.nativeHandle()->views.at(dock.windows().front()).area;
        const auto tool_frame = docking::window_frame(dock_app_theme(app.nativeHandle()), tool_surface.right + tool_surface.left,
#ifdef _WIN32
            GetDpiForWindow(tool_window)
#else
            96
#endif
        );
        pointer(tool_window, UI_POINTER_DOWN, (tool_frame.close.left + tool_frame.close.right) / 2, tool_frame.height / 2);
        pointer(tool_window, UI_POINTER_UP, (tool_frame.close.left + tool_frame.close.right) / 2, tool_frame.height / 2);
        pump(); CHECK(tool.state() == UI_DOCK_HIDDEN); CHECK(exists(main));
        // Popup menus are drawn above Dock chrome and must also receive input first.
        auto root = app.createNode(UI_DOCK_PANEL);
        app.setRoot(root);
        auto combo = app.createNode(UI_COMBO_BOX);
        auto spacer = app.createNode(UI_TEXT_BLOCK);
        root.append(combo); root.append(spacer); root.append(host);
        sunfleck::detail::check(ui_node_set_dock(combo.nativeHandle(), UI_DOCK_TOP));
        sunfleck::detail::check(ui_node_set_dock(spacer.nativeHandle(), UI_DOCK_TOP));
        const char* themes[] = {"Fluent 2", "Material 3", "Radix", "Liquid Glass", "Sunfleck Lite"};
        combo.setItems(themes, 5); combo.setValue(4);
        struct ThemeSelection { ui_app* app; ui_result status; } selection{app.nativeHandle(), UI_ERROR_INVALID_STATE};
        combo.setAction([](ui_node* node, void* context) noexcept {
            auto& state = *static_cast<ThemeSelection*>(context);
            uint32_t value = 0;
            state.status = ui_node_get_value(node, &value);
            if (state.status == UI_OK) { state.status = ui_app_set_builtin_theme(state.app, static_cast<ui_builtin_theme>(value), 0, 0); }
        }, &selection);
        sunfleck::detail::check(ui_app_set_builtin_theme(app.nativeHandle(), UI_BUILTIN_SUNFLECK_LITE, 0, 0));
        pump();
        const auto combo_bounds = combo.nativeHandle()->visible;
        pointer(main, UI_POINTER_DOWN, combo_bounds.left + 20, combo_bounds.top + 12);
        pointer(main, UI_POINTER_UP, combo_bounds.left + 20, combo_bounds.top + 12); pump();
        CHECK(combo.nativeHandle()->expanded);
        const auto option = combo.nativeHandle()->children[1]->visible;
        const int option_x = (option.left + option.right) / 2, option_y = (option.top + option.bottom) / 2;
        CHECK(docking::consumes(*dock.nativeHandle(), 0, option_x, option_y));
        const auto before_selection = dock.save();
        pointer(main, UI_POINTER_DOWN, option_x, option_y);
        CHECK(!dock.nativeHandle()->drag.prepared);
        pointer(main, UI_POINTER_UP, option_x, option_y); pump();
        CHECK(combo.value() == 1); CHECK(!combo.nativeHandle()->expanded); CHECK(selection.status == UI_OK);
        CHECK(dock_app_theme(app.nativeHandle()).shape == builtin_themes[UI_BUILTIN_MATERIAL_3].shape);
        CHECK(dock.save() == before_selection);
        editor.floatWindow(); pump(); const auto final_float = named("Sunfleck Dock"); CHECK(final_float);
        close_window(main); CHECK(app.poll(0) == UI_EXITED); CHECK(!exists(main)); CHECK(!exists(final_float));
#ifdef _WIN32
        sunfleck::App reload(renderer);
        reload.loadFleck("<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockGroup><DockDocument Id='doc'><StackPanel><TextArea Id='editor' Text='Initial'/><TextBlock Id='label' Text='Before'/></StackPanel></DockDocument></DockGroup></DockHost></Window>");
        sunfleck::Dock reload_dock(reload.findFleckNode("dock"));
        reload.openWindow();
        reload_dock.find("doc").floatWindow({180, 160, 420, 320});
        const auto reload_pump = [&] { for (int i = 0; i < 12; ++i) { (void)reload.poll(5); } };
        reload_pump();
        const auto live_window = named("Sunfleck Dock");
        CHECK(live_window);
        auto live_editor = reload.findFleckNode("editor");
        auto live_label = reload.findFleckNode("label");
        const auto reload_editor_bounds = live_editor.nativeHandle()->bounds;
        SetFocus(live_window);
        pointer(live_window, UI_POINTER_DOWN, reload_editor_bounds.left + 5, reload_editor_bounds.top + 5);
        pointer(live_window, UI_POINTER_UP, reload_editor_bounds.left + 5, reload_editor_bounds.top + 5);
        if (!binding_node_focused(live_editor.nativeHandle())) {
            const auto v=live_editor.nativeHandle()->visible, b=live_editor.nativeHandle()->bounds;
            std::fprintf(stderr,"Dock editor bounds %d,%d,%d,%d visible %d,%d,%d,%d focus=%d accepts=%d\n",b.left,b.top,b.right,b.bottom,v.left,v.top,v.right,v.bottom,GetFocus()==live_window,live_editor.nativeHandle()->accepts_input());
        }
        CHECK(binding_node_focused(live_editor.nativeHandle()));
        live_editor.setText("Draft");
        const auto layout_before = reload_dock.save();
        reload.refreshFleck("<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockGroup><DockDocument Id='doc'><StackPanel><TextArea Id='editor' Text='Initial'/><TextBlock Id='label' Text='After'/></StackPanel></DockDocument></DockGroup></DockHost></Window>");
        reload_pump();
        CHECK(exists(live_window) && named("Sunfleck Dock") == live_window && reload_dock.save() == layout_before);
        CHECK(live_editor.text() == "Draft" && live_label.text() == "After");
        CHECK(reload.findFleckNode("editor").nativeHandle() == live_editor.nativeHandle());
        CHECK(binding_node_focused(live_editor.nativeHandle()));
#endif
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
#ifndef _WIN32
    XCloseDisplay(driver);
    if (xtst) { dlclose(xtst); }
#endif
    return 0;
}
