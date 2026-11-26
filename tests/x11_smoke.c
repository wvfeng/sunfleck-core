#include <sunfleck/ui.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#ifdef SUNFLECK_TEST_GLES3
#include <GLES3/gl3.h>
#include <dlfcn.h>
#endif

static ui_result set_fixed_width(ui_node* node, double pixels) {
    ui_binding_value value = {UI_VALUE_LENGTH};
    value.integer = UI_LENGTH_PIXELS; value.number = pixels;
    return ui_node_set_property(node, UI_PROPERTY_WIDTH, &value);
}

static Window find_child(Display* display, Window base, Atom close_protocol, const char* title)
{
    Window root, parent, *children;
    unsigned count;
    if (!XQueryTree(display, base, &root, &parent, &children, &count)) {
        return 0;
    }
    Window result = 0;
    for (unsigned i = 0; i < count && !result; ++i) {
        result = find_child(display, children[i], close_protocol, title);
        if (result) {
            break;
        }
        char* name = NULL;
        if (XFetchName(display, children[i], &name) && name) {
            if (strcmp(name, title) == 0) {
                Atom* protocols;
                int protocol_count;
                if (XGetWMProtocols(display, children[i], &protocols, &protocol_count)) {
                    for (int j = 0; j < protocol_count; ++j) {
                        if (protocols[j] == close_protocol) {
                            result = children[i];
                        }
                    }
                    XFree(protocols);
                }
            }
            XFree(name);
        }
    }
    if (children) {
        XFree(children);
    }
    return result;
}

static int discovery_error(Display* display, XErrorEvent* event)
{
    if (event->error_code != BadWindow) {
        fprintf(stderr, "Unexpected X11 discovery error: %u\n", event->error_code);
        exit(1);
    }
    return 0;
}

static Window find_titled_window(Display* display, const char* title)
{
    const Atom close_protocol = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSync(display, False);
    XErrorHandler previous = XSetErrorHandler(discovery_error);
    const Window result = find_child(display, DefaultRootWindow(display), close_protocol, title);
    XSync(display, False);
    XSetErrorHandler(previous);
    return result;
}

static Window find_window(Display* display)
{
    return find_titled_window(display, "Sunfleck");
}

static int frame(ui_app* app, Display* display, Window window, int x, int y,
                 unsigned long expected)
{
    XSync(display, False);
    unsigned long pixel = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const ui_result status = app ? ui_app_poll(app, 0) : UI_TIMEOUT;
        if (status != UI_OK && status != UI_TIMEOUT) {
            return 0;
        }
        XWindowAttributes attributes;
        if (XGetWindowAttributes(display, window, &attributes) &&
            attributes.map_state == IsViewable && x < attributes.width && y < attributes.height) {
            XImage* image = XGetImage(display, window, x, y, 1, 1, AllPlanes, ZPixmap);
            if (image) {
                pixel = XGetPixel(image, 0, 0);
                XDestroyImage(image);
                if ((pixel & 0xffffff) == expected) {
                    return 1;
                }
            }
        }
        poll(NULL, 0, 10);
    }
    fprintf(stderr, "X11 pixel at %d,%d: %08lx, expected %06lx\n", x, y, pixel, expected);
    return 0;
}

static int verify(ui_app* app, Display* display, int dpi)
{
    if (ui_app_poll(app, 1000) != UI_OK) {
        return 1;
    }
#ifdef SUNFLECK_TEST_GLES3
    void* gles = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_NOLOAD);
    if (gles) {
        typedef const GLubyte* (*GetString)(GLenum);
        const GetString get_string = (GetString)dlsym(gles, "glGetString");
        const char* renderer = get_string ? (const char*)get_string(GL_RENDERER) : NULL;
        if (renderer) {
            printf("GL_RENDERER: %s\n", renderer);
        }
        dlclose(gles);
    }
#endif
    const Window window = find_window(display);
    if (!window) {
        return 2;
    }
    const int screen = DefaultScreen(display);
    const int mm = DisplayWidthMM(display, screen);
    int actual_dpi = mm > 0 ? (int)(DisplayWidth(display, screen) * 25.4 / mm + 0.5) : 96;
    actual_dpi = actual_dpi < 48 ? 48 : (actual_dpi > 384 ? 384 : actual_dpi);
    const int point = (32 * actual_dpi + 48) / 96;
    if (!frame(app, display, window, point, point, 0x884900)) {
        return 15;
    }
    XEvent event = {0};
    event.xbutton.type = ButtonRelease;
    event.xbutton.display = display;
    event.xbutton.window = window;
    event.xbutton.button = Button1;
    event.xbutton.x = point;
    event.xbutton.y = point;
    XSendEvent(display, window, False, ButtonReleaseMask, &event);
    if (!frame(app, display, window, point, point, 0xf3dfbe)) {
        return 3;
    }
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    event = (XEvent){0};
    event.xkey.type = KeyRelease;
    event.xkey.display = display;
    event.xkey.window = window;
    event.xkey.keycode = XKeysymToKeycode(display, XK_space);
    XSendEvent(display, window, False, KeyReleaseMask, &event);
    if (!frame(app, display, window, point, point, 0x884900)) {
        return 4;
    }
    if (dpi > 110) {
        XImage* image = XGetImage(display, window, 25, 25, 1, 1, AllPlanes, ZPixmap);
        if (!image) {
            return 5;
        }
        const int scaled = XGetPixel(image, 0, 0) == 0xfbf8f2;
        XDestroyImage(image);
        if (!scaled) {
            return 5;
        }
    }
    XResizeWindow(display, window, 400, 240);
    if (!frame(NULL, display, window, 399, 239, 0xfbf8f2)) {
        return 16;
    }
    if (!frame(app, display, window, 399, 239, 0xfbf8f2)) {
        return 6;
    }
    XUnmapWindow(display, window);
    XSync(display, False);
    if (ui_app_poll(app, 1000) != UI_OK || ui_app_poll(app, 0) != UI_TIMEOUT) {
        return 7;
    }
    XMapWindow(display, window);
    if (!frame(app, display, window, point, point, 0x884900)) {
        return 8;
    }
    event = (XEvent){0};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = (long)XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSendEvent(display, window, False, NoEventMask, &event);
    XSync(display, False);
    return ui_app_poll(app, 1000) == UI_EXITED ? 0 : 9;
}

static int verify_swap_failure(ui_app* app, Display* display, int recover)
{
    if (ui_app_poll(app, 1000) != UI_OK) {
        return 11;
    }
    const Window window = find_window(display);
    if (!window) {
        return 12;
    }
    XImage* image = XGetImage(display, window, 32, 32, 1, 1, AllPlanes, ZPixmap);
    if (!image) {
        return 13;
    }
    const int correct = XGetPixel(image, 0, 0) == 0x884900;
    XDestroyImage(image);
    if (!correct) {
        return 13;
    }
    XClearArea(display, window, 0, 0, 0, 0, True);
    XSync(display, False);
    const ui_result status = ui_app_poll(app, 1000);
    if (recover) {
        return status == UI_OK && find_window(display) == window ? 0 : 14;
    }
    if (status != UI_ERROR_PLATFORM ||
        ui_app_poll(app, 0) != UI_EXITED || find_window(display)) {
        return 14;
    }
    return 0;
}

static unsigned long default_color(ui_theme_color token)
{
    ui_theme theme;
    if (ui_theme_init(&theme, 0, 0) != UI_OK) { return 0; }
    return theme.colors[token];
}

static int verify_editor(ui_app* app, Display* display)
{
    ui_node* editor = NULL;
    ui_node* table = NULL;
    ui_node* row = NULL;
    ui_node* identifier = NULL;
    const ui_grid_track columns[] = {{UI_GRID_PIXELS, 85}, {UI_GRID_STAR, 1}};
    if (ui_node_create(app, UI_DATA_GRID, &table) != UI_OK ||
        ui_node_create(app, UI_GRID, &row) != UI_OK ||
        ui_node_create(app, UI_BADGE, &identifier) != UI_OK ||
        ui_node_create(app, UI_TEXT_BOX, &editor) != UI_OK ||
        ui_grid_set_tracks(row, NULL, 0, columns, 2) != UI_OK ||
        set_fixed_width(identifier, 85) != UI_OK ||
        ui_node_append(table, row) != UI_OK || ui_node_append(row, identifier) != UI_OK ||
        ui_node_append(row, editor) != UI_OK ||
        ui_node_set_grid_cell(editor, 0, 1, 1, 1) != UI_OK ||
        ui_node_set_text(editor, "A\xe4\xb8\xad" "B") != UI_OK ||
        ui_app_set_root(app, table) != UI_OK || ui_app_poll(app, 1000) != UI_OK) {
        return 20;
    }
    const Window window = find_window(display);
    if (!window || !frame(app, display, window, 150, 20, default_color(UI_THEME_CONTROL))) {
        return 21;
    }
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XEvent event = {0};
    event.xbutton.type = ButtonRelease;
    event.xbutton.display = display;
    event.xbutton.window = window;
    event.xbutton.button = Button1;
    event.xbutton.x = 150;
    event.xbutton.y = 28;
    XSendEvent(display, window, False, ButtonReleaseMask, &event);
    const KeySym keys[] = {XK_Home, XK_Right, XK_x};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        event = (XEvent){0};
        event.xkey.type = KeyPress;
        event.xkey.display = display;
        event.xkey.window = window;
        event.xkey.keycode = XKeysymToKeycode(display, keys[i]);
        XSendEvent(display, window, False, KeyPressMask, &event);
    }
    XSync(display, False);
    if (ui_app_poll(app, 1000) != UI_OK) {
        return 22;
    }
    char text[16];
    uint32_t size = sizeof(text);
    if (ui_node_get_text(editor, text, &size) != UI_OK || strcmp(text, "Ax\xe4\xb8\xad" "B") != 0) {
        return 23;
    }
    return ui_app_request_close(app) == UI_OK && ui_app_poll(app, 0) == UI_EXITED ? 0 : 24;
}

static void follow_slider(ui_node* slider, void* context)
{
    uint32_t value = 0;
    if (ui_node_get_value(slider, &value) == UI_OK) {
        ui_node_set_value(context, value);
    }
}

static void send_pointer(Display* display, Window window, int type, int x, int y)
{
    XEvent event = {0};
    long mask;
    if (type == MotionNotify) {
        event.xmotion.type = type;
        event.xmotion.display = display;
        event.xmotion.window = window;
        event.xmotion.state = Button1Mask;
        event.xmotion.x = x;
        event.xmotion.y = y;
        mask = PointerMotionMask;
    } else {
        event.xbutton.type = type;
        event.xbutton.display = display;
        event.xbutton.window = window;
        event.xbutton.button = Button1;
        event.xbutton.x = x;
        event.xbutton.y = y;
        mask = type == ButtonPress ? ButtonPressMask : ButtonReleaseMask;
    }
    XSendEvent(display, window, False, mask, &event);
    XSync(display, False);
}

static int verify_slider(ui_app* app, Display* display, int dpi)
{
    ui_node *root = NULL, *slider = NULL, *progress = NULL;
    if (ui_node_create(app, UI_STACK_PANEL, &root) != UI_OK ||
        ui_node_create(app, UI_SLIDER, &slider) != UI_OK ||
        ui_node_create(app, UI_PROGRESS_BAR, &progress) != UI_OK ||
        ui_node_append(root, slider) != UI_OK || ui_node_append(root, progress) != UI_OK ||
        ui_node_set_action(slider, follow_slider, progress) != UI_OK ||
        ui_app_set_root(app, root) != UI_OK || ui_app_poll(app, 1000) != UI_OK) {
        return 30;
    }
    const Window window = find_window(display);
    XWindowAttributes attributes;
    if (!window || !XGetWindowAttributes(display, window, &attributes)) {
        return 31;
    }

    const int y = (32 * dpi + 48) / 96;
    if (!frame(app, display, window, attributes.width / 2, y, default_color(UI_THEME_CONTROL))) {
        return 31;
    }
    send_pointer(display, window, ButtonPress, (26 * dpi + 48) / 96, y);
    send_pointer(display, window, MotionNotify, attributes.width / 2, -10);
    uint32_t value = 0, mirrored = 0;
    if (ui_app_poll(app, 0) != UI_OK || ui_node_get_value(slider, &value) != UI_OK ||
        value < 40 || value > 60 || ui_node_get_value(progress, &mirrored) != UI_OK ||
        mirrored != value) {
        return 32;
    }

    send_pointer(display, window, ButtonRelease, attributes.width + 10, -10);
    if (ui_app_poll(app, 0) != UI_OK || ui_node_get_value(slider, &value) != UI_OK ||
        value != 100 || ui_node_get_value(progress, &mirrored) != UI_OK || mirrored != value) {
        return 33;
    }

    ui_node* grid = NULL;
    if (ui_node_create(app, UI_DATA_GRID, &grid) != UI_OK) {
        return 34;
    }
    for (unsigned index = 0; index < 16; ++index) {
        ui_node *row = NULL, *cell = NULL;
        if (ui_node_create(app, UI_GRID, &row) != UI_OK ||
            ui_node_create(app, UI_TEXT_BLOCK, &cell) != UI_OK ||
            ui_node_append(grid, row) != UI_OK || ui_node_append(row, cell) != UI_OK) {
            return 35;
        }
    }
    if (ui_app_set_root(app, grid) != UI_OK) {
        return 36;
    }
    const int bar_x = attributes.width - (21 * dpi + 48) / 96;
    const int bar_y = (21 * dpi + 48) / 96;
    if (!frame(app, display, window, bar_x, bar_y, 0x884900)) {
        return 37;
    }
    send_pointer(display, window, ButtonPress, bar_x, bar_y);
    if (!frame(app, display, window, bar_x, bar_y, 0xf3dfbe)) {
        return 38;
    }
    const int bottom = attributes.height - (21 * dpi + 48) / 96;
    send_pointer(display, window, MotionNotify, bar_x, attributes.height + 20);
    if (!frame(app, display, window, bar_x, bar_y, default_color(UI_THEME_CONTROL)) ||
        !frame(app, display, window, bar_x, bottom, 0xf3dfbe)) {
        return 39;
    }
    send_pointer(display, window, ButtonRelease, bar_x, attributes.height + 20);
    return frame(app, display, window, bar_x, bottom, 0x884900) ? 0 : 40;
}

struct drop_observation {
    unsigned calls;
    int matches;
};

static int observe_drop(ui_node* node, const ui_drop_data* data, void* context)
{
    (void)node;
    struct drop_observation* seen = context;
    ++seen->calls;
    seen->matches = data->kind == UI_DROP_TEXT ? strcmp(data->text, "external text") == 0 :
        data->count == 2 && strcmp(data->paths[0], "/tmp/alpha beta.txt") == 0 &&
        strcmp(data->paths[1], "/tmp/a+b.txt") == 0;
    return seen->matches;
}

static void xdnd_message(Display* d, Window target, Window source, const char* type, long a, long b, long c, long action)
{
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = target;
    event.xclient.message_type = XInternAtom(d, type, False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = (long)source;
    event.xclient.data.l[1] = a;
    event.xclient.data.l[2] = b;
    event.xclient.data.l[3] = c;
    event.xclient.data.l[4] = action;
    XSendEvent(d, target, False, NoEventMask, &event);
    XSync(d, False);
}

static int transfer(ui_app* app, Display* d, Window target, Window source,
                    Atom type, const char* payload, int position_x, int position_y, int accepted)
{
    const Atom selection = XInternAtom(d, "XdndSelection", False);
    const Atom status = XInternAtom(d, "XdndStatus", False);
    const Atom finished = XInternAtom(d, "XdndFinished", False);
    const Atom copy = XInternAtom(d, "XdndActionCopy", False);
    const long position = (long)(((unsigned)position_x & 65535u) << 16 | ((unsigned)position_y & 65535u));
    XSetSelectionOwner(d, selection, source, CurrentTime);
    xdnd_message(d, target, source, "XdndEnter", 5L << 24, (long)type, 0, 0);
    xdnd_message(d, target, source, "XdndPosition", 0, position, CurrentTime, (long)copy);
    XEvent event;
    const ui_result positioned = ui_app_poll(app, 50);
    XSync(d, False);
    if (positioned != UI_OK || !XCheckTypedWindowEvent(d, source, ClientMessage, &event) ||
        event.xclient.message_type != status || !(event.xclient.data.l[1] & 1) ||
        (Atom)event.xclient.data.l[4] != copy) {
        fprintf(stderr, "Xdnd target 0x%lx did not accept position (%d,%d), poll=%d\n",
            target, position_x, position_y, positioned);
        return 0;
    }
    xdnd_message(d, target, source, "XdndDrop", 0, CurrentTime, 0, 0);
    XSelectionRequestEvent request = {0};
    for (unsigned attempt = 0; attempt < 50 && !request.property; ++attempt) {
        ui_app_poll(app, 10);
        XSync(d, False);
        while (XCheckTypedWindowEvent(d, source, SelectionRequest, &event)) {
            if (event.xselectionrequest.requestor == target && event.xselectionrequest.selection == selection &&
                event.xselectionrequest.target == type) {
                request = event.xselectionrequest;
                break;
            }
        }
    }
    if (!request.property) {
        fprintf(stderr, "Xdnd target did not request the selection\n");
        return 0;
    }
    XChangeProperty(d, target, request.property, request.target, 8, PropModeReplace,
        (const unsigned char*)payload, (int)strlen(payload));
    event = (XEvent){0};
    event.xselection.type = SelectionNotify;
    event.xselection.requestor = target;
    event.xselection.selection = request.selection;
    event.xselection.target = request.target;
    event.xselection.property = request.property;
    event.xselection.time = request.time;
    XSendEvent(d, target, False, NoEventMask, &event);
    XSync(d, False);
    for (unsigned attempt = 0; attempt < 50; ++attempt) {
        ui_app_poll(app, 10);
        XSync(d, False);
        while (XCheckTypedWindowEvent(d, source, ClientMessage, &event)) {
            if (event.xclient.message_type == finished) {
                const int success = !!(event.xclient.data.l[1] & 1);
                if (success != accepted) {
                    fprintf(stderr, "Xdnd finish for atom %lu: accepted=%d expected=%d\n", type, success, accepted);
                }
                return success == accepted;
            }
        }
    }
    fprintf(stderr, "Xdnd target did not send XdndFinished\n");
    return 0;
}

static int verify_drop(ui_app* app, Display* d)
{
    ui_node* editor = NULL;
    struct drop_observation seen = {0};
    if (ui_node_create(app, UI_TEXT_BOX, &editor) != UI_OK || ui_app_set_root(app, editor) != UI_OK ||
        ui_node_set_drop_handler(editor, UI_DROP_TEXT | UI_DROP_FILES, observe_drop, &seen) != UI_OK ||
        ui_app_poll(app, 1000) != UI_OK) { return 41; }
    const Window target = find_titled_window(d, "Sunfleck Xdnd test"), root = DefaultRootWindow(d);
    if (!target || !frame(app, d, target, 25, 25, default_color(UI_THEME_CONTROL))) { return 42; }
    const Window source = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0);
    Window child = 0;
    int x = 0, y = 0;
    if (!target || !XTranslateCoordinates(d, target, root, 25, 25, &x, &y, &child)) {
        XDestroyWindow(d, source);
        return 42;
    }
    const Atom text = XInternAtom(d, "UTF8_STRING", False);
    const Atom files = XInternAtom(d, "text/uri-list", False);
    const int passed = transfer(app, d, target, source, text, "external text", x, y, 1) && seen.calls == 1 &&
        transfer(app, d, target, source, files, "# comment\r\nfile:///tmp/alpha%20beta.txt\r\nfile://localhost/tmp/a%2Bb.txt\r\n", x, y, 1) &&
        seen.calls == 2 && seen.matches &&
        transfer(app, d, target, source, files, "file://remote/tmp/no.txt\r\n", x, y, 0) && seen.calls == 2;
    XDestroyWindow(d, source);
    return passed ? 0 : 43;
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        return 1;
    }
    const int editor = argc == 3 && strncmp(argv[2], "editor", 6) == 0;
    if (editor && setenv("XMODIFIERS", "@im=none", 1) != 0) {
        return 1;
    }
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        return 1;
    }
    ui_app* app = NULL;
    int result = 1;
    const int slider = argc == 3 && strcmp(argv[2], "slider") == 0;
    const int drop = argc == 3 && strcmp(argv[2], "drop") == 0;
    const int explicit_gpu = argc == 3 && !slider && !drop && strncmp(argv[2], "auto", 4) != 0 && strcmp(argv[2], "editor") != 0;
    const int expect_failure = argc == 3 && strcmp(argv[2], "failure") == 0;
    ui_app_options options = {sizeof(options), UI_API_VERSION,
        explicit_gpu ? UI_GLES3 : (argc == 3 && !editor && !slider && !drop ? UI_AUTO : UI_SOFTWARE), 0};
    if (ui_app_create(&options, &app) == UI_OK) {
        const ui_result opened = drop && ui_app_set_window_options(app, "Sunfleck Xdnd test", 500, 300) != UI_OK
            ? UI_ERROR_INVALID_STATE : ui_app_open_window(app);
        if (expect_failure) {
            XSync(display, False);
            result = opened == UI_ERROR_PLATFORM && !find_window(display) ? 0 : 10;
        } else if (opened == UI_OK) {
            if (editor) {
                result = verify_editor(app, display);
            } else if (slider) {
                result = verify_slider(app, display, atoi(argv[1]));
            } else if (drop) {
                result = verify_drop(app, display);
            } else {
                result = argc == 3 && (strcmp(argv[2], "swap") == 0 || strcmp(argv[2], "auto-swap") == 0)
                    ? verify_swap_failure(app, display, strcmp(argv[2], "auto-swap") == 0) : verify(app, display, atoi(argv[1]));
            }
        }
    }
    ui_app_destroy(app);
    XCloseDisplay(display);
    if (result) {
        fprintf(stderr, "X11 smoke check %d failed\n", result);
    }
    return result;
}
