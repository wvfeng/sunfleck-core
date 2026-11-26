#pragma once
#include <sunfleck/ui.h>
#include <initializer_list>
#include <utility>

namespace hosted_window {
// Restore geometry belongs to the window state, independently of a pointer
// gesture. All stored coordinates use the view's current device-pixel scale.
struct State {
    int drag = 0, grab_x = 0, grab_y = 0, minimized_height = 0;
    ui_rect drag_origin{}, normal_bounds{};

    void scale(unsigned before, unsigned after)
    {
        const auto value = [&](int n) { return static_cast<int>(int64_t(n) * after / before); };
        for (auto* rect : {&drag_origin, &normal_bounds}) {
            rect->x = value(rect->x); rect->y = value(rect->y);
            rect->width = value(rect->width); rect->height = value(rect->height);
        }
        grab_x = value(grab_x); grab_y = value(grab_y);
        minimized_height = value(minimized_height);
    }

    void toggle_maximize(ui_rect& bounds, bool& maximized, int width, int height)
    {
        if (minimized_height) { bounds.height = std::exchange(minimized_height, 0); }
        if (maximized) { bounds = normal_bounds; }
        else { normal_bounds = bounds; bounds = {0, 0, width, height}; }
        maximized = !maximized;
    }

    void toggle_minimize(ui_rect& bounds, int caption_height)
    {
        if (minimized_height) { bounds.height = std::exchange(minimized_height, 0); }
        else { minimized_height = bounds.height; bounds.height = caption_height; }
    }
};
}
