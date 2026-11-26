#include "FleckFile.hpp"
#include <iostream>

static void require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}

int main(int argc, char** argv)
{
    if (argc != 2) { return 1; }
    const auto path = std::filesystem::current_path() /
        ("counter-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".fleck");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    try {
        std::ifstream input(std::filesystem::u8path(argv[1]), std::ios::binary);
        require(bool(input), "Cannot read test page");
        std::string source{std::istreambuf_iterator<char>(input), {}};
        auto save = [&](const std::string& text) {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << text; file.close();
            require(!file.fail(), "Cannot save test page");
        };
        save(source);
        sunfleck::App app(UI_SOFTWARE, true);
        CounterModel model(app);
        auto bindings = sunfleck::generated::counter_cpp::bind(app, model);
        FleckFile page(app, *bindings, path);
        app.openWindow(); app.poll(0);
        auto settle = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                app.poll(1); if (auto error = bindings->takeError()) { std::rethrow_exception(error); }
                if (!bindings->properties().running() && !app.fleckRefreshStatus().pending) { return; }
            } while (std::chrono::steady_clock::now() < deadline);
            throw std::runtime_error("Command or refresh did not finish");
        };
        require(app.findFleckNode("syncCount").text() == "同步次数：0", "Initial binding failed");
        app.activateButton();
        require(model.SyncCount() == 1 && app.findFleckNode("syncCount").text() == "同步次数：1", "Sync command failed");

        const auto root = app.fleckRoot().nativeHandle();
        save("<Window"); page.checkForChanges(); settle();
        require(app.fleckRefreshStatus().result != UI_OK && app.fleckRoot().nativeHandle() == root,
            "Invalid saved page did not retain the previous page");

        // Remove the sync button so activateButton exercises the real async binding.
        const auto begin = source.find("    <Button Id=\"sync\"");
        require(begin != std::string::npos, "Sync button not found");
        source.erase(begin, source.find('\n', begin) - begin + 1);
        save(source); page.checkForChanges(); settle();
        require(app.fleckRefreshStatus().result == UI_OK && model.SyncCount() == 1, "Valid save lost model state");
        app.activateButton(); app.activateButton();
        require(bindings->properties().running(), "Async command was not started");
        require(model.AsyncCount() == 0, "Async command blocked the UI");
        const auto countNode = app.findFleckNode("asyncCount").nativeHandle();
        const auto caption = source.find("Spacing=\"12\"");
        require(caption != std::string::npos, "Layout not found");
        source.replace(caption, 12, "Spacing=\"16\"");
        save(source); page.checkForChanges();
        require(app.fleckRefreshStatus().pending, "Busy refresh was not queued");
        settle();
        require(model.AsyncCount() == 1 && app.findFleckNode("asyncCount").text() == "异步次数：1",
            "Async single-flight or UI notification failed");
        require(app.fleckRefreshStatus().result == UI_OK && app.findFleckNode("asyncCount").nativeHandle() == countNode,
            "Queued refresh did not preserve node identity");
        const auto metadata = bindings->properties().describe("CounterModel");
        require(metadata.find("IncrementAsync") != std::string::npos && metadata.find("main.fleck") != std::string::npos,
            "Model metadata or source location missing");
        app.activateButton();
        app.requestClose();
        require(app.poll(0) == UI_EXITED, "Closing during async work failed");
        std::cout << "Bindings, sync/async commands, single-flight, file refresh, rollback, state and cancellation passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

