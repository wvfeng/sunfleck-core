#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/dock_json.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

extern "C" ui_result load_development_with_model(ui_app*, ui_model*, const ui_fleck_data_context*, ui_fleck_diagnostic*);
static void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
static std::string read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Fixture missing");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void refresh_page()
{
#ifdef _WIN32
    const bool measure_memory = std::getenv("SUNFLECK_MEASURE_REFRESH_MEMORY") != nullptr;
    const auto sample_memory = [](unsigned refreshes) {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        require(K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) != 0, "Could not sample process memory");
        std::cout << refreshes << ',' << counters.PrivateUsage << ',' << counters.WorkingSetSize << '\n';
    };
#endif
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    unsigned transient_reads = 0;
    model.property("Transient", [&] { ++transient_reads; return std::string("Bound"); });
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel><TextBox Id='editor' Text='Initial'/><TextBlock Id='label' Text='Before'/></StackPanel></Window>");
    app.openWindow();
    auto editor = app.findFleckNode("editor");
    auto label = app.findFleckNode("label");
    const auto id = label.debugId();
    editor.setText("Draft");
    editor.nativeHandle()->caret = 2;
    editor.nativeHandle()->select_all = true;
    constexpr std::string_view next =
        "<Window xmlns='urn:sunfleck:v1'><StackPanel><TextBox Id='editor' Text='Initial'/><TextBlock Id='label' Text='After'/></StackPanel></Window>";
    constexpr std::string_view with_transient =
        "<Window xmlns='urn:sunfleck:v1'><StackPanel><TextBox Id='editor' Text='Initial'/><TextBlock Id='label' Text='After'/><TextBlock Id='transient' Text='{Binding Transient}'/></StackPanel></Window>";
    const auto live_nodes = binding_app_nodes(app.nativeHandle()).size();
    uint64_t last_transient_id = 0;
#ifdef _WIN32
    if (measure_memory) {
        std::cout << "refreshes,private_bytes,working_set_bytes\n";
        sample_memory(0);
    }
#endif
    for (int i = 0; i < 500; ++i) {
        app.refreshFleck(with_transient, model.nativeHandle());
        const auto transient_id = app.findFleckNode("transient").debugId();
        require(transient_id > last_transient_id, "Refresh recycled a retired object ID");
        transient_reads = 0;
        model.notify("Transient");
        require(transient_reads == 1 && app.findFleckNode("transient").text() == "Bound",
            "Transient object did not receive exactly one property update");
        app.refreshFleck(next, model.nativeHandle());
        const auto& nodes = binding_app_nodes(app.nativeHandle());
        const auto has_transient_binding = std::any_of(nodes.begin(), nodes.end(), [&](const auto& node) {
            return std::any_of(node->bindings.begin(), node->bindings.end(), [&](const auto& binding) {
                return binding.model == model.nativeHandle() && binding.path == "Transient";
            });
        });
        require(!has_transient_binding && nodes.size() == live_nodes,
            "Removed transient object retained a live binding or node");
        ui_node* retired = nullptr;
        require(ui_app_find_debug_node(app.nativeHandle(), transient_id, &retired) == UI_ERROR_INVALID_ARGUMENT,
            "Transient object ID survived refresh");
        last_transient_id = transient_id;
#ifdef _WIN32
        if (measure_memory && (i + 1) % 50 == 0) {
            sample_memory(static_cast<unsigned>((i + 1) * 2));
        }
#endif
    }
    require(app.debugSnapshot().find("\"fleckRefresh\":{\"sequence\":\"1000\",\"result\":0,\"elapsedMicroseconds\":") != std::string::npos,
        "Snapshot omitted the latest Fleck refresh result");
    require(label.text() == "After" && label.debugId() == id && editor.text() == "Draft", "Refresh lost object identity or editor state");
    require(editor.nativeHandle()->caret == 2 && editor.nativeHandle()->select_all,
        "Structural refresh lost the editor caret or selection");
    app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel><TextBlock Id='label' Text='Reordered'/><TextBlock Id='extra' Text='Added'/><TextBox Id='editor' Text='Initial'/></StackPanel></Window>");
    require(app.findFleckNode("extra").text() == "Added" && label.debugId() == id && editor.text() == "Draft", "Insertion and reorder lost state");
    constexpr std::string_view incompatible = "<Window xmlns='urn:sunfleck:v1'><UnknownControl/></Window>";
    require(ui_app_refresh_fleck(app.nativeHandle(), incompatible.data(), incompatible.size(), nullptr, nullptr) == UI_ERROR_INVALID_ARGUMENT,
        "Refresh accepted an incompatible tree");
    require(label.text() == "Reordered" && app.findFleckNode("label").nativeHandle() == label.nativeHandle(), "Failed refresh modified the page");
    const auto removed_id = app.findFleckNode("extra").debugId();
    app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel><Button Id='label' Content='Replaced'/><TextBox Id='editor' Text='Initial'/></StackPanel></Window>");
    ui_node* removed = nullptr;
    require(ui_app_find_debug_node(app.nativeHandle(), removed_id, &removed) == UI_ERROR_INVALID_ARGUMENT &&
        ui_app_find_debug_node(app.nativeHandle(), id, &removed) == UI_ERROR_INVALID_ARGUMENT,
        "Retired object ID still resolves");
    require(app.findFleckNode("label").debugId() != id && editor.text() == "Draft", "Replacement lost retained state");
    const auto root = app.findFleckNode("editor").nativeHandle()->parent->debug_id;
    app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><Panel><TextBox Id='editor' Text='Initial'/></Panel></Window>");
    require(app.poll(0) == UI_OK && app.findFleckNode("editor").nativeHandle() == editor.nativeHandle() &&
        editor.text() == "Draft" && ui_app_find_debug_node(app.nativeHandle(), root, &removed) == UI_ERROR_INVALID_ARGUMENT,
        "Root replacement lost the retained editor or kept the retired root");
}
static void refresh_application_state()
{
    sunfleck::App app(UI_SOFTWARE, true);
    const std::string first = "<Window xmlns='urn:sunfleck:v1'><TextBox Id='editor' Text='Initial'/></Window>";
    const std::string next = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='editor' Text='Replacement'/></Window>";
    app.loadFleck(first);
    app.openWindow();
    app.findFleckNode("editor").setText("Business draft");
    struct State { unsigned captured = 0, completed = 0; std::string text; bool reject = false, fail_complete = false; ui_result result = UI_OK; } state;
    app.setFleckRefreshStateCallback([](ui_app* app, ui_fleck_refresh_phase phase, ui_result result, void* context) -> ui_result {
        auto& state = *static_cast<State*>(context);
        ui_node* editor = nullptr;
        require(ui_app_find_fleck_node(app, "editor", &editor) == UI_OK, "State hook cannot resolve the current editor");
        require(ui_app_set_fleck_refresh_state_callback(app, nullptr, nullptr) == UI_ERROR_INVALID_STATE &&
            ui_app_poll(app, 0) == UI_ERROR_INVALID_STATE &&
            ui_app_refresh_fleck(app, "", 0, nullptr, nullptr) == UI_ERROR_INVALID_STATE,
            "State hook allowed reentrant refresh or registration");
        if (phase == UI_FLECK_REFRESH_CAPTURE) {
            ++state.captured;
            if (state.reject) { return UI_ERROR_INVALID_STATE; }
            uint32_t length = 0;
            require(ui_node_get_text(editor, nullptr, &length) == UI_OK, "State capture size query failed");
            state.text.resize(length);
            require(ui_node_get_text(editor, state.text.data(), &length) == UI_OK, "State capture failed");
            state.text.resize(length - 1);
        } else {
            ++state.completed;
            state.result = result;
            if (result == UI_OK) { require(ui_node_set_text(editor, state.text.c_str()) == UI_OK, "State restore failed"); }
            if (state.fail_complete) { return UI_ERROR_INVALID_STATE; }
        }
        return UI_OK;
    }, &state);
    const auto old = app.findFleckNode("editor").debugId();
    app.refreshFleck(next);
    require(state.captured == 1 && state.completed == 1 && state.result == UI_OK &&
        app.findFleckNode("editor").text() == "Business draft" && app.findFleckNode("editor").debugId() != old,
        "Application state was not restored onto the replacement node");
    ui_fleck_diagnostic diagnostic{};
    require(ui_app_refresh_fleck(app.nativeHandle(), "<broken", 7, nullptr, &diagnostic) == UI_ERROR_INVALID_ARGUMENT && state.captured == 1,
        "Malformed XML invoked application capture");
    state.reject = true;
    require(ui_app_refresh_fleck(app.nativeHandle(), first.data(), first.size(), nullptr, &diagnostic) == UI_ERROR_INVALID_STATE &&
        diagnostic.code == UI_FLECK_RUNTIME && state.completed == 1 && app.findFleckNode("editor").text() == "Business draft",
        "Capture rejection changed the old page or invoked completion");
    state.reject = false;
    const auto missing = "<Window xmlns='urn:sunfleck:v1'><TextBox Id='editor' Text='{Binding Missing}'/></Window>";
    require(ui_app_refresh_fleck(app.nativeHandle(), missing, std::strlen(missing), nullptr, &diagnostic) != UI_OK &&
        state.completed == 2 && state.result != UI_OK && app.findFleckNode("editor").text() == "Business draft",
        "Failed staging did not complete the state capture against the old page");
    app.queueFleckRefresh(first);
    require(app.poll(0) == UI_OK && state.completed == 3 && app.findFleckNode("editor").text() == "Business draft",
        "Queued refresh skipped application state hooks");
    state.fail_complete = true;
    app.refreshFleck(next);
    require(app.fleckRefreshStatus().result == UI_OK && app.findFleckNode("editor").text() == "Business draft" &&
        ui_app_poll(app.nativeHandle(), 0) == UI_ERROR_INVALID_STATE, "Completion error undid a commit or was silently lost");
    app.setFleckRefreshStateCallback(nullptr);
}
static void refresh_scroll_and_expansion()
{
    sunfleck::App app(UI_SOFTWARE, true);
    const std::string page = "<Window xmlns='urn:sunfleck:v1' Height='180'><StackPanel><ScrollView Id='scroll'><StackPanel><TreeView><TreeItem Id='branch' Text='Branch'><TreeItem Text='Child'/></TreeItem></TreeView>" +
        std::string("<TextBlock Id='label' Text='Before'/>") +
        "<TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/><TextBlock/></StackPanel></ScrollView></StackPanel></Window>";
    app.loadFleck(page);
    app.openWindow();
    auto* scroll = app.findFleckNode("scroll").nativeHandle();
    auto* branch = app.findFleckNode("branch").nativeHandle();
    branch->expanded = true;
    require(app.poll(0) == UI_OK && scroll->scroll_limit > 0, "Fixture did not produce a scroll range");
    scroll->offset = std::min(12.0, scroll->scroll_limit);
    const auto offset = scroll->offset;
    auto updated = page;
    updated.replace(updated.find("Before"), 6, "After");
    for (int i = 0; i < 20; ++i) {
        app.refreshFleck(i % 2 == 0 ? updated : page);
        require(app.poll(0) == UI_OK && scroll->offset == offset && branch->expanded,
            "Refresh lost scrolling or tree expansion");
    }
}

static void refresh_control_values()
{
    for (const auto* kind : {"ListView", "ComboBox", "TabView", "NavigationView"}) {
        const auto item = std::string(kind) == "TabView" || std::string(kind) == "NavigationView" ? "TabItem" : "ListItem";
        const auto child = [&](const char* id) {
            return "<" + std::string(item) + " Id='" + id + "' Text='" + id + "'/>";
        };
        const auto page = [&](const std::string& children, unsigned value = 0) {
            return "<Window xmlns='urn:sunfleck:v1'><" + std::string(kind) + " Id='selector' Value='" +
                std::to_string(value) + "'>" + children + "</" + kind + "></Window>";
        };
        sunfleck::App app(UI_SOFTWARE, true);
        app.loadFleck(page(child("a") + child("b") + child("c")));
        auto selector = app.findFleckNode("selector");
        selector.setValue(1);
        app.openWindow();
        const auto selected = app.findFleckNode("b").debugId();
        const auto reordered = page(child("c") + child("a") + child("b"));
        for (unsigned i = 0; i < 4; ++i) {
            app.refreshFleck(reordered);
            require(selector.value() == 2 && app.findFleckNode("b").debugId() == selected,
                "Refresh lost selection identity after reordering");
        }
        app.refreshFleck(page(child("c") + child("a") + child("b"), 1));
        require(selector.value() == 1, "Changed declarative selection did not take effect");
        selector.setValue(2);
        app.refreshFleck(page(child("c") + child("a"), 1));
        require(selector.value() == 1, "Removing the selected item left an invalid selection");
    }
    sunfleck::App app(UI_SOFTWARE, true);
    const std::string page = "<Window xmlns='urn:sunfleck:v1'><StackPanel>"
        "<CheckBox Id='check' Value='0'/><Switch Id='switch'/><ToggleButton Id='toggle'/>"
        "<Slider Id='slider' Value='20'/><SpinBox Id='spin' Value='10'/>"
        "</StackPanel></Window>";
    app.loadFleck(page);
    app.findFleckNode("check").setValue(1);
    app.findFleckNode("switch").setValue(1);
    app.findFleckNode("toggle").setValue(1);
    app.findFleckNode("slider").setValue(70);
    app.findFleckNode("spin").setValue(40);
    app.openWindow();
    for (unsigned i = 0; i < 4; ++i) {
        app.refreshFleck(page);
        require(app.findFleckNode("check").value() == 1 && app.findFleckNode("switch").value() == 1 &&
            app.findFleckNode("toggle").value() == 1 && app.findFleckNode("slider").value() == 70 &&
            app.findFleckNode("spin").value() == 40, "Refresh lost an unbound control value");
    }
    auto changed = page;
    changed.replace(changed.find("Value='20'"), 10, "Value='30'");
    app.refreshFleck(changed);
    require(app.findFleckNode("slider").value() == 30, "Changed declarative numeric value did not take effect");
    sunfleck::PropertyModel model(app);
    model.property("Checked", [] { return false; });
    changed.replace(changed.find("Value='0'"), 9, "Value='{Binding Checked, Mode=OneWay}'");
    ui_fleck_diagnostic diagnostic{};
    require(ui_app_refresh_fleck(app.nativeHandle(), changed.data(), changed.size(), model.nativeHandle(), &diagnostic) == UI_OK,
        diagnostic.message);
    require(app.findFleckNode("check").value() == 0, "Retained local state overrode a new binding");
}

static void refresh_radio_groups()
{
    sunfleck::App app(UI_SOFTWARE, true);
    const auto page = [](const std::string& first, const std::string& second = "<RadioButton Id='other' Value='1'/>") {
        return "<Window xmlns='urn:sunfleck:v1'><StackPanel><StackPanel Id='group'>" + first +
            "</StackPanel><StackPanel Id='otherGroup'>" + second + "</StackPanel></StackPanel></Window>";
    };
    const std::string a = "<RadioButton Id='a' Value='1'/>", b = "<RadioButton Id='b'/>";
    app.loadFleck(page(a + b));
    app.openWindow();
    const auto a_id = app.findFleckNode("a").debugId(), b_id = app.findFleckNode("b").debugId();
    app.findFleckNode("b").setValue(1);
    for (unsigned i = 0; i < 6; ++i) {
        app.refreshFleck(page(i % 2 ? a + b : b + a));
        require(app.findFleckNode("a").value() == 0 && app.findFleckNode("b").value() == 1 &&
            app.findFleckNode("other").value() == 1 && app.findFleckNode("a").debugId() == a_id &&
            app.findFleckNode("b").debugId() == b_id, "Refresh lost radio selection, identity or group isolation");
    }
    const std::string off = "<RadioButton Id='a' Value='0'/>";
    app.refreshFleck(page(off + b));
    require(app.findFleckNode("a").value() == 0 && app.findFleckNode("b").value() == 0,
        "Changed radio declaration did not reset its whole group");
    app.findFleckNode("b").setValue(1);
    app.refreshFleck(page(off + b + "<RadioButton Id='c' Value='1'/>"));
    require(app.findFleckNode("b").value() == 0 && app.findFleckNode("c").value() == 1,
        "New checked radio conflicted with retained group state");
    app.refreshFleck(page(a + b));
    require(app.findFleckNode("a").value() == 1 && app.findFleckNode("b").value() == 0,
        "Removed selected radio did not use the new group default");
    app.findFleckNode("b").setValue(1);
    app.refreshFleck(page(a, "<RadioButton Id='other' Value='1'/>" + b));
    require(app.findFleckNode("a").value() == 1 && app.findFleckNode("b").value() == 0 &&
        app.findFleckNode("other").value() == 1 && app.findFleckNode("b").debugId() == b_id,
        "Moving a selected radio carried a conflicting selection into another group");
    sunfleck::PropertyModel model(app);
    model.property("Selected", [] { return false; });
    app.refreshFleck(page("<RadioButton Id='a' Value='{Binding Selected, Mode=OneWay}'/>",
        "<RadioButton Id='other' Value='1'/>" + b), model.nativeHandle());
    require(app.findFleckNode("a").value() == 0, "New radio binding was overridden by local state");
}

static void queued_refresh()
{
    sunfleck::App app(UI_SOFTWARE, true);
    const auto poll = [&] { const auto result = app.poll(0); return result == UI_OK || result == UI_TIMEOUT; };
    constexpr std::string_view first = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='label' Text='Initial'/></Window>";
    constexpr std::string_view last = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='label' Text='Latest'/></Window>";
    app.loadFleck(first);
    app.openWindow();
    auto label = app.findFleckNode("label");
    auto* busy = label.nativeHandle();
    busy->running_actions = 1;
    app.queueFleckRefresh("<Window xmlns='urn:sunfleck:v1'><TextBlock Text='Superseded'/></Window>");
    std::string copied(last);
    app.queueFleckRefresh(copied);
    copied.assign(copied.size(), '?');
    for (unsigned i = 0; i < 3; ++i) {
        require(poll() && app.fleckRefreshStatus().pending && app.fleckRefreshStatus().sequence == 0 &&
            label.text() == "Initial", "Busy queue changed the page or produced a terminal report");
    }
    require(app.debugSnapshot().find("\"pending\":true") != std::string::npos, "Inspector omitted a queued refresh");
    busy->running_actions = 0;
    require(poll() && !app.fleckRefreshStatus().pending && app.fleckRefreshStatus().sequence == 1 &&
        label.text() == "Latest", "Queue did not copy/coalesce the latest XML and apply it after busy state ended");
    app.queueFleckRefresh("<Window");
    require(poll() && !app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_ERROR_INVALID_ARGUMENT &&
        app.fleckRefreshStatus().diagnostic.code != UI_FLECK_NONE && label.text() == "Latest", "Invalid queued XML damaged the page or was retried");
    const auto sequence = app.fleckRefreshStatus().sequence;
    app.queueFleckRefresh(first);
    app.cancelFleckRefresh();
    require(poll() && label.text() == "Latest" && app.fleckRefreshStatus().sequence == sequence, "Cancelled queue was applied");
    app.queueFleckRefresh(last);
    app.refreshFleck(first);
    require(!app.fleckRefreshStatus().pending && poll() && label.text() == "Initial", "Synchronous refresh did not supersede an old queued request");
    constexpr std::string_view bound = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='label' Text='{Binding Name}'/></Window>";
    {
        sunfleck::PropertyModel model(app);
        model.property("Name", []() -> std::string { throw std::runtime_error("Queued getter failure"); });
        app.queueFleckRefresh(bound, model.nativeHandle());
        require(poll() && !app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_ERROR_INVALID_STATE &&
            app.fleckRefreshStatus().diagnostic.code == UI_FLECK_VALUE && label.text() == "Initial", "Terminal queued binding failure was retried or changed the old UI");
    }
    {
        sunfleck::PropertyModel retired(app);
        retired.property("Name", [] { return std::string("Retired model"); });
        app.queueFleckRefresh(bound, retired.nativeHandle());
    }
    require(poll() && !app.fleckRefreshStatus().pending && app.fleckRefreshStatus().result == UI_ERROR_INVALID_STATE &&
        label.text() == "Initial", "Queued request dereferenced a retired model");
    {
        sunfleck::PropertyModel model(app);
        bool replace = true;
        model.property("Name", [&] {
            if (replace) { replace = false; app.queueFleckRefresh(last); }
            return std::string("Intermediate");
        });
        app.queueFleckRefresh(bound, model.nativeHandle());
        require(poll() && app.fleckRefreshStatus().pending && label.text() == "Intermediate",
            "Refresh discarded a newer request queued by its getter");
        require(poll() && !app.fleckRefreshStatus().pending && label.text() == "Latest",
            "Requeued source was invalidated while the previous request used its input");
    }
    {
        sunfleck::App pages(UI_SOFTWARE, true);
        pages.loadFleck(first);
        const auto initial_root = pages.fleckRoot();
        pages.loadFleck(last);
        pages.openWindow();
        pages.queueFleckRefresh("<Window xmlns='urn:sunfleck:v1'><TextBlock Text='Wrong page'/></Window>");
        pages.setRoot(initial_root);
        require(pages.poll(0) == UI_OK && !pages.fleckRefreshStatus().pending && pages.fleckRefreshStatus().result == UI_ERROR_INVALID_STATE &&
            pages.findFleckNode("label").text() == "Initial", "Queued source crossed a page-session switch");
    }
}

static void refresh_exported_adapter()
{
    sunfleck::App app(UI_SOFTWARE, true);
    auto source = app.createTextSource("Original");
    const ui_fleck_text_export item{"Name", source.nativeHandle()};
    const ui_fleck_data_context exports{&item, 1, nullptr, 0};
    constexpr std::string_view page =
        "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='label' Text='{Binding Name}'/></Window>";
    require(ui_app_load_fleck_with_context(app.nativeHandle(), page.data(), page.size(), &exports, nullptr) == UI_OK,
        "Could not load legacy text export");
    app.openWindow();
    app.refreshFleck(page);
    auto label = app.findFleckNode("label");
    require(label.text() == "Original", "Refresh lost the page-owned adapter");
    sunfleck::PropertyModel replacement(app);
    replacement.property("Name", [] { return std::string("Replacement"); });
    app.refreshFleck(page, replacement.nativeHandle());
    require(source.nativeHandle()->property_observers.empty() && label.text() == "Replacement",
        "Replacing a page-owned adapter left a source subscription");
}

static void diagnostic_failures()
{
    {
        sunfleck::App page_app(UI_SOFTWARE, true);
        sunfleck::PropertyModel model(page_app);
        bool broken = false;
        model.property("Caption", [&] {
            if (broken) { throw std::invalid_argument("Rejected getter"); }
            return std::string("Retained");
        });
        const std::string page = "<Window xmlns='urn:sunfleck:v1'>\n<Button Id='button' Content='{Binding Caption}'/>\n</Window>";
        require(ui_app_load_fleck_with_model(page_app.nativeHandle(), page.data(), page.size(), model.nativeHandle(), nullptr, nullptr) == UI_OK,
            "Diagnostic page failed to load");
        page_app.setFleckSourceUri("file:///diagnostic.fleck");
        broken = true;
        require(ui_model_notify_property_changed(model.nativeHandle(), "Caption") != UI_OK, "Failed getter was accepted");
        const auto diagnostic = page_app.bindingDiagnostics();
        const auto* button = page_app.findFleckNode("button").nativeHandle();
        require(diagnostic.find("\"page\":\"" + std::to_string(button->source->page_id) + "\"") != std::string::npos,
            "Diagnostic lost its page identity");
        require(diagnostic.find("\"propertySource\":{\"uri\":\"file:///diagnostic.fleck\",\"range\":{\"offset\":" +
            std::to_string(page.find("{Binding Caption}"))) != std::string::npos,
            "Diagnostic did not identify the failing property value");
        broken = false;
        model.notify("Caption");
        require(page_app.bindingDiagnostics().find("\"active\":false") != std::string::npos,
            "Property source diagnostic did not recover");
    }
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel properties(app);
    properties.property("Name", [] { return std::string("Retained"); });
    auto button = app.createNode(UI_BUTTON);
    properties.bind(button, UI_PROPERTY_CONTENT, "Name");
    app.setRoot(button);
    struct State { ui_model* model; ui_result completed = UI_OK; } state{properties.nativeHandle()};
    const ui_async_command_options options{sizeof(options), 2,
        [](void*, const ui_cancellation_token*) -> ui_result { return UI_ERROR_INVALID_ARGUMENT; }, nullptr,
        [](void* raw, ui_result result) { static_cast<State*>(raw)->completed = result; }, &state};
    ui_command* raw = nullptr;
    require(ui_command_create_async(app.nativeHandle(), &options, &raw) == UI_OK, "Async command creation failed");
    std::unique_ptr<ui_command, decltype(&ui_command_destroy)> command(raw, ui_command_destroy);
    properties.command("RegisteredAsync", raw);
    require(properties.describe("Commands").find("\"commandModes\":[\"Async\"]") != std::string::npos,
        "Native async command adapter did not describe supported modes");
    require(ui_node_bind_command(button.nativeHandle(), raw) == UI_OK, "Async command binding failed");
    require(ui_node_set_command_mode(button.nativeHandle(), UI_COMMAND_SYNC) == UI_ERROR_INVALID_ARGUMENT, "Async accepted Sync override");
    require(ui_node_set_command_mode(button.nativeHandle(), UI_COMMAND_ASYNC) == UI_OK, "Command mode failed");
    app.openWindow(); app.poll(0); app.activateButton();
    for (unsigned i = 0; i < 1000 && state.completed == UI_OK; ++i) { app.poll(1); }
    require(state.completed == UI_ERROR_INVALID_ARGUMENT, "Async execution error lost");
    require(button.text() == "Retained", "Failed command changed displayed value");

    bool malformed = false;
    const ui_model_property property{sizeof(property), 1, "NativeValue", UI_VALUE_STRING,
        [](void* raw, ui_binding_value* value) -> ui_result {
            *value = {};
            value->kind = *static_cast<bool*>(raw) ? UI_VALUE_INT : UI_VALUE_STRING;
            value->string = "Retained";
            return UI_OK;
        }, nullptr, &malformed};
    require(ui_model_add_property(properties.nativeHandle(), &property) == UI_OK, "Native property registration failed");
    properties.bind(button, UI_PROPERTY_CONTENT, "NativeValue");
    malformed = true;
    require(ui_model_notify_property_changed(properties.nativeHandle(), "NativeValue") == UI_ERROR_INVALID_ARGUMENT, "Getter type mismatch accepted");
    const auto json = app.bindingDiagnostics();
    require(json.find("\"code\":\"type_mismatch\"") != std::string::npos && json.find("\"actualType\":\"integer\"") != std::string::npos, "Getter type mismatch not distinguished from callback failure");
}
static void diagnostic_classification_matrix()
{
    enum class Cause { missing_model, foreign_model, missing_path, invalid_binding, read_only, text_type,
        getter, getter_type, null_string, invalid_bool, nonfinite, target_range, target_fraction, enabled_type, command_mode };
    struct Case { Cause cause; const char* code; };
    constexpr Case cases[] = {
        {Cause::missing_model, "model_missing"}, {Cause::foreign_model, "model_owner"},
        {Cause::missing_path, "path_missing"}, {Cause::invalid_binding, "invalid_binding"},
        {Cause::read_only, "read_only"}, {Cause::text_type, "type_mismatch"}, {Cause::getter, "getter_failed"},
        {Cause::getter_type, "type_mismatch"}, {Cause::null_string, "invalid_source_value"},
        {Cause::invalid_bool, "invalid_source_value"}, {Cause::nonfinite, "invalid_source_value"},
        {Cause::target_range, "invalid_target_value"}, {Cause::target_fraction, "invalid_target_value"},
        {Cause::enabled_type, "type_mismatch"}, {Cause::command_mode, "command_mismatch"}
    };
    for (const auto& test : cases) {
        sunfleck::App app(UI_SOFTWARE, true), foreign(UI_SOFTWARE, true);
        sunfleck::PropertyModel model(app), other(foreign);
        auto target = app.createNode(test.cause == Cause::read_only || test.cause == Cause::text_type ? UI_TEXT_BOX :
            test.cause == Cause::target_range || test.cause == Cause::target_fraction ? UI_SLIDER :
            test.cause == Cause::command_mode ? UI_BUTTON : UI_TEXT_BLOCK);
        const auto property = test.cause == Cause::target_range || test.cause == Cause::target_fraction ? UI_PROPERTY_VALUE :
            test.cause == Cause::enabled_type ? UI_PROPERTY_IS_ENABLED : test.cause == Cause::command_mode ? UI_PROPERTY_COMMAND : UI_PROPERTY_TEXT;
        if (property == UI_PROPERTY_TEXT) { target.setText("Retained"); }
        struct State { ui_binding_value value; ui_result status = UI_OK; } state{};
        state.value.kind = UI_VALUE_STRING; state.value.string = "Valid";
        ui_value_kind kind = UI_VALUE_STRING;
        if (test.cause == Cause::getter) { state.status = UI_ERROR_UNSUPPORTED; }
        if (test.cause == Cause::getter_type) { state.value.kind = UI_VALUE_INT; }
        if (test.cause == Cause::null_string) { state.value.string = nullptr; }
        if (test.cause == Cause::invalid_bool) { kind = state.value.kind = UI_VALUE_BOOL; state.value.integer = 2; }
        if (test.cause == Cause::nonfinite) { kind = state.value.kind = UI_VALUE_DOUBLE; state.value.number = std::numeric_limits<double>::quiet_NaN(); }
        if (test.cause == Cause::text_type || test.cause == Cause::target_range) {
            kind = state.value.kind = UI_VALUE_INT; state.value.integer = 101;
        }
        if (test.cause == Cause::target_fraction) { kind = state.value.kind = UI_VALUE_DOUBLE; state.value.number = 1.5; }
        ui_command* command = nullptr;
        unsigned command_reads = 0;
        if (test.cause == Cause::command_mode) {
            require(ui_command_create(app.nativeHandle(), [](ui_node*, void*) {}, nullptr, &command) == UI_OK, "Matrix command creation failed");
            model.command("RegisteredSync", command);
            model.property("DynamicCommand", [&] { ++command_reads; return command; });
            require(model.describe("Commands").find("\"commandModes\":[\"Sync\"]") != std::string::npos && command_reads == 0,
                "Command metadata queried a getter or lost inferred synchronous capabilities");
            model.setCommandModes("DynamicCommand", UI_COMMAND_SUPPORTS_SYNC | UI_COMMAND_SUPPORTS_ASYNC);
            require(model.describe("Commands").find("\"commandModes\":[\"Sync\",\"Async\"]") != std::string::npos && command_reads == 0,
                "Explicit dynamic command capabilities were not exported independently of its getter");
            require(ui_model_set_command_modes(model.nativeHandle(), "DynamicCommand", 4) == UI_ERROR_INVALID_ARGUMENT,
                "Invalid command capability flags were accepted");
            kind = state.value.kind = UI_VALUE_COMMAND; state.value.command = command;
            target.setCommandMode(UI_COMMAND_ASYNC);
        }
        const ui_model_property source{sizeof(source), 1, "Value", kind,
            [](void* raw, ui_binding_value* value) -> ui_result { const auto& state = *static_cast<State*>(raw); *value = state.value; return state.status; },
            test.cause == Cause::read_only ? nullptr : +[](void*, const ui_binding_value*) -> ui_result { return UI_OK; }, &state};
        require(ui_model_add_property(model.nativeHandle(), &source) == UI_OK, "Matrix property registration failed");
        ui_binding_options options{sizeof(options), 1, property, test.cause == Cause::missing_path ? "Missing" : "Value",
            test.cause == Cause::invalid_binding || test.cause == Cause::read_only || test.cause == Cause::text_type ? UI_BIND_TWO_WAY : UI_BIND_ONE_WAY,
            UI_UPDATE_PROPERTY_CHANGED, ""};
        const auto result = ui_node_bind_property(target.nativeHandle(), test.cause == Cause::missing_model ? nullptr :
            test.cause == Cause::foreign_model ? other.nativeHandle() : model.nativeHandle(), &options);
        const auto expected = test.cause == Cause::getter ? UI_ERROR_UNSUPPORTED : UI_ERROR_INVALID_ARGUMENT;
        const auto json = app.bindingDiagnostics();
        require(result == expected && json.find("\"code\":\"" + std::string(test.code) + "\"") != std::string::npos &&
            json.find("\"phase\":\"bind\"") != std::string::npos && json.find("\"active\":true") != std::string::npos,
            "Binding failure classification or original result was lost");
        if (property == UI_PROPERTY_TEXT) { require(target.text() == "Retained", "Rejected binding changed the old display"); }
        // Rebinding the same path successfully recovers its diagnostic.
        if (test.cause == Cause::missing_path) {
            auto missing = source; missing.path = "Missing";
            require(ui_model_add_property(model.nativeHandle(), &missing) == UI_OK, "Missing path repair failed");
        }
        state.status = UI_OK; state.value.kind = kind;
        state.value.string = "Valid"; state.value.integer = 1; state.value.number = 10;
        options.mode = UI_BIND_ONE_WAY;
        if (command) { target.setCommandMode(UI_COMMAND_SYNC); }
        sunfleck::PropertyModel repaired(app);
        if (test.cause == Cause::enabled_type) {
            repaired.property("Value", [] { return true; });
        }
        require(ui_node_bind_property(target.nativeHandle(), test.cause == Cause::enabled_type ? repaired.nativeHandle() : model.nativeHandle(), &options) == UI_OK &&
            app.bindingDiagnostics().find("\"active\":false") != std::string::npos, "Successful binding did not recover the failure");
    }
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    auto initiating = app.createNode(UI_TEXT_BLOCK), rejected = app.createNode(UI_TEXT_BLOCK);
    bool reenter = true;
    ui_result nested = UI_OK;
    model.property("Busy", [&] {
        if (reenter) {
            const ui_binding_options options{sizeof(options), 1, UI_PROPERTY_TEXT, "Busy", UI_BIND_ONE_WAY, UI_UPDATE_PROPERTY_CHANGED, ""};
            nested = ui_node_bind_property(rejected.nativeHandle(), model.nativeHandle(), &options);
        }
        return std::string("Kept");
    });
    model.bind(initiating, UI_PROPERTY_TEXT, "Busy");
    require(nested == UI_ERROR_INVALID_STATE && app.bindingDiagnostics().find("\"code\":\"model_busy\"") != std::string::npos,
        "Reentrant model binding was not diagnosed");
    reenter = false;
    model.bind(rejected, UI_PROPERTY_TEXT, "Busy");
    require(app.bindingDiagnostics().find("\"active\":false") != std::string::npos, "Model busy diagnostic did not recover");
}

static void diagnostic_update_matrix()
{
    enum class Cause { getter, wrong_kind, null_text, invalid_bool, nonfinite, range, fraction, setter, line_break };
    struct Case { Cause cause; const char* code; ui_value_kind kind; };
    constexpr Case cases[] = {
        {Cause::getter, "getter_failed", UI_VALUE_STRING}, {Cause::wrong_kind, "type_mismatch", UI_VALUE_STRING},
        {Cause::null_text, "invalid_source_value", UI_VALUE_STRING}, {Cause::invalid_bool, "invalid_source_value", UI_VALUE_BOOL},
        {Cause::nonfinite, "invalid_source_value", UI_VALUE_DOUBLE}, {Cause::range, "invalid_target_value", UI_VALUE_INT},
        {Cause::fraction, "invalid_target_value", UI_VALUE_DOUBLE}, {Cause::setter, "setter_failed", UI_VALUE_STRING},
        {Cause::line_break, "invalid_source_value", UI_VALUE_STRING}
    };
    for (const bool write : {false, true}) {
        for (const auto& test : cases) {
            if ((!write && (test.cause == Cause::setter || test.cause == Cause::line_break)) ||
                (write && (test.cause == Cause::range || test.cause == Cause::fraction))) { continue; }
            sunfleck::App app(UI_SOFTWARE, true);
            sunfleck::PropertyModel model(app);
            auto editor = app.createNode(test.kind == UI_VALUE_STRING ? UI_TEXT_BOX : test.kind == UI_VALUE_BOOL ? UI_CHECK_BOX : UI_SPIN_BOX);
            const auto property = test.kind == UI_VALUE_STRING ? UI_PROPERTY_TEXT : UI_PROPERTY_VALUE;
            struct State { ui_binding_value value{}; std::string text = "Saved"; ui_result get = UI_OK, set = UI_OK; unsigned sets = 0; } state;
            const auto repair = [&] {
                state.get = state.set = UI_OK;
                state.value = {}; state.value.kind = test.kind;
                state.value.string = state.text.c_str(); state.value.integer = test.kind == UI_VALUE_BOOL ? 1 : 20; state.value.number = 20;
            };
            repair();
            const ui_model_property source{sizeof(source), 1, "Value", test.kind,
                [](void* raw, ui_binding_value* value) -> ui_result { auto& state = *static_cast<State*>(raw); *value = state.value; return state.get; },
                [](void* raw, const ui_binding_value* value) -> ui_result {
                    auto& state = *static_cast<State*>(raw); ++state.sets;
                    if (state.set != UI_OK) { return state.set; }
                    state.value = *value;
                    if (value->kind == UI_VALUE_STRING) { state.text = value->string; state.value.string = state.text.c_str(); }
                    return UI_OK;
                }, &state};
            require(ui_model_add_property(model.nativeHandle(), &source) == UI_OK, "Update matrix registration failed");
            model.bind(editor, property, "Value", UI_BIND_TWO_WAY);
            const auto poison = [&] {
                switch (test.cause) {
                case Cause::getter: state.get = UI_ERROR_UNSUPPORTED; break;
                case Cause::wrong_kind: state.value.kind = UI_VALUE_INT; break;
                case Cause::null_text: state.value.string = nullptr; break;
                case Cause::invalid_bool: state.value.integer = 2; break;
                case Cause::nonfinite: state.value.number = std::numeric_limits<double>::infinity(); break;
                case Cause::range: state.value.integer = 101; break;
                case Cause::fraction: state.value.number = 1.5; break;
                case Cause::setter: state.set = UI_ERROR_UNSUPPORTED; break;
                case Cause::line_break: break;
                }
            };
            const auto attempt = [&] {
                if (!write) { return ui_model_notify_property_changed(model.nativeHandle(), "Value"); }
                if (property == UI_PROPERTY_TEXT) { editor.nativeHandle()->text = test.cause == Cause::line_break ? L"Invalid\nDraft" : L"Draft"; }
                else { editor.nativeHandle()->value = 0; }
                return ui_node_update_source(editor.nativeHandle(), property);
            };
            poison();
            const auto expected = test.cause == Cause::getter || test.cause == Cause::setter ? UI_ERROR_UNSUPPORTED : UI_ERROR_INVALID_ARGUMENT;
            require(attempt() == expected && attempt() == expected, "Update matrix changed the original error result");
            const auto diagnostics = app.bindingDiagnostics();
            const auto phase = write ? "write" : test.cause == Cause::range || test.cause == Cause::fraction ? "refresh" : "read";
            require(diagnostics.find("\"code\":\"" + std::string(test.code) + "\"") != std::string::npos &&
                diagnostics.find("\"phase\":\"" + std::string(phase) + "\"") != std::string::npos &&
                diagnostics.find("\"occurrences\":2") != std::string::npos && diagnostics.find("\"active\":true") != std::string::npos,
                "Read/refresh/write classification, coalescing or active state was lost");
            if (!write) {
                require(property == UI_PROPERTY_TEXT ? editor.text() == "Saved" : editor.value() == (test.kind == UI_VALUE_BOOL ? 1u : 20u),
                    "Failed notification changed the old display");
            } else {
                require(state.text == "Saved", "Rejected write modified business data");
                if (test.cause == Cause::setter || test.cause == Cause::line_break) {
                    require(editor.text() == "Saved", "Rejected setter/input did not restore the committed display");
                }
            }
            repair();
            if (write) {
                editor.nativeHandle()->text = L"Draft";
                require(ui_node_update_source(editor.nativeHandle(), property) == UI_OK, "Write repair failed");
            } else { model.notify("Value"); }
            require(app.bindingDiagnostics().find("\"active\":false") != std::string::npos, "Repaired update did not recover its diagnostic");
        }
    }
}

static void diagnostic_logging()
{
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    bool broken = false;
    for (const auto* path : {"First", "Second"}) {
        model.property(path, [&] {
            if (broken) { throw std::runtime_error("Log fixture getter"); }
            return std::string("Kept");
        });
    }
    auto first = app.createNode(UI_TEXT_BLOCK), second = app.createNode(UI_TEXT_BLOCK);
    model.bind(first, UI_PROPERTY_TEXT, "First");
    model.bind(second, UI_PROPERTY_TEXT, "Second");
    app.setRoot(first);
    app.openWindow();
    unsigned snapshots = 0;
    app.setDiagnosticCallback([](const char*, void* raw) { ++*static_cast<unsigned*>(raw); }, &snapshots);
    struct Log { ui_app* app; std::vector<std::string> batches; } log{app.nativeHandle(), {}};
    const auto sink = [](const char* json, void* raw) {
        auto& log = *static_cast<Log*>(raw);
        log.batches.emplace_back(json);
        require(ui_app_dispatch_diagnostics(log.app) == UI_ERROR_INVALID_STATE, "Log callback reentered dispatch");
        require(ui_app_set_diagnostic_log_callback(log.app, nullptr, nullptr) == UI_ERROR_INVALID_STATE,
            "Log callback replaced its sink during dispatch");
    };
    app.setDiagnosticLogCallback(sink, &log);
    broken = true;
    require(ui_model_notify_property_changed(model.nativeHandle(), "First") == UI_ERROR_INVALID_STATE &&
        ui_model_notify_property_changed(model.nativeHandle(), "Second") == UI_ERROR_INVALID_STATE,
        "Logging changed getter error codes");
    app.dispatchDiagnostics();
    app.dispatchDiagnostics();
    require(snapshots == 1 && log.batches.size() == 1 && log.batches.back().find("First") != std::string::npos &&
        log.batches.back().find("Second") != std::string::npos, "Log sink displaced snapshot callback or replayed unchanged history");
    broken = false;
    model.notify("First");
    app.poll(0);
    require(snapshots == 2 && log.batches.size() == 2 && log.batches.back().find("First") != std::string::npos &&
        log.batches.back().find("Second") == std::string::npos && log.batches.back().find("\"active\":false") != std::string::npos,
        "Log sink did not isolate changed recovery records");
    app.setDiagnosticLogCallback(nullptr);
    model.notify("Second");
    app.dispatchDiagnostics();
    require(snapshots == 3 && log.batches.size() == 2 && first.text() == "Kept" && second.text() == "Kept",
        "Detached logging affected diagnostics or displayed values");
    app.setDiagnosticLogCallback(sink, &log);
    app.dispatchDiagnostics();
    require(snapshots == 3 && log.batches.size() == 3 && log.batches.back().find("Second") != std::string::npos,
        "New log sink did not receive retained history independently");
    app.setDiagnosticLogCallback(nullptr);
    app.setDiagnosticCallback(nullptr);
}
static void refresh_pending_editor()
{
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel model(app);
    std::string value = "Saved";
    bool reject = true;
    model.property("Name", [&] { return value; }, [&](std::string next) {
        if (reject) { throw std::invalid_argument("Rejected"); }
        value = std::move(next);
    });
    constexpr std::string_view page =
        "<Window xmlns='urn:sunfleck:v1'><StackPanel><TextBox Id='editor' Text='{Binding Name, Mode=TwoWay, UpdateSourceTrigger=LostFocus}'>"
        "<TextBox.VisualStateGroups><VisualStateGroup Name='ValidationStates'>"
        "<VisualState Name='InvalidUnfocused'><Storyboard><Timeline Property='Opacity' DurationMs='100' HoldEnd='true'>"
        "<KeyFrame Position='0' Value='0.5'/><KeyFrame Position='1' Value='0.5'/></Timeline></Storyboard></VisualState>"
        "<VisualState Name='InvalidFocused'><Storyboard><Timeline Property='Opacity' DurationMs='100' HoldEnd='true'>"
        "<KeyFrame Position='0' Value='0.75'/><KeyFrame Position='1' Value='0.75'/></Timeline></Storyboard></VisualState>"
        "</VisualStateGroup></TextBox.VisualStateGroups></TextBox></StackPanel></Window>";
    require(ui_app_load_fleck_with_model(app.nativeHandle(), page.data(), page.size(), model.nativeHandle(), nullptr, nullptr) == UI_OK,
        "Could not load deferred editor");
    app.openWindow();
    auto editor = app.findFleckNode("editor");
    editor.nativeHandle()->text = L"Draft";
    binding_user_changed(*editor.nativeHandle(), UI_PROPERTY_TEXT);
    app.refreshFleck(page, model.nativeHandle());
    require(editor.text() == "Draft" && value == "Saved", "Refresh lost or committed the deferred draft");
    constexpr std::string_view remove_editor =
        "<Window xmlns='urn:sunfleck:v1'><Panel><TextBlock Text='Replacement'/></Panel></Window>";
    require(ui_app_refresh_fleck(app.nativeHandle(), remove_editor.data(), remove_editor.size(),
        model.nativeHandle(), nullptr) == UI_ERROR_INVALID_STATE &&
        app.findFleckNode("editor").nativeHandle() == editor.nativeHandle() && editor.text() == "Draft",
        "Root replacement discarded an uncommitted draft");
    app.advanceAnimationClock(0);
    require(ui_node_update_source(editor.nativeHandle(), UI_PROPERTY_TEXT) != UI_OK && value == "Saved",
        "A rejected setter modified the model");
    require(app.bindingDiagnostics().find("\"code\":\"setter_failed\"") != std::string::npos,
        "Setter failure diagnostic missing");
    app.poll(0);
    require(editor.nativeHandle()->opacity == 0.5, "Rejected source write did not enter the invalid state");
    editor.focus();
    app.poll(0);
    require(editor.nativeHandle()->opacity == 0.75, "Focusing an invalid editor did not update its appearance");
    reject = false;
    editor.nativeHandle()->text = L"Draft";
    binding_user_changed(*editor.nativeHandle(), UI_PROPERTY_TEXT);
    editor.updateSource(UI_PROPERTY_TEXT);
    app.poll(0);
    require(editor.nativeHandle()->opacity == 1, "Successful source write did not leave the invalid state");
    require(value == "Draft", "Refreshed draft no longer commits");
    reject = true;
    editor.nativeHandle()->text = L"Rejected again";
    binding_user_changed(*editor.nativeHandle(), UI_PROPERTY_TEXT);
    require(ui_node_update_source(editor.nativeHandle(), UI_PROPERTY_TEXT) != UI_OK, "Expected a rejected write");
    app.poll(0);
    require(editor.nativeHandle()->opacity == 0.75, "Second failure did not enter focused error appearance");
    require(ui_node_clear_binding(editor.nativeHandle(), UI_PROPERTY_TEXT) == UI_OK, "Could not detach invalid binding");
    app.poll(0);
    require(editor.nativeHandle()->opacity == 1, "Detached binding kept its error appearance");
}
int main(int argc, char** argv)
{
    try {
        require(argc == 4, "Expected XML, binary and source map fixtures");
        refresh_page();
        refresh_application_state();
        refresh_exported_adapter();
        refresh_scroll_and_expansion();
        refresh_control_values();
        refresh_radio_groups();
        queued_refresh();
        refresh_pending_editor();
        const auto xml = read(argv[1]), binary = read(argv[2]), map = read(argv[3]);
        require(sunfleck::App::fleckSchema().find("\"name\":\"Button\"") != std::string::npos, "Control schema missing");
        const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, UI_HEADLESS};
        for (unsigned mode = 0; mode < 3; ++mode) {
            sunfleck::App app(options);
            sunfleck::PropertyModel properties(app);
            unsigned reads = 0;
            std::string name = "Business";
            properties.property("Name", [&] { ++reads; return name; }, [&](std::string value) { name = std::move(value); });
            properties.property("Unused", [] { return 123; });
            bool broken = false;
            properties.property("SometimesBroken", [&]() -> std::string {
                if (broken) { throw std::runtime_error("Getter failure"); }
                return "Valid";
            });
            ui_fleck_diagnostic diagnostic{};
            const auto result = mode == 0 ? ui_app_load_fleck_with_model(app.nativeHandle(), xml.data(), xml.size(), properties.nativeHandle(), nullptr, &diagnostic)
                : mode == 1 ? ui_app_load_fleck_resource_with_model(app.nativeHandle(), binary.data(), binary.size(), properties.nativeHandle(), nullptr, &diagnostic)
                : load_development_with_model(app.nativeHandle(), properties.nativeHandle(), nullptr, &diagnostic);
            require(result == UI_OK, diagnostic.message);
            app.attachFleckSourceMap(map);
            auto label = app.findFleckNode("label");
            const auto* raw_label = label.nativeHandle();
            const auto attribute = std::find_if(raw_label->location.attributes.begin(), raw_label->location.attributes.end(),
                [](const auto& value) { return value.name == "Text"; });
            require(raw_label->source && raw_label->source->page_id != 0 && raw_label->source->model == "DevelopmentModel" &&
                raw_label->source->model_id == properties.nativeHandle()->debug_id &&
                attribute != raw_label->location.attributes.end() &&
                xml.substr(attribute->value.offset, attribute->value.length) == "{Binding Name}" &&
                raw_label->declared_bindings.size() == 1 && raw_label->declared_bindings[0].model_id == properties.nativeHandle()->debug_id &&
                raw_label->bindings[0].model == properties.nativeHandle(),
                "XML/binary/generated C lost the original property, page or model instance association");
            const auto id = label.debugId();
            require(id != 0 && app.findDebugNode(id).nativeHandle() == label.nativeHandle(), "Object identity mismatch");
            const auto previous_reads = reads;
            const auto metadata = properties.describe("DevelopmentModel");
            require(metadata.find("Unused") != std::string::npos, "Unbound properties must be discoverable");
            require(metadata.find("\"writable\":true") != std::string::npos, "Writability missing");
            require(metadata.find("development.cpp") != std::string::npos, "C++ registration source missing");
            const ui_model_property table[] = {
                {sizeof(ui_model_property), 1, "TableFirst", UI_VALUE_INT, [](void*, ui_binding_value* value) -> ui_result { *value = {}; value->kind = UI_VALUE_INT; return UI_OK; }, nullptr, nullptr},
                {sizeof(ui_model_property), 1, "Unused", UI_VALUE_INT, [](void*, ui_binding_value* value) -> ui_result { *value = {}; value->kind = UI_VALUE_INT; return UI_OK; }, nullptr, nullptr}
            };
            require(ui_model_add_properties(properties.nativeHandle(), table, 2) == UI_ERROR_INVALID_ARGUMENT, "Duplicate table entry accepted");
            require(properties.describe("Model").find("TableFirst") == std::string::npos, "Partial property table was retained");
            require(ui_model_add_properties(properties.nativeHandle(), table, 1) == UI_OK, "Valid property table rejected");
            const auto snapshot = app.debugSnapshot();
            require(reads == previous_reads, "Debug queries executed a getter");
            require(snapshot.find("development.fleck") != std::string::npos, "Source URI missing");
            require(snapshot.find("\"line\":3") != std::string::npos, "Source line missing");
            require(snapshot.find("\"endColumn\":") != std::string::npos, "Source extent missing");
            require(snapshot.find("\"path\":\"Name\"") != std::string::npos, "Binding model link missing");
            require(snapshot.find("\"model\":\"DevelopmentModel\",\"modelId\":\"" +
                std::to_string(properties.nativeHandle()->debug_id) + "\"") != std::string::npos,
                "XML/binary/generated C snapshot lost the page model name and actual model identity");
            char short_buffer[2] = {'Q', '\0'};
            size_t required = sizeof(short_buffer);
            require(ui_app_get_debug_snapshot(app.nativeHandle(), short_buffer, &required) == UI_ERROR_INVALID_ARGUMENT && short_buffer[0] == 'Q' && required > 2, "Buffer contract broken");
            auto stale = map;
            const auto hash = stale.find("\"fingerprint\":\"") + 15;
            stale[hash] = stale[hash] == '0' ? '1' : '0';
            require(ui_app_attach_fleck_source_map(app.nativeHandle(), stale.data(), stale.size(), &diagnostic) == UI_ERROR_INVALID_ARGUMENT, "Stale map accepted");
            require(app.debugSnapshot().find("development.fleck") != std::string::npos, "Failed map damaged source");
            app.openWindow();
            require(label.debugId() == id, "Object identity changed across frames");
            require(ui_app_get_debug_snapshot(nullptr, nullptr, &required) == UI_ERROR_INVALID_ARGUMENT, "Invalid app accepted");
            auto target = app.createNode(UI_TEXT_BLOCK);
            properties.bind(target, UI_PROPERTY_TEXT, "SometimesBroken");
            broken = true;
            for (unsigned i = 0; i < 3; ++i) {
                require(ui_model_notify_property_changed(properties.nativeHandle(), "SometimesBroken") == UI_ERROR_INVALID_STATE, "Getter error was lost");
            }
            require(app.bindingDiagnostics().find("\"occurrences\":3") != std::string::npos, "Repeated errors not coalesced");
            unsigned events = 0;
            app.setDiagnosticCallback([](const char*, void* context) { ++*static_cast<unsigned*>(context); }, &events);
            app.dispatchDiagnostics(); app.dispatchDiagnostics();
            require(events == 1, "Duplicate event without changes");
            broken = false;
            properties.notify("SometimesBroken");
            app.dispatchDiagnostics();
            require(events == 2 && app.bindingDiagnostics().find("\"active\":false") != std::string::npos, "Recovery not published");
            app.setDiagnosticCallback(nullptr);
        }
        diagnostic_failures();
        diagnostic_logging();
        diagnostic_classification_matrix();
        diagnostic_update_matrix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
