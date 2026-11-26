#include <sunfleck/dock.hpp>
#include "../src/dock.h"
#include "../src/ui_tree.h"
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include "x11_test_window.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

namespace {
void require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}
constexpr auto plain = "<Window xmlns='urn:sunfleck:v1' Title='Fleck refresh desktop' Width='640' Height='400'>"
    "<StackPanel Id='root'><TextBox Id='editor' Text='Initial'/><Button Id='button' Content='Before'/></StackPanel></Window>";
constexpr auto dock_page = "<Window xmlns='urn:sunfleck:v1'><DockHost Id='workspace'><DockGroup>"
    "<DockDocument Id='document' Title='Document'><TextBox Id='editor' Text='Initial'/></DockDocument>"
    "<DockDocument Id='peer' Title='Peer'><TextBlock Id='label' Text='Before'/></DockDocument>"
    "</DockGroup></DockHost></Window>";
constexpr auto replacement = "<Window xmlns='urn:sunfleck:v1'><Panel Id='replacement'><TextBlock Id='label' Text='Replaced'/></Panel></Window>";

#ifdef _WIN32
using NativeWindow = HWND;
NativeWindow named(const char* title) { return FindWindowA("SunfleckPreview", title); }
void focus(NativeWindow window) { SetFocus(window); }
void pointer(NativeWindow window, bool down, int x, int y)
{
    SendMessageW(window, down ? WM_LBUTTONDOWN : WM_LBUTTONUP, down ? MK_LBUTTON : 0, MAKELPARAM(x, y));
}
void type(NativeWindow window) { SendMessageW(window, WM_CHAR, L'Z', 0); }
void frame(NativeWindow window, bool custom)
{
    RECT client{}, outer{};
    require(GetClientRect(window, &client) && GetWindowRect(window, &outer), "Cannot query window frame");
    require((client.right == outer.right - outer.left && client.bottom == outer.bottom - outer.top) == custom,
        "Native decorations did not follow Dock workspace lifetime");
}
#else
Display* driver = nullptr;
using NativeWindow = Window;
NativeWindow named(const char* title)
{
    return x11_test::find_window(driver, title);
}
void focus(NativeWindow window)
{
    XSetInputFocus(driver, window, RevertToParent, CurrentTime);
    XSync(driver, False);
}
void pointer(NativeWindow window, bool down, int x, int y)
{
    XEvent event{};
    event.xbutton.type = down ? ButtonPress : ButtonRelease;
    event.xbutton.window = window;
    event.xbutton.button = Button1;
    event.xbutton.x = x;
    event.xbutton.y = y;
    XSendEvent(driver, window, False, down ? ButtonPressMask : ButtonReleaseMask, &event);
    XSync(driver, False);
}
void type(NativeWindow window)
{
    XEvent event{};
    event.xkey.type = KeyPress;
    event.xkey.window = window;
    event.xkey.display = driver;
    event.xkey.root = DefaultRootWindow(driver);
    event.xkey.keycode = XKeysymToKeycode(driver, XK_Z);
    event.xkey.state = ShiftMask;
    XSendEvent(driver, window, False, KeyPressMask, &event);
    XSync(driver, False);
}
void frame(NativeWindow window, bool custom)
{
    Atom actual = 0;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* bytes = nullptr;
    const auto motif = XInternAtom(driver, "_MOTIF_WM_HINTS", False);
    require(XGetWindowProperty(driver, window, motif, 0, 5, False, motif,
        &actual, &format, &count, &remaining, &bytes) == Success, "Cannot query X11 frame hints");
    const bool valid = count == 0 ? !custom : format == 32 && count == 5 &&
        reinterpret_cast<unsigned long*>(bytes)[2] == (custom ? 0ul : 1ul);
    if (bytes) { XFree(bytes); }
    require(valid, "X11 decorations did not follow Dock workspace lifetime");
}
#endif

void run()
{
    sunfleck::App app(UI_SOFTWARE);
    app.loadFleck(plain);
    app.openWindow();
    const auto pump = [&] {
        for (unsigned i = 0; i < 12; ++i) {
            const auto result = app.poll(5);
            require(result == UI_OK || result == UI_TIMEOUT, "Desktop Poll failed");
        }
    };
    pump();
    const auto main = named("Fleck refresh desktop");
    require(main != 0, "Native main window missing");
    frame(main, false);
    auto editor = app.findFleckNode("editor");
    const auto editor_id = editor.debugId();
    const auto click = [&](NativeWindow window, sunfleck::Node node) {
        auto rect = node.nativeHandle()->visible;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while ((rect.right <= rect.left || rect.bottom <= rect.top) &&
               std::chrono::steady_clock::now() < deadline) {
            pump();
            rect = node.nativeHandle()->visible;
        }
        require(rect.right > rect.left && rect.bottom > rect.top, "Input target is not visible");
        focus(window);
        pointer(window, true, rect.left + 8, (rect.top + rect.bottom) / 2);
        pointer(window, false, rect.left + 8, (rect.top + rect.bottom) / 2);
        pump();
    };
    click(main, editor);
    type(main);
    pump();
    const auto draft = editor.text();
    require(draft.find('Z') != std::string::npos, "Native editor input was not delivered");
    const auto caret = editor.nativeHandle()->caret;
    auto updated = std::string(plain);
    updated.replace(updated.find("Before"), 6, "After");
    app.refreshFleck(updated);
    pump();
    require(editor.debugId() == editor_id && editor.text() == draft && editor.nativeHandle()->caret == caret &&
        app.findFleckNode("button").text() == "After", "Refresh lost editor identity/draft/caret or failed to apply declaration");
    type(main);
    pump();
    require(editor.text().size() == draft.size() + 1, "Refresh lost native editor focus");
    const auto preserved = editor.text();
    ui_fleck_diagnostic diagnostic{};
    require(ui_app_refresh_fleck(app.nativeHandle(), "<broken", 7, nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT &&
        editor.text() == preserved, "Parse failure changed the live page");
    const auto invalid_binding = "<Window xmlns='urn:sunfleck:v1'><TextBox Id='editor' Text='{Binding Missing}'/></Window>";
    require(ui_app_refresh_fleck(app.nativeHandle(), invalid_binding, std::strlen(invalid_binding), nullptr, &diagnostic) != UI_OK &&
        editor.debugId() == editor_id && editor.text() == preserved, "Binding failure changed the live page");

    // A queued replacement must wait while an ordinary control is pressed.
    auto button = app.findFleckNode("button");
    click(main, button); // Also leave XIM editor focus before structural replacement.
    const auto bounds = button.nativeHandle()->visible;
    pointer(main, true, bounds.left + 8, (bounds.top + bounds.bottom) / 2);
    pump();
    app.queueFleckRefresh(dock_page);
    pump();
    require(app.fleckRefreshStatus().pending && app.findFleckNode("button").debugId() == button.debugId(),
        "Queue replaced a pressed control");
    pointer(main, false, bounds.left + 8, (bounds.top + bounds.bottom) / 2);
    pump();
    require(!app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_OK, "Queue did not resume after release");
    frame(main, true);
    // Native caption capture also protects structural refresh, even though it
    // does not set the ordinary control's pressed/dragged fields.
#ifdef _WIN32
    RECT client{};
    require(GetClientRect(main, &client) != 0, "Cannot query caption geometry");
    const auto caption = docking::window_frame(dock_app_theme(app.nativeHandle()), client.right, GetDpiForWindow(main));
#else
    XWindowAttributes attributes{};
    require(XGetWindowAttributes(driver, main, &attributes) != 0, "Cannot query caption geometry");
    const auto caption = docking::window_frame(dock_app_theme(app.nativeHandle()), attributes.width, 96);
#endif
    pointer(main, true, (caption.minimize.left + caption.minimize.right) / 2, caption.height / 2);
    pump();
    app.queueFleckRefresh(dock_page);
    pump();
    require(app.fleckRefreshStatus().pending, "Queue ignored native caption capture");
    pointer(main, false, 30, caption.height + 80); // Release off the button without minimizing.
    pump();
    require(!app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_OK,
        "Queue did not resume after native caption release");
    sunfleck::Dock dock(app.findFleckNode("workspace"));
    dock.find("document").floatWindow({100, 100, 360, 260});
    pump();
    const auto floating = named("Sunfleck Dock");
    require(floating != 0 && dock.windows().size() == 1, "Refresh could not create a native Dock window");
    editor = app.findFleckNode("editor");
    require(editor.debugId() == editor_id, "Adding Dock lost the stable editor");
    click(floating, editor);
    type(floating);
    pump();
    const auto floated_draft = editor.text();
    const auto layout = dock.save();
    auto dock_updated = std::string(dock_page);
    dock_updated.replace(dock_updated.find("Before"), 6, "Updated");
    app.refreshFleck(dock_updated);
    pump();
    require(dock.save() == layout && named("Sunfleck Dock") == floating && editor.text() == floated_draft,
        "Dock refresh replaced the native window, layout or draft");
    type(floating);
    pump();
    require(editor.text().size() == floated_draft.size() + 1, "Dock refresh lost native editor focus");
#ifdef _WIN32
    SendMessageW(floating, WM_IME_STARTCOMPOSITION, 0, 0);
    app.queueFleckRefresh(replacement);
    pump();
    require(app.fleckRefreshStatus().pending, "Queue ignored floating-window IME composition");
    SendMessageW(floating, WM_IME_ENDCOMPOSITION, 0, 0);
#else
    app.queueFleckRefresh(replacement);
    // Server-side XIM may own a preedit string even when no callback reports it.
    // Giving focus back to the main window makes editor retirement safe.
    focus(main);
#endif
    pump();
    require(!app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_OK &&
        app.findFleckNode("label").text() == "Replaced" && !named("Sunfleck Dock"),
        "Queued root/workspace removal failed to retire the floating window");
    frame(main, false);
    ui_node* retired = nullptr;
    require(ui_app_find_debug_node(app.nativeHandle(), editor_id, &retired) == UI_ERROR_INVALID_ARGUMENT,
        "Retired native editor remained addressable");
    // A second add/remove cycle catches stale custom-frame state.
    app.refreshFleck(dock_page);
    pump();
    frame(main, true);
    app.refreshFleck(replacement);
    pump();
    frame(main, false);
}
}

int main()
{
#ifndef _WIN32
    // The fixture injects literal X11 keys; candidate-window input has its own acceptance run.
    if (setenv("XMODIFIERS", "@im=none", 1) != 0) { return 1; }
    driver = XOpenDisplay(nullptr);
    if (!driver) { std::fprintf(stderr, "X11 driver display unavailable\n"); return 1; }
#endif
    int result = 0;
    try { run(); std::puts("Fleck desktop refresh passed"); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
#ifndef _WIN32
    XCloseDisplay(driver);
#endif
    return result;
}
