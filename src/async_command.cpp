#include "async_command.h"
#include "ui_tree.h"
#include <future>
#include <utility>
#include <thread>

struct ui_completion {
    std::atomic_uint references{2}; // Run and begin's owned reference.
    std::thread::id thread = std::this_thread::get_id();
    ui_app* owner = nullptr;
    bool ready = false;
    ui_result result = UI_OK;
};

void ui_completion_release(ui_completion* completion)
{
    if (completion && completion->references.fetch_sub(1) == 1) { delete completion; }
}
ui_result ui_completion_complete(ui_completion* completion, ui_result result)
{
    if (!completion || result < UI_OK || result > UI_ERROR_VERSION_MISMATCH || result == UI_TIMEOUT || result == UI_EXITED) { return UI_ERROR_INVALID_ARGUMENT; }
    if (completion->thread != std::this_thread::get_id()) { return UI_ERROR_WRONG_THREAD; }
    if (!completion->owner) { return UI_CANCELLED; }
    if (completion->ready) { return UI_ERROR_INVALID_STATE; }
    completion->ready = true;
    completion->result = result;
    return UI_OK;
}

struct AsyncCommandRun {
    ui_command& command;
    ui_node& node;
    ui_cancellation_token cancellation;
    std::future<ui_result> future;
    ui_model* source_model = nullptr;
    ui_completion* completion = nullptr;
    ~AsyncCommandRun()
    {
        if (completion) { completion->owner = nullptr; ui_completion_release(completion); }
    }
};
namespace {
thread_local AsyncCommandRun* executing = nullptr;
ui_result execute(AsyncCommandRun& run)
{
    struct Scope { AsyncCommandRun* previous; ~Scope() { executing = previous; } } scope{std::exchange(executing, &run)};
    try {
        if (ui_cancellation_requested(&run.cancellation)) { return UI_CANCELLED; }
        return run.command.async.execute(run.command.async.context, &run.cancellation);
    } catch (...) { return UI_ERROR_INVALID_STATE; }
}
void finish(ui_command& command, ui_result result, bool discard)
{
    auto run = command.run;
    if (!run) { return; }
    --run->node.deferred_actions;
    command.run.reset();
    binding_app_dirty(command.owner);
    if (run->completion) { run->completion->owner = nullptr; }
    const auto completed = command.external.begin ? command.external.completed : command.async.completed;
    void* context = command.external.begin ? command.external.context : command.async.context;
    if (!discard && completed) {
        try { completed(context, result); }
        catch (...) { binding_app_error(command.owner, UI_ERROR_INVALID_STATE); }
    } else if (!discard && result != UI_OK && result != UI_CANCELLED) {
        binding_app_error(command.owner, result);
    }
}
}

const ui_cancellation_token* async_command_token(ui_app* app)
{
    return executing && executing->command.owner == app ? &executing->cancellation : nullptr;
}
uint32_t ui_cancellation_requested(const ui_cancellation_token* token)
{
    return token && token->requested->load() ? 1u : 0u;
}
ui_result ui_command_cancel(ui_command* command)
{
    if (!command) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(command->owner); status != UI_OK) { return status; }
    auto run = command->run;
    if (!run || run->cancellation.requested->exchange(true)) { return UI_OK; }
    struct DispatchScope {
        ui_app* app; bool previous;
        ~DispatchScope() { async_app_set_dispatching(app, previous); }
    } scope{command->owner, async_app_set_dispatching(command->owner, true)};
    async_cancel_posts(command->owner, run->cancellation);
    const auto cancel = command->external.begin ? command->external.cancel : command->async.cancel;
    void* context = command->external.begin ? command->external.context : command->async.context;
    if (cancel) {
        try { cancel(context); }
        catch (...) { return UI_ERROR_INVALID_STATE; }
    }
    return UI_OK;
}
ui_result ui_app_cancel_commands(ui_app* app)
{
    if (const auto status = binding_app_check(app); status != UI_OK) { return status; }
    const bool previous = async_app_set_dispatching(app, true);
    ui_result result = UI_OK;
    for (auto& command : async_app_commands(app)) {
        const auto status = ui_command_cancel(command.get());
        if (status != UI_OK) { result = status; }
    }
    async_app_set_dispatching(app, previous);
    return result;
}
bool async_uses_model(const ui_command& command,const ui_model* model) {
    return command.run && command.run->source_model == model;
}
ui_result async_command_start(ui_node& node, ui_command& command, ui_model* source_model)
{
    if (command.run) { return UI_OK; }
    if (!command.asynchronous() || node.command_mode == UI_COMMAND_SYNC) { return UI_ERROR_INVALID_ARGUMENT; }
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    if (!command.external.begin) { return UI_ERROR_UNSUPPORTED; }
#endif
    try {
        auto run = std::make_shared<AsyncCommandRun>(command, node);
        run->source_model = source_model;
        command.run = run;
        ++node.deferred_actions;
        if (command.external.begin) {
            run->completion = new ui_completion;
            run->completion->owner = command.owner;
            // C callbacks must not throw. begin owns its reference on entry.
            command.external.begin(command.external.context, run->completion);
        } else {
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
            run->future = std::async(std::launch::async, [raw = run.get()] { return execute(*raw); });
#endif
        }
        binding_app_dirty(command.owner);
        return UI_OK;
    } catch (...) {
        if (command.run) { finish(command, UI_ERROR_INVALID_STATE, false); }
        return UI_ERROR_INVALID_STATE;
    }
}
void async_command_poll(ui_app* app)
{
    for (auto& command : async_app_commands(app)) {
        auto run = command->run;
        if (run && ((run->completion && run->completion->ready) ||
            (run->future.valid() && run->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready))) {
            // Completion must follow all progress posted before execute returned.
            if (async_has_posts(app, run->cancellation)) { continue; }
            auto result = run->completion ? run->completion->result : run->future.get();
            if (result == UI_OK && ui_cancellation_requested(&run->cancellation)) { result = UI_CANCELLED; }
            finish(*command, result, false);
        }
    }
}
void async_command_dispose(ui_command& command)
{
    if (!command.run) { return; }
    struct DispatchScope {
        ui_app* app; bool previous;
        ~DispatchScope() { async_app_set_dispatching(app, previous); }
    } scope{command.owner, async_app_set_dispatching(command.owner, true)};
    auto run = command.run;
    binding_app_error(command.owner, ui_command_cancel(&command));
    if (run->future.valid()) { run->future.wait(); }
    finish(command, UI_CANCELLED, true);
}
ui_result ui_command_create_async(ui_app* app, const ui_async_command_options* options, ui_command** out_command)
{
    if (!out_command) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_command = nullptr;
    if (const auto status = binding_app_check(app); status != UI_OK) { return status; }
    if (!options || options->struct_size != sizeof(*options) || options->version != 2 || !options->execute) { return UI_ERROR_INVALID_ARGUMENT; }
    ui_command* command = nullptr;
    const auto result = ui_command_create(app, [](ui_node*, void*) {}, nullptr, &command);
    if (result == UI_OK) { command->async = *options; *out_command = command; }
    return result;
}
ui_result ui_command_create_external(ui_app* app, const ui_external_command_options* options, ui_command** out_command)
{
    if (!out_command) { return UI_ERROR_INVALID_ARGUMENT; }
    *out_command = nullptr;
    if (const auto status = binding_app_check(app); status != UI_OK) { return status; }
    if (!options || options->struct_size != sizeof(*options) || options->version != 1 || !options->begin) { return UI_ERROR_INVALID_ARGUMENT; }
    ui_command* command = nullptr;
    const auto result = ui_command_create(app, [](ui_node*, void*) {}, nullptr, &command);
    if (result == UI_OK) { command->external = *options; *out_command = command; }
    return result;
}
ui_result ui_command_is_running(ui_command* command, uint32_t* running)
{
    if (!command || !running) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(command->owner); status != UI_OK) { return status; }
    *running = command->run ? 1u : 0u;
    return UI_OK;
}
ui_result ui_model_is_running(ui_model* model, uint32_t* running)
{
    if (!model || !running) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(model->owner); status != UI_OK) { return status; }
    *running = 0;
    for (const auto& command : async_app_commands(model->owner))
        if (async_uses_model(*command,model)) { *running = 1; return UI_OK; }
    for (const auto& node : binding_app_nodes(model->owner)) {
        if (node->command && node->command->run) {
            for (const auto& binding : node->bindings) {
                if (binding.model == model && binding.property == UI_PROPERTY_COMMAND) { *running = 1; }
            }
        }
    }
    return UI_OK;
}
ui_result ui_node_set_command_mode(ui_node* node, ui_command_mode mode)
{
    if (!node || mode < UI_COMMAND_DEFAULT || mode > UI_COMMAND_ASYNC) { return UI_ERROR_INVALID_ARGUMENT; }
    if (const auto status = binding_app_check(node->owner); status != UI_OK) { return status; }
    if (node->command && node->command->run) { return UI_ERROR_INVALID_STATE; }
    if (node->command && ((mode == UI_COMMAND_ASYNC && !node->command->asynchronous()) ||
        (mode == UI_COMMAND_SYNC && node->command->asynchronous()))) { return UI_ERROR_INVALID_ARGUMENT; }
    node->command_mode = mode;
    return UI_OK;
}
