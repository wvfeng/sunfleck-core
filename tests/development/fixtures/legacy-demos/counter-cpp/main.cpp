#include "FleckFile.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    try {
        const bool exporting = argc == 3 && std::string_view(argv[1]) == "--export";
        const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE,
            exporting ? unsigned{UI_HEADLESS} : unsigned{UI_DEBUG_PIPE}};
        sunfleck::App app(options);
        CounterModel counter(app);
        auto bindings = sunfleck::generated::counter_cpp::bind(app, counter);
        if (exporting) {
            std::ofstream file(std::filesystem::u8path(argv[2]), std::ios::binary);
            file << "{\"version\":1,\"models\":[" << bindings->properties().describe("CounterModel") << "]}\n";
            file.close();
            return file.fail() ? 1 : 0;
        }
        FleckFile page(app, *bindings, std::filesystem::u8path(argc > 1 ? argv[1] : "main.fleck"));
        app.openWindow();
        while (app.poll(50) != UI_EXITED) {
            if (auto error = bindings->takeError()) { std::rethrow_exception(error); }
            page.checkForChanges();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
