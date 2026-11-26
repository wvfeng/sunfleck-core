#pragma once
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <memory>
#include <string>
#include <vector>
#include <sunfleck/ui.h>

namespace accessibility {
constexpr UINT request_message = WM_APP + 0x365;
enum class Action { inspect, focus, invoke, expand, collapse, select, toggle, number, text };
struct Element {
    uint64_t id{}, parent{};
    ui_node_kind kind{};
    RECT screen{};
    std::wstring name, automation_id, value;
    unsigned number{};
    bool enabled{}, focusable{}, focused{}, expanded{}, selected{};
};
struct Request {
    Action action = Action::inspect;
    uint64_t target{};
    unsigned number{};
    std::wstring text;
    std::vector<Element> elements;
};
class Bridge;
using Window = std::shared_ptr<Bridge>;
Window create(HWND window);
void focus_changed(const Window&, uint64_t id);
LRESULT provider(const Window&, WPARAM, LPARAM);
using Handler = HRESULT (*)(void*, Request&);
LRESULT dispatch(const Window&, WPARAM ticket, void* app, Handler);
}

