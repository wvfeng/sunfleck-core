#include <sunfleck/dock.hpp>
#include "dock_theme_generated.hpp"
#include <fstream>
#include <iterator>
#include <cstdio>

extern "C" ui_result load_dock_theme_c(ui_app*, ui_fleck_diagnostic*);

int main(int argc, char** argv)
{
    if (argc != 3) { return 1; }
    try {
        std::ifstream xml_file(argv[1], std::ios::binary), binary_file(argv[2], std::ios::binary);
        const std::string xml{std::istreambuf_iterator<char>(xml_file), {}}, binary{std::istreambuf_iterator<char>(binary_file), {}};
        if (xml.empty() || !binary.starts_with(std::string("FLKTH\x01", 6))) { return 2; }
        sunfleck::App app{UI_SOFTWARE, true}; app.setWindowOptions("Theme compilation", 500, 320);
        auto host = app.createNode(UI_DOCK_HOST); app.setRoot(host); sunfleck::Dock dock(host);
        dock.add("document", "Theme document", app.createNode(UI_TEXT_AREA)); app.openWindow();
        sunfleck::loadTheme(app, xml); sunfleck::detail::check(app.poll(0));
        uint32_t width = 0, height = 0; const auto* pixels = ui_app_pixels(app.nativeHandle(), &width, &height);
        const std::vector<uint8_t> expected(pixels, pixels + static_cast<size_t>(width) * height * 4);
        const auto verify = [&] {
            sunfleck::detail::check(app.poll(0));
            const auto* actual = ui_app_pixels(app.nativeHandle(), &width, &height);
            if (!actual || !std::equal(expected.begin(), expected.end(), actual)) { throw std::runtime_error("Compiled theme pixel mismatch"); }
        };
        sunfleck::loadTheme(app, binary); verify();
        sunfleck::detail::check(load_dock_theme_c(app.nativeHandle(), nullptr)); verify();
        sunfleck::detail::check(load_dock_theme_cpp(app.nativeHandle(), nullptr)); verify();
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 3; }
}
