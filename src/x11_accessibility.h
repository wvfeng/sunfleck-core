#pragma once

#include <sunfleck/ui.h>
#include <cstdint>
#include <string>
#include <vector>

namespace x11_accessibility {
struct Element {
    uint64_t id = 0, parent = 0;
    ui_node_kind kind{};
    std::string name, text;
    int x = 0, y = 0, width = 0, height = 0;
    bool enabled = true, focusable = false, focused = false, checked = false;
    int caret = 0;
    bool active = false;
};

enum class Action { focus, activate };
using Perform = bool (*)(void*, uint64_t, Action);
struct Window;

Window* open(void* context, Perform perform);
void update(Window* window, std::vector<Element> elements);
void close(Window* window);
void poll();
}
