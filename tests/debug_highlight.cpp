#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try {
        sunfleck::App app(UI_SOFTWARE, true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1' Width='320' Height='240'><StackPanel Id='outer' Padding='20'><Card Id='card' Padding='12'><TextBlock Id='leaf' Text='Hover'/></Card><TextBlock Text='Outside'/></StackPanel></Window>");
        app.openWindow();
        const auto pixels = [&] {
            app.poll(0);
            uint32_t w = 0, h = 0;
            const auto* data = ui_app_pixels(app.nativeHandle(), &w, &h);
            require(data && w == 320 && h == 240, "Missing frame");
            return std::vector<uint8_t>(data, data + w * h * 4);
        };
        const auto baseline = pixels();
        for (const char* name : {"leaf", "card", "outer"}) {
            auto node = app.findFleckNode(name);
            app.setDebugHighlight(node.debugId());
            const auto highlighted = pixels();
            require(highlighted != baseline, "Highlight did not change pixels");
            const auto bounds = node.nativeHandle()->visible;
            for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
                if (x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom) continue;
                for (int c = 0; c < 4; ++c) require(highlighted[(y * 320 + x) * 4 + c] == baseline[(y * 320 + x) * 4 + c], "Highlight escaped clipping bounds");
            }
            require(ui_app_set_debug_highlight(app.nativeHandle(), UINT64_MAX, 1000) == UI_ERROR_INVALID_ARGUMENT, "Invalid ID accepted");
            require(ui_app_set_debug_highlight(app.nativeHandle(), node.debugId(), 5001) == UI_ERROR_INVALID_ARGUMENT, "Unbounded lease accepted");
            app.setDebugHighlight(0);
            require(pixels() == baseline, "Clear left overlay pixels");
        }
        app.setDebugHighlight(app.findFleckNode("card").debugId(), 10);
        require(pixels() != baseline, "Short lease did not render");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        require(pixels() == baseline, "Expired lease did not clear");
        app.setDebugHighlight(app.findFleckNode("leaf").debugId());
        app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock Text='Replacement'/></Window>");
        app.poll(0);
        require(app.debugSnapshot().find("\"highlight\":\"0\"") != std::string::npos, "Retired node highlight survived");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
