#pragma once

#include "gpu_surface.h"
#include <memory>
#include <string>
#include <string_view>

namespace macos {
struct Event {
    enum Kind { resize, focus, close, pointer, context_menu, scroll, key, text, copy, cut } kind;
    int x = 0, y = 0, value = 0;
    unsigned modifiers = 0;
    std::string_view characters;
};

enum Key {
    left = 128, right, up, down, home, end, forward_delete
};

class Window {
public:
    using Callback = bool (*)(void*, const Event&, std::string*);

    Window();
    ~Window();
    bool open(const char* title, int width, int height, void* context, Callback callback);
    void show();
    void title(const char* title);
    bool visible() const;
    void move(int x, int y);
    ui_rect bounds() const;
    void convert_point(double& x, double& y, bool to_screen) const;
    void present(int width, int height, std::span<const uint8_t> pixels);
    std::unique_ptr<GpuSurface> metal();
    static bool poll(unsigned timeout_ms);

private:
    struct State;
    std::unique_ptr<State> state_;
};

}
