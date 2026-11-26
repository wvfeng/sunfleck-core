#include <sunfleck/property_model.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

using Clock = std::chrono::steady_clock;
template<class Work> double timed(Work work)
{
    const auto start = Clock::now(); work();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
static size_t privateBytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return counters.PrivateUsage;
#endif
    return 0;
}
int main(int argc, char** argv)
{
    try {
        const int nodes = argc > 1 ? std::stoi(argv[1]) : 100;
        if (nodes < 1 || nodes > 10000) return 1;
        const auto memoryBefore = privateBytes();
        const auto start = Clock::now();
        sunfleck::App app(UI_SOFTWARE, true);
        sunfleck::PropertyModel model(app);
        int count = 0;
        model.property("Count", [&] { return count; });
        ui_command* raw = nullptr;
        sunfleck::detail::check(ui_command_create(app.nativeHandle(), [](ui_node*, void* value) {
            ++*static_cast<int*>(value);
        }, &count, &raw));
        std::unique_ptr<ui_command, decltype(&ui_command_destroy)> command(raw, ui_command_destroy);
        model.command("Increment", raw);
        std::string xml = "<Window xmlns='urn:sunfleck:v1' Width='800' Height='600'><StackPanel Spacing='1'><Button Command='{Binding Increment}' Content='Increment'/>";
        for (int i = 0; i < nodes; ++i) xml += "<TextBlock Id='n" + std::to_string(i) + "' Text='{Binding Count}'/>";
        xml += "</StackPanel></Window>";
        sunfleck::detail::check(ui_app_load_fleck_with_model(app.nativeHandle(), xml.data(), xml.size(), model.nativeHandle(), nullptr, nullptr));
        app.openWindow(); app.poll(0);
        const auto startup = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const auto memoryLoaded = privateBytes();
        std::vector<double> frames, refreshes, idle;
        for (int i = 0; i < 55; ++i) {
            const auto frame = timed([&] { app.activateButton(); model.notify("Count"); app.poll(0); });
            if (app.findFleckNode("n0").text() != std::to_string(count)) throw std::runtime_error("Binding failed during benchmark");
            const auto refresh = timed([&] { app.refreshFleck(xml, model.nativeHandle()); app.poll(0); });
            const auto noChange = timed([&] { app.poll(0); });
            if (i >= 5) { frames.push_back(frame); refreshes.push_back(refresh); idle.push_back(noChange); }
        }
        auto array = [](const auto& values) {
            std::cout << '[';
            for (size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << values[i]; }
            std::cout << ']';
        };
        std::cout << "{\"nodes\":" << nodes << ",\"viewport\":[800,600],\"warmup\":5,\"samples\":50,\"startupMs\":" << startup
            << ",\"privateBeforeBytes\":" << memoryBefore << ",\"privateLoadedBytes\":" << memoryLoaded
            << ",\"privateAfterBytes\":" << privateBytes() << ",\"commandNotifyFrameMs\":";
        array(frames); std::cout << ",\"refreshFrameMs\":"; array(refreshes);
        std::cout << ",\"idlePollMs\":"; array(idle); std::cout << "}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
