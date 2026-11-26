#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstring>

namespace x11_test {
inline bool has_client_title(Display* display, Window window, const char* title)
{
    char* name = nullptr;
    if (!XFetchName(display, window, &name) || !name) {
        return false;
    }
    const bool matches = std::strcmp(name, title) == 0;
    XFree(name);
    if (!matches) {
        return false;
    }
    Atom* protocols = nullptr;
    int count = 0;
    if (XGetWMProtocols(display, window, &protocols, &count)) {
        XFree(protocols);
        return true;
    }
    XWindowAttributes attributes{};
    return XGetWindowAttributes(display, window, &attributes) && attributes.override_redirect;
}

inline Window find_child(Display* display, Window parent, const char* title, Window target)
{
    Window root = 0, owner = 0, *children = nullptr;
    unsigned count = 0;
    if (!XQueryTree(display, parent, &root, &owner, &children, &count)) {
        return 0;
    }
    Window result = 0;
    for (unsigned index = 0; index < count && !result; ++index) {
        const auto window = children[index];
        if (window == target) {
            result = window;
        } else {
            result = find_child(display, window, title, target);
            if (!result && title && has_client_title(display, window, title)) {
                result = window;
            }
        }
    }
    if (children) {
        XFree(children);
    }
    return result;
}

inline Window find_window(Display* display, const char* title, Window target = 0)
{
    // A reparenting WM may copy the client title to its own frame.
    XGrabServer(display);
    const auto result = find_child(display, DefaultRootWindow(display), title, target);
    XUngrabServer(display);
    XFlush(display);
    return result;
}
}
