#include <sunfleck/fleck_page.hpp>
#include "../src/ui_tree.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <atomic>

#define REQUIRE(value) do { if (!(value)) { throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #value); } } while (false)

class BindingTestModel {
public:
    explicit BindingTestModel(sunfleck::App& app) : app_(app) {}
    inline static std::atomic<bool> progress_seen = false;
    inline static std::atomic<bool> continue_work = false;
    mutable int getterReads = 0;
    struct Details { std::string directField = "Field"; } details;
    int count() const { ++getterReads; return count_; }
    std::string caption() const { return caption_; }
    void configureConverters(sunfleck::PropertyModel& properties) {
        properties.addConverter("Twice",UI_VALUE_INT,[](void*,const ui_binding_value* inputs,uint32_t count,ui_binding_value* result) {
            if (count!=1 || inputs[0].kind!=UI_VALUE_INT) return UI_ERROR_INVALID_ARGUMENT;
            result->kind=UI_VALUE_INT;
            result->integer=2*inputs[0].integer;
            return UI_OK;
        });
    }
    void caption(std::string value) { caption_ = std::move(value); changed_.notify("caption"); }
    bool enabled() const { return enabled_; }
    void enabled(bool value) { enabled_ = value; changed_.notify("enabled"); }
    std::string name() const { return name_; }
    void name(std::string value)
    {
        if (value == "reject") { throw std::runtime_error("Rejected name"); }
        name_ = std::move(value);
        changed_.notify("name");
    }
    int volume() const { return volume_; }
    void volume(int value) { volume_ = value; changed_.notify("volume"); }
    void increment(sunfleck::CancellationToken token)
    {
        app_.invokeAsync([&] { caption("Processing..."); progress_seen = true; }, token).get();
        while (!continue_work.load()) { token.throwIfCancelled(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        app_.invokeAsync([&] { count_ += *resource_; changed_.notify("count"); caption("Increment"); }, token).get();
    }
    auto subscribePropertyChanged(sunfleck::PropertyChangedHandler handler) { return changed_.subscribe(std::move(handler)); }
private:
    sunfleck::App& app_;
    std::unique_ptr<int> resource_ = std::make_unique<int>(1); // Deliberately noncopyable model.
    int count_ = 0, volume_ = 25;
    bool enabled_ = true;
    std::string name_ = "Initial", caption_ = "Increment";
    sunfleck::PropertyChangedEvent changed_;
};
#include "property_page.hpp"

namespace {
std::string xml, binary;
using Page = sunfleck::FleckPage<BindingTestModel>;
ui_result load_xml(ui_app* app, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_with_model(app, xml.data(), xml.size(), model, context, diagnostic);
}
ui_result load_binary(ui_app* app, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    return ui_app_load_fleck_resource_with_model(app, binary.data(), binary.size(), model, context, diagnostic);
}
std::unique_ptr<Page> load(sunfleck::App& app, BindingTestModel& model, unsigned mode)
{
    if (mode == 0) { return sunfleck::generated::load_properties::load(app, model); }
    if (mode == 3) { return sunfleck::generated::load_properties::load(app, model, xml); }
    auto page = std::make_unique<Page>(app, model);
    model.configureConverters(page->properties());
    page->bindAsyncCommand<&BindingTestModel::increment>("increment");
    page->bindProperty("caption", [](const auto& m) { return m.caption(); });
    page->bindProperty("count", [](const auto& m) { return m.count(); });
    page->bindProperty("enabled", [](const auto& m) { return m.enabled(); }, [](auto& m, bool v) { m.enabled(v); });
    page->bindProperty("name", [](const auto& m) { return m.name(); }, [](auto& m, std::string v) { m.name(std::move(v)); });
    page->bindProperty("volume", [](const auto& m) { return m.volume(); }, [](auto& m, int v) { m.volume(v); });
    page->bindProperty("details.directField", [](const auto& m) { return m.details.directField; }, [](auto& m, std::string v) { m.details.directField = std::move(v); });
    page->load(mode == 1 ? load_xml : load_binary);
    return page;
}
std::vector<uint8_t> exercise(unsigned mode)
{
    BindingTestModel::progress_seen = false;
    BindingTestModel::continue_work = false;
    // Always release a waiting worker if an assertion throws.
    struct Release { ~Release() { BindingTestModel::continue_work = true; } };
    sunfleck::App app(UI_SOFTWARE, true);
    BindingTestModel model(app);
    auto page = load(app, model, mode);
    Release release;
    auto count = app.findFleckNode("count"), button = app.findFleckNode("increment"), editor = app.findFleckNode("editor");
    REQUIRE(count.text() == "Count: 0" && app.findFleckNode("other").text() == "Total, 0");
    REQUIRE(app.findFleckNode("literal").text() == "{Binding caption}");
    auto field = app.findFleckNode("field");
    REQUIRE(field.text() == "Field");
    field.nativeHandle()->text = L"Edited field";
    binding_user_changed(*field.nativeHandle(), UI_PROPERTY_TEXT);
    REQUIRE(model.details.directField == "Edited field");
    model.details.directField = "Notified field";
    page->notifyPropertyChanged("details.directField");
    REQUIRE(field.text() == "Notified field");
    app.openWindow(); page->poll(0);
    app.activateButton(); app.activateButton();
    REQUIRE(page->busy() && button.text() == "Increment" && model.count() == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (button.text() != "Processing..." && std::chrono::steady_clock::now() < deadline) { page->poll(0); std::this_thread::yield(); }
    REQUIRE(BindingTestModel::progress_seen && page->busy() && button.text() == "Processing..." && model.caption() == "Processing...");
    REQUIRE(model.count() == 0);
    editor.nativeHandle()->text = L"While running";
    binding_user_changed(*editor.nativeHandle(), UI_PROPERTY_TEXT);
    REQUIRE(model.name() == "While running");
    BindingTestModel::continue_work = true;
    while (page->busy() && std::chrono::steady_clock::now() < deadline) { page->poll(2); }
    REQUIRE(!page->busy() && model.count() == 1 && count.text() == "Count: 1");
    REQUIRE(app.findFleckNode("twice").text()=="2");
    REQUIRE(app.findFleckNode("snapshot").text() == "0");
    model.caption("Updated caption");
    REQUIRE(button.text() == "Updated caption");
    // The original model keeps its event subscription throughout execution.
    model.name("Changed");
    REQUIRE(editor.text() == "Changed" && app.findFleckNode("name").text() == "Changed");
    editor.nativeHandle()->text = L"Edited";
    binding_user_changed(*editor.nativeHandle(), UI_PROPERTY_TEXT);
    REQUIRE(model.name() == "Edited" && app.findFleckNode("name").text() == "Edited");
    auto deferred = app.findFleckNode("deferred");
    deferred.nativeHandle()->text = L"Deferred";
    binding_user_changed(*deferred.nativeHandle(), UI_PROPERTY_TEXT);
    REQUIRE(model.name() == "Edited");
    deferred.updateSource(UI_PROPERTY_TEXT);
    REQUIRE(model.name() == "Deferred" && editor.text() == "Deferred");
    editor.nativeHandle()->text = L"reject";
    REQUIRE(ui_node_update_source(editor.nativeHandle(), UI_PROPERTY_TEXT) == UI_ERROR_INVALID_STATE);
    REQUIRE(editor.text() == "Deferred" && page->takeError());
    auto slider = app.findFleckNode("slider");
    slider.nativeHandle()->value = 75;
    binding_user_changed(*slider.nativeHandle(), UI_PROPERTY_VALUE);
    REQUIRE(model.volume() == 75 && app.findFleckNode("progress").value() == 75);
    model.enabled(false); page->poll(0);
    REQUIRE(!button.enabled());
    app.activateButton(); REQUIRE(!page->busy() && model.count() == 1);
    model.enabled(true);
    button.setText("Custom");
    page->refresh();
    REQUIRE(button.text() == "Custom" && button.enabled());
    page->poll(0);
    uint32_t width = 0, height = 0;
    const auto* pixels = ui_app_pixels(app.nativeHandle(), &width, &height);
    REQUIRE(pixels);
    std::vector<uint8_t> frame(pixels, pixels + size_t(width) * height * 4);
    page.reset();
    model.name("After unsubscribe");
    REQUIRE(editor.text().empty()); // Destroyed inherited context resolves the default.

    return frame;
}
void notifications()
{
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    auto root = app.createNode(UI_STACK_PANEL), label = app.createNode(UI_TEXT_BLOCK), slider = app.createNode(UI_SLIDER);
    root.append(label); root.append(slider); app.setRoot(root);
    int number = 2, reads = 0;
    model.property("Number", [&] { ++reads; return number; }, [&](int value) { number = value; });
    model.bind(label, UI_PROPERTY_TEXT, "Number", UI_BIND_ONE_WAY, UI_UPDATE_PROPERTY_CHANGED, "Value: {0}");
    model.bind(slider, UI_PROPERTY_VALUE, "Number");
    const auto before = reads;
    model.beginUpdate(); number = 3; model.notify("Number"); number = 4; model.notify("Number");
    REQUIRE(label.text() == "Value: 2");
    model.endUpdate(); REQUIRE(reads == before + 1 && label.text() == "Value: 4" && slider.value() == 4);
    ui_result thread_status = UI_OK;
    std::thread worker([&] {
        thread_status = ui_model_notify_property_changed(model.nativeHandle(), "Number");
        app.post([&] { number = 5; model.notify("Number"); });
    });
    worker.join(); REQUIRE(thread_status == UI_ERROR_WRONG_THREAD);
    app.openWindow(); app.poll(0); REQUIRE(label.text() == "Value: 5");
    number = 101;
    REQUIRE(ui_model_notify_property_changed(model.nativeHandle(), "Number") == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(slider.value() == 5);
    number = 6; model.notify();
    app.post([] { throw std::runtime_error("Posted error"); });
    try { app.poll(0); REQUIRE(false); } catch (const std::runtime_error& e) { REQUIRE(std::string(e.what()) == "Posted error"); }
}
void errors()
{
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    model.property("Caption", [] { return std::string("test"); });
    ui_fleck_diagnostic diagnostic{};
    const std::string valid = "<Window xmlns='urn:sunfleck:v1'><Button Id='keep' Content='{Binding Caption}'/></Window>";
    REQUIRE(ui_app_load_fleck_with_model(app.nativeHandle(), valid.data(), valid.size(), model.nativeHandle(), nullptr, &diagnostic) == UI_OK);
    auto previous = app.findFleckNode("keep");
    const char* invalid[] = {
        "<Button TextBinding='Caption'/>",
        "<Button Content='{Binding Missing}'/>",
        "<Button IsEnabled='{Binding Caption}'/>",
        "<Button Content='{Binding Caption, Mode=TwoWay}'/>",
        "<Button Content='{Binding Caption, Mode=OneWay, Mode=OneTime}'/>",
        "<Button Content='{Binding Caption, StringFormat=oops}'/>",
        "<Button Content='{Binding Caption, Unknown=1}'/>",
        "<Button Content='{Binding Caption}' Text='{Binding Caption}'/>",
        "<Button Command='Caption'/>"
    };
    for (auto element : invalid) {
        const auto input = std::string("<Window xmlns='urn:sunfleck:v1'>") + element + "</Window>";
        REQUIRE(ui_app_load_fleck_with_model(app.nativeHandle(), input.data(), input.size(), model.nativeHandle(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
        REQUIRE(diagnostic.code != UI_FLECK_NONE && diagnostic.line && diagnostic.column);
        REQUIRE(app.findFleckNode("keep").nativeHandle() == previous.nativeHandle());
    }
    auto old = binary; old[5] = 3;
    REQUIRE(ui_app_load_fleck_resource_with_model(app.nativeHandle(), old.data(), old.size(), model.nativeHandle(), nullptr, &diagnostic) == UI_ERROR_VERSION_MISMATCH);
    const std::string two_way = "<Window xmlns='urn:sunfleck:v1'>\n  <Button Content='{Binding Caption, Mode=TwoWay}'/>\n</Window>";
    REQUIRE(ui_app_load_fleck_with_model(app.nativeHandle(), two_way.data(), two_way.size(), model.nativeHandle(), nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(diagnostic.code == UI_FLECK_VALUE && diagnostic.line == 2 && diagnostic.column == 11);
    REQUIRE(std::string(diagnostic.message) == "Button.Content does not support TwoWay; use OneWay for model changes");
    REQUIRE(app.findFleckNode("keep").nativeHandle() == previous.nativeHandle());
}
void shared_editors()
{
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    auto area = app.createNode(UI_TEXT_AREA), line = app.createNode(UI_TEXT_BOX);
    std::string value = "Initial";
    int writes = 0;
    model.property("Text", [&] { return value; }, [&](std::string next) { ++writes; value = std::move(next); });
    model.bind(area, UI_PROPERTY_TEXT, "Text");
    model.bind(line, UI_PROPERTY_TEXT, "Text");
    REQUIRE(!binding_allows_line_breaks(*area.nativeHandle()));
    area.nativeHandle()->text = L"First\nSecond";
    REQUIRE(ui_node_update_source(area.nativeHandle(), UI_PROPERTY_TEXT) == UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(writes == 0 && value == "Initial" && area.text() == "Initial" && line.text() == "Initial");
    // A rejected low-level bind must preserve the active property subscription.
    REQUIRE(ui_node_bind_text(line.nativeHandle(), nullptr, UI_BIND_ONE_TIME) == UI_ERROR_INVALID_ARGUMENT);
    value = "Changed"; model.notify("Text");
    REQUIRE(line.text() == "Changed");
    line.clearBinding(UI_PROPERTY_TEXT);
    REQUIRE(binding_allows_line_breaks(*area.nativeHandle()));
    area.nativeHandle()->text = L"First\nSecond";
    area.updateSource(UI_PROPERTY_TEXT);
    REQUIRE(writes == 1 && value == "First\nSecond");
}
class FailureModel {
public:
    explicit FailureModel(sunfleck::App& owner) : app(owner) {}
    std::string caption() const { return text; }
    std::string text = "Initial";
    bool fail = true;
    inline static std::atomic<bool> release = false;
    inline static std::atomic<bool> exited = false;
    void run(sunfleck::CancellationToken token)
    {
        struct Exit { ~Exit() { exited = true; } } exit;
        app.invokeAsync([&] { text = "Provisional"; changed.notify("caption"); }, token).get();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!release && std::chrono::steady_clock::now() < deadline) { token.throwIfCancelled(); std::this_thread::yield(); }
        if (fail) { throw std::runtime_error("Async failure"); }
        app.invokeAsync([&] { text = "Finished"; changed.notify("caption"); }, token).get();
    }
    auto subscribePropertyChanged(sunfleck::PropertyChangedHandler handler) { return changed.subscribe(std::move(handler)); }
private:
    sunfleck::App& app;
    sunfleck::PropertyChangedEvent changed;
};
ui_result load_failure(ui_app* app, ui_model* model, const ui_fleck_data_context* context, ui_fleck_diagnostic* diagnostic)
{
    const std::string input = "<Window xmlns='urn:sunfleck:v1'><Button Id='run' Content='{Binding caption}' Command='{Binding run}' CommandMode='Async'/></Window>";
    return ui_app_load_fleck_with_model(app, input.data(), input.size(), model, context, diagnostic);
}
void async_failure_and_disposal()
{
    for (bool dispose : {false, true}) {
        sunfleck::App app(UI_SOFTWARE, true);
        FailureModel model(app);
        FailureModel::release = false; FailureModel::exited = false;
        auto page = std::make_unique<sunfleck::FleckPage<FailureModel>>(app, model);
        // Release worker before destroying page during exception unwinding.
        struct Guard { ~Guard() { FailureModel::release = true; } } guard;
        page->bindAsyncCommand<&FailureModel::run>("run");
        page->bindProperty("caption", [](const auto& m) { return m.caption(); });
        page->load(load_failure);
        app.openWindow(); page->poll(0); app.activateButton();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        auto button = app.findFleckNode("run");
        while (button.text() != "Provisional" && std::chrono::steady_clock::now() < deadline) { page->poll(0); }
        REQUIRE(button.text() == "Provisional" && model.caption() == "Provisional" && page->busy());
        FailureModel::release = true;
        if (dispose) {
            page.reset();
            REQUIRE(FailureModel::exited && button.text().empty() && model.caption() == "Provisional");
            app.poll(0);
        } else {
            while (page->busy() && std::chrono::steady_clock::now() < deadline) { page->poll(0); }
            REQUIRE(!page->busy() && model.caption() == "Provisional" && button.text() == "Provisional");
            const auto error = page->takeError(); REQUIRE(error);
            try { std::rethrow_exception(error); } catch (const std::runtime_error& e) { REQUIRE(std::string(e.what()) == "Async failure"); }
            // A failed command releases its single-flight state and may be retried.
            model.fail = false;
            app.activateButton();
            while (page->busy() && std::chrono::steady_clock::now() < deadline) { page->poll(0); }
            REQUIRE(!page->busy() && model.caption() == "Finished" && button.text() == "Finished");
        }
    }
}
void native_async_source_lifetime()
{
    sunfleck::App app(UI_SOFTWARE, true);
    auto source = app.createTextSource("Initial");
    sunfleck::PropertyModel properties(app);
    properties.textSource("Text", source);
    auto button = app.createNode(UI_BUTTON);
    properties.bind(button, UI_PROPERTY_CONTENT, "Text");
    app.setRoot(button);
    struct State { sunfleck::App& app; ui_text_source* source; std::atomic<bool> release = false; } state{app, source.nativeHandle()};
    const ui_async_command_options options{sizeof(options), 2,
        [](void* raw, const ui_cancellation_token* token) -> ui_result {
            auto& state = *static_cast<State*>(raw);
            state.app.invokeAsync([&] { REQUIRE(ui_text_source_set(state.source, "Busy") == UI_OK); }, sunfleck::CancellationToken(token)).get();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!state.release && std::chrono::steady_clock::now() < deadline) {
                if (ui_cancellation_requested(token)) { return UI_CANCELLED; }
                std::this_thread::yield();
            }
            return UI_OK;
        },
        nullptr,
        [](void* raw, ui_result result) { if (result == UI_OK) { ui_text_source_set(static_cast<State*>(raw)->source, "Final"); } }, &state};
    ui_command* raw = nullptr;
    REQUIRE(ui_command_create_async(app.nativeHandle(), &options, &raw) == UI_OK);
    std::unique_ptr<ui_command, decltype(&ui_command_destroy)> command(raw, ui_command_destroy);
    struct Guard { State& state; ~Guard() { state.release = true; } } guard{state};
    REQUIRE(ui_node_bind_command(button.nativeHandle(), raw) == UI_OK);
    app.openWindow(); app.poll(0); app.activateButton();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (button.text() != "Busy" && std::chrono::steady_clock::now() < deadline) { app.poll(0); }
    REQUIRE(button.text() == "Busy");
    REQUIRE(ui_text_source_set(source.nativeHandle(), "External") == UI_OK);
    REQUIRE(button.text() == "External");
    state.release = true;
    uint32_t running = 1;
    while (running && std::chrono::steady_clock::now() < deadline) { app.poll(0); REQUIRE(ui_command_is_running(raw, &running) == UI_OK); }
    REQUIRE(!running && button.text() == "Final");
}
void cancel_pending_cpp_dispatch()
{
    sunfleck::App app(UI_SOFTWARE, true);
    auto button = app.createNode(UI_BUTTON);
    app.setRoot(button);
    struct State {
        sunfleck::App& app;
        std::atomic<bool> queued = false, exited = false;
        bool changed = false;
    } state{app};
    const ui_async_command_options options{sizeof(options), 2,
        [](void* raw, const ui_cancellation_token* token) -> ui_result {
            auto& state = *static_cast<State*>(raw);
            // Explicit token works even on a child thread without native TLS.
            auto child = std::async(std::launch::async, [&] {
                auto pending = state.app.invokeAsync([&] { state.changed = true; }, sunfleck::CancellationToken(token));
                state.queued = true;
                try { pending.get(); return UI_ERROR_INVALID_STATE; }
                catch (const sunfleck::Error& error) { return error.result(); }
            });
            const auto result = child.get();
            state.exited = true;
            return result;
        }, nullptr, nullptr, &state};
    ui_command* raw = nullptr;
    REQUIRE(ui_command_create_async(app.nativeHandle(), &options, &raw) == UI_OK);
    std::unique_ptr<ui_command, decltype(&ui_command_destroy)> command(raw, ui_command_destroy);
    REQUIRE(ui_node_bind_command(button.nativeHandle(), raw) == UI_OK);
    app.openWindow(); app.poll(0); app.activateButton();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!state.queued && std::chrono::steady_clock::now() < deadline) { std::this_thread::yield(); }
    REQUIRE(state.queued);
    command.reset(); // Must release the queued future before joining the worker.
    REQUIRE(state.exited && !state.changed);
    app.poll(0);
    REQUIRE(!state.changed);
}
void native_completion_order()
{
    sunfleck::App app(UI_SOFTWARE, true);
    auto button = app.createNode(UI_BUTTON);
    app.setRoot(button);
    struct State {
        ui_app* app;
        std::atomic<bool> posted = false;
        int progress = 0, completed = 0;
    } state{app.nativeHandle()};
    const ui_async_command_options options{sizeof(options), 2,
        [](void* raw, const ui_cancellation_token* token) -> ui_result {
            auto& state = *static_cast<State*>(raw);
            const auto result = ui_app_post_cancellable(state.app, token,
                [](void* raw) { ++static_cast<State*>(raw)->progress; }, raw, nullptr);
            state.posted = true;
            return result;
        }, nullptr,
        [](void* raw, ui_result result) {
            auto& state = *static_cast<State*>(raw);
            state.completed = result == UI_OK && state.progress == 1 ? 1 : -1;
        }, &state};
    ui_command* raw = nullptr;
    REQUIRE(ui_command_create_async(app.nativeHandle(), &options, &raw) == UI_OK);
    std::unique_ptr<ui_command, decltype(&ui_command_destroy)> command(raw, ui_command_destroy);
    REQUIRE(ui_node_bind_command(button.nativeHandle(), raw) == UI_OK);
    app.openWindow(); app.poll(0); app.activateButton();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!state.posted && std::chrono::steady_clock::now() < deadline) { std::this_thread::yield(); }
    REQUIRE(state.posted);
    // Simulate a slow frame/debugger pause: execute finishes before UI resumes.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    while (!state.completed && std::chrono::steady_clock::now() < deadline) { app.poll(0); }
    REQUIRE(state.completed == 1 && state.progress == 1);
}
}
static void collection_bindings() {
    sunfleck::App app(UI_SOFTWARE,true);
    sunfleck::PropertyModel rows(app);
    rows.enableTextCollection();
    const char* initial[] = {"same","same","last"};
    rows.spliceItems(0,0,initial);
    auto original = rows.item(1);
    REQUIRE(rows.item(0).first != original.first);
    const std::string page = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='scope'><TextBlock Id='count' Text='{Binding Count}'/>"
                  "<TextBlock Id='revision' Text='{Binding Revision}'/></StackPanel></Window>";
    REQUIRE(ui_app_load_fleck_with_model(app.nativeHandle(),page.data(),page.size(),rows.nativeHandle(),nullptr,nullptr)==UI_OK);
    auto count = app.findFleckNode("count"), revision = app.findFleckNode("revision");
    REQUIRE(count.text()=="3" && revision.text()=="1");
    auto list = app.createNode(UI_LIST_VIEW);
    list.setItemsSource(rows.nativeHandle());
    app.setRoot(list);
    app.openWindow();
    app.poll(0);
    REQUIRE(list.childCount(UI_VISUAL_TREE)==3);
    list.setValue(1);
    auto selected = list.child(UI_VISUAL_TREE,1).nativeHandle();
    rows.moveItems(1,1,0);
    app.poll(0);
    REQUIRE(rows.item(0)==original && revision.text()=="2");
    REQUIRE(list.value()==0 && list.child(UI_VISUAL_TREE,0).nativeHandle()==selected);
    const char* invalid[]={"ok","\xff"};
    REQUIRE(ui_collection_splice(rows.nativeHandle(),0,1,invalid,2)==UI_ERROR_INVALID_ARGUMENT);
    REQUIRE(rows.item(0)==original && revision.text()=="2");
    rows.spliceItems(1,2,{});
    app.poll(0);
    REQUIRE(count.text()=="1" && revision.text()=="3");
    REQUIRE(list.childCount(UI_VISUAL_TREE)==1 && list.child(UI_VISUAL_TREE,0).text()=="same");
    list.setItemsSource(nullptr);
    app.poll(0);
    REQUIRE(list.childCount(UI_VISUAL_TREE)==0);
    std::vector<const char*> many(100000,"row");
    rows.spliceItems(1,0,many);
    REQUIRE(count.text()=="100001" && rows.item(100000).second=="row");
    list.setItemsSource(rows.nativeHandle());
    REQUIRE(list.childCount(UI_VISUAL_TREE)<32);
    app.poll(0);
    const auto realized = list.childCount(UI_VISUAL_TREE);
    REQUIRE(realized>0 && realized<64);
    list.setValue(100000);
    auto chosen = rows.item(100000).first;
    list.nativeHandle()->offset = list.nativeHandle()->scroll_limit;
    binding_app_dirty(app.nativeHandle());
    app.poll(0);
    REQUIRE(list.childCount(UI_VISUAL_TREE)<64);
    REQUIRE(list.child(UI_VISUAL_TREE,list.childCount(UI_VISUAL_TREE)-1).itemIdentity()==chosen);
    rows.moveItems(100000,1,0);
    app.poll(0);
    REQUIRE(list.value()==0);
    {
        sunfleck::PropertyModel detail(app), records(app);
        std::string caption="entry";
        detail.property("Caption",[&]{return caption;});
        int quantity = 7;
        detail.property("Quantity", [&]{ return quantity; }, [&](int value){ quantity = value; });
        records.enableTextCollection();
        std::vector<ui_model*> contexts(many.size(),detail.nativeHandle());
        records.spliceModels(0,0,many,contexts);
        auto table=app.createNode(UI_DATA_GRID);
        std::vector<ui_data_grid_column> columns(200,{"Caption","Name",96,0});
        columns.front() = {"Quantity", "Quantity", 96, 1};
        table.setGridColumns(columns.data(),static_cast<uint32_t>(columns.size()));
        table.setItemsSource(records.nativeHandle());
        app.setRoot(table);
        app.poll(0);
        REQUIRE(table.childCount(UI_VISUAL_TREE)>0 && table.childCount(UI_VISUAL_TREE)<32);
        auto row=table.child(UI_VISUAL_TREE,0);
        auto cells=row.child(UI_VISUAL_TREE,0).childCount(UI_VISUAL_TREE);
        REQUIRE(cells>0 && cells<16);
        auto cell = row.child(UI_VISUAL_TREE,0).child(UI_VISUAL_TREE,0);
        auto edit = [&](const wchar_t* value) {
            cell.nativeHandle()->text = value;
            binding_user_changed(*cell.nativeHandle(), UI_PROPERTY_TEXT);
        };
        edit(L"11");
        REQUIRE(quantity == 7);
        cell.finishCellEdit(false);
        REQUIRE(cell.text() == "7" && quantity == 7);
        edit(L"-23");
        cell.finishCellEdit(true);
        REQUIRE(quantity == -23);
        edit(L"5x");
        REQUIRE(ui_data_grid_finish_edit(cell.nativeHandle(),1) == UI_ERROR_INVALID_ARGUMENT);
        REQUIRE(quantity == -23 && cell.text() == "-23");
        REQUIRE(cell.validation(UI_PROPERTY_TEXT).code == "invalid_source_value");
        edit(L"12");
        cell.finishCellEdit(true);
        REQUIRE(cell.validation(UI_PROPERTY_TEXT).status == UI_OK);
        table.nativeHandle()->offset=table.nativeHandle()->scroll_limit;
        table.nativeHandle()->horizontal_offset=table.nativeHandle()->horizontal_limit;
        binding_app_dirty(app.nativeHandle());
        app.poll(0);
        REQUIRE(table.nativeHandle()->table->first>180);
        REQUIRE(table.child(UI_VISUAL_TREE,table.childCount(UI_VISUAL_TREE)-1).itemIdentity()==records.item(99999).first);
        app.setRoot(list);
    }
    {
        sunfleck::PropertyModel detail(app);
        ui_model* contexts[] = {detail.nativeHandle()};
        const char* labels[] = {"detail"};
        rows.spliceModels(0,100001,labels,contexts);
        app.poll(0);
        REQUIRE(rows.itemModel(0)==detail.nativeHandle());
        REQUIRE(binding_data_context(*list.child(UI_VISUAL_TREE,0).nativeHandle())==detail.nativeHandle());
    }
    REQUIRE(rows.itemModel(0)==nullptr && rows.itemCount()==1);
    sunfleck::PropertyModel view(app);
    const char* choices[] = {"zinc","tin","tin","lead"};
    rows.spliceItems(0,1,choices);
    view.setCollectionView(rows,UI_COLLECTION_ASCENDING,"in");
    REQUIRE(view.itemCount()==3 && view.item(0)==rows.item(1));
    REQUIRE(view.item(1)==rows.item(2) && view.item(2)==rows.item(0));
    rows.moveItems(2,1,1);
    REQUIRE(view.item(0)==rows.item(1));
    REQUIRE(ui_collection_splice(view.nativeHandle(),0,1,nullptr,0)==UI_ERROR_INVALID_STATE);
    REQUIRE(ui_collection_set_view(view.nativeHandle(),view.nativeHandle(),UI_COLLECTION_ASCENDING,"")==UI_ERROR_INVALID_ARGUMENT);
    view.setCollectionView(rows,UI_COLLECTION_DESCENDING,"");
    REQUIRE(view.itemCount()==4 && view.item(0)==rows.item(0));
    view.setCollectionView(rows);
    view.setGroupByLabel(true);
    REQUIRE(view.groupCount()==3 && view.group(0).key=="zinc");
    rows.moveItems(1,1,3);
    REQUIRE(view.group(1).key=="tin" && view.group(1).first==1 && view.group(1).count==2);
    REQUIRE(view.item(1)==rows.item(1) && view.item(2)==rows.item(3));
    view.setGroupByLabel(false);
    REQUIRE(view.groupCount()==0 && view.item(2)==rows.item(2));
}

int main(int argc, char** argv)
{
    try {
        REQUIRE(argc == 3);
        std::ifstream xml_file(argv[1], std::ios::binary), binary_file(argv[2], std::ios::binary);
        xml.assign(std::istreambuf_iterator<char>(xml_file), {});
        binary.assign(std::istreambuf_iterator<char>(binary_file), {});
        REQUIRE(!xml.empty() && binary.starts_with(std::string("FLECK")+char(UI_FLECK_RESOURCE_VERSION)));
        const auto generated = exercise(0);
        REQUIRE(generated == exercise(1) && generated == exercise(2) && generated == exercise(3));
        // Old payloads must be recompiled, even when their node kinds are unchanged.
        for (unsigned version : {5, 6, 7, 8, 9, 11}) {
            auto previous = binary; previous[5] = char(version);
            sunfleck::App app(UI_SOFTWARE, true);
            ui_fleck_diagnostic diagnostic{};
            REQUIRE(ui_app_load_fleck_resource(app.nativeHandle(), previous.data(), previous.size(), &diagnostic) == UI_ERROR_VERSION_MISMATCH);
            REQUIRE(std::string(diagnostic.message).find("regenerate") != std::string::npos);
        }
        {
            sunfleck::App app(UI_SOFTWARE, true);
            BindingTestModel model(app);
            auto bindings = sunfleck::generated::load_properties::bind(app, model);
            const auto metadata = bindings->properties().describe("BindingTestModel");
            REQUIRE(model.getterReads == 0 && metadata.find("Async") != std::string::npos);
            REQUIRE(metadata.find("properties.fleck") != std::string::npos);
            bindings->load(xml);
            REQUIRE(model.getterReads > 0 && app.findFleckNode("count").text() == "Count: 0");
        }
        collection_bindings();
        notifications(); errors(); shared_editors(); async_failure_and_disposal(); native_async_source_lifetime(); cancel_pending_cpp_dispatch(); native_completion_order();
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
