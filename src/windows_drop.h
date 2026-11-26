#pragma once
#include <windows.h>
#include <sunfleck/ui.h>
namespace win_drop {
using Receiver = bool (*)(void* owner, POINT point, ui_drop_kind kind, const ui_drop_data* data);
HRESULT attach(HWND window, void* owner, Receiver receiver);
void detach(HWND window);
ui_result drag(HWND window, ui_app* app, const ui_drop_data& data);
}
