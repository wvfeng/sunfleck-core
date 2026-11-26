#pragma once
#include <sunfleck/ui.h>
#include <memory>
#include <string>

namespace devtools {
// Transport advances only on the App owner thread. No worker can outlive App.
class DebugPipe {
    struct State;
    std::unique_ptr<State> state_;
public:
    explicit DebugPipe(const char* name);
    ~DebugPipe();
    DebugPipe(const DebugPipe&) = delete;
    DebugPipe& operator=(const DebugPipe&) = delete;
    const std::string& name() const;
    void poll(ui_app* app);
};
}
