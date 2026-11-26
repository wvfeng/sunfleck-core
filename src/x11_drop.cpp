#include "x11_drop.h"

#include <X11/Xatom.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace x11_drop {
namespace {
constexpr size_t limit = 1024 * 1024;
struct FreeProperty {
    void operator()(unsigned char* data) const { if (data) { XFree(data); } }
};
struct Property {
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    std::unique_ptr<unsigned char, FreeProperty> data;
};

Property read(Display* display, Window window, Atom atom, unsigned length, bool remove = false)
{
    Property result;
    unsigned char* data = nullptr;
    XGetWindowProperty(display, window, atom, 0, length, remove, AnyPropertyType,
        &result.type, &result.format, &result.count, &result.remaining, &data);
    result.data.reset(data);
    return result;
}

void send(Display* display, Window destination, Window window, Atom type, bool accepted, Atom action)
{
    XEvent response{};
    response.xclient.type = ClientMessage;
    response.xclient.window = destination;
    response.xclient.message_type = type;
    response.xclient.format = 32;
    response.xclient.data.l[0] = static_cast<long>(window);
    response.xclient.data.l[1] = accepted ? 1 : 0;
    if (accepted && action) {
        response.xclient.data.l[2] = static_cast<long>(action);
    }
    XSendEvent(display, destination, False, NoEventMask, &response);
    XFlush(display);
}

void reset(Target& t)
{
    t.source = 0;
    t.version = 0;
    t.offered_text = t.offered_files = t.requested = 0;
    t.pending = t.incremental = false;
    t.payload.clear();
}

void finish(Target& t, Display* d, Window w, bool accepted)
{
    if (t.source) {
        send(d, t.source, w, t.finished, t.version >= 5 && accepted, t.copy);
    }
    reset(t);
}

int hex(unsigned char ch)
{
    if (ch >= '0' && ch <= '9') { return ch - '0'; }
    if (ch >= 'A' && ch <= 'F') { return ch - 'A' + 10; }
    if (ch >= 'a' && ch <= 'f') { return ch - 'a' + 10; }
    return -1;
}

bool file_path(std::string_view uri, std::string& path)
{
    if (!uri.starts_with("file:")) { return false; }
    uri.remove_prefix(5);
    if (uri.starts_with("//")) {
        uri.remove_prefix(2);
        const auto slash = uri.find('/');
        if (slash == uri.npos || (slash && uri.substr(0, slash) != "localhost")) { return false; }
        uri.remove_prefix(slash);
    }
    if (!uri.starts_with('/')) { return false; }
    for (size_t i = 0; i < uri.size(); ++i) {
        unsigned char ch = uri[i];
        if (ch == '%') {
            if (uri.size() - i < 3) { return false; }
            const int high = hex(uri[i + 1]), low = hex(uri[i + 2]);
            if (high < 0 || low < 0) { return false; }
            ch = static_cast<unsigned char>((high << 4) | low);
            i += 2;
        } else if (ch == '?' || ch == '#') {
            return false;
        }
        if (!ch) { return false; }
        path.push_back(static_cast<char>(ch));
    }
    return true;
}

bool deliver(Target& t, Receive receive, void* context)
{
    if (t.payload.find('\0') != std::string::npos) { return false; }
    if (t.requested != t.uri_list) {
        const ui_drop_data data{UI_DROP_TEXT, t.payload.c_str()};
        return receive(context, t.x, t.y, UI_DROP_TEXT, &data);
    }
    std::vector<std::string> paths;
    std::string_view remaining(t.payload);
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        auto line = remaining.substr(0, end);
        remaining.remove_prefix(end == remaining.npos ? remaining.size() : end + 1);
        if (line.ends_with('\r')) { line.remove_suffix(1); }
        if (line.empty() || line.front() == '#') { continue; }
        std::string path;
        if (!file_path(line, path)) { return false; }
        paths.push_back(std::move(path));
    }
    if (paths.empty()) { return false; }
    std::vector<const char*> borrowed;
    borrowed.reserve(paths.size());
    for (const auto& path : paths) { borrowed.push_back(path.c_str()); }
    const ui_drop_data data{UI_DROP_FILES, nullptr, borrowed.data(), static_cast<uint32_t>(borrowed.size())};
    return receive(context, t.x, t.y, UI_DROP_FILES, &data);
}

void offer(Target& t, Atom type)
{
    if (type == t.uri_list) { t.offered_files = type; }
    if (type == t.utf8 || type == t.plain_utf8 || (type == t.plain && !t.offered_text)) {
        t.offered_text = type;
    }
}
}

void attach(Target& t, Display* d, Window w)
{
    char* names[] = {const_cast<char*>("XdndAware"), const_cast<char*>("XdndEnter"),
        const_cast<char*>("XdndLeave"), const_cast<char*>("XdndPosition"), const_cast<char*>("XdndStatus"),
        const_cast<char*>("XdndDrop"), const_cast<char*>("XdndFinished"), const_cast<char*>("XdndSelection"),
        const_cast<char*>("XdndTypeList"), const_cast<char*>("XdndActionCopy"), const_cast<char*>("text/uri-list"),
        const_cast<char*>("UTF8_STRING"), const_cast<char*>("text/plain"),
        const_cast<char*>("text/plain;charset=utf-8"), const_cast<char*>("SUNFLECK_XDND"), const_cast<char*>("INCR")};
    Atom atoms[sizeof(names) / sizeof(*names)]{};
    XInternAtoms(d, names, sizeof(atoms) / sizeof(*atoms), False, atoms);
    t = {atoms[0], atoms[1], atoms[2], atoms[3], atoms[4], atoms[5], atoms[6], atoms[7],
        atoms[8], atoms[9], atoms[10], atoms[11], atoms[12], atoms[13], atoms[14], atoms[15]};
    const unsigned long version = 5;
    XChangeProperty(d, w, t.aware, XA_ATOM, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(&version), 1);
}

void cancel(Target& t, Display* d, Window w)
{
    if (t.pending) {
        XDeleteProperty(d, w, t.property);
        finish(t, d, w, false);
    } else {
        reset(t);
    }
}

void expire(Target& t, Display* d, Window w)
{
    if (t.pending && std::chrono::steady_clock::now() >= t.deadline) { cancel(t, d, w); }
}

bool event(Target& t, Display* d, Window w, const XEvent& e, Receive receive, void* context)
{
    if (e.type == ClientMessage && e.xclient.window == w && e.xclient.format == 32) {
        const auto& m = e.xclient;
        const auto source = static_cast<Window>(m.data.l[0]);
        if (m.message_type == t.enter) {
            if (t.pending) { return true; }
            reset(t);
            const auto version = (static_cast<unsigned long>(m.data.l[1]) >> 24) & 0xff;
            if (source && version >= 3) {
                t.source = source;
                t.version = std::min(5ul, version);
                if (m.data.l[1] & 1) {
                    const auto list = read(d, source, t.types, 256);
                    if (list.type == XA_ATOM && list.format == 32 && !list.remaining) {
                        const auto* values = reinterpret_cast<const Atom*>(list.data.get());
                        for (unsigned long i = 0; i < list.count; ++i) { offer(t, values[i]); }
                    }
                } else {
                    for (int i = 2; i < 5; ++i) { offer(t, static_cast<Atom>(m.data.l[i])); }
                }
            }
            return true;
        }
        if (!source || source != t.source || t.pending) { return false; }
        if (m.message_type == t.leave) { reset(t); return true; }
        if (m.message_type == t.position) {
            const auto point = static_cast<uint32_t>(m.data.l[2]);
            Window child = 0;
            t.requested = None;
            if (static_cast<Atom>(m.data.l[4]) == t.copy && XTranslateCoordinates(d, DefaultRootWindow(d), w,
                static_cast<int16_t>(point >> 16), static_cast<int16_t>(point), &t.x, &t.y, &child)) {
                if (t.offered_files && receive(context, t.x, t.y, UI_DROP_FILES, nullptr)) { t.requested = t.offered_files; }
                else if (t.offered_text && receive(context, t.x, t.y, UI_DROP_TEXT, nullptr)) { t.requested = t.offered_text; }
            }
            XEvent reply{};
            reply.xclient.type = ClientMessage;
            reply.xclient.window = source;
            reply.xclient.message_type = t.status;
            reply.xclient.format = 32;
            reply.xclient.data.l[0] = static_cast<long>(w);
            reply.xclient.data.l[1] = t.requested ? 3 : 2;
            reply.xclient.data.l[4] = t.requested ? static_cast<long>(t.copy) : None;
            XSendEvent(d, source, False, NoEventMask, &reply);
            XFlush(d);
            return true;
        }
        if (m.message_type == t.drop) {
            if (!t.requested || XGetSelectionOwner(d, t.selection) != source) {
                finish(t, d, w, false);
            } else {
                t.pending = true;
                t.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                XConvertSelection(d, t.selection, t.requested, t.property, w, static_cast<Time>(m.data.l[2]));
                XFlush(d);
            }
            return true;
        }
    }
    const bool selection = e.type == SelectionNotify && e.xselection.requestor == w &&
        e.xselection.selection == t.selection && e.xselection.target == t.requested &&
        (e.xselection.property == t.property || e.xselection.property == None);
    const bool chunk = e.type == PropertyNotify && e.xproperty.window == w && e.xproperty.atom == t.property &&
        e.xproperty.state == PropertyNewValue && t.incremental;
    if (!t.pending || (!selection && !chunk)) { return false; }
    if (selection && e.xselection.property == None) {
        finish(t, d, w, false);
        return true;
    }
    try {
        const auto value = read(d, w, t.property, limit / 4, true);
        if (selection && value.type == t.incr && value.format == 32 && value.count == 1) {
            t.incremental = true;
            XFlush(d);
        } else if (value.type != t.requested || value.format != 8 || value.remaining || value.count > limit - t.payload.size()) {
            XDeleteProperty(d, w, t.property);
            finish(t, d, w, false);
        } else {
            if (value.count) { t.payload.append(reinterpret_cast<const char*>(value.data.get()), value.count); }
            if (!t.incremental || !value.count) { finish(t, d, w, deliver(t, receive, context)); }
            else { XFlush(d); }
        }
    } catch (...) {
        XDeleteProperty(d, w, t.property);
        finish(t, d, w, false);
    }
    return true;
}
}
