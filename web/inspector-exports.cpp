#include <sunfleck/web_inspector.h>
#include <emscripten.h>
#include <string>

namespace {
ui_app* inspected_app = nullptr;
std::string snapshot;
}

extern "C" {
void sunfleck_web_debug_bind(ui_app* app)
{
    inspected_app = app;
}

EMSCRIPTEN_KEEPALIVE const char* sunfleck_web_debug_snapshot()
{
    if (!inspected_app) { return nullptr; }
    size_t size = snapshot.size();
    if (size) {
        const auto result = ui_app_get_debug_snapshot(inspected_app, snapshot.data(), &size);
        if (result == UI_OK) { return snapshot.c_str(); }
        if (result != UI_ERROR_INVALID_ARGUMENT || size <= snapshot.size()) { return nullptr; }
    } else if (ui_app_get_debug_snapshot(inspected_app, nullptr, &size) != UI_OK) {
        return nullptr;
    }
    snapshot.resize(size);
    return ui_app_get_debug_snapshot(inspected_app, snapshot.data(), &size) == UI_OK ? snapshot.c_str() : nullptr;
}

EMSCRIPTEN_KEEPALIVE int sunfleck_web_debug_highlight(uint32_t low, uint32_t high)
{
    return ui_app_set_debug_highlight(inspected_app, (uint64_t{high} << 32) | low, 500);
}

EMSCRIPTEN_KEEPALIVE int sunfleck_web_debug_pick(uint32_t enabled)
{
    return ui_app_set_debug_picking(inspected_app, enabled);
}

EMSCRIPTEN_KEEPALIVE int sunfleck_web_debug_pointer(uint32_t view, uint32_t action, int32_t x, int32_t y)
{
    return ui_host_debug_pointer(inspected_app, view, static_cast<ui_pointer_action>(action), x, y);
}
}
