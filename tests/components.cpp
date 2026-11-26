#include "fleck.h"
#include "ui_tree.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sunfleck/ui.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static BOOL CALLBACK find_component_window(HWND window, LPARAM result) {
    wchar_t name[64];
    if (GetClassNameW(window, name, 64) && wcscmp(name, L"SunfleckPreview") == 0) {
        *reinterpret_cast<HWND*>(result) = window;
        return FALSE;
    }
    return TRUE;
}
#endif
#define CHECK(x)                                                               \
    do {                                                                       \
        if (!(x)) {                                                            \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);               \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)
static std::string text(ui_node *node) {
    uint32_t n = 0;
    CHECK(ui_node_get_text(node, nullptr, &n) == UI_OK);
    std::string s(n, '\0');
    CHECK(ui_node_get_text(node, s.data(), &n) == UI_OK);
    s.resize(n - 1);
    return s;
}
static ui_result name_get(void *context, ui_binding_value *value) {
    value->kind = UI_VALUE_STRING;
    value->string = static_cast<std::string *>(context)->c_str();
    return UI_OK;
}
static ui_result model_get(void *context, ui_binding_value *value) {
    value->kind = UI_VALUE_MODEL;
    value->model = *static_cast<ui_model **>(context);
    return UI_OK;
}
static ui_component *component(ui_app *app, const char *id) {
    ui_node *n = nullptr;
    ui_component *c = nullptr;
    CHECK(ui_app_find_fleck_node(app, id, &n) == UI_OK);
    CHECK(ui_node_get_component(n, &c) == UI_OK);
    return c;
}
static ui_node *part(ui_component *c, const char *id) {
    ui_node *n = nullptr;
    CHECK(ui_component_find_node(c, id, &n) == UI_OK);
    return n;
}
int main(int argc, char** argv) {
    const bool window = argc > 1 && std::string_view(argv[1]) == "--window";
    ui_app *app = nullptr;
    ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE,
                           window ? 0u : UI_HEADLESS};
    CHECK(ui_app_create(&options, &app) == UI_OK);
    const char *card =
        R"(<UserControl xmlns="urn:sunfleck:v1" Class="Example.UserCard" Namespace="urn:test"><StackPanel><TextBlock Id="name" Text="{Binding Name}"/><TextBox Id="edit"/><Label Id="nameLabel" Text="Name" Anchor="edit"/><ContentPresenter DataContext="null"/></StackPanel></UserControl>)";
    const char *rating =
        R"(<ControlTemplate xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Class="Example.RatingTemplate" Namespace="urn:test" TargetType="c:Rating"><Slider Id="PART_Value" Value="{Binding Value, RelativeSource=TemplatedParent, Mode=TwoWay}"/></ControlTemplate>)";
    const char *compact =
        R"(<ControlTemplate xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Class="Example.Compact" Namespace="urn:test" TargetType="c:Rating"><StackPanel><TextBlock Id="display" Text="{TemplateBinding Value}"/><Slider Id="PART_Value" Value="{Binding Value, RelativeSource=TemplatedParent, Mode=TwoWay}"/></StackPanel></ControlTemplate>)";
    const char *invalid =
        R"(<ControlTemplate xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Class="Example.Invalid" Namespace="urn:test" TargetType="c:Rating"><TextBlock Text="broken"/></ControlTemplate>)";
    ui_fleck_resource_entry resources[] = {
        {"card.fleck", card, strlen(card)},
        {"rating.fleck", rating, strlen(rating)},
        {"compact.fleck", compact, strlen(compact)},
        {"invalid.fleck", invalid, strlen(invalid)}};
    ui_fleck_diagnostic diagnostic{};
    CHECK(ui_app_register_fleck_resources(app, resources, 4, &diagnostic) ==
          UI_OK);
    ui_component_property value{};
    value.name = "Value";
    value.default_value.kind = UI_VALUE_INT;
    value.default_value.integer = 25;
    value.default_mode = UI_BIND_TWO_WAY;
    ui_template_part required{"PART_Value", UI_SLIDER, 1};
    const char *events[] = {"Changed"};
    ui_component_type type{};
    type.struct_size = sizeof(type);
    type.version = UI_COMPONENT_TYPE_VERSION;
    type.namespace_uri = "urn:test";
    type.name = "Rating";
    type.template_namespace = "urn:test";
    type.template_name = "RatingTemplate";
    type.properties = &value;
    type.property_count = 1;
    type.parts = &required;
    type.part_count = 1;
    type.events = events;
    type.event_count = 1;
    CHECK(ui_app_register_component_type(app, &type) == UI_OK);
    CHECK(ui_app_register_component_type(app, &type) ==
          UI_ERROR_INVALID_ARGUMENT);
    std::string first = "Alice", second = "Bob";
    ui_model *a = nullptr;
    ui_model *b = nullptr;
    ui_model *parent = nullptr;
    CHECK(ui_model_create(app, &a) == UI_OK);
    CHECK(ui_model_create(app, &b) == UI_OK);
    CHECK(ui_model_create(app, &parent) == UI_OK);
    ui_model_property pa{sizeof(pa), 1,       "Name", UI_VALUE_STRING,
                         name_get,   nullptr, &first};
    CHECK(ui_model_add_property(a, &pa) == UI_OK);
    pa.context = &second;
    CHECK(ui_model_add_property(b, &pa) == UI_OK);
    ui_model_property reference{
        sizeof(reference), 1,       "Primary", UI_VALUE_MODEL,
        model_get,         nullptr, &a};
    CHECK(ui_model_add_property(parent, &reference) == UI_OK);
    // Model metadata must describe reference kinds without indexing beyond
    // the scalar-kind table or evaluating a business getter.
    size_t metadata_size = 0;
    CHECK(ui_model_describe(parent, "Parent", nullptr, &metadata_size) == UI_OK);
    std::string metadata(metadata_size, '\0');
    CHECK(ui_model_describe(parent, "Parent", metadata.data(), &metadata_size) == UI_OK);
    CHECK(metadata.find("\"type\":\"model\"") != std::string::npos);
    reference.path = "Secondary";
    reference.context = &b;
    CHECK(ui_model_add_property(parent, &reference) == UI_OK);
    const char *page =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><StackPanel><c:UserCard Id="a" DataContext="{Binding Primary}"><TextBlock Id="supplied" Text="{Binding Name}"/></c:UserCard><c:UserCard Id="b" DataContext="{Binding Secondary}"/><c:Rating Id="rating" Value="42"/></StackPanel></Window>)";
    auto status = ui_app_load_fleck_with_model(app, page, strlen(page), parent,
                                               nullptr, &diagnostic);
    if (status != UI_OK) {
        std::fprintf(stderr, "%s\n", diagnostic.message);
    }
    CHECK(status == UI_OK);
    ui_node* supplied = nullptr;
    CHECK(ui_app_find_fleck_node(app, "supplied", &supplied) == UI_OK && text(supplied) == "Alice");
    ui_node *logical = nullptr, *visual = nullptr, *child = nullptr;
    CHECK(ui_node_get_parent(supplied, UI_LOGICAL_TREE, &logical) == UI_OK);
    CHECK(ui_node_get_parent(supplied, UI_VISUAL_TREE, &visual) == UI_OK && visual != logical && visual->kind == UI_CONTENT_PRESENTER);
    CHECK(ui_node_get_child(logical, UI_LOGICAL_TREE, 0, &child) == UI_OK && child == supplied);
    CHECK(ui_node_get_child(visual, UI_VISUAL_TREE, 0, &child) == UI_OK && child == supplied);
    uint32_t count = 1;
    CHECK(ui_node_get_child_count(visual, UI_LOGICAL_TREE, &count) == UI_OK && count == 0);
    auto *ca = component(app, "a");
    auto *cb = component(app, "b");
    auto *cr = component(app, "rating");
    CHECK(text(part(ca, "name")) == "Alice");
    CHECK(text(part(cb, "name")) == "Bob");
    ui_node *hidden = nullptr;
    CHECK(ui_app_find_fleck_node(app, "name", &hidden) != UI_OK);
    first = "Carol";
    CHECK(ui_model_notify_property_changed(a, "Name") == UI_OK);
    CHECK(text(part(ca, "name")) == "Carol");
    CHECK(text(part(cb, "name")) == "Bob");
    CHECK(ui_component_set_data_context(ca, b) == UI_OK);
    CHECK(text(supplied) == "Bob");
    CHECK(text(part(ca, "name")) == "Bob");
    ui_binding_value current{};
    CHECK(ui_component_get_property(cr, "Value", &current) == UI_OK);
    CHECK(current.integer == 42);
    CHECK(ui_component_apply_template(cr, "urn:test", "Compact", &diagnostic) ==
          UI_OK);
    CHECK(text(part(cr, "display")) == "42");
    current.integer = 68;
    CHECK(ui_component_set_property(cr, "Value", &current) == UI_OK);
    CHECK(text(part(cr, "display")) == "68");
    CHECK(ui_component_apply_template(cr, "urn:test", "Invalid", &diagnostic) !=
          UI_OK);
    CHECK(text(part(cr, "display")) == "68");
    int raised = 0;
    uint64_t token = 0;
    CHECK(ui_component_subscribe(
              cr, "Changed",
              [](ui_component *, const ui_binding_value *, void *context) {
                  ++*static_cast<int *>(context);
              },
              &raised, &token) == UI_OK);
    CHECK(ui_component_raise(cr, "Changed", &current) == UI_OK);
    CHECK(raised == 1);
    CHECK(ui_component_unsubscribe(cr, token) == UI_OK);
    CHECK(ui_component_raise(cr, "Changed", &current) == UI_OK);
    CHECK(raised == 1);
    CHECK(ui_model_destroy(b) == UI_OK);
    ui_component_subscription *scoped = nullptr;
    CHECK(ui_component_subscribe_scoped(
              cr, "Changed",
              [](ui_component *, const ui_binding_value *, void *) {}, nullptr,
              &scoped) == UI_OK);
    CHECK(ui_component_get_data_context(ca, &b) == UI_OK);
    CHECK(b == nullptr);
    // Compiled definitions and bundles retain component identity and bindings
    // without XML.
    fleck::Page binary_page;
    const char *binary_xml =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><c:Rating Id="binary" Value="39" Template="c:Compact"/></Window>)";
    CHECK(fleck::parse(binary_xml, binary_page, diagnostic));
    for (const auto *xml : {rating, compact}) {
        fleck::Page resource;
        CHECK(fleck::parse(xml, resource, diagnostic));
        binary_page.resources.push_back(std::move(resource));
    }
    binary_page.component_manifest =
        R"({"version":1,"types":[{"namespace":"urn:test","name":"Rating","template":"RatingTemplate","properties":[{"name":"Value","type":"int","mode":"TwoWay"}],"parts":[{"name":"PART_Value","kind":"Slider","required":1}]}]})";
    const auto bytes = fleck::compile_resource(binary_page);
    ui_app *other = nullptr;
    CHECK(ui_app_create(&options, &other) == UI_OK);
    CHECK(ui_app_load_fleck(other, page, strlen(page), &diagnostic) !=
          UI_OK); // Registries are App-local.
    for (size_t size = 0; size < bytes.size(); size += 13) {
        CHECK(ui_app_load_fleck_resource(other, bytes.data(), size,
                                         &diagnostic) != UI_OK);
    }
    CHECK(ui_app_load_fleck_resource(other, bytes.data(), bytes.size(),
                                     &diagnostic) == UI_OK);
    CHECK(text(part(component(other, "binary"), "display")) == "39");
    ui_component *created = nullptr;
    CHECK(ui_component_create(other, "urn:test", "Rating", nullptr, &created,
                              &diagnostic) == UI_OK);
    CHECK(ui_component_apply_template(created, "urn:test", "Compact",
                                      &diagnostic) == UI_OK);
    CHECK(text(part(created, "display")) == "0");
    ui_window* secondary = nullptr;
    CHECK(ui_app_create_window(other, &secondary) == UI_OK);
    ui_component* secondary_rating = nullptr;
    CHECK(ui_window_invoke(secondary, [](ui_app* target, void* result) -> ui_result {
        const char* page = R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><c:Rating Id="secondary" Value="73"/></Window>)";
        const auto loaded = ui_app_load_fleck(target, page, strlen(page), nullptr);
        if (loaded == UI_OK) *static_cast<ui_component**>(result) = component(target, "secondary");
        return loaded;
    }, &secondary_rating) == UI_OK);
    CHECK(ui_component_apply_template(secondary_rating, "urn:test", "Compact", &diagnostic) == UI_OK);
    auto* secondary_display = part(secondary_rating, "display");
    CHECK(text(secondary_display) == "73" && secondary_display->window_owner == secondary);
    CHECK(text(part(component(other, "binary"), "display")) == "39");
    for (const std::string kind : {"Button", "ToggleButton", "Button", "CheckBox", "RadioButton", "Switch", "Hyperlink"}) {
        const std::string schema = "<ControlTemplate xmlns=\"urn:sunfleck:v1\" Class=\"Example.Face" + kind +
            "\" Namespace=\"urn:test\" TargetType=\"" + kind +
            "\"><Border><ContentPresenter Id=\"label\"/></Border></ControlTemplate>";
        const std::string markup = "<Window xmlns=\"urn:sunfleck:v1\" xmlns:c=\"urn:test\"><" +
            kind + " Id=\"action\" Template=\"c:Face" + kind + "\"/></Window>";
        ui_fleck_resource_entry entry{"face.fleck", schema.data(), schema.size()};
        CHECK(ui_app_register_fleck_resources(other, &entry, 1, &diagnostic) == UI_OK);
        fleck::Page declaration;
        CHECK(fleck::parse(markup, declaration, diagnostic));
        CHECK(fleck::has_components(declaration));
        const auto compiled = fleck::compile_resource(declaration);
        for (bool binary : {false, true}) {
            const auto loaded = binary ? ui_app_load_fleck_resource(other, compiled.data(), compiled.size(), &diagnostic) :
                ui_app_load_fleck(other, markup.data(), markup.size(), &diagnostic);
            if (loaded != UI_OK) std::fprintf(stderr, "%s: %s\n", kind.c_str(), diagnostic.message);
            CHECK(loaded == UI_OK);
            ui_node* action = nullptr;
            CHECK(ui_app_find_fleck_node(other, "action", &action) == UI_OK);
            auto* owner = component(other, "action");
            CHECK(ui_node_set_text(action, "Start") == UI_OK);
            CHECK(part(owner, "label")->display_text() == L"Start");
            const auto applied = ui_component_apply_template(owner, "urn:test", ("Face" + kind).c_str(), &diagnostic);
            if (applied != UI_OK) std::fprintf(stderr, "%s (%s): %d %s\n", kind.c_str(), binary ? "binary" : "XML", applied, diagnostic.message);
            CHECK(applied == UI_OK);
            CHECK(ui_node_set_text(action, "Finish") == UI_OK);
            auto* label = part(owner, "label");
            CHECK(label->display_text() == L"Finish");
            CHECK(ui_component_apply_template(owner, "urn:test", "Compact", &diagnostic) != UI_OK);
            CHECK(part(owner, "label") == label && label->display_text() == L"Finish");
        }
    }
    ui_app_destroy(other);
    // Cycles are rejected before a visual tree is installed.
    const char *recursive =
        R"(<UserControl xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Class="Loop" Namespace="urn:test"><c:Loop/></UserControl>)";
    ui_fleck_resource_entry cycle{"loop.fleck", recursive, strlen(recursive)};
    CHECK(ui_app_register_fleck_resources(app, &cycle, 1, &diagnostic) ==
          UI_OK);
    const char *cycle_page =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><c:Loop/></Window>)";
    CHECK(ui_app_load_fleck(app, cycle_page, strlen(cycle_page), &diagnostic) !=
          UI_OK);
    CHECK(std::string(diagnostic.message).find("Recursive") !=
          std::string::npos);
    CHECK(std::string(diagnostic.message).find("loop.fleck") !=
          std::string::npos);

    // Namespace declarations are lexical, and default namespaces can name
    // components.
    const char *nested =
        R"(<UserControl xmlns="urn:sunfleck:v1" Class="Example.Pair" Namespace="urn:test">
    <StackPanel><UserCard xmlns="urn:test" Id="left"/><c:UserCard xmlns:c="urn:test" Id="right"/></StackPanel>
  </UserControl>)";
    ui_fleck_resource_entry pair{"pair.fleck", nested, strlen(nested)};
    CHECK(ui_app_register_fleck_resources(app, &pair, 1, &diagnostic) == UI_OK);
    const char *nested_page =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Height="640"><StackPanel>
    <c:Pair Id="outer"/><c:UserCard Id="sibling"/>
  </StackPanel></Window>)";
    CHECK(ui_app_load_fleck_with_model(app, nested_page, strlen(nested_page), a,
                                       nullptr, &diagnostic) == UI_OK);
    auto *outer = component(app, "outer");
    ui_component *left = nullptr, *right = nullptr;
    CHECK(ui_node_get_component(part(outer, "left"), &left) == UI_OK);
    CHECK(ui_node_get_component(part(outer, "right"), &right) == UI_OK);
    CHECK(left != right && part(left, "edit") != part(right, "edit"));
    CHECK(text(part(left, "name")) == "Carol");
    CHECK(part(left, "nameLabel")->anchor ==
          part(left, "edit"));
    CHECK(part(right, "nameLabel")->anchor ==
          part(right, "edit"));
    CHECK(ui_node_set_text(part(left, "edit"), "left draft") == UI_OK);
    CHECK(text(part(right, "edit")).empty());
    CHECK(ui_component_set_data_context(outer, nullptr) == UI_OK);
    first = "Detached";
    CHECK(ui_model_notify_property_changed(a, "Name") == UI_OK);
    CHECK(text(part(left, "name")) != "Detached");
    CHECK(text(part(component(app, "sibling"), "name")) == "Detached");
    CHECK(ui_component_set_data_context(outer, a) == UI_OK);
    CHECK(text(part(left, "name")) == "Detached" &&
          text(part(right, "name")) == "Detached");
    CHECK(text(part(left, "edit")) == "left draft");

    // Validation uses component metadata, without needing a business model.
    const char *bad_binding =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><c:Rating Value="{Binding Missing, RelativeSource=Self}"/></Window>)";
    CHECK(ui_app_validate_fleck(app, bad_binding, strlen(bad_binding),
                                &diagnostic) != UI_OK);
    CHECK(ui_app_validate_fleck(app, rating, strlen(rating), &diagnostic) ==
          UI_OK);
    const char *bad_resource =
        R"(<UserControl xmlns="urn:sunfleck:v1" xmlns:c="urn:test" Class="Example.Bad" Namespace="urn:test">
    <StackPanel>
      <c:Rating Typo="1"/>
    </StackPanel>
  </UserControl>)";
    ui_fleck_resource_entry bad{"bad.fleck", bad_resource,
                                strlen(bad_resource)};
    CHECK(ui_app_register_fleck_resources(app, &bad, 1, &diagnostic) == UI_OK);
    const char *bad_page =
        R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><c:Bad/></Window>)";
    CHECK(ui_app_validate_fleck(app, bad_page, strlen(bad_page), &diagnostic) !=
          UI_OK);
    CHECK(diagnostic.line == 3);
    CHECK(std::string(diagnostic.message).find("bad.fleck") !=
          std::string::npos);
    CHECK(text(part(left, "edit")) ==
          "left draft"); // Failed validation preserves the live page.
#ifdef _WIN32
    if (window) {
        CHECK(ui_app_open_window(app) == UI_OK);
        CHECK(ui_app_poll(app, 0) == UI_OK);
        HWND hwnd = nullptr;
        EnumThreadWindows(GetCurrentThreadId(), find_component_window, reinterpret_cast<LPARAM>(&hwnd));
        CHECK(hwnd != nullptr);
        auto click = [&](ui_node* node) {
            const auto bounds = node->bounds;
            CHECK(bounds.right > bounds.left && bounds.bottom > bounds.top);
            const auto point = MAKELPARAM((bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2);
            CHECK(PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point));
            CHECK(PostMessageW(hwnd, WM_LBUTTONUP, 0, point));
            CHECK(ui_app_poll(app, 0) == UI_OK);
        };
        auto* edit = part(right, "edit");
        unsigned previews = 0;
        CHECK(ui_node_set_pointer_handler(edit->parent, UI_POINTER_DOWN, UI_EVENT_PREVIEW,
            +[](ui_node*,ui_node*,ui_pointer_action,ui_event_phase,int32_t,int32_t,void* context) {
                ++*static_cast<unsigned*>(context);
                return 1;
            }, &previews) == UI_OK);
        click(edit);
        CHECK(previews == 1);
        CHECK(ui_node_set_pointer_handler(edit->parent, UI_POINTER_DOWN, UI_EVENT_PREVIEW, nullptr, nullptr) == UI_OK);
        click(edit);
        CHECK(PostMessageW(hwnd, WM_CHAR, 'R', 0));
        CHECK(ui_app_poll(app, 0) == UI_OK);
        CHECK(text(part(right, "edit")) == "R");
        CHECK(text(part(left, "edit")) == "left draft");
        // A label's local Anchor focuses only the editor in its own instance.
        click(part(left, "nameLabel"));
        CHECK(PostMessageW(hwnd, WM_CHAR, 'L', 0));
        CHECK(ui_app_poll(app, 0) == UI_OK);
        CHECK(text(part(left, "edit")).find('L') != std::string::npos);
        CHECK(text(part(right, "edit")) == "R");
        const char* drag_page = R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:test"><StackPanel IsFocusScope="true" Id="scope"><c:Rating Id="captured"/></StackPanel></Window>)";
        CHECK(ui_app_refresh_fleck(app, drag_page, strlen(drag_page), nullptr, &diagnostic) == UI_OK);
        CHECK(ui_app_poll(app, 0) == UI_OK);
        auto* captured = component(app, "captured");
        auto* slider = part(captured, "PART_Value");
        ui_node* scope = nullptr;
        CHECK(ui_app_find_fleck_node(app, "scope", &scope) == UI_OK);
        CHECK(ui_node_focus(slider) == UI_OK);
        unsigned shortcuts = 0;
        ui_command* refresh = nullptr;
        CHECK(ui_command_create(app, +[](ui_node*, void* p) { ++*static_cast<unsigned*>(p); }, &shortcuts, &refresh) == UI_OK);
        CHECK(ui_node_set_command_binding(scope, "Refresh", refresh) == UI_OK);
        CHECK(ui_node_set_input_binding(scope, UI_KEY_F5, 0, "Refresh") == UI_OK);
        SendMessageW(hwnd, WM_KEYDOWN, VK_F5, 0);
        CHECK(shortcuts == 1);
        CHECK(ui_node_set_input_binding(scope, UI_KEY_F5, 0, nullptr) == UI_OK);
        SendMessageW(hwnd, WM_KEYDOWN, VK_F5, 0);
        CHECK(shortcuts == 1);
        CHECK(ui_command_destroy(refresh) == UI_OK);
        const auto bounds = slider->bounds;
        const auto point = MAKELPARAM((bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2);
        unsigned cancelled = 0;
        CHECK(ui_node_set_pointer_handler(slider, UI_POINTER_CANCEL, UI_EVENT_DIRECT,
            +[](ui_node*, ui_node*, ui_pointer_action, ui_event_phase, int32_t, int32_t, void* context) {
                ++*static_cast<unsigned*>(context);
                return 1;
            }, &cancelled) == UI_OK);
        SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
        CHECK(slider->dragging && GetCapture() == hwnd);
        SendMessageW(hwnd, WM_CANCELMODE, 0, 0);
        CHECK(cancelled == 1 && !slider->dragging && GetCapture() != hwnd);
        SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
        CHECK(slider->dragging && GetCapture() == hwnd);
        CHECK(ui_component_apply_template(captured, "urn:test", "Invalid", &diagnostic) != UI_OK);
        CHECK(slider->dragging && GetCapture() == hwnd);
        ui_node* focus = nullptr;
        CHECK(ui_node_get_logical_focus(scope, &focus) == UI_OK && focus == slider);
        CHECK(ui_component_apply_template(captured, "urn:test", "Compact", &diagnostic) == UI_OK);
        CHECK(GetCapture() != hwnd);
        CHECK(ui_node_get_logical_focus(scope, &focus) == UI_OK && !focus);
        SendMessageW(hwnd, WM_LBUTTONUP, 0, point);
        CHECK(ui_app_request_close(app) == UI_OK);
    }
#endif
    ui_app_destroy(app);
    CHECK(ui_component_subscription_destroy(scoped) == UI_OK);
    return 0;
}
