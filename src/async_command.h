#pragma once
#include <sunfleck/ui.h>
#include <memory>
#include <atomic>
#include <vector>

struct AsyncCommandRun;
struct ui_cancellation_token {
    std::shared_ptr<std::atomic_bool> requested = std::make_shared<std::atomic_bool>(false);
};
std::vector<std::unique_ptr<ui_command>>& async_app_commands(ui_app* app);
bool async_app_set_dispatching(ui_app* app, bool dispatching);
ui_result async_command_start(ui_node& node, ui_command& command, ui_model* source_model);
bool async_uses_model(const ui_command& command,const ui_model* model);
void async_command_poll(ui_app* app);
void async_command_dispose(ui_command& command);
const ui_cancellation_token* async_command_token(ui_app* app);
void async_cancel_posts(ui_app* app, const ui_cancellation_token& token);
bool async_has_posts(ui_app* app, const ui_cancellation_token& token);
void async_app_shutdown(ui_app* app);
