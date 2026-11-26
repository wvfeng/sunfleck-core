#pragma once

#include <sunfleck/ui.h>
#include <X11/Xlib.h>
#include <chrono>
#include <string>

namespace x11_drop {
struct Target {
    Atom aware = 0, enter = 0, leave = 0, position = 0, status = 0, drop = 0, finished = 0;
    Atom selection = 0, types = 0, copy = 0, uri_list = 0, utf8 = 0, plain = 0, plain_utf8 = 0;
    Atom property = 0, incr = 0;
    Window source = 0;
    unsigned version = 0;
    Atom offered_text = 0, offered_files = 0, requested = 0;
    int x = 0, y = 0;
    bool pending = false, incremental = false;
    std::string payload;
    std::chrono::steady_clock::time_point deadline;
};

using Receive = bool (*)(void*, int, int, ui_drop_kind, const ui_drop_data*);
void attach(Target& target, Display* display, Window window);
void cancel(Target& target, Display* display, Window window);
bool event(Target& target, Display* display, Window window, const XEvent& event, Receive receive, void* context);
void expire(Target& target, Display* display, Window window);
}
