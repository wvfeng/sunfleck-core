#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <source_location>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

extern "C" ui_result load_node_scale(ui_app*, ui_fleck_diagnostic*);

static void require(bool condition, const char* message = "Node scale assertion failed",
    const std::source_location where = std::source_location::current())
{
    if (!condition) { throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line())); }
}

static std::string read(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    require(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

#ifdef _WIN32
// Observe the real desktop input path without consuming or synthesizing messages.
struct InputTrace {
    inline static InputTrace* active = nullptr;
    HWND window;
    WNDPROC original;
    ui_node* list;
    std::ofstream stream;
    unsigned entries = 0;
    MSG last_pending{};

    InputTrace(const std::string& title, ui_node* node, const std::filesystem::path& path)
        : window(FindWindowA("SunfleckPreview", title.c_str())), list(node), stream(path)
    {
        DWORD process = 0;
        require(!active && stream.good() && window &&
            GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId());
        original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(observe)));
        require(original != nullptr);
        active = this;
        stream << "phase,message,wparam,lparam,focus,foreground,offset,value,hwnd,class" << std::endl;
    }

    ~InputTrace()
    {
        if (IsWindow(window)) {
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        }
        active = nullptr;
    }

    void record(const char* phase, HWND target, UINT message, WPARAM wp, LPARAM lp)
    {
        if (++entries > 2000) { return; }
        char name[128]{};
        GetClassNameA(target, name, sizeof(name));
        stream << phase << ',' << message << ',' << wp << ',' << lp << ',' << GetFocus() << ','
               << GetForegroundWindow() << ',' << list->offset << ',' << list->value << ','
               << target << ',' << name << std::endl;
    }

    void pending()
    {
        MSG next{};
        if (PeekMessageW(&next, nullptr, 0, 0, PM_NOREMOVE) &&
            (next.hwnd != last_pending.hwnd || next.message != last_pending.message ||
             next.time != last_pending.time)) {
            record("pending", next.hwnd, next.message, next.wParam, next.lParam);
            last_pending = next;
        }
    }

    static LRESULT CALLBACK observe(HWND window, UINT message, WPARAM wp, LPARAM lp)
    {
        auto& trace = *active;
        const bool input = message == WM_MOUSEWHEEL || message == WM_LBUTTONDOWN ||
            message == WM_LBUTTONUP || message == WM_KEYDOWN || message == WM_KEYUP ||
            message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_CLOSE ||
            message == WM_CAPTURECHANGED;
        if (input) { trace.record("before", window, message, wp, lp); }
        const auto result = CallWindowProcW(trace.original, window, message, wp, lp);
        if (input) { trace.record("after", window, message, wp, lp); }
        return result;
    }
};

static void thread_helper_messages(sunfleck::App& app)
{
    WNDCLASSW helper_class{};
    helper_class.hInstance = GetModuleHandleW(nullptr);
    helper_class.lpszClassName = L"SunfleckScaleInputHelper";
    helper_class.lpfnWndProc = [](HWND window, UINT message, WPARAM wp, LPARAM lp) -> LRESULT {
        if (message == WM_APP + 1) {
            *reinterpret_cast<bool*>(lp) = true;
            return 0;
        }
        return DefWindowProcW(window, message, wp, lp);
    };
    require(RegisterClassW(&helper_class) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
    const auto helper = CreateWindowExW(0, helper_class.lpszClassName, L"Input helper", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, helper_class.hInstance, nullptr);
    require(helper != nullptr);
    bool received = false;
    require(PostMessageW(helper, WM_APP + 1, 0, reinterpret_cast<LPARAM>(&received)));
    const auto status = app.poll(0);
    DestroyWindow(helper);
    require(status == UI_OK && received);
}

static void scrollbar_wheel_themes()
{
    sunfleck::App app(UI_SOFTWARE);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1' Title='Sunfleck wheel themes' Width='400' Height='240'>"
                  "<NavigationView><TabItem Text='Overlays'><ScrollView Id='scroll' HorizontalScrollBarVisibility='Auto'>"
                  "<Canvas Width='750' Height='800'/></ScrollView></TabItem></NavigationView></Window>");
    auto scroll = app.findFleckNode("scroll");
    auto* node = scroll.nativeHandle();
    app.openWindow();
    const auto window = FindWindowA("SunfleckPreview", "Sunfleck wheel themes");
    DWORD process = 0;
    require(window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId());
    const auto wheel = [&](UINT message, int delta, int x, int y) {
        POINT point{x, y};
        require(ClientToScreen(window, &point));
        SendMessageW(window, message, MAKEWPARAM(0, static_cast<WORD>(delta)), MAKELPARAM(point.x, point.y));
    };
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        app.setTheme(static_cast<ui_builtin_theme>(id));
        scroll.scrollTo(0, 0);
        require(app.poll(0) == UI_OK && node->scroll_limit > 0 && node->horizontal_limit > 0);
        const auto bar = node->scrollbar;
        const int x = (bar.left + bar.right) / 2, y = (bar.top + bar.bottom) / 2;
        wheel(WM_MOUSEWHEEL, -WHEEL_DELTA, x, y);
        const double full_step = node->offset;
        require(full_step > 0, "One wheel detent scrolls over the vertical track");
        wheel(WM_MOUSEWHEEL, WHEEL_DELTA / 4, x, y);
        require(node->offset == full_step * 0.75, "A partial wheel detent retains its distance");
        wheel(WM_MOUSEHWHEEL, WHEEL_DELTA / 4, x, y);
        require(node->horizontal_offset == full_step * 0.25 && node->offset == full_step * 0.75,
                "Partial horizontal wheel input uses the horizontal axis");
        require(app.poll(0) == UI_OK);
        const auto thumb = node->scrollbar_thumb;
        const int thumb_y = (thumb.top + thumb.bottom) / 2;
        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, thumb_y));
        SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, bar.bottom - 1));
        SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(x, bar.bottom - 1));
        require(node->offset == node->scroll_limit && !node->dragging, "Thumb drag still reaches the end");
    }
    app.requestClose();
    require(app.poll(0) == UI_EXITED);
}

static void scrollbar_resize_during_drag()
{
    for (const auto kind : {UI_SCROLL_VIEW, UI_LIST_VIEW, UI_TREE_VIEW, UI_DATA_GRID}) {
        sunfleck::App app(UI_SOFTWARE);
        auto root = app.createNode(kind);
        for (unsigned i = 0; i < 10; ++i) {
            auto item = app.createNode(kind == UI_LIST_VIEW ? UI_LIST_ITEM : kind == UI_TREE_VIEW ? UI_TREE_ITEM :
                kind == UI_DATA_GRID ? UI_GRID : UI_TEXT_BLOCK);
            root.append(item);
            if (kind == UI_DATA_GRID) { item.append(app.createNode(UI_TEXT_BLOCK)); }
        }
        app.setRoot(root);
        app.setWindowOptions("Sunfleck scrollbar resize", 300, 180);
        app.openWindow();
        require(app.poll(0) == UI_OK);
        const auto window = FindWindowA("SunfleckPreview", "Sunfleck scrollbar resize");
        DWORD process = 0;
        require(window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId());
        auto* node = root.nativeHandle();
        const auto thumb = node->scrollbar_thumb;
        require(node->scroll_limit > 0 && thumb.bottom > thumb.top, "Resize fixture has no scrollbar");
        const int x = (thumb.left + thumb.right) / 2, y = (thumb.top + thumb.bottom) / 2;
        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        require(node->dragging, "Resize fixture did not begin dragging");
        require(SetWindowPos(window, nullptr, 0, 0, 300, 1000, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
        require(app.poll(0) == UI_OK && node->scroll_limit == 0, "Enlarged resize fixture still overflows");
        SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, 150));
        SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(x, 150));
        require(node->value == 0 && node->offset == 0 && !node->dragging, "Resize during drag changed value/offset or retained dragging");
        app.requestClose();
        require(app.poll(0) == UI_EXITED);
    }
}
#endif

static void scrollbar_geometry()
{
    for (unsigned dpi : {96u, 144u, 192u})
    for (const auto kind : {UI_SCROLL_VIEW, UI_LIST_VIEW, UI_TREE_VIEW, UI_DATA_GRID}) {
        ui_node root{};
        root.kind = kind;
        std::vector<ui_node> items(30);
        std::vector<ui_node> cells(30);
        for (std::size_t i = 0; i < items.size(); ++i) {
            auto& item = items[i];
            item.kind = kind == UI_LIST_VIEW ? UI_LIST_ITEM : kind == UI_TREE_VIEW ? UI_TREE_ITEM :
                kind == UI_DATA_GRID ? UI_GRID : UI_TEXT_BLOCK;
            item.parent = &root;
            item.text = L"Row";
            if (kind == UI_DATA_GRID) {
                cells[i].kind = UI_TEXT_BLOCK;
                cells[i].parent = &item;
                cells[i].text = L"Cell";
                item.children.push_back(&cells[i]);
            }
            root.children.push_back(&item);
        }
        const auto draw = [&](int width, int height) {
            std::vector<FillRectCommand> commands;
            std::vector<ui_node*> interactive, scrollable;
            render_tree(root, {0, 0, width, height}, builtin_palette(UI_BUILTIN_FLUENT_2, false, false),
                builtin_themes[UI_BUILTIN_FLUENT_2].shape, dpi, nullptr, false, false,
                nullptr, nullptr, commands, interactive, scrollable);
        };
        draw(240, 180);
        require(root.scroll_limit > 0 && root.scrollbar.right > root.scrollbar.left);
        require(root.scrollbar_thumb.top == root.scrollbar.top);
        root.offset = root.scroll_limit / 2;
        draw(240, 180);
        require(root.scrollbar_thumb.top > root.scrollbar.top && root.scrollbar_thumb.bottom < root.scrollbar.bottom,
            "Partial scroll keeps thumb within the track");
        root.offset = root.scroll_limit;
        draw(240, 180);
        require(root.scrollbar_thumb.bottom == root.scrollbar.bottom);
        require(root.scrollbar_thumb.bottom > root.scrollbar_thumb.top);
        require(items.back().visible.bottom > items.back().visible.top);
        draw(1, 180);
        require(root.scrollbar.right >= root.scrollbar.left && items.front().bounds.right >= items.front().bounds.left);
        draw(240, 100000);
        require(root.scroll_limit == 0 && root.offset == 0 && root.scrollbar.right == root.scrollbar.left);
    }
}

static void desktop(const char* xml_path, const char* binary_path, const char* output_path, bool native_input)
{
    const auto xml = read(xml_path), binary = read(binary_path);
    auto output = std::filesystem::absolute(output_path);
    if (native_input) {
#ifndef _WIN32
        throw std::runtime_error("Native input driver currently requires Windows");
#endif
        output /= "run-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    std::filesystem::create_directories(output);
    std::cout << "Desktop evidence: " << output.string() << std::endl;
    std::ofstream result(output / "result.log");
    require(result.good());
    result << "input," << (native_input ? "win32-messages" : "external") << std::endl;
    for (unsigned mode = 0; mode < 3; ++mode) {
        sunfleck::App app(UI_SOFTWARE);
        ui_fleck_diagnostic diagnostic{};
        const auto status = mode == 0 ? ui_app_load_fleck(app.nativeHandle(), xml.data(), xml.size(), &diagnostic)
            : mode == 1 ? ui_app_load_fleck_resource(app.nativeHandle(), binary.data(), binary.size(), &diagnostic)
            : load_node_scale(app.nativeHandle(), &diagnostic);
        require(status == UI_OK);
        auto list = app.findFleckNode("list");
        auto& children = list.nativeHandle()->children;
        require(children.size() == 4096);
        require(ui_node_set_text(children.front(), "First item 0") == UI_OK);
        require(ui_node_set_text(children.back(), "Last item 4095") == UI_OK);
        list.setValue(0);
        const auto title = "Sunfleck scale 4096 - " + std::string(mode == 0 ? "XML" : mode == 1 ? "binary" : "generated C");
        app.setWindowOptions(title.c_str(), 500, 440);
        app.openWindow();
#ifdef _WIN32
        InputTrace trace(title, list.nativeHandle(), output / ("input-" + std::to_string(mode) + ".csv"));
#endif
        result << mode << ",children," << children.size() << std::endl;
        unsigned previous = 0, changes = 0, input_stage = 0;
        bool selected_large_index = false;
        const auto started = std::chrono::steady_clock::now();
        for (;;) {
            const auto polled = app.poll(16);
            if (polled == UI_EXITED) { break; }
            require(polled == UI_OK || polled == UI_TIMEOUT);
#ifdef _WIN32
            trace.pending();
#endif
            const auto selected = list.value();
            if (selected != previous) {
                ++changes;
                selected_large_index |= selected > 511;
                result << mode << ",selected," << selected << std::endl;
                previous = selected;
            }
#ifdef _WIN32
            if (native_input) {
                require(std::chrono::steady_clock::now() - started < std::chrono::seconds(20));
                const auto window = FindWindowA("SunfleckPreview", title.c_str());
                DWORD process = 0;
                require(window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId());
                if (input_stage == 0) {
                    thread_helper_messages(app);
                    result << mode << ",threadHelperMessage,passed" << std::endl;
                    auto* node = list.nativeHandle();
                    const auto drag = [&](bool bottom) {
                        const auto bar = node->scrollbar, thumb = node->scrollbar_thumb;
                        require(bar.right > bar.left && thumb.bottom > thumb.top);
                        const int x = (bar.left + bar.right) / 2;
                        const int y = (thumb.top + thumb.bottom) / 2;
                        const int target = bottom ? bar.bottom - 1 : bar.top;
                        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
                        SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, target));
                        SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(x, target));
                        require(node->offset == (bottom ? node->scroll_limit : 0) && list.value() == 0);
                        require(app.poll(0) == UI_OK);
                    };
                    drag(true);
                    require(children.back()->visible.bottom > children.back()->visible.top);
                    drag(false);
                    const auto bar = node->scrollbar;
                    SendMessageW(window, WM_LBUTTONUP, 0,
                        MAKELPARAM((bar.left + bar.right) / 2, (bar.top + bar.bottom) / 2));
                    require(node->offset > 0 && node->offset < node->scroll_limit && list.value() == 0);
                    SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(bar.left, bar.top));
                    require(node->offset == 0 && list.value() == 0);
                    require(app.poll(0) == UI_OK);
                    result << mode << ",scrollbarDragAndTrack,passed" << std::endl;
                    // Exercise the native wheel path; do not set selection or offset through APIs.
                    const auto bounds = list.nativeHandle()->visible;
                    POINT point{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
                    require(ClientToScreen(window, &point));
                    unsigned wheels = 0;
                    while (list.nativeHandle()->offset < list.nativeHandle()->scroll_limit) {
                        const auto before = list.nativeHandle()->offset;
                        SendMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA * 240)), MAKELPARAM(point.x, point.y));
                        require(list.nativeHandle()->offset > before && ++wheels < 1000);
                    }
                    require(wheels > 0);
                    result << mode << ",wheelMessages," << wheels << std::endl;
                } else if (input_stage == 1) {
                    const auto bounds = children.back()->visible;
                    require(bounds.right > bounds.left && bounds.bottom > bounds.top);
                    SetFocus(window);
                    require(GetFocus() == window);
                    const auto point = MAKELPARAM((bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2);
                    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, point);
                    SendMessageW(window, WM_LBUTTONUP, 0, point);
                } else if (input_stage == 2 || input_stage == 3) {
                    require(selected == (input_stage == 2 ? 4095u : 4094u));
                    const auto key = input_stage == 2 ? VK_LEFT : VK_RIGHT;
                    SendMessageW(window, WM_KEYDOWN, key, 0);
                    SendMessageW(window, WM_KEYUP, key, 0);
                } else if (input_stage == 4) {
                    require(selected == 4095 && changes == 3);
                    SendMessageW(window, WM_CLOSE, 0, 0);
                }
                ++input_stage;
            }
#endif
        }
        require(changes >= 2 && selected_large_index && list.value() == 4095);
        std::ofstream snapshot(output / ("final-" + std::to_string(mode) + ".json"));
        snapshot << app.debugSnapshot();
        require(snapshot.good());
        result << mode << ",passed" << std::endl;
        std::cout << mode << ",passed,changes=" << changes << ",final=" << list.value() << std::endl;
    }
#ifdef _WIN32
    if (native_input) {
        scrollbar_wheel_themes();
        result << "scrollbarWheelThemes,passed" << std::endl;
        scrollbar_resize_during_drag();
        result << "resizeDuringDrag,passed" << std::endl;
        sunfleck::App app(UI_SOFTWARE);
        app.openWindow();
        auto second = app.createWindow();
        second.withContext([&] { app.openWindow(); });
        require(app.poll(0) == UI_OK, "Quit fixture did not open");
        PostQuitMessage(17);
        ui_result status = UI_TIMEOUT;
        for (unsigned attempt = 0; attempt < 20 && status != UI_EXITED; ++attempt) {
            status = app.poll(0);
            require(status == UI_OK || status == UI_TIMEOUT || status == UI_EXITED, "Quit poll failed");
        }
        require(status == UI_EXITED, "Thread quit did not close the app");
        MSG quit{};
        bool retained = false;
        // PostQuitMessage is low priority. Win32 may still have teardown messages
        // queued (observed message 0x60); drain them as the outer loop would before
        // requiring WM_QUIT. A filtered, single Peek can return no message here.
        for (unsigned attempt = 0; attempt < 256 && PeekMessageW(&quit, nullptr, 0, 0, PM_REMOVE); ++attempt) {
            if (quit.message == WM_QUIT) { retained = true; break; }
            TranslateMessage(&quit);
            DispatchMessageW(&quit);
        }
        require(retained && quit.wParam == 17, "Thread quit exit code was not preserved after multi-window cleanup");
        result << "threadQuitPreserved,passed" << std::endl;
    }
#endif
}

int main(int argc, char** argv)
{
    try {
        if ((argc == 5 || (argc == 6 && std::string_view(argv[5]) == "--native-input")) && std::string_view(argv[3]) == "--desktop") {
            desktop(argv[1], argv[2], argv[4], argc == 6);
            return 0;
        }
        require(argc == 3);
        scrollbar_geometry();
        const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
        for (unsigned count : {511u, 512u, 513u, 4096u}) {
            sunfleck::App app(options);
            auto root = app.createNode(UI_LIST_VIEW);
            for (unsigned i = 0; i < count; ++i) { root.append(app.createNode(UI_LIST_ITEM)); }
            root.setValue(count - 1);
            require(root.value() == count - 1);
            require(ui_node_set_value(root.nativeHandle(), count) == UI_ERROR_INVALID_ARGUMENT);
            app.setRoot(root);
            app.openWindow();
            require(app.poll(0) == UI_OK);

            sunfleck::App batch(options);
            auto list = batch.createNode(UI_COMBO_BOX);
            const std::vector<const char*> labels(count, "Item");
            list.setItems(labels.data(), count);
            list.setValue(count - 1);
            require(list.value() == count - 1);
        }
        const auto xml = read(argv[1]);
        const auto binary = read(argv[2]);
        for (unsigned mode = 0; mode < 3; ++mode) {
            sunfleck::App app(options);
            ui_fleck_diagnostic diagnostic{};
            const auto status = mode == 0 ? ui_app_load_fleck(app.nativeHandle(), xml.data(), xml.size(), &diagnostic)
                : mode == 1 ? ui_app_load_fleck_resource(app.nativeHandle(), binary.data(), binary.size(), &diagnostic)
                : load_node_scale(app.nativeHandle(), &diagnostic);
            if (status != UI_OK) { std::cerr << diagnostic.message << '\n'; }
            require(status == UI_OK);
            ui_node* list = nullptr;
            require(ui_app_find_fleck_node(app.nativeHandle(), "list", &list) == UI_OK);
            uint32_t selected = 0;
            require(ui_node_get_value(list, &selected) == UI_OK && selected == 4095);
            require(ui_node_set_value(list, 4096) == UI_ERROR_INVALID_ARGUMENT);
            app.openWindow();
            require(app.poll(0) == UI_OK);
        }
        sunfleck::App invalid(options);
        ui_fleck_diagnostic diagnostic{};
        auto damaged = binary;
        const auto byte = [&](size_t at) { return uint32_t(static_cast<unsigned char>(binary.at(at))); };
        const size_t title_length = byte(14) | (byte(15) << 8) | (byte(16) << 16) | (byte(17) << 24);
        const size_t count_offset = 18 + title_length;
        damaged.replace(count_offset, 4, 4, '\xff');
        require(ui_app_load_fleck_resource(invalid.nativeHandle(), damaged.data(), damaged.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        require(ui_app_load_fleck_resource(invalid.nativeHandle(), binary.data(), binary.size() - 1, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        require(ui_app_load_fleck_resource(invalid.nativeHandle(), binary.data(), 9 * 1024 * 1024 + 1, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        auto invalid_index = xml;
        invalid_index.replace(invalid_index.find("Value='4095'"), 12, "Value='4096'");
        require(ui_app_load_fleck(invalid.nativeHandle(), invalid_index.data(), invalid_index.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        const std::string huge(1024 * 1024 + 1, ' ');
        require(ui_app_load_fleck(invalid.nativeHandle(), huge.data(), huge.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        require(diagnostic.code == UI_FLECK_LIMIT);
        std::string deep = "<Window xmlns='urn:sunfleck:v1'>";
        for (unsigned i = 0; i < 64; ++i) { deep += "<StackPanel>"; }
        for (unsigned i = 0; i < 64; ++i) { deep += "</StackPanel>"; }
        deep += "</Window>";
        require(ui_app_load_fleck(invalid.nativeHandle(), deep.data(), deep.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        require(diagnostic.code == UI_FLECK_LIMIT);
        require(ui_app_load_fleck_resource(invalid.nativeHandle(), binary.data(), binary.size(), &diagnostic) == UI_OK);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
