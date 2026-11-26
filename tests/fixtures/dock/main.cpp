#include <sunfleck/dock.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

#ifdef DOCK_EXAMPLE_FLECK
extern "C" ui_result load_dock_workspace(ui_app*, const ui_fleck_data_context*, ui_fleck_diagnostic*);
#endif

using sunfleck::App;
using sunfleck::Node;
using sunfleck::Dock;

namespace {
constexpr const char* action_ids[] = {"reset", "float_tab", "float_group", "auto_hide", "show_tools", "save", "load"};
constexpr const char* action_titles[] = {"Reset layout", "Float tab", "Float group", "Auto hide", "Show tools", "Save layout", "Load layout"};
constexpr const char* editor_text = "Edit this document, then float or move its tab.\n\nThe same content node keeps its text and binding across windows.";
constexpr const char* explorer_text = "Sunfleck\n\n  src\n    main.cpp\n\n  docs\n    notes.md";
constexpr const char* second_text = "A second document. Float group moves both tabs together.";
constexpr const char* output_text = "Build completed successfully.\n0 errors · 0 warnings";
constexpr const char* ready_text = "Ready · Drag tabs to dock, drag outside to float. Esc cancels.";

// Application defaults use stable IDs, independently of registration order.
constexpr const char* initial_layout = R"({"version":1,"root":{"id":1,"axis":1,"ratio":2400,
"first":{"id":2,"axis":0,"kind":1,"active":"explorer","tabs":["explorer"]},
"second":{"id":3,"axis":2,"ratio":7800,
"first":{"id":4,"axis":0,"kind":0,"active":"editor_a","tabs":["editor_a","editor_b"]},
"second":{"id":5,"axis":0,"kind":1,"active":"output","tabs":["output"]}}},"floating":[],"items":[
{"id":"explorer","state":0,"group":2,"index":0,"edge":0},
{"id":"editor_a","state":0,"group":4,"index":0,"edge":0},
{"id":"editor_b","state":0,"group":4,"index":1,"edge":0},
{"id":"output","state":0,"group":5,"index":0,"edge":0}]})";

constexpr const char* dock_style = R"(
<Resource Key="dock.title_height" Value="22"/>
<Resource Key="dock.tab_height" Value="28"/>
<Resource Key="dock.splitter_width" Value="4"/>
<DockTemplate Part="Splitter"><Fill Background="panel"/></DockTemplate>)";

struct View {
    Node host, status, editor, theme, dark, compact, protect;
    std::vector<Node> actions;
};

Node child(App& app, Node parent, ui_node_kind kind, const char* text = "")
{
    auto node = app.createNode(kind);
    if (*text) {
        node.setText(text);
    }
    parent.append(node);
    return node;
}

View build(App& app)
{
#ifdef DOCK_EXAMPLE_FLECK
    sunfleck::detail::check(load_dock_workspace(app.nativeHandle(), nullptr, nullptr));
    View view{app.findFleckNode("workspace"), app.findFleckNode("status"), app.findFleckNode("editor"),
              app.findFleckNode("theme"), app.findFleckNode("dark"), app.findFleckNode("compact"), app.findFleckNode("protect"), {}};
    for (auto id : action_ids) {
        view.actions.push_back(app.findFleckNode(id));
    }
    return view;
#else
    app.setWindowOptions("Sunfleck Dock Workspace", 1100, 760);
    auto root = app.createNode(UI_DOCK_PANEL);
    root.setLayout(8, 4);
    auto header = child(app, root, UI_STACK_PANEL);
    header.setLayout(6, 0);
    sunfleck::detail::check(ui_node_set_dock(header.nativeHandle(), UI_DOCK_TOP));
    auto appearance = child(app, header, UI_TOOL_BAR);
    appearance.setLayout(6, 0);
    auto title = child(app, appearance, UI_LABEL, "Dock Workspace");
    title.setMinWidth(140);
    auto theme = child(app, appearance, UI_COMBO_BOX);
    theme.setWidth(200);
    const char* names[] = {"Fluent 2", "Material 3", "Radix Themes", "Apple Liquid Glass", "Sunfleck Lite"};
    theme.setItems(names, 5);
    auto dark = child(app, appearance, UI_SWITCH, "Dark");
    dark.setWidth(106);
    auto compact = child(app, appearance, UI_SWITCH, "Compact");
    compact.setWidth(134);
    compact.setValue(1);
    auto protect = child(app, appearance, UI_CHECK_BOX, "Protect Editor A");
    protect.setWidth(180);
    protect.setValue(1);
    auto toolbar = child(app, header, UI_TOOL_BAR);
    toolbar.setLayout(6, 0);
    std::vector<Node> actions;
    for (auto label : action_titles) {
        auto button = app.createNode(UI_BUTTON);
        button.setText(label);
        button.setWidth(112);
        actions.push_back(button);
    }
    for (unsigned index : {1u, 2u, 3u, 4u}) { toolbar.append(actions[index]); }
    child(app, toolbar, UI_LABEL); // Flexible space separates docking and file actions.
    for (unsigned index : {5u, 6u, 0u}) { toolbar.append(actions[index]); }
    auto status = child(app, root, UI_LABEL, ready_text);
    sunfleck::detail::check(ui_node_set_dock(status.nativeHandle(), UI_DOCK_BOTTOM));
    auto host = child(app, root, UI_DOCK_HOST);
    app.setRoot(root);
    Dock dock(host);
    auto editor = app.createNode(UI_TEXT_AREA);
    editor.setText(editor_text);
    auto a = dock.add("editor_a", "Editor A", editor, UI_DOCK_DOCUMENT_KIND, UI_DOCK_DEFAULT_CAPABILITIES, 240);
    auto b_content = app.createNode(UI_TEXT_AREA);
    b_content.setText(second_text);
    dock.add("editor_b", "Editor B", b_content);
    auto explorer_content = app.createNode(UI_TEXT_BLOCK);
    explorer_content.setText(explorer_text);
    auto explorer = dock.add("explorer", "Explorer", explorer_content, UI_DOCK_TOOL_KIND, UI_DOCK_DEFAULT_CAPABILITIES, 160);
    explorer.move(&a, UI_DOCK_POSITION_LEFT);
    auto output_content = app.createNode(UI_TEXT_BLOCK);
    output_content.setText(output_text);
    auto output = dock.add("output", "Output", output_content, UI_DOCK_TOOL_KIND);
    output.move(&a, UI_DOCK_POSITION_BOTTOM);
    a.activate();
    dock.restore(initial_layout);
    return {host, status, editor, theme, dark, compact, protect, std::move(actions)};
#endif
}

// File IO is application policy; the library only serializes the layout in memory.
void save_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
    if (!file) {
        throw std::runtime_error("Cannot save layout file");
    }
}

std::string load_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 0 || size > 8 * 1024 * 1024) {
        throw std::runtime_error("Cannot read layout file (missing or too large)");
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    if (!file.read(text.data(), size)) {
        throw std::runtime_error("Cannot read layout file");
    }
    return text;
}

class Workspace {
    App& app_;
    View view_;
    Dock dock_;
    sunfleck::TextSource document_;
    std::filesystem::path layout_path_;
    ui_dock_item* protected_item_ = nullptr;

    static void action(ui_node* node, void* context) noexcept
    {
        auto& self = *static_cast<Workspace*>(context);
        try {
            self.activate(node);
        } catch (const std::exception& error) {
            // Exceptions never cross the C callback boundary. Keep the sample usable.
            ui_node_set_text(self.view_.status.nativeHandle(), error.what());
        } catch (...) {
            ui_node_set_text(self.view_.status.nativeHandle(), "Operation failed");
        }
    }

    static uint32_t closing(ui_dock*, ui_dock_item* item, ui_dock_event_kind event, void* context) noexcept
    {
        auto& self = *static_cast<Workspace*>(context);
        if (event != UI_DOCK_CLOSING || item != self.protected_item_) {
            return 0;
        }
        uint32_t protect = 0;
        if (ui_node_get_value(self.view_.protect.nativeHandle(), &protect) != UI_OK || protect) {
            ui_node_set_text(self.view_.status.nativeHandle(), "Close cancelled. Turn off Protect Editor A to allow closing.");
            return 1;
        }
        return 0;
    }

    void apply_theme()
    {
        sunfleck::applyTheme(app_, static_cast<ui_builtin_theme>(view_.theme.value()), dock_style,
            view_.dark.value() != 0, view_.compact.value() != 0);
    }

    void activate(ui_node* node)
    {
        if (node == view_.theme.nativeHandle() || node == view_.dark.nativeHandle() || node == view_.compact.nativeHandle()) {
            apply_theme();
            view_.status.setText("Theme updated in the workspace and floating windows.");
            return;
        }
        if (node == view_.protect.nativeHandle()) {
            view_.status.setText(view_.protect.value() ? "Editor A is protected from closing." : "Editor A can now be closed.");
            return;
        }
        for (std::size_t i = 0; i < view_.actions.size(); ++i) {
            if (view_.actions[i].nativeHandle() == node) {
                execute(i);
                return;
            }
        }
    }

public:
    Workspace(App& app, std::filesystem::path layout_path)
        : app_(app), view_(build(app)), dock_(view_.host), document_(app.createTextSource(editor_text)),
          layout_path_(std::move(layout_path))
    {
        sunfleck::detail::check(ui_node_bind_text(view_.editor.nativeHandle(), document_.nativeHandle(), UI_BIND_TWO_WAY));
        dock_.captureDefault();
        protected_item_ = dock_.find("editor_a").nativeHandle();
        dock_.setEvent(closing, this);
        for (auto button : view_.actions) {
            button.setAction(action, this);
        }
        for (auto node : {view_.theme, view_.dark, view_.compact, view_.protect}) {
            node.setAction(action, this);
        }
        apply_theme();
    }

    ~Workspace() { ui_dock_set_event(dock_.nativeHandle(), nullptr, nullptr); }

    void execute(std::size_t index)
    {
        auto editor = dock_.find("editor_a");
        auto explorer = dock_.find("explorer");
        if (index == 0) {
            dock_.reset();
        } else if (index == 1 || index == 2) {
            editor.show();
            editor.floatWindow({180, 160, 640, 420}, index == 2);
        } else if (index == 3) {
            explorer.autoHide(UI_DOCK_LEFT);
        } else if (index == 4) {
            if (explorer.state() == UI_DOCK_AUTO_HIDDEN) {
                explorer.pin();
            } else {
                explorer.show();
            }
            dock_.find("output").show();
        } else if (index == 5) {
            save_file(layout_path_, dock_.save());
        } else if (index == 6) {
            dock_.restore(load_file(layout_path_));
        }
        const std::string message = index >= 5 ? std::string(action_titles[index]) + ": " + layout_path_.string() : action_titles[index];
        view_.status.setText(message.c_str());
    }

    bool verify()
    {
        app_.poll(0);
        document_.set("Edited content survives floating and layout restore.");
        execute(2);
        app_.poll(0);
        if (dock_.windows().size() != 1 || dock_.find("editor_b").state() != UI_DOCK_FLOATING) {
            return false;
        }
        execute(3);
        execute(5);
        execute(0);
        execute(6);
        app_.poll(0);
        auto editor = dock_.find("editor_a");
        if (editor.state() != UI_DOCK_FLOATING || dock_.find("explorer").state() != UI_DOCK_AUTO_HIDDEN || editor.close()) {
            return false;
        }
        // Rejected restore must retain the saved valid layout.
        const auto saved = dock_.save();
        save_file(layout_path_, "{broken");
        try {
            execute(6);
            return false;
        } catch (const sunfleck::Error&) {
            if (dock_.save() != saved) {
                return false;
            }
        }
        execute(4);
        view_.protect.setValue(0);
        if (!editor.close()) {
            return false;
        }
        execute(0);
        document_.set("Binding remains alive after reset.");
        app_.poll(0);
        return view_.editor.text() == "Binding remains alive after reset." && dock_.windows().empty();
    }
};

}

int main()
{
    try {
        App app(UI_SOFTWARE, true);
        const auto layout_path = std::filesystem::current_path() /
#ifdef DOCK_EXAMPLE_FLECK
            "dock-fleck-test.json";
#else
            "dock-code-test.json";
#endif
        Workspace workspace(app, layout_path);
        app.openWindow();
        const bool passed = workspace.verify();
        std::error_code ignored;
        std::filesystem::remove(layout_path, ignored);
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
