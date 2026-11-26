#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sunfleck/ui.hpp>
#include <future>
#include <cstdio>
#include <stdexcept>

static void require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}
static std::string exchange(const std::string& name, const std::string& request)
{
    const auto path = "\\\\.\\pipe\\" + name;
    HANDLE pipe = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    require(pipe != INVALID_HANDLE_VALUE, "client could not connect");
    OVERLAPPED operation{};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    struct Close { HANDLE pipe, event; ~Close() { CloseHandle(pipe); CloseHandle(event); } } close{pipe, operation.hEvent};
    const auto complete = [&](BOOL done, DWORD& count) {
        if (done) { return; }
        require(GetLastError() == ERROR_IO_PENDING, "client I/O failed");
        if (WaitForSingleObject(operation.hEvent, 4000) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &operation);
            GetOverlappedResult(pipe, &operation, &count, TRUE);
            throw std::runtime_error("client I/O timed out");
        }
        require(GetOverlappedResult(pipe, &operation, &count, FALSE), "client I/O completion failed");
    };
    DWORD count;
    complete(WriteFile(pipe, request.data(), static_cast<DWORD>(request.size()), &count, &operation), count);
    std::string response;
    while (response.find('\n') == std::string::npos) {
        ResetEvent(operation.hEvent);
        char buffer[4096];
        complete(ReadFile(pipe, buffer, sizeof(buffer), &count, &operation), count);
        require(count != 0, "empty response");
        response.append(buffer, count);
    }
    return response;
}
int main()
{
    try {
        const auto pipe = "sunfleck-native-test-" + std::to_string(GetCurrentProcessId());
        sunfleck::App app(UI_SOFTWARE, true), other(UI_SOFTWARE, true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><TextBox Id='editor' Text='Native snapshot'/></Window>");
        app.openWindow();
        app.enableDebugServer(pipe.c_str());
        app.enableDebugServer(pipe.c_str());
        require(ui_app_enable_debug_server(other.nativeHandle(), pipe.c_str()) == UI_ERROR_PLATFORM, "name conflict was not synchronous");
        require(ui_app_enable_debug_server(app.nativeHandle(), "bad/name") == UI_ERROR_INVALID_ARGUMENT, "invalid name accepted");
        other.enableDebugServer((pipe + "-other").c_str());
        require(ui_app_enable_debug_server(app.nativeHandle(), (pipe + "-other").c_str()) == UI_ERROR_PLATFORM, "occupied replacement accepted");
        auto ask = [&](const std::string& request) {
            // Retire a disconnected client and arm the next connection.
            for (int i = 0; i < 4; ++i) { app.poll(0); }
            auto answer = std::async(std::launch::async, [&] { return exchange(pipe, request); });
            while (answer.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) { app.poll(0); Sleep(1); }
            return answer.get();
        };
        const auto first = ask("{\"method\":\"hello\"}\n");
        require(first.find("\"protocol\":1") != std::string::npos, "hello failed");
        const auto snapshot = ask("{\"method\":\"snapshot\"}\n");
        require(snapshot.find("Native snapshot") != std::string::npos && snapshot.find("debugSession") != std::string::npos && snapshot.find("processId") != std::string::npos, "snapshot missing runtime identity or nodes");
        require(ask("{\"method\":\"request\",\"request\":\"cancelPick\"}\n").find("\"method\":\"cancelPick\"") != std::string::npos, "request protocol differs");
        require(ask("{broken}\n").find("\"error\"") != std::string::npos, "malformed input accepted");
        require(ask("{\"method\":\"execute\"}\n").find("\"error\"") != std::string::npos, "unknown method accepted");
        std::string large = "<Window xmlns='urn:sunfleck:v1'><StackPanel>";
        for (int i = 0; i < 400; ++i) { large += "<TextBlock Text='Chunk " + std::to_string(i) + "'/>"; }
        large += "</StackPanel></Window>";
        app.refreshFleck(large);
        const auto chunked = ask("{\"method\":\"snapshot\"}\n");
        require(chunked.size() > 65536 && chunked.find("Chunk 399") != std::string::npos && chunked.back() == '\n', "large snapshot was truncated");
        app.enableDebugServer(nullptr);
        other.enableDebugServer(pipe.c_str());
        other.enableDebugServer(nullptr);
        app.enableDebugServer(pipe.c_str());
        require(ask("{\"method\":\"hello\"}\n") != first, "replacement reused old session");
        // Destruction while an incomplete client read is pending must not hang.
        for (int i = 0; i < 4; ++i) { app.poll(0); }
        HANDLE incomplete = CreateFileA(("\\\\.\\pipe\\" + pipe).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        require(incomplete != INVALID_HANDLE_VALUE, "incomplete client failed");
        app.poll(0);
        app.enableDebugServer(nullptr);
        CloseHandle(incomplete);
        std::puts("Native Pipe: ownership, failed replacement, protocol, reconnect and pending I/O shutdown passed.");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
