#include <sunfleck/ui.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include "height.hpp"

class HeightModel {};

extern "C" ui_result load_height_c(ui_app*, ui_fleck_diagnostic*);
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static std::string read(const char* path) { std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}}; }
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected XML and resource paths");
        const auto xml = read(argv[1]), binary = read(argv[2]);
        for (int mode = 0; mode < 4; ++mode) {
            sunfleck::App app(UI_SOFTWARE, true);
            HeightModel model;
            std::unique_ptr<sunfleck::FleckPage<HeightModel>> generated;
            ui_fleck_diagnostic diagnostic{};
            if (mode == 0) app.loadFleck(xml.c_str());
            if (mode == 1) require(ui_app_load_fleck_resource(app.nativeHandle(), binary.data(), binary.size(), &diagnostic) == UI_OK, diagnostic.message);
            if (mode == 2) require(load_height_c(app.nativeHandle(), &diagnostic) == UI_OK, diagnostic.message);
            if (mode == 3) generated = sunfleck::generated::load_height_c::load(app, model);
            app.openWindow(); app.poll(0);
            auto bounds = [&](const char* id) { return app.findFleckNode(id).nativeHandle()->bounds; };
            require(bounds("short").bottom - bounds("short").top == 10, "Explicit TextBlock height");
            require(bounds("auto").bottom - bounds("auto").top == 48, "Automatic measurement regressed");
            require(bounds("auto").top == bounds("short").bottom, "Stack spacing includes old height");
            require(bounds("inside").bottom - bounds("inside").top == 12, "Grid child stretched despite explicit height");
            require(bounds("zero").bottom == bounds("zero").top, "Zero height");
            require(bounds("last").top == bounds("row").bottom + 3, "Container measurement and top margin");
            require(bounds("last").right - bounds("last").left == 100, "Compiled width");
            require(bounds("last").right == bounds("root").right - 4, "Compiled right alignment and margin");
            require(app.findFleckNode("last").minWidth() == 80 && app.findFleckNode("last").maxHeight() == 30, "Compiled size constraints");
            auto node = app.findFleckNode("short");
            node.setHeight(23); app.poll(0);
            require(node.height() == 23 && bounds("short").bottom - bounds("short").top == 23, "Native height setter");
            require(ui_node_set_height(node.nativeHandle(), -2) == UI_ERROR_INVALID_ARGUMENT, "Negative height accepted");
            require(ui_node_set_height(node.nativeHandle(), 4097) == UI_ERROR_INVALID_ARGUMENT, "Oversized height accepted");
            app.refreshFleck(xml.c_str()); app.poll(0);
            require(app.findFleckNode("short").height() == 10, "Refresh failed to apply new layout");
            require(ui_host_resize(app.nativeHandle(), 640, 480, 192) == UI_OK, "DPI resize");
            app.poll(0);
            require(bounds("short").bottom - bounds("short").top == 20, "Height must use logical pixels");
        }
        for (const char* value : {"bad", "-1", "10000001", "{Binding H, Mode=TwoWay}"}) {
            fleck::Page page; ui_fleck_diagnostic diagnostic{};
            auto invalid = std::string("<Window xmlns='urn:sunfleck:v1'><TextBlock Height='") + value + "'/></Window>";
            require(!fleck::parse(invalid, page, diagnostic), "Invalid Height accepted by native parser");
        }
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        require(fleck::parse("<Window xmlns='urn:sunfleck:v1'><TextBlock Height='Auto'/></Window>", page, diagnostic), "Auto height rejected");
        require(fleck::parse("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='current' Height='10'/></Window>", page, diagnostic), "Current tree fixture");
        auto tree = fleck::compile_resource(page);
        require(tree[5] == UI_FLECK_RESOURCE_VERSION, "Unified resource version");
        sunfleck::App current(UI_SOFTWARE, true);
        require(ui_app_load_fleck_resource(current.nativeHandle(), tree.data(), tree.size(), &diagnostic) == UI_OK, "Current resource rejected");
        require(current.findFleckNode("current").height() == 10, "Current resource height");
        for (int version : {5, 6, 7, 8, 9}) {
            auto old = tree; old[5] = char(version);
            require(ui_app_load_fleck_resource(current.nativeHandle(), old.data(), old.size(), &diagnostic) == UI_ERROR_VERSION_MISMATCH, "Old tree version must fail explicitly");
            require(current.findFleckNode("current").height() == 10, "Rejected version preserves previous page");
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
