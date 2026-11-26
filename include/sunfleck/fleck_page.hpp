#pragma once

#include <sunfleck/ui.hpp>
#include <sunfleck/property_model.hpp>

#include <exception>
#include <functional>
#include <optional>
#include <type_traits>

namespace sunfleck {

// A page and its model must outlive the event loop, on the same UI thread.
// Async methods use the original model. Post changes to the UI thread.
template<class Model>

class FleckPage {
public:
    FleckPage(App& app, Model& model) : app_(app), model_(model), properties_(app)
    {
        if constexpr (NotifiesPropertyChanged<Model>) {
            connection_ = model.subscribePropertyChanged([this](std::string_view path) { properties_.notifyIfRegistered(path); });
        }
    }
    ~FleckPage()
    {
        connection_.reset();
        // Destroy commands first: native disposal joins their workers before
        // releasing the callbacks and model they reference.
        context_.reset();
    }

    FleckPage(const FleckPage&) = delete;
    FleckPage& operator=(const FleckPage&) = delete;

    template<auto Method>
        requires (requires(Model& model, CancellationToken token) { { (model.*Method)(token) } -> std::same_as<void>; } ||
            requires(Model& model) { { (model.*Method)() } -> std::same_as<void>; })
    void bindAsyncCommand(const char* name)
    {
        requireUnloaded();
        actions_.push_back(std::make_unique<Action>(Action{*this, name, nullptr,
            [](Model& model, CancellationToken token) {
                if constexpr (requires { (model.*Method)(token); }) { (model.*Method)(token); }
                else { (model.*Method)(); }
            }}));
    }

    template<auto Method>
        requires (requires(Model& model, Node node) { { (model.*Method)(node) } -> std::same_as<void>; } ||
            requires(Model& model) { { (model.*Method)() } -> std::same_as<void>; })
    void bindCommand(const char* name)
    {
        requireUnloaded();
        actions_.push_back(std::make_unique<Action>(Action{*this, name, [](FleckPage& page, Node node) {
            if constexpr (requires { (page.model_.*Method)(node); }) { (page.model_.*Method)(node); }
            else { (page.model_.*Method)(); }
            page.refresh();
        }}));
    }

    void load(FleckModelLoader loader)
    {
        if (loaded_) { throw Error(UI_ERROR_INVALID_STATE); }
        prepareBindings();
        context_->load(nullptr, loader, properties_.nativeHandle());
        loaded_ = true;
    }
    // Create adapters and commands without loading a page or evaluating getters.
    void prepareBindings()
    {
        if (context_) { return; }
        DataContext context(app_.nativeHandle());
        exportBindings(context);
        context_.emplace(std::move(context));
    }
    void load(std::string_view xml)
    {
        if (loaded_) { throw Error(UI_ERROR_INVALID_STATE); }
        prepareBindings();
        detail::check(ui_app_load_fleck_with_model(app_.nativeHandle(), xml.data(), xml.size(),
            properties_.nativeHandle(), nullptr, nullptr));
        loaded_ = true;
    }
    PropertyModel& properties() noexcept { return properties_; }
    template<class Read>
    void bindProperty(const char* name, Read read)
    {
        requireUnloaded();
        properties_.property(name, [this, read] { return read(model_); });
    }
    template<class Read, class Write>
    void bindProperty(const char* name, Read read, Write write)
    {
        requireUnloaded();
        properties_.property(name, [this, read] { return read(model_); }, [this, write](auto value) {
            write(model_, std::move(value));
        });
    }
    void notifyPropertyChanged(std::string_view path = {}) { properties_.notify(path); }
    template<bool Writable, class Read, class Write>
    void bindGeneratedProperty(const char* name, Read read, Write write)
    {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(read(model_))>, TextSource>) {
            properties_.textSource(name, read(model_));
        } else if constexpr (Writable) {
            bindProperty(name, read, write);
        } else {
            bindProperty(name, read);
        }
    }

    // Export hook for App::loadFleck. Business models need no Sunfleck types.
    void exportBindings(DataContext& context)
    {
        for (const auto& action : actions_) {
            auto* command = action->worker ? context.asyncCommand(action->name.c_str(), action->options())
                : context.command<&Action::invoke>(action->name.c_str(), *action);
            properties_.command(action->name.c_str(), command);
        }
    }

    bool busy() const { return properties_.running(); }
    std::exception_ptr takeError() noexcept
    {
        if (lastError_) { return std::exchange(lastError_, {}); }
        return properties_.takeError();
    }

    void refresh()
    {
        properties_.notify();
    }

    ui_result poll(uint32_t timeoutMs)
    {
        return app_.poll(timeoutMs);
    }

    void run()
    {
        while (poll(1000) != UI_EXITED) {
        }
    }

private:
    struct Action {
        FleckPage& page;
        std::string name;
        void (*execute)(FleckPage&, Node);
        void (*worker)(Model&, CancellationToken) = nullptr;
        std::exception_ptr error;
        ui_async_command_options options()
        {
            return {sizeof(ui_async_command_options), 2,
                [](void* context, const ui_cancellation_token* token) noexcept -> ui_result {
                    auto& action = *static_cast<Action*>(context);
                    action.error = {};
                    try { action.worker(action.page.model_, CancellationToken(token)); return UI_OK; }
                    catch (const Error& failure) {
                        if (failure.result() == UI_CANCELLED) { return UI_CANCELLED; }
                        action.error = std::current_exception(); return failure.result();
                    }
                    catch (...) { action.error = std::current_exception(); return UI_ERROR_INVALID_STATE; }
                }, nullptr,
                [](void* context, ui_result result) {
                    auto& action = *static_cast<Action*>(context);
                    if (action.error) { action.page.lastError_ = std::exchange(action.error, {}); }
                    else if (result != UI_OK && result != UI_CANCELLED) { action.page.lastError_ = std::make_exception_ptr(Error(result)); }
                }, this};
        }

        void invoke(Node node) noexcept
        {
            try {
                execute(page, node);
            } catch (...) {
                page.lastError_ = std::current_exception();
            }
        }
    };

    void requireUnloaded() const
    {
        if (context_) {
            throw Error(UI_ERROR_INVALID_STATE);
        }
    }

    App& app_;
    Model& model_;
    PropertyModel properties_;
    PropertyChangedConnection connection_;
    std::vector<std::unique_ptr<Action>> actions_;
    std::exception_ptr lastError_;
    std::optional<DataContext> context_;
    bool loaded_ = false;
};

} // namespace sunfleck
