#include "debug_pipe.h"
#include "dock_json.h"
#include <atomic>
#include <algorithm>
#include <charconv>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#endif

namespace devtools {
#ifdef _WIN32
struct DebugPipe::State {
    std::string name, session, input, output;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    OVERLAPPED operation{};
    enum Phase { connect, read, write } phase = connect;
    bool pending = false;
    DWORD transferred = 0;
    size_t written = 0;
    ULONGLONG deadline = 0;
    char buffer[4096]{};

    ~State()
    {
        cancel();
        if (pipe != INVALID_HANDLE_VALUE) { CloseHandle(pipe); }
        if (operation.hEvent) { CloseHandle(operation.hEvent); }
    }
    void cancel()
    {
        if (pending) {
            CancelIoEx(pipe, &operation);
            DWORD ignored;
            GetOverlappedResult(pipe, &operation, &ignored, TRUE);
            pending = false;
        }
    }
    void disconnect()
    {
        cancel();
        DisconnectNamedPipe(pipe);
        phase = connect;
        input.clear(); output.clear(); written = 0; deadline = 0;
    }
    std::string response(ui_app* app, const std::string& line)
    {
        using docking::json::quote;
        try {
            const auto request = docking::json::Reader(line).parse();
            const auto method = request.at("method").string();
            if (method == "hello") {
                return "{\"protocol\":1,\"session\":" + quote(session) + ",\"methods\":[\"snapshot\",\"request\"]}";
            }
            if (method == "snapshot") {
                size_t size = 0;
                auto result = ui_app_get_debug_snapshot(app, nullptr, &size);
                if (result != UI_OK) { throw std::runtime_error("Debug snapshot is unavailable."); }
                std::string snapshot(size, '\0');
                result = ui_app_get_debug_snapshot(app, snapshot.data(), &size);
                if (result != UI_OK) { throw std::runtime_error("Debug snapshot is unavailable."); }
                snapshot.resize(size - 1);
                snapshot.pop_back();
                return "{\"result\":" + snapshot + ",\"debugSession\":" + quote(session) +
                    ",\"processId\":" + std::to_string(GetCurrentProcessId()) +
                    ",\"debugProtocol\":{\"version\":1,\"methods\":[\"snapshot\",\"pick\",\"cancelPick\",\"highlight\"]}}}";
            }
            if (method == "request") {
                const auto action = request.at(request.has("request") ? "request" : "command").string();
                if (action != "pick" && action != "cancelPick" && action != "highlight") { throw std::runtime_error("Unsupported debug request."); }
                ui_result result;
                if (action == "highlight") {
                    const auto& text = request.at("object").string();
                    uint64_t id = 0;
                    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
                    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) { throw std::runtime_error("Invalid highlight object identity."); }
                    result = ui_app_set_debug_highlight(app, id, 1000);
                } else { result = ui_app_set_debug_picking(app, action == "pick"); }
                return "{\"result\":{\"session\":" + quote(session) + ",\"result\":" +
                    std::to_string(result) + ",\"method\":" + quote(action) + "}}";
            }
            throw std::runtime_error("Unsupported debug method.");
        } catch (const std::exception& error) { return "{\"error\":" + quote(error.what()) + "}"; }
    }
    void poll(ui_app* app)
    {
        if (deadline && GetTickCount64() >= deadline) { disconnect(); }
        // Bound work per frame, including a client pipelining many requests.
        for (unsigned work = 0; work < 8; ++work) {
            if (!pending) {
                if (phase == read) {
                    const auto newline = input.find('\n');
                    if (newline != std::string::npos) {
                        output = response(app, input.substr(0, newline)) + '\n';
                        input.erase(0, newline + 1);
                        written = 0; phase = write;
                    }
                }
                ResetEvent(operation.hEvent);
                transferred = 0;
                BOOL done = phase == connect ? ConnectNamedPipe(pipe, &operation) :
                    phase == read ? ReadFile(pipe, buffer, sizeof(buffer), &transferred, &operation) :
                    WriteFile(pipe, output.data() + written,
                        static_cast<DWORD>(std::min<size_t>(output.size() - written, 65536)), &transferred, &operation);
                if (!done) {
                    const auto error = GetLastError();
                    if (error == ERROR_IO_PENDING) { pending = true; return; }
                    if (!(phase == connect && error == ERROR_PIPE_CONNECTED)) { disconnect(); return; }
                }
            } else {
                if (!GetOverlappedResult(pipe, &operation, &transferred, FALSE)) {
                    if (GetLastError() == ERROR_IO_INCOMPLETE) { return; }
                    pending = false; disconnect(); return;
                }
                pending = false;
            }
            if (phase == connect) { phase = read; deadline = GetTickCount64() + 2000; }
            else if (phase == read) {
                if (!transferred) { disconnect(); return; }
                input.append(buffer, transferred);
                if (input.size() > 65536) { disconnect(); return; }
            } else {
                written += transferred;
                if (written == output.size()) { output.clear(); phase = read; deadline = GetTickCount64() + 2000; }
            }
        }
    }
};

DebugPipe::DebugPipe(const char* name) : state_(std::make_unique<State>())
{
    state_->name = name;
    // Explicit current-user ACL and remote rejection match the managed server.
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) { throw std::runtime_error("Cannot read debug Pipe owner."); }
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> user(size);
    const bool owner = GetTokenInformation(token, TokenUser, user.data(), size, &size) != FALSE;
    CloseHandle(token);
    LPWSTR sid = nullptr;
    if (!owner || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid)) {
        throw std::runtime_error("Cannot read debug Pipe owner.");
    }
    const std::wstring acl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
        throw std::runtime_error("Cannot secure debug Pipe.");
    }
    SECURITY_ATTRIBUTES security{sizeof(security), descriptor, FALSE};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0);
    std::wstring wide(count > 0 ? count : 0, L'\0');
    if (count) { MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide.data(), count); wide.pop_back(); }
    const auto path = L"\\\\.\\pipe\\" + wide;
    state_->pipe = count ? CreateNamedPipeW(path.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 4096, 0, &security) : INVALID_HANDLE_VALUE;
    LocalFree(descriptor);
    if (state_->pipe == INVALID_HANDLE_VALUE) { throw std::runtime_error("Cannot open debug Pipe; choose a unique local name."); }
    state_->operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!state_->operation.hEvent) { throw std::runtime_error("Cannot create debug Pipe event."); }
    static std::atomic_uint64_t sequence{0};
    state_->session = "pipe-" + std::to_string(GetCurrentProcessId()) + "-" +
        std::to_string(GetTickCount64()) + "-" + std::to_string(++sequence);
}
void DebugPipe::poll(ui_app* app) { state_->poll(app); }
const std::string& DebugPipe::name() const { return state_->name; }
#else
struct DebugPipe::State {};
DebugPipe::DebugPipe(const char*) { throw std::runtime_error("Named Pipe requires Windows."); }
void DebugPipe::poll(ui_app*) {}
const std::string& DebugPipe::name() const { static const std::string empty; return empty; }
#endif
DebugPipe::~DebugPipe() = default;
}
