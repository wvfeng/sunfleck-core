#include <sunfleck/ui.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/fleck.h"
#include "../src/ui_tree.h"
#include <iostream>
#include <thread>
#include <sstream>
#include <cstring>
#include <sunfleck/components.hpp>
#include <sunfleck/component_metadata.hpp>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static ui_binding_value number(int64_t n) { ui_binding_value v{}; v.kind = UI_VALUE_INT; v.integer = n; return v; }


static void inherited_property_tests() {
    struct ValidationState {
        int limit = 12;
        ui_component* ancestor = nullptr;
        ui_result reentry = UI_OK;
    } limit;
    sunfleck::App app(UI_SOFTWARE, true);
    auto metadata = sunfleck::componentPropertyMetadata(UI_PROPERTY_BINDABLE | UI_PROPERTY_TWO_WAY | UI_PROPERTY_ANIMATABLE | UI_PROPERTY_INHERITS,
        +[](ui_component* component, const ui_binding_value* value, void*) -> ui_result {
            void* state = nullptr;
            if (ui_component_get_state(component, &state) != UI_OK) return UI_ERROR_INVALID_STATE;
            if (!state) return UI_OK;
            auto& validation = *static_cast<ValidationState*>(state);
            if (auto* ancestor = std::exchange(validation.ancestor, nullptr)) {
                auto nested = number(8);
                validation.reentry = ui_component_set_property(ancestor, "Level", &nested);
            }
            return value->integer > validation.limit ? UI_ERROR_INVALID_ARGUMENT : UI_OK;
        });
    ui_component_property property{"Level", number(7), 0, UI_BIND_TWO_WAY, &metadata};
    ui_component_type type{sizeof(type), UI_COMPONENT_TYPE_VERSION, "urn:inherit", "Scope", "urn:inherit", "ScopeTemplate", &property, 1};
    require(ui_app_register_component_type(app.nativeHandle(), &type) == UI_OK, "Inheritable property metadata");
    const char* xml = R"(<ControlTemplate xmlns="urn:sunfleck:v1" xmlns:c="urn:inherit" Class="ScopeTemplate" Namespace="urn:inherit" TargetType="c:Scope"><StackPanel Id="children"><TextBlock Id="level" Text="{TemplateBinding Level}"/></StackPanel></ControlTemplate>)";
    ui_fleck_resource_entry resource{"inherit.fleck", xml, std::strlen(xml)};
    ui_fleck_diagnostic diagnostic{};
    require(ui_app_register_fleck_resources(app.nativeHandle(), &resource, 1, &diagnostic) == UI_OK, "Inheritance template");
    auto parent = sunfleck::FrameworkElement::create(app, "urn:inherit", "Scope");
    auto child = sunfleck::FrameworkElement::create(app, "urn:inherit", "Scope");
    auto sibling = sunfleck::FrameworkElement::create(app, "urn:inherit", "Scope");
    auto leaf = sunfleck::FrameworkElement::create(app, "urn:inherit", "Scope");
    parent.set("Level", 10);
    parent.findNode("children").append(sibling.node());
    parent.findNode("children").append(child.node());
    auto bridge = app.createNode(UI_STACK_PANEL);
    child.findNode("children").append(bridge); bridge.append(leaf.node());
    require(child.get<int>("Level") == 10 && leaf.get<int>("Level") == 10 && leaf.findNode("level").text() == "10",
        "Custom inheritance crosses ordinary template nodes and attaches subtrees");
    require(child.valueSource("Level") == UI_PROPERTY_SOURCE_INHERITED && child.baseValue<int>("Level") == 10,
        "Inherited custom value reports source and base");
    child.set("Level", 8); parent.set("Level", 11);
    require(child.get<int>("Level") == 8 && leaf.get<int>("Level") == 8 && sibling.get<int>("Level") == 11,
        "Local override controls its own descendants");
    child.clearLayer("Level");
    require(child.get<int>("Level") == 11 && leaf.get<int>("Level") == 11, "Clear local reveals latest inherited value");
    child.setLayer("Level", UI_PRECEDENCE_STYLE, 6); parent.set("Level", 10);
    require(child.get<int>("Level") == 6 && leaf.get<int>("Level") == 6, "Style overrides inherited value");
    child.clearLayer("Level", UI_PRECEDENCE_STYLE);
    require(child.get<int>("Level") == 10, "Style removal restores inheritance");
    require(ui_component_set_state(child.nativeHandle(), &limit, nullptr) == UI_OK, "Per-instance validator state");
    limit.ancestor = parent.nativeHandle();
    auto bad = number(20);
    require(ui_component_set_property(parent.nativeHandle(), "Level", &bad) == UI_ERROR_INVALID_ARGUMENT &&
        parent.get<int>("Level") == 10 && child.get<int>("Level") == 10 && sibling.get<int>("Level") == 10,
        "Descendant validation rejects parent update before any slot publishes");
    require(limit.reentry == UI_ERROR_INVALID_STATE, "Descendant validator cannot publish an ancestor write during preflight");
    child.setLayer("Level", UI_PRECEDENCE_ANIMATION, 5);
    parent.set("Level", 20);
    auto unattached = sunfleck::FrameworkElement::create(app, "urn:inherit", "Scope");
    require(ui_component_set_state(unattached.nativeHandle(), &limit, nullptr) == UI_OK, "Attachment validation state");
    require(ui_node_append(parent.findNode("children").nativeHandle(), unattached.node().nativeHandle()) == UI_ERROR_INVALID_ARGUMENT &&
        unattached.node().nativeHandle()->parent == nullptr && unattached.get<int>("Level") == 7,
        "Rejected inherited value leaves subtree unattached and unchanged");
    require(child.get<int>("Level") == 5 && leaf.get<int>("Level") == 5 && child.baseValue<int>("Level") == 20,
        "Inherited base tracks parent while animation masks it");
    require(ui_component_clear_property_layer(child.nativeHandle(), "Level", UI_PRECEDENCE_ANIMATION) == UI_ERROR_INVALID_ARGUMENT &&
        child.get<int>("Level") == 5, "Invalid revealed inheritance retains animation");
    parent.set("Level", 11); child.clearLayer("Level", UI_PRECEDENCE_ANIMATION);
    int value = 9;
    sunfleck::PropertyModel source(app);
    source.property("Level", [&] { return value; }, [&](int next) { value = next; });
    parent.node().bindProperty(parent.node().resolveProperty("Level"), source.nativeHandle(), "Level", UI_BIND_TWO_WAY);
    require(leaf.get<int>("Level") == 9, "Binding installation propagates inheritance");
    value = 10; source.notify("Level");
    require(leaf.get<int>("Level") == 10 && child.valueSource("Level") == UI_PROPERTY_SOURCE_INHERITED,
        "Binding notifications propagate inheritance without adding child bindings");
    child.set("Level", 4);
    require(value == 10 && leaf.get<int>("Level") == 4, "Inherited values do not write ancestor binding source");
    child.clearLayer("Level"); parent.clearLayer("Level");
    require(parent.get<int>("Level") == 7 && child.get<int>("Level") == 7 &&
        leaf.valueSource("Level") == UI_PROPERTY_SOURCE_DEFAULT, "Removing final ancestor value restores descendant defaults");
    parent.set("Level", 7);
    require(leaf.valueSource("Level") == UI_PROPERTY_SOURCE_INHERITED, "Explicit default is still inherited");
    require(ui_component_set_property_layer(child.nativeHandle(), "Level", UI_PRECEDENCE_INHERITED, &bad) == UI_ERROR_INVALID_ARGUMENT,
        "Automatic inherited layer cannot be forged by callers");
}

static void component_context_tests() {
    sunfleck::App app(UI_SOFTWARE, true);
    ui_binding_value emptyModel{}; emptyModel.kind = UI_VALUE_MODEL;
    ui_component_property props[] = {{"Amount", number(2), 0, UI_BIND_TWO_WAY, nullptr},
        {"ChildModel", emptyModel, 0, UI_BIND_ONE_WAY, nullptr}, {"AliasModel", emptyModel, 0, UI_BIND_ONE_WAY, nullptr}};
    ui_component_type type{sizeof(type), UI_COMPONENT_TYPE_VERSION, "urn:scope", "Scoped", "urn:scope", "ScopeTemplate", props, 3};
    require(ui_app_register_component_type(app.nativeHandle(), &type) == UI_OK, "Context type");
    const char* definition = R"(<ControlTemplate xmlns="urn:sunfleck:v1" xmlns:c="urn:scope" Class="ScopeTemplate" Namespace="urn:scope" TargetType="c:Scoped"><StackPanel Id="body"><TextBlock Id="caption" Text="{Binding Caption}"/><TextBlock Id="amount" Text="{TemplateBinding Amount}"/><StackPanel Id="branch" DataContext="{Binding Child}"><TextBlock Id="child" Text="{Binding Caption}"/></StackPanel><StackPanel DataContext="{TemplateBinding AliasModel}"><TextBlock Id="derived" Text="{Binding Caption}"/></StackPanel></StackPanel></ControlTemplate>)";
    ui_fleck_resource_entry resource{"scope.fleck", definition, std::strlen(definition)};
    ui_fleck_diagnostic diagnostic{};
    require(ui_app_register_fleck_resources(app.nativeHandle(), &resource, 1, &diagnostic) == UI_OK, "Context template");
    std::string first = "first", second = "second", child = "child";
    int firstAmount = 11, secondAmount = 22, secondReads = 0;
    sunfleck::PropertyModel childModel(app), otherChildModel(app), firstModel(app), secondModel(app), invalidModel(app), invalidChildModel(app);
    childModel.property("Caption", [&] { return child; });
    otherChildModel.property("Caption", [] { return std::string("other child"); });
    firstModel.property("Caption", [&] { return first; });
    firstModel.property("Amount", [&] { return firstAmount; }, [&](int value) { firstAmount = value; });
    firstModel.property("Child", [&] { return childModel.nativeHandle(); });
    secondModel.property("Caption", [&] { return second; });
    secondModel.property("Amount", [&] { ++secondReads; return secondAmount; }, [&](int value) { secondAmount = value; });
    secondModel.property("Child", [&] { return otherChildModel.nativeHandle(); });
    invalidModel.property("Caption", [] { return std::string("must not publish"); });
    invalidModel.property("Amount", [] { return 99; }, [](int) {});
    invalidModel.property("Child", [&] { return invalidChildModel.nativeHandle(); });
    const std::string page = R"(<Window xmlns="urn:sunfleck:v1" xmlns:c="urn:scope"><StackPanel Id="root"><c:Scoped Id="a" Amount="{Binding Amount}" AliasModel="{Binding ChildModel, RelativeSource=Self}" ChildModel="{Binding Child}"/><c:Scoped Id="b" DataContext="null" Amount="{Binding Amount}" AliasModel="{Binding AliasModel, RelativeSource=Self}"/></StackPanel></Window>)";
    require(ui_app_load_fleck_with_model(app.nativeHandle(), page.c_str(), page.size(), firstModel.nativeHandle(), nullptr, &diagnostic) == UI_OK, diagnostic.message);
    auto root = app.findFleckNode("root"), host = app.findFleckNode("a");
    sunfleck::FrameworkElement a(host), b(app.findFleckNode("b"));
    auto part = [](sunfleck::FrameworkElement& component, const char* name) { return component.findNode(name); };
    require(a.dataContext() == firstModel.nativeHandle() && host.dataContext() == firstModel.nativeHandle(), "Component and host share context");
    require(part(a, "caption").text() == "first" && part(a, "child").text() == "child" && part(a, "amount").text() == "11", "Template inherits ordinary and custom bindings");
    require(part(b, "caption").text().empty() && part(b, "child").text().empty() && b.get<int>("Amount") == 2, "Component null literal makes bindings dormant");
    root.setDataContext(secondModel.nativeHandle());
    require(secondReads == 1, "Context transaction reads custom source once before commit");
    require(part(a, "derived").text() == "other child", "Context preflight follows Self and TemplateBinding model dependencies");
    require(a.dataContext() == secondModel.nativeHandle() && part(a, "caption").text() == "second" && a.get<int>("Amount") == 22, "Parent context updates component and template");
    require(ui_node_set_data_context(root.nativeHandle(), invalidModel.nativeHandle()) == UI_ERROR_INVALID_ARGUMENT &&
        root.dataContext() == secondModel.nativeHandle() && part(a, "caption").text() == "second" && a.get<int>("Amount") == 22,
        "Invalid nested source rejects mixed component transaction before publication");
    a.set("Amount", 25);
    require(secondAmount == 25 && firstAmount == 11, "Reconnected component TwoWay writes new source");
    first = "stale"; firstAmount = 77; firstModel.notify("Caption"); firstModel.notify("Amount");
    require(part(a, "caption").text() == "second" && a.get<int>("Amount") == 25, "Old context subscriptions detached");
    a.setDataContext(nullptr);
    require(host.dataContext() == nullptr && part(a, "caption").text().empty() && a.get<int>("Amount") == 2, "Component setter clears inherited binding values");
    host.clearDataContext();
    require(a.dataContext() == secondModel.nativeHandle() && part(a, "caption").text() == "second" && a.get<int>("Amount") == 25, "Host clear restores component inheritance");
    auto context = host.resolveProperty("DataContext");
    ui_binding_value styled{}; styled.kind = UI_VALUE_MODEL; styled.model = firstModel.nativeHandle();
    host.setPropertyLayer(context, UI_PRECEDENCE_STYLE, styled);
    require(part(a, "caption").text() == "stale" && a.get<int>("Amount") == 77, "Component styled context uses unified slot");
    host.setDataContext(nullptr); host.clearDataContext();
    require(host.propertySource(context) == UI_PROPERTY_SOURCE_STYLE && part(a, "caption").text() == "stale", "Clear component local reveals style");
    host.clearPropertyLayer(context, UI_PRECEDENCE_STYLE);
    part(a, "body").setDataContext(nullptr);
    root.setDataContext(firstModel.nativeHandle());
    require(part(a, "caption").text().empty() && a.get<int>("Amount") == 77, "Ordinary local null inside template cuts inheritance");
    part(a, "body").clearDataContext();
    require(part(a, "caption").text() == "stale" && part(a, "child").text() == "child", "Clearing template boundary restores nested context");
    b.node().clearDataContext();
    require(part(b, "caption").text() == "stale" && b.get<int>("Amount") == 77, "Clear literal null activates dormant custom binding");
    require(ui_component_apply_template(a.nativeHandle(), "urn:scope", "ScopeTemplate", &diagnostic) == UI_OK && part(a, "caption").text() == "stale", "Template replacement keeps inherited context");
    app.openWindow(); app.poll(0);
    host.setDataContext(nullptr);
    app.refreshFleck(page, firstModel.nativeHandle()); app.poll(0);
    a = sunfleck::FrameworkElement(app.findFleckNode("a"));
    require(a.dataContext() == nullptr && part(a, "caption").text().empty(), "Refresh preserves component explicit null override");
    a.clearDataContext();
    auto released = std::make_unique<sunfleck::PropertyModel>(app);
    released->property("Caption", [] { return std::string("temporary"); });
    released->property("Amount", [] { return 31; }, [](int) {});
    released->property("Child", [&] { return childModel.nativeHandle(); });
    root = app.findFleckNode("root");
    root.setDataContext(released->nativeHandle());
    require(part(a, "caption").text() == "temporary" && a.get<int>("Amount") == 31, "Temporary inherited model attached");
    released.reset();
    require(a.dataContext() == nullptr && part(a, "caption").text().empty() && part(a, "child").text().empty() &&
        a.get<int>("Amount") == 2, "Model destruction clears component and nested template context bindings");
}

static void timeline_tests() {
    sunfleck::App app(UI_SOFTWARE,true);
    auto node = app.createNode(UI_BORDER);
    app.setRoot(node);
    app.openWindow();
    ui_binding_value low{}, high{};
    low.kind = high.kind = UI_VALUE_DOUBLE;
    high.number = 1;
    ui_key_frame frames[] = {{0,low,UI_EASE_LINEAR},{1,high,UI_EASE_LINEAR}};
    ui_timeline track{frames,2,1000,100,2,1,1};
    app.advanceAnimationClock(0);
    node.startAnimation(UI_PROPERTY_OPACITY,track);
    app.advanceAnimationClock(350);
    app.poll(0);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == .25,"Poll samples the manual monotonic clock");
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_PAUSE);
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_SEEK,600);
    app.poll(0);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == .5,"Paused timeline samples the midpoint after its delay");
    high.number = .8;
    node.setPropertyValue(UI_PROPERTY_OPACITY,high);
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_SEEK,2850);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == .75,"Second cycle samples forward");
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_SEEK,3850);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == .25,"Second cycle reverses");
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_SEEK,4100);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == 0,"AutoReverse holds the starting value at completion");
    frames[1].value.number = 2;
    require(ui_node_start_animation(node.nativeHandle(),UI_PROPERTY_OPACITY,&track) == UI_ERROR_INVALID_ARGUMENT,
            "Invalid replacement preserves the previous animation");
    node.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_STOP);
    require(node.propertyValue(UI_PROPERTY_OPACITY).number == .8,"Stop restores the current base value");
    int completions = 0;
    app.setAnimationCompleted([](ui_node*,ui_property,void* context){ ++*static_cast<int*>(context); },&completions);
    app.setReducedMotion(true);
    frames[1].value.number = 1;
    node.startAnimation(UI_PROPERTY_OPACITY,track);
    app.poll(0); app.poll(0);
    require(completions == 1 && node.propertyValue(UI_PROPERTY_OPACITY).number == 0,
            "Reduced motion applies the terminal frame and delivers completion once");

    const char* xml = "<Window xmlns='urn:sunfleck:v1'><Border Id='target' Background='#010203'>"
        "<Border.Storyboard><Storyboard>"
        "<Timeline Property='Background' DurationMs='1000'><KeyFrame Position='0' Value='#000000'/>"
        "<KeyFrame Position='1' Value='#FFFFFF'/></Timeline>"
        "<Timeline Property='Margin' DurationMs='1000'><KeyFrame Position='0' Value='0'/>"
        "<KeyFrame Position='1' Value='4,8,12,16' Easing='EaseIn'/></Timeline>"
        "</Storyboard></Border.Storyboard></Border></Window>";
    fleck::Page parsed;
    ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml,parsed,diagnostic),"Parse declarative Storyboard");
    const auto bytes = fleck::compile_resource(parsed);
    for (bool compiled : {false,true}) {
        sunfleck::App page(UI_SOFTWARE,true);
        if (compiled) require(ui_app_load_fleck_resource(page.nativeHandle(),bytes.data(),bytes.size(),&diagnostic)==UI_OK,
                              "Load compiled Storyboard");
        else page.loadFleck(xml);
        page.openWindow();
        auto target = page.findFleckNode("target");
        target.beginStoryboard();
        target.controlAnimation(UI_PROPERTY_BACKGROUND,UI_ANIMATION_SEEK,500);
        target.controlAnimation(UI_PROPERTY_MARGIN,UI_ANIMATION_SEEK,500);
        require(uint32_t(target.propertyValue(UI_PROPERTY_BACKGROUND).integer) == 0xff808080,"Storyboard interpolates ARGB");
        require(target.propertyValue(UI_PROPERTY_MARGIN).geometry[3] == 4,"Storyboard eases Thickness");
        page.refreshFleck(xml);
        page.poll(0);
        target = page.findFleckNode("target");
        require(uint32_t(target.propertyValue(UI_PROPERTY_BACKGROUND).integer) == 0xff010203,
                "Refreshing a reused target retires its animation");
    }
}

static void dictionary_source_tests() {
    const std::string colors = "<ResourceDictionary xmlns='urn:sunfleck:v1'><Resource Key='Fade' Value='0.6'/></ResourceDictionary>";
    const std::string theme = "<ResourceDictionary xmlns='urn:sunfleck:v1'><ResourceDictionary.MergedDictionaries>"
        "<ResourceDictionary Source='colors.fleck'/></ResourceDictionary.MergedDictionaries>"
        "<Style TargetType='Border'><Setter Property='Opacity' Value='{StaticResource Fade}'/></Style></ResourceDictionary>";
    const std::string page = "<Window xmlns='urn:sunfleck:v1'><Window.Resources><ResourceDictionary Source='themes/base.fleck'/>"
        "</Window.Resources><Border Id='box'/></Window>";
    sunfleck::App app(UI_SOFTWARE,true);
    ui_fleck_diagnostic diagnostic{};
    ui_fleck_resource_entry entries[] = {{"themes/base.fleck",theme.data(),theme.size()},{"themes/colors.fleck",colors.data(),colors.size()}};
    require(ui_app_load_fleck_with_resources(app.nativeHandle(),page.data(),page.size(),entries,2,nullptr,&diagnostic)==UI_OK,
            "Load nested dictionary independent of registration order");
    auto box = app.findFleckNode("box");
    require(box.propertyValue(UI_PROPERTY_OPACITY).number==0.6,"Imported implicit style resolves imported key");
    app.openWindow(); app.poll(0);
    const std::string cycle = "<ResourceDictionary xmlns='urn:sunfleck:v1'><ResourceDictionary.MergedDictionaries>"
        "<ResourceDictionary Source='base.fleck'/></ResourceDictionary.MergedDictionaries></ResourceDictionary>";
    entries[1].utf8 = cycle.data(); entries[1].length = cycle.size();
    require(ui_app_register_fleck_resources(app.nativeHandle(),entries,2,&diagnostic)!=UI_OK &&
            std::string(diagnostic.message).find("cycle")!=std::string::npos,"Cross-file cycle reports a diagnostic");
    app.refreshFleck(page);
    require(app.findFleckNode("box").propertyValue(UI_PROPERTY_OPACITY).number==0.6,"Rejected dictionary batch retains previous resources");
}

int main() {
    try {
        dictionary_source_tests();
        {
            sunfleck::App app(UI_SOFTWARE,true);
            auto parent = app.createNode(UI_STACK_PANEL);
            auto branch = app.createNode(UI_STACK_PANEL);
            auto child = app.createNode(UI_BORDER);
            child.setResourceReference(UI_PROPERTY_OPACITY,"Fade");
            branch.append(child);
            ui_binding_value fade{};
            fade.kind = UI_VALUE_DOUBLE; fade.number = 2;
            parent.setResource("Fade",fade);
            require(ui_node_append(parent.nativeHandle(),branch.nativeHandle()) == UI_ERROR_INVALID_ARGUMENT &&
                    !branch.nativeHandle()->parent, "Incompatible ancestor resource rolls back subtree attachment");
            fade.number = .6; parent.setResource("Fade",fade);
            parent.append(branch);
            require(child.propertyValue(UI_PROPERTY_OPACITY).number == .6, "Attached subtree resolves inherited resource");
        }
        {
            sunfleck::App app(UI_SOFTWARE,true);
            sunfleck::PropertyModel model(app);
            bool busy = true;
            model.property("Busy",[&] { return busy; });
            const char* xml = "<Window xmlns='urn:sunfleck:v1'><Window.Resources><Style TargetType='Button'>"
                "<DataTrigger Binding='{Binding Busy}' Value='true'><Setter Property='Opacity' Value='0.4'/></DataTrigger>"
                "</Style></Window.Resources><StackPanel><StackPanel Id='scope'><Button Id='save'/></StackPanel></StackPanel></Window>";
            fleck::Page page;
            ui_fleck_diagnostic diagnostic{};
            require(fleck::parse(xml,page,diagnostic),"Parse DataTrigger");
            auto bytes = fleck::compile_resource(page);
            require(ui_app_load_fleck_resource_with_model(app.nativeHandle(),bytes.data(),bytes.size(),model.nativeHandle(),nullptr,&diagnostic) == UI_OK,
                    diagnostic.message);
            auto save = app.findFleckNode("save");
            require(save.propertyValue(UI_PROPERTY_OPACITY).number == .4, "Compiled DataTrigger reads initial DataContext");
            busy = false; model.notify("Busy");
            require(save.propertyValue(UI_PROPERTY_OPACITY).number == 1, "Model change leaves DataTrigger");
            app.findFleckNode("scope").setDataContext(nullptr);
            busy = true; model.notify("Busy");
            require(save.propertyValue(UI_PROPERTY_OPACITY).number == 1, "Null context cuts off DataTrigger");
            app.findFleckNode("scope").clearDataContext();
            require(save.propertyValue(UI_PROPERTY_OPACITY).number == .4, "Cleared context reconnects DataTrigger");
        }
        timeline_tests();
        inherited_property_tests();
        component_context_tests();
        sunfleck::App app(UI_SOFTWARE, true);
        {
            const std::string xml = R"(<Window xmlns='urn:sunfleck:v1'>
                <Window.Resources>
                  <Resource Key='size' Value='24'/><Resource Key='Tone' Value='0.35'/>
                  <Style TargetType='ListView'><Setter Property='Value' Value='1'/>
                    <MultiTrigger><MultiTrigger.Conditions><Condition Property='IsEnabled' Value='false'/><Condition Property='Value' Value='1'/><Condition Property='Visibility' Value='Visible'/></MultiTrigger.Conditions>
                      <Setter Property='Opacity' Value='0.2'/>
                    </MultiTrigger>
                  </Style>
                  <Style TargetType='TextBlock'>
                    <Setter Property='FontSize' Value='{StaticResource size}'/>
                    <Setter Property='Opacity' Value='0.5'/>
                  </Style>
                  <Style Key='base' TargetType='Button'><Setter Property='Opacity' Value='{DynamicResource Fade}'/><Setter Property='Height' Value='32'/></Style>
                  <Style Key='wide' TargetType='Button' BasedOn='{StaticResource base}'>
                    <Setter Property='Width' Value='120'/>
                    <Style.Triggers><Trigger Property='IsEnabled' Value='false'><Setter Property='Opacity' Value='0.15'/></Trigger></Style.Triggers>
                  </Style>
                </Window.Resources>
                <StackPanel Id='scope'>
                  <StackPanel Width='{StaticResource size}' Id='inner'>
                    <StackPanel.Resources><ResourceDictionary>
                      <ResourceDictionary.MergedDictionaries>
                        <ResourceDictionary><Resource Key='size' Value='10'/></ResourceDictionary>
                        <ResourceDictionary><Resource Key='size' Value='16'/></ResourceDictionary>
                      </ResourceDictionary.MergedDictionaries>
                    </ResourceDictionary></StackPanel.Resources>
                    <TextBlock Id='descendant' Width='{StaticResource size}' Opacity='{DynamicResource Tone}'/>
                  </StackPanel>
                  <TextBlock Id='implicit' Width='{StaticResource size}'/>
                  <ListView Id='choices'><ListItem/><ListItem/></ListView>
                  <Button Id='explicit' Style='{StaticResource wide}' Width='140'/>
                </StackPanel>
              </Window>)";
            fleck::Page parsed;
            ui_fleck_diagnostic diagnostic{};
            require(fleck::parse(xml, parsed, diagnostic), "Parse styled page");
            const auto bytes = fleck::compile_resource(parsed);
            for (bool compiled : {false, true}) {
                sunfleck::App styled(UI_SOFTWARE, true);
                if (compiled) require(ui_app_load_fleck_resource(styled.nativeHandle(),bytes.data(),bytes.size(),&diagnostic) == UI_OK,
                                      "Load compiled style declarations");
                else styled.loadFleck(xml);
                styled.openWindow(); styled.poll(0);
                auto implicit = styled.findFleckNode("implicit");
                require(styled.findFleckNode("choices").value() == 1, "Style selects an indexed item after staging children");
                auto explicitStyle = styled.findFleckNode("explicit");
                require(implicit.width() == 24 && styled.findFleckNode("inner").width() == 16 &&
                        styled.findFleckNode("descendant").width() == 16, "Static resources respect owner, ancestor and sibling scopes");
                require(implicit.propertyValue(UI_PROPERTY_FONT_SIZE).number == 24 &&
                    implicit.propertySource(UI_PROPERTY_FONT_SIZE) == UI_PROPERTY_SOURCE_STYLE &&
                    implicit.propertyValue(UI_PROPERTY_OPACITY).number == .5,
                    "Fleck implicit Style setters apply through the style layer");
                require(explicitStyle.width() == 140 &&
                    explicitStyle.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL,
                    "Explicit local values override a Fleck Style setter");
                ui_binding_value fade{};
                fade.kind = UI_VALUE_DOUBLE;
                fade.number = .25;
                auto direct = styled.findFleckNode("descendant");
                auto inner = styled.findFleckNode("inner");
                require(direct.propertyValue(UI_PROPERTY_OPACITY).number == .35, "Direct dynamic resource reads Fleck declaration");
                inner.setResource("Tone",fade);
                require(direct.propertyValue(UI_PROPERTY_OPACITY).number == .25, "Direct resource follows nearer override");
                inner.clearResource("Tone");
                require(direct.propertyValue(UI_PROPERTY_OPACITY).number == .35, "Direct reference returns to declaration");
                direct.clearProperty(UI_PROPERTY_OPACITY);
                inner.setResource("Tone",fade);
                require(direct.propertyValue(UI_PROPERTY_OPACITY).number == .5, "Clearing a local reference restores implicit style and detaches resource");
                styled.setThemeResource("Fade",fade);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .25, "DynamicResource picks up theme after load");
                fade.number = .75;
                styled.setResource("Fade",fade);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .75, "Application resource overrides theme");
                auto ancestor = styled.findFleckNode("scope");
                ancestor.setEnabled(false);
                auto choices = styled.findFleckNode("choices");
                require(choices.propertyValue(UI_PROPERTY_OPACITY).number == .2, "MultiTrigger applies when both conditions match");
                ui_binding_value hidden{};
                hidden.kind = UI_VALUE_ENUM;
                hidden.integer = UI_HIDDEN;
                require(ui_node_set_property(choices.nativeHandle(), UI_PROPERTY_VISIBILITY, &hidden)==UI_OK &&
                    choices.propertyValue(UI_PROPERTY_OPACITY).number==1,"Visibility change clears the multi-trigger layer");
                choices.clearProperty(UI_PROPERTY_VISIBILITY);
                require(choices.propertyValue(UI_PROPERTY_OPACITY).number==.2,"Clearing visibility reactivates the multi-trigger");
                choices.setValue(0);
                require(choices.propertyValue(UI_PROPERTY_OPACITY).number == 1, "MultiTrigger clears when one condition changes");
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .15 &&
                        explicitStyle.propertySource(UI_PROPERTY_OPACITY) == UI_PROPERTY_SOURCE_TRIGGER, "Ancestor IsEnabled drives Trigger layer");
                fade.number = .65;
                styled.setResource("Fade",fade);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .15, "Trigger masks dynamic style update");
                ancestor.setEnabled(true);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .65, "Leaving trigger restores latest dynamic style");
                fade.number = .75;
                styled.setResource("Fade",fade);
                auto scope = styled.findFleckNode("scope");
                fade.number = .4;
                scope.setResource("Fade",fade);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .4, "Ancestor dictionary overrides application");
                fade.number = .6;
                explicitStyle.setResource("Fade",fade);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .6, "Nearest resource wins");
                explicitStyle.clearResource("Fade");
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .4, "Clear restores ancestor resource");
                scope.clearResource("Fade");
                fade.number = 3;
                require(ui_app_set_resource(styled.nativeHandle(),"Fade",&fade) == UI_ERROR_INVALID_ARGUMENT &&
                        explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .75, "Invalid dynamic update preserves dictionary and targets");
                fade.number = .5;
                explicitStyle.setPropertyValue(UI_PROPERTY_OPACITY,fade);
                styled.clearResource("Fade");
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .5, "Local value masks resource update");
                explicitStyle.clearProperty(UI_PROPERTY_OPACITY);
                require(explicitStyle.propertyValue(UI_PROPERTY_OPACITY).number == .25, "Clear local exposes current resource");
                styled.clearThemeResource("Fade");
                require(explicitStyle.propertySource(UI_PROPERTY_OPACITY) == UI_PROPERTY_SOURCE_DEFAULT, "Removing last resource clears style layer");
                explicitStyle.clearProperty(UI_PROPERTY_WIDTH);
                require(explicitStyle.width() == 120 && explicitStyle.height() == 32 &&
                        explicitStyle.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_STYLE, "Clear reveals derived style");
                const std::string cycle = "<Window xmlns='urn:sunfleck:v1'><Window.Resources><Style Key='a' BasedOn='a' TargetType='Button'/></Window.Resources><Button/></Window>";
                require(ui_app_refresh_fleck(styled.nativeHandle(), cycle.data(), cycle.size(), nullptr, &diagnostic) != UI_OK &&
                        styled.findFleckNode("explicit").width() == 120, "Invalid style refresh preserves installed page");
            }
        }
        {
            const auto unversioned = +[](ui_fleck_builder* builder) -> ui_result {
                const ui_fleck_node_spec node{UI_TEXT_BLOCK, "old generated page"};
                return ui_fleck_add_node(builder, UI_FLECK_NO_PARENT, &node);
            };
            ui_fleck_diagnostic diagnostic{};
            require(ui_app_build_fleck(app.nativeHandle(), "old", 100, 100, unversioned, &diagnostic) == UI_ERROR_VERSION_MISMATCH,
                "Unversioned compiled builder rejected before reading legacy node payload");
            const auto versioned = +[](ui_fleck_builder* builder) -> ui_result {
                if (auto status = ui_fleck_require_version(builder, UI_API_VERSION); status != UI_OK) return status;
                const ui_fleck_node_spec node{UI_TEXT_BLOCK, "versioned builder", "manual"};
                return ui_fleck_add_node(builder, UI_FLECK_NO_PARENT, &node);
            };
            require(ui_app_build_fleck(app.nativeHandle(), "new", 100, 100, versioned, &diagnostic) == UI_OK &&
                app.findFleckNode("manual").text() == "versioned builder", "Versioned manual builder remains usable");
            const auto metrics = +[](ui_fleck_builder* builder) -> ui_result {
                if (auto status=ui_fleck_require_version(builder,UI_API_VERSION); status!=UI_OK) return status;
                ui_fleck_node_spec node{UI_STACK_PANEL, "", "metrics"};
                node.spacing=.125; node.padding=1.75;
                return ui_fleck_add_node(builder,UI_FLECK_NO_PARENT,&node);
            };
            require(ui_app_build_fleck(app.nativeHandle(),"metrics",100,100,metrics,&diagnostic)==UI_OK &&
                app.findFleckNode("metrics").propertyValue(UI_PROPERTY_SPACING).number==.125 &&
                app.findFleckNode("metrics").propertyValue(UI_PROPERTY_PADDING).number==1.75,"Builder preserves fractional metrics");
            const auto invalidMetrics = +[](ui_fleck_builder* builder) -> ui_result {
                if (auto status=ui_fleck_require_version(builder,UI_API_VERSION); status!=UI_OK) return status;
                ui_fleck_node_spec node{UI_STACK_PANEL}; node.padding=std::numeric_limits<double>::quiet_NaN();
                return ui_fleck_add_node(builder,UI_FLECK_NO_PARENT,&node);
            };
            require(ui_app_build_fleck(app.nativeHandle(),"invalid",100,100,invalidMetrics,&diagnostic)==UI_ERROR_INVALID_ARGUMENT &&
                app.findFleckNode("metrics").propertyValue(UI_PROPERTY_PADDING).number==1.75,"Invalid builder preserves installed tree");
            const auto oldMetrics = +[](ui_fleck_builder* builder) -> ui_result { return ui_fleck_require_version(builder,3); };
            require(ui_app_build_fleck(app.nativeHandle(),"old",100,100,oldMetrics,&diagnostic)==UI_ERROR_VERSION_MISMATCH,"Integer metric ABI builder rejected");
        }
        {
            sunfleck::App idle(UI_SOFTWARE, true);
            idle.loadFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='idle' Width='80'/></Window>");
            idle.openWindow(); idle.poll(0);
            auto stable = idle.findFleckNode("idle");
            stable.setWidth(80);
            stable.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE, number(40));
            require(ui_app_poll(idle.nativeHandle(), 0) == UI_TIMEOUT, "Equal and hidden values do not redraw");
        }
        auto node = app.createNode(UI_TEXT_BLOCK);
        const auto registeredWidth = node.resolveProperty("Width");
        require(node.describeProperty(registeredWidth).kind == UI_VALUE_LENGTH &&
            node.describeProperty(registeredWidth).id == registeredWidth.id, "Registered built-in descriptor");
        node.setPropertyValue(registeredWidth, number(72));
        require(node.width() == 72 && node.propertySource(registeredWidth) == UI_PROPERTY_SOURCE_LOCAL, "Registered and legacy setters share slots");
        node.clearProperty(registeredWidth);
        ui_property_id untouched = 1234;
        require(ui_node_resolve_property(node.nativeHandle(), "Missing", &untouched) == UI_ERROR_INVALID_ARGUMENT && untouched == 1234,
            "Failed resolution leaves caller output intact");
        ui_result foreignThread = UI_OK;
        std::thread worker([&] { foreignThread = ui_node_resolve_property(node.nativeHandle(), "Width", &untouched); }); worker.join();
        require(foreignThread == UI_ERROR_WRONG_THREAD, "Registered identities enforce UI thread");
        require(node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_DEFAULT, "Initial source");
        auto width = number(80);
        node.setPropertyValue(UI_PROPERTY_WIDTH, width);
        require(node.width() == 80 && node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL, "Generic setter and old getter");
        node.setWidth(90);
        require(node.propertyValue(UI_PROPERTY_WIDTH).number == 90, "Old setter and generic getter");
        auto bad = number(INT64_MAX);
        require(ui_node_set_property(node.nativeHandle(), UI_PROPERTY_WIDTH, &bad) == UI_ERROR_INVALID_ARGUMENT, "Reject overflow");
        require(node.width() == 90, "Rejected update preserves value");
        node.clearProperty(UI_PROPERTY_WIDTH);
        require(node.width() == -1 && node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_DEFAULT, "Clear restores default/source");
        node.setWidth(-1);
        require(node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL, "Explicit default is local");

        int extent = 120;
        {
            auto root = app.createNode(UI_STACK_PANEL);
            auto branch = app.createNode(UI_STACK_PANEL);
            auto leaf = app.createNode(UI_TEXT_BLOCK);
            root.append(branch); branch.append(leaf);
            leaf.bindContext(UI_PROPERTY_TEXT, "Caption", UI_BIND_ONE_WAY);
            require(leaf.dataContext() == nullptr && leaf.dataContextSource() == UI_PROPERTY_SOURCE_DEFAULT, "Empty context binding is dormant");
            std::string first = "first", second = "second";
            sunfleck::PropertyModel firstModel(app), secondModel(app), invalidModel(app);
            firstModel.property("Caption", [&] { return first; });
            secondModel.property("Caption", [&] { return second; });
            root.setDataContext(firstModel.nativeHandle());
            require(leaf.text() == "first" && leaf.dataContextSource() == UI_PROPERTY_SOURCE_INHERITED, "Parent context activates binding");
            branch.setDataContext(nullptr);
            require(leaf.text().empty() && leaf.dataContext() == nullptr && branch.dataContextSource() == UI_PROPERTY_SOURCE_LOCAL, "Explicit null cuts inheritance");
            first = "new first"; firstModel.notify("Caption");
            require(leaf.text().empty(), "Null context unsubscribes old source");
            branch.clearDataContext();
            require(leaf.text() == "new first", "Clear resumes latest inherited model");
            root.setDataContext(secondModel.nativeHandle());
            require(root.propertyValue(UI_PROPERTY_DATA_CONTEXT).model == secondModel.nativeHandle() &&
                leaf.propertySource(UI_PROPERTY_DATA_CONTEXT) == UI_PROPERTY_SOURCE_INHERITED, "DataContext shares generic value and source APIs");
            ui_binding_value styleContext{}; styleContext.kind = UI_VALUE_MODEL; styleContext.model = firstModel.nativeHandle();
            branch.setPropertyLayer(UI_PROPERTY_DATA_CONTEXT, UI_PRECEDENCE_STYLE, styleContext);
            require(leaf.text() == "new first" && branch.dataContextSource() == UI_PROPERTY_SOURCE_STYLE,
                "Styled context propagates through the same inheritance path");
            branch.setDataContext(nullptr);
            require(leaf.text().empty(), "Local null overrides styled context");
            branch.clearDataContext();
            require(leaf.text() == "new first" && branch.dataContextSource() == UI_PROPERTY_SOURCE_STYLE,
                "Clear local context reveals style context");
            branch.clearPropertyLayer(UI_PROPERTY_DATA_CONTEXT, UI_PRECEDENCE_STYLE);
            require(leaf.text() == "second", "Clear last context layer restores parent inheritance");
            first = "stale"; firstModel.notify("Caption");
            require(leaf.text() == "second", "Switch detaches previous source");
            require(ui_node_set_data_context(root.nativeHandle(), invalidModel.nativeHandle()) == UI_ERROR_INVALID_ARGUMENT, "Missing path rejects context transaction");
            require(root.dataContext() == secondModel.nativeHandle() && leaf.text() == "second", "Rejected context preserves source and value");
            auto widthLeaf = app.createNode(UI_TEXT_BLOCK);
            branch.append(widthLeaf);
            int secondWidth = 100;
            secondModel.property("Extent", [&] { return secondWidth; });
            widthLeaf.bindContext(UI_PROPERTY_WIDTH, "Extent", UI_BIND_ONE_WAY);
            invalidModel.property("Caption", [] { return std::string("must not publish"); });
            invalidModel.property("Extent", [] { return -8; });
            require(ui_node_set_data_context(root.nativeHandle(), invalidModel.nativeHandle()) == UI_ERROR_INVALID_ARGUMENT &&
                leaf.text() == "second" && widthLeaf.width() == 100, "Validate whole subtree before committing any value");
            second = "still attached"; secondModel.notify("Caption");
            require(leaf.text() == second, "Rejected transaction preserves subscriptions");
            auto late = app.createNode(UI_TEXT_BLOCK);
            late.bindContext(UI_PROPERTY_TEXT, "Caption", UI_BIND_ONE_WAY);
            root.append(late);
            require(late.text() == second, "Append activates inherited context");
            auto badChild = app.createNode(UI_TEXT_BLOCK);
            badChild.bindContext(UI_PROPERTY_TEXT, "Missing", UI_BIND_ONE_WAY);
            require(ui_node_append(root.nativeHandle(), badChild.nativeHandle()) == UI_ERROR_INVALID_ARGUMENT && !badChild.nativeHandle()->parent,
                "Invalid append rolls back tree relationship");
            leaf.setText("local");
            second = "not local"; secondModel.notify("Caption");
            require(leaf.text() == "local", "Local setter removes context binding");
            auto* raw = secondModel.nativeHandle();
            require(widthLeaf.dataContext() == raw, "Context handle remains borrowed");
        }
        sunfleck::PropertyModel model(app);
        {
            sunfleck::App closing(UI_SOFTWARE, true);
            auto button = closing.createNode(UI_BUTTON);
            closing.setRoot(button);
            ui_command *fallback = nullptr, *bound = nullptr;
            const auto action = +[](ui_node*, void*) {};
            require(ui_command_create(closing.nativeHandle(), action, nullptr, &fallback) == UI_OK &&
                ui_command_create(closing.nativeHandle(), action, nullptr, &bound) == UI_OK, "Create closing context commands");
            ui_binding_value command{}; command.kind = UI_VALUE_COMMAND; command.command = fallback;
            button.setPropertyLayer(UI_PROPERTY_COMMAND, UI_PRECEDENCE_STYLE, command);
            ui_model* disposable = nullptr;
            require(ui_model_create(closing.nativeHandle(), &disposable) == UI_OK && ui_model_add_command(disposable, "Run", bound) == UI_OK,
                "Create closing context source");
            button.setDataContext(disposable);
            button.bindContext(UI_PROPERTY_COMMAND, "Run");
            closing.openWindow(); closing.poll(0); closing.requestClose();
            require(ui_app_poll(closing.nativeHandle(), 0) == UI_EXITED, "Close before disposing model");
            require(ui_model_destroy(disposable) == UI_OK && button.dataContext() == nullptr &&
                button.propertyValue(UI_PROPERTY_COMMAND).command == fallback, "Closed window context disposal restores command fallback without rebinding work");
            require(ui_node_bind_command(button.nativeHandle(), bound) == UI_ERROR_INVALID_STATE,
                "Closing still rejects new application command attachment");
        }
        {
            auto parent = app.createNode(UI_STACK_PANEL);
            auto boundary = app.createNode(UI_STACK_PANEL);
            auto child = app.createNode(UI_TEXT_BLOCK);
            auto once = app.createNode(UI_TEXT_BLOCK);
            parent.append(boundary); boundary.append(child); boundary.append(once);
            sunfleck::PropertyModel ancestor(app);
            ancestor.property("Caption", [] { return std::string("ancestor"); });
            parent.setDataContext(ancestor.nativeHandle());
            child.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE, number(55));
            {
                sunfleck::PropertyModel disposable(app);
                disposable.property("Caption", [] { return std::string("temporary"); });
                disposable.property("Extent", [] { return 80; });
                boundary.setDataContext(disposable.nativeHandle());
                child.bindContext(UI_PROPERTY_TEXT, "Caption", UI_BIND_ONE_WAY);
                child.bindContext(UI_PROPERTY_WIDTH, "Extent", UI_BIND_ONE_WAY);
                once.bindContext(UI_PROPERTY_TEXT, "Caption", UI_BIND_ONE_TIME);
                child.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION, number(120));
                require(child.text() == "temporary" && once.text() == "temporary", "Temporary context resolves both binding modes");
            }
            require(child.text().empty() && once.text().empty() && child.dataContext() == nullptr &&
                boundary.dataContextSource() == UI_PROPERTY_SOURCE_LOCAL, "Model destruction clears inherited values and retains null boundary");
            require(child.width() == 120 && child.basePropertyValue(UI_PROPERTY_WIDTH).number == 55,
                "Context destruction clears binding base without removing animation or style");
            child.clearPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION);
            require(child.width() == 55, "Removing overlay reveals fallback after context destruction");
            ancestor.property("Extent", [] { return 90; });
            boundary.clearDataContext();
            require(child.text() == "ancestor" && once.text() == "ancestor" && child.width() == 90,
                "Dormant declarations reactivate after destroyed context is cleared");
        }
        {
            sunfleck::App contextApp(UI_SOFTWARE, true);
            sunfleck::PropertyModel source(contextApp), replacement(contextApp);
            source.property("Caption", [] { return std::string("page context"); });
            replacement.property("Caption", [] { return std::string("replacement"); });
            const char* xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='context'><TextBlock Id='bound' Text='{Binding Caption}'/></StackPanel></Window>";
            ui_fleck_diagnostic diagnostic{};
            require(ui_app_load_fleck_with_model(contextApp.nativeHandle(), xml, std::strlen(xml), source.nativeHandle(), nullptr, &diagnostic) == UI_OK, "Fleck context load");
            auto root = contextApp.findFleckNode("context");
            auto label = contextApp.findFleckNode("bound");
            require(label.text() == "page context" && label.dataContext() == source.nativeHandle(), "Fleck bindings inherit page model");
            root.setDataContext(replacement.nativeHandle());
            require(label.text() == "replacement", "Fleck context binding switches model");
            fleck::Page page;
            require(fleck::parse(xml, page, diagnostic), "Parse context page");
            auto resource = fleck::compile_resource(page);
            require(ui_app_load_fleck_resource_with_model(contextApp.nativeHandle(), resource.data(), resource.size(), source.nativeHandle(), nullptr, &diagnostic) == UI_OK, "Compiled context page");
            require(contextApp.findFleckNode("bound").text() == "page context", "Compiled and XML context parity");
            contextApp.openWindow(); contextApp.poll(0);
            auto retained = contextApp.findFleckNode("context");
            retained.setDataContext(replacement.nativeHandle());
            contextApp.refreshFleck(xml, source.nativeHandle());
            require(contextApp.findFleckNode("bound").text() == "replacement" && retained.dataContext() == replacement.nativeHandle(),
                "Successful refresh preserves explicit context by identity");
            retained.setDataContext(nullptr);
            contextApp.refreshFleck(xml, source.nativeHandle());
            require(contextApp.findFleckNode("bound").text().empty() && retained.dataContextSource() == UI_PROPERTY_SOURCE_LOCAL,
                "Explicit null context survives refresh");
            auto orphan = contextApp.createNode(UI_TEXT_BLOCK);
            ui_model* disposable = nullptr;
            require(ui_model_create(contextApp.nativeHandle(), &disposable) == UI_OK, "Create borrowed context model");
            orphan.setDataContext(disposable);
            require(ui_model_destroy(disposable) == UI_OK && orphan.dataContext() == nullptr && orphan.dataContextSource() == UI_PROPERTY_SOURCE_LOCAL,
                "Destroyed context becomes explicit null without dangling handles");
        }
        model.property("Extent", [&] { return extent; });
        for (bool compiled : {false, true}) {
            sunfleck::App contexts(UI_SOFTWARE, true);
            sunfleck::PropertyModel outer(contexts), otherOuter(contexts), first(contexts), second(contexts), invalid(contexts);
            first.property("Caption", [] { return std::string("first child"); });
            second.property("Caption", [] { return std::string("second child"); });
            first.property("Nested", [&] { return second.nativeHandle(); });
            second.property("Nested", [&] { return first.nativeHandle(); });
            ui_model* selected = first.nativeHandle();
            outer.property("Selected", [&] { return selected; });
            otherOuter.property("Selected", [&] { return second.nativeHandle(); });
            const char* xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root'>"
                "<StackPanel Id='scope' DataContext='{Binding Selected}'><TextBlock Id='caption' Text='{Binding Caption}'/>"
                "<TextBlock Id='nested' Text='{Binding Caption}' DataContext='{Binding Nested}'/></StackPanel>"
                "<TextBlock Id='isolated' DataContext='null' Text='{Binding Missing}'/></StackPanel></Window>";
            ui_fleck_diagnostic diagnostic{};
            if (compiled) {
                fleck::Page page; require(fleck::parse(xml, page, diagnostic), "Parse nested context page");
                auto bytes = fleck::compile_resource(page);
                require(ui_app_load_fleck_resource_with_model(contexts.nativeHandle(), bytes.data(), bytes.size(), outer.nativeHandle(), nullptr, &diagnostic) == UI_OK,
                    "Load compiled nested context page");
            } else require(ui_app_load_fleck_with_model(contexts.nativeHandle(), xml, std::strlen(xml), outer.nativeHandle(), nullptr, &diagnostic) == UI_OK,
                "Load XML nested context page");
            auto captionNode = contexts.findFleckNode("caption"), nested = contexts.findFleckNode("nested");
            require(captionNode.text() == "first child" && nested.text() == "second child" &&
                contexts.findFleckNode("isolated").text().empty(), "Nested context and explicit null are independent of attribute order");
            selected = second.nativeHandle(); outer.notify("Selected");
            require(captionNode.text() == "second child" && nested.text() == "first child", "Context source notifications reconnect nested descendants");
            selected = invalid.nativeHandle();
            require(ui_model_notify_property_changed(outer.nativeHandle(), "Selected") == UI_ERROR_INVALID_ARGUMENT &&
                captionNode.text() == "second child" && nested.text() == "first child" &&
                contexts.findFleckNode("scope").dataContext() == second.nativeHandle(), "Invalid derived context preserves entire previous subtree");
            selected = first.nativeHandle(); outer.notify("Selected");
            contexts.findFleckNode("root").setDataContext(otherOuter.nativeHandle());
            require(captionNode.text() == "second child" && nested.text() == "first child", "Parent replacement reevaluates context binding against new parent");
            selected = nullptr; outer.notify("Selected");
            require(captionNode.text() == "second child", "Old parent no longer controls derived context");
            contexts.findFleckNode("root").setDataContext(nullptr);
            require(captionNode.text().empty() && nested.text().empty(), "Null parent makes nested declarations dormant");
            contexts.findFleckNode("root").setDataContext(otherOuter.nativeHandle());
            require(captionNode.text() == "second child", "Dormant nested contexts reactivate");
            contexts.openWindow(); contexts.poll(0);
            contexts.findFleckNode("scope").setDataContext(first.nativeHandle());
            contexts.refreshFleck(xml, otherOuter.nativeHandle());
            require(contexts.findFleckNode("caption").text() == "first child" &&
                contexts.findFleckNode("nested").text() == "second child" &&
                contexts.findFleckNode("scope").dataContextSource() == UI_PROPERTY_SOURCE_LOCAL,
                "Refresh retains an explicit context override of the original Fleck binding");
        }
        {
            sunfleck::App nullContext(UI_SOFTWARE, true);
            nullContext.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel DataContext='null'><TextBlock Id='nullChild' Text='{Binding Caption}'/></StackPanel></Window>");
            require(nullContext.findFleckNode("nullChild").text().empty(), "Explicit null authorizes dormant Fleck bindings without a page model");
        }
        model.bind(node, UI_PROPERTY_WIDTH, "Extent", UI_BIND_ONE_WAY);
        require(node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_BINDING, "Binding source");
        require(ui_node_set_property(node.nativeHandle(), UI_PROPERTY_WIDTH, &bad) == UI_ERROR_INVALID_ARGUMENT, "Reject bound update");
        require(node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_BINDING, "Invalid update preserves binding");
        auto editor = app.createNode(UI_TEXT_BOX);
        std::string caption = "stable";
        model.property("Caption", [&] { return caption; });
        model.bind(editor, UI_PROPERTY_TEXT, "Caption", UI_BIND_ONE_WAY);
        editor.nativeHandle()->caret = 2;
        editor.nativeHandle()->select_all = true;
        model.notify("Caption");
        require(editor.nativeHandle()->caret == 2 && editor.nativeHandle()->select_all, "Equal effective update preserves editing state");
        node.clearProperty(UI_PROPERTY_WIDTH);
        extent = 240; model.notify("Extent");
        require(node.width() == -1, "Clear removes subscription");
        model.bind(node, UI_PROPERTY_WIDTH, "Extent", UI_BIND_ONE_TIME);
        require(node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_BINDING, "OneTime declaration source");
        require(ui_node_clear_binding(node.nativeHandle(), UI_PROPERTY_WIDTH) == UI_OK, "Clear binding");
        require(node.width() == 240 && node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL, "ClearBinding retains snapshot as local");

        // Base updates must survive an overlay even when equal to its rendered value.
        node.clearProperty(UI_PROPERTY_WIDTH);
        node.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE, number(60));
        require(node.width() == 60 && node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_STYLE, "Style fallback");
        model.bind(node, UI_PROPERTY_WIDTH, "Extent", UI_BIND_ONE_WAY);
        node.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION, number(300));
        require(node.width() == 300 && node.basePropertyValue(UI_PROPERTY_WIDTH).number == 240, "Animation overlay");
        extent = 300; model.notify("Extent");
        require(node.basePropertyValue(UI_PROPERTY_WIDTH).number == 300, "Binding updates hidden base");
        extent = 310; model.notify("Extent");
        require(node.width() == 300 && node.basePropertyValue(UI_PROPERTY_WIDTH).number == 310, "Animation preserves new base");
        require(ui_node_set_property_layer(node.nativeHandle(), UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE, &bad) == UI_ERROR_INVALID_ARGUMENT, "Invalid hidden layer rejected");
        node.clearPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION);
        require(node.width() == 310 && node.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_BINDING, "Remove overlay restores binding");
        node.clearProperty(UI_PROPERTY_WIDTH);
        require(node.width() == 60, "Clear local reveals style");
        node.clearPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE);
        require(node.width() == -1, "Remove style restores default");
        ui_binding_value disabled{}; disabled.kind = UI_VALUE_BOOL;
        require(ui_node_set_property_layer(node.nativeHandle(), UI_PROPERTY_IS_ENABLED, UI_PRECEDENCE_ANIMATION, &disabled) == UI_ERROR_INVALID_ARGUMENT, "Nonanimatable metadata enforced");

        auto slider = app.createNode(UI_SLIDER);
        int progress = 20;
        model.property("Progress", [&] { return progress; }, [&](int value) { progress = value; });
        model.bind(slider, UI_PROPERTY_VALUE, "Progress", UI_BIND_TWO_WAY);
        slider.setPropertyLayer(UI_PROPERTY_VALUE, UI_PRECEDENCE_ANIMATION, number(80));
        progress = 80; model.notify("Progress");
        require(slider.basePropertyValue(UI_PROPERTY_VALUE).integer == 80, "Equal overlay still updates base");
        slider.nativeHandle()->value = 45;
        binding_user_changed(*slider.nativeHandle(), UI_PROPERTY_VALUE);
        require(progress == 45 && slider.value() == 80, "User input updates source without writing animation");
        slider.clearPropertyLayer(UI_PROPERTY_VALUE, UI_PRECEDENCE_ANIMATION);
        require(slider.value() == 45 && slider.propertySource(UI_PROPERTY_VALUE) == UI_PROPERTY_SOURCE_BINDING, "Input preserves binding and base");

        BindingValue owned;
        double points[] = {2.5, -4};
        ui_binding_value geometry{}; geometry.kind = UI_VALUE_POINT; geometry.geometry = points;
        require(binding_copy_value(geometry, owned, UI_VALUE_POINT) == UI_OK, "Copy geometry");
        points[0] = 900;
        BindingValue moved(std::move(owned));
        BindingValue copied(moved);
        require(copied.value.geometry[0] == 2.5 && copied.value.geometry != moved.value.geometry, "Geometry owns independent storage");
        require(binding_copy_value(copied.value, copied, UI_VALUE_POINT) == UI_OK && copied.value.geometry[1] == -4, "Aliased copy is safe");

        BindingValue literal;
        require(binding_parse_value("1,2,3,4", UI_VALUE_THICKNESS, literal) == UI_OK && literal.geometry[2] == 3, "Typed Fleck literal");
        require(binding_parse_value("1,2,", UI_VALUE_THICKNESS, literal) == UI_ERROR_INVALID_ARGUMENT && literal.geometry[2] == 3, "Invalid literal is atomic");
        require(binding_parse_value("#80abcdef", UI_VALUE_COLOR, literal) == UI_OK && literal.value.integer == 0x80abcdef, "ARGB literal");
        require(binding_parse_value("Auto", UI_VALUE_LENGTH, literal) == UI_OK && literal.value.integer == UI_LENGTH_AUTO, "Length literal");
        ui_point position{2.5, 7};
        model.property("Position", [&] { return position; }, [&](ui_point point) { position = point; });
        require(model.describe("typed").find("point") != std::string::npos, "C++ typed model metadata");
        ui_binding_value encoded{}; std::string buffer;
        sunfleck::PropertyModel::encode(position, encoded, buffer);
        require(sunfleck::PropertyModel::decode<ui_point>(encoded).y == 7, "C++ geometry roundtrip");

        const char* templateXml = "<ControlTemplate xmlns='urn:sunfleck:v1' xmlns:c='urn:typed' Class='TypedTemplate' Namespace='urn:typed' TargetType='c:Typed'><TextBlock Text='typed'/></ControlTemplate>";
        ui_fleck_resource_entry templateEntry{"typed.fleck", templateXml, std::strlen(templateXml)};
        ui_fleck_diagnostic typedDiagnostic{};
        require(ui_app_register_fleck_resources(app.nativeHandle(), &templateEntry, 1, &typedDiagnostic) == UI_OK, "Typed template registration");
        ui_component_property pointProperty{};
        pointProperty.name = "Position";
        sunfleck::PropertyModel::encode(position, pointProperty.default_value, buffer);
        ui_component_type typedDescriptor{};
        typedDescriptor.struct_size = sizeof(typedDescriptor); typedDescriptor.version = UI_COMPONENT_TYPE_VERSION;
        typedDescriptor.namespace_uri = "urn:typed"; typedDescriptor.name = "Typed";
        typedDescriptor.template_namespace = "urn:typed"; typedDescriptor.template_name = "TypedTemplate";
        typedDescriptor.properties = &pointProperty; typedDescriptor.property_count = 1;
        const sunfleck::metadata::Type typeMetadata{typedDescriptor, "Typed", "Control", {__FILE__, __LINE__}};
        std::ostringstream manifest; sunfleck::metadata::write(manifest, std::span(&typeMetadata, 1));
        auto manifestText = manifest.str();
        require(ui_app_register_component_manifest(app.nativeHandle(), manifestText.c_str(), manifestText.size(), &typedDiagnostic) == UI_OK, "Typed manifest defaults roundtrip");
        const std::string typedPage = "<Window xmlns='urn:sunfleck:v1' xmlns:c='urn:typed'><c:Typed Id='typed' Position='3.25,-5'/></Window>";
        app.loadFleck(typedPage);
        sunfleck::FrameworkElement typed(app.findFleckNode("typed"));
        auto typedNode = app.findFleckNode("typed");
        const auto registeredPoint = typedNode.resolveProperty("Position");
        const auto registeredDescription = typedNode.describeProperty(registeredPoint);
        require(ui_node_resolve_property(typedNode.nativeHandle(), "DataContext", &untouched) == UI_OK &&
            typedNode.propertyValue(sunfleck::RegisteredProperty{untouched}).kind == UI_VALUE_MODEL,
            "Component context uses the registered host property");
        require(std::string(registeredDescription.owner_namespace) == "urn:typed" &&
            std::string(registeredDescription.owner_name) == "Typed" && registeredDescription.kind == UI_VALUE_POINT,
            "Registered custom descriptor identifies declaring type");
        ui_binding_value wrongTarget{};
        require(ui_node_get_registered_property(node.nativeHandle(), registeredPoint.id, &wrongTarget) == UI_ERROR_INVALID_ARGUMENT,
            "Custom property cannot target unrelated node");
        require(typed.get<ui_point>("Position").x == 3.25, "Typed component literal");
        typedNode.bindProperty(registeredPoint, model.nativeHandle(), "Position", UI_BIND_TWO_WAY);
        typed.set("Position", ui_point{8, 10});
        require(position.x == 8 && position.y == 10, "Typed C++ TwoWay binding");
        typed.setLayer("Position", UI_PRECEDENCE_STYLE, ui_point{1, 1});
        typed.setLayer("Position", UI_PRECEDENCE_ANIMATION, ui_point{20, 30});
        position = {15, 16}; model.notify("Position");
        require(typed.get<ui_point>("Position").x == 20 && typed.baseValue<ui_point>("Position").x == 15, "Component overlay preserves binding updates");
        require(typed.valueSource("Position") == UI_PROPERTY_SOURCE_ANIMATION, "Component animation source");
        require(typedNode.propertyValue(registeredPoint).geometry[0] == 20 &&
            typedNode.basePropertyValue(registeredPoint).geometry[0] == 15 &&
            typedNode.propertySource(registeredPoint) == UI_PROPERTY_SOURCE_ANIMATION, "Registered reads expose shared component layers");
        ui_registered_binding_options unsupported{sizeof(unsupported), 1, registeredPoint.id, "Position", UI_BIND_ONE_WAY, UI_UPDATE_LOST_FOCUS, nullptr};
        require(ui_node_bind_registered_property(typedNode.nativeHandle(), model.nativeHandle(), &unsupported) == UI_ERROR_INVALID_ARGUMENT,
            "Unsupported component trigger is rejected before detaching binding");
        typed.clearLayer("Position", UI_PRECEDENCE_ANIMATION);
        require(typed.get<ui_point>("Position").x == 15 && typed.valueSource("Position") == UI_PROPERTY_SOURCE_BINDING, "Component restores live binding");
        typed.clearLayer("Position");
        require(typed.get<ui_point>("Position").x == 1 && typed.valueSource("Position") == UI_PROPERTY_SOURCE_STYLE, "Component local clear reveals style");
        typed.clearLayer("Position", UI_PRECEDENCE_STYLE);
        require(typed.get<ui_point>("Position").x == 2.5 && typed.valueSource("Position") == UI_PROPERTY_SOURCE_DEFAULT, "Component default restored");
        require(typed.describeProperty("Position").flags & UI_PROPERTY_ANIMATABLE, "Component metadata");
        fleck::Page typedParsed;
        require(fleck::parse(typedPage, typedParsed, typedDiagnostic), "Typed page parse");
        auto typedBytes = fleck::compile_resource(typedParsed);
        uint64_t byteHash = 14695981039346656037ull;
        for (unsigned char byte : typedBytes) byteHash = (byteHash ^ byte) * 1099511628211ull;
        require(fleck::resource_fingerprint(typedParsed) == byteHash, "Streaming identity hashes every serialized byte");
        require(ui_app_load_fleck_resource(app.nativeHandle(), typedBytes.data(), typedBytes.size(), &typedDiagnostic) == UI_OK, "Typed compiled page");
        require(app.findFleckNode("typed").resolveProperty("Position") == registeredPoint &&
            app.findFleckNode("typed").propertyValue(registeredPoint).geometry[1] == -5, "Property identity survives resource replacement");
        require(sunfleck::FrameworkElement(app.findFleckNode("typed")).get<ui_point>("Position").y == -5, "Typed compiled literal parity");

        {
            sunfleck::App metadataApp(UI_SOFTWARE, true);
            require(ui_app_register_fleck_resources(metadataApp.nativeHandle(), &templateEntry, 1, &typedDiagnostic) == UI_OK, "Metadata template");
            int callbacks = 0;
            auto metadata = sunfleck::componentPropertyMetadata(UI_PROPERTY_BINDABLE | UI_PROPERTY_ANIMATABLE | UI_PROPERTY_AFFECTS_MEASURE,
                +[](ui_component* component, const ui_binding_value* value, void* context) -> ui_result {
                    ++*static_cast<int*>(context);
                    if (ui_component_set_property(component, "Position", value) != UI_ERROR_INVALID_STATE) return UI_ERROR_INVALID_STATE;
                    return value->geometry[0] < 0 ? UI_ERROR_INVALID_ARGUMENT : UI_OK;
                },
                +[](ui_component*, const ui_binding_value* value, ui_binding_value* result, void*) -> ui_result {
                    static thread_local ui_point point;
                    point = {value->geometry[0], value->geometry[1]};
                    if (point.x == 997) throw std::bad_alloc{};
                    if (point.x == 998) throw std::runtime_error("Business coercion failure");
                    if (point.x == 999) return UI_ERROR_INVALID_ARGUMENT;
                    point.x = std::min(point.x, 10.0);
                    *result = *value; result->geometry = &point.x;
                    return UI_OK;
                }, &callbacks);
            auto declaration = typedDescriptor;
            auto declaredProperty = pointProperty; declaredProperty.metadata = &metadata;
            declaration.properties = &declaredProperty;
            declaration.version = 1;
            require(ui_app_register_component_type(metadataApp.nativeHandle(), &declaration) == UI_ERROR_VERSION_MISMATCH, "Old component array layout rejected");
            declaration.version = UI_COMPONENT_TYPE_VERSION;
            metadata.flags |= 256;
            require(ui_app_register_component_type(metadataApp.nativeHandle(), &declaration) == UI_ERROR_INVALID_ARGUMENT, "Unknown metadata flags rejected atomically");
            metadata.flags &= ~256u;
            require(ui_app_register_component_type(metadataApp.nativeHandle(), &declaration) == UI_OK, "Custom property metadata");
            require(ui_app_validate_fleck(metadataApp.nativeHandle(), typedPage.c_str(), typedPage.size(), &typedDiagnostic) == UI_OK && callbacks == 0, "Static validation never invokes metadata callbacks");
            metadataApp.loadFleck(typedPage);
            sunfleck::FrameworkElement value(metadataApp.findFleckNode("typed"));
            auto metadataNode = metadataApp.findFleckNode("typed");
            const auto metadataPoint = metadataNode.resolveProperty("Position");
            require(metadataPoint != registeredPoint &&
                ui_node_get_registered_property(metadataNode.nativeHandle(), registeredPoint.id, &wrongTarget) == UI_ERROR_INVALID_ARGUMENT,
                "Same named type in different App has distinct identity");
            require(value.describeProperty("Position").flags == metadata.flags && callbacks > 0, "Declared flags and validation");
            value.set("Position", ui_point{100, 2});
            require(value.get<ui_point>("Position").x == 10 && value.baseValue<ui_point>("Position").x == 100, "Coercion preserves requested base value");
            ui_point rejected{-1, 3}; ui_binding_value candidate{}; candidate.kind = UI_VALUE_POINT; candidate.geometry = &rejected.x;
            require(ui_node_set_registered_property(metadataNode.nativeHandle(), metadataPoint.id, UI_PRECEDENCE_LOCAL, &candidate) == UI_ERROR_INVALID_ARGUMENT && value.get<ui_point>("Position").x == 10, "Registered validation rejection preserves effective value");
            value.setLayer("Position", UI_PRECEDENCE_ANIMATION, ui_point{5, 7});
            value.set("Position", rejected); // The selected animation is the value validated.
            require(ui_component_clear_property_layer(value.nativeHandle(), "Position", UI_PRECEDENCE_ANIMATION) == UI_ERROR_INVALID_ARGUMENT &&
                value.valueSource("Position") == UI_PROPERTY_SOURCE_ANIMATION, "Invalid revealed base preserves animation layer");
            value.set("Position", ui_point{8, 9}); value.clearLayer("Position", UI_PRECEDENCE_ANIMATION);
            require(value.get<ui_point>("Position").x == 8, "Valid base can be revealed");
            value.set("Position", ui_point{100, 2});
            metadataApp.openWindow(); metadataApp.poll(0);
            metadataApp.refreshFleck(typedPage); metadataApp.poll(0);
            value = sunfleck::FrameworkElement(metadataApp.findFleckNode("typed"));
            require(metadataApp.findFleckNode("typed").resolveProperty("Position") == metadataPoint, "Property identity survives transactional refresh");
            require(value.get<ui_point>("Position").x == 10 && value.baseValue<ui_point>("Position").x == 100, "Refresh recoerces retained layers");
            sunfleck::PropertyModel sourceModel(metadataApp); ui_point sourcePoint{4, 5};
            sourceModel.property("Position", [&] { return sourcePoint; }, [&](ui_point p) { sourcePoint = p; });
            require(ui_component_bind_property(value.nativeHandle(), "Position", sourceModel.nativeHandle(), "Position", UI_BIND_TWO_WAY) == UI_ERROR_INVALID_ARGUMENT, "Metadata disallows TwoWay");
            value.bind("Position", sourceModel, "Position", UI_BIND_ONE_WAY);
            rejected = {999, 0};
            require(ui_component_set_property(value.nativeHandle(), "Position", &candidate) == UI_ERROR_INVALID_ARGUMENT && value.get<ui_point>("Position").x == 4, "Coercer failure retains value and binding");
            sourcePoint = {6, 7}; sourceModel.notify("Position");
            require(value.get<ui_point>("Position").x == 6, "Rejected update retains subscription");
            for (double fault : {997.,998.}) {
                rejected={fault,0};
                require(ui_component_set_property(value.nativeHandle(),"Position",&candidate)==
                    (fault==997?UI_ERROR_OUT_OF_MEMORY:UI_ERROR_INVALID_STATE) &&
                    value.get<ui_point>("Position").x==6 && value.valueSource("Position")==UI_PROPERTY_SOURCE_BINDING,
                    "Allocation and business exceptions in coercion preserve value and binding");
            }
            sourcePoint={7,8}; sourceModel.notify("Position");
            require(value.get<ui_point>("Position").x==7,"Binding remains usable after callback exceptions");
        }

        {
            sunfleck::App identityApp(UI_SOFTWARE, true);
            require(ui_app_register_fleck_resources(identityApp.nativeHandle(), &templateEntry, 1, &typedDiagnostic) == UI_OK, "Identity template");
            ui_component_property declared[] = {{"Width", number(12), 0, UI_BIND_ONE_WAY, nullptr},
                {"Status", number(7), 1, UI_BIND_ONE_WAY, nullptr}};
            auto declaration = typedDescriptor; declaration.properties = declared; declaration.property_count = 2;
            require(ui_app_register_component_type(identityApp.nativeHandle(), &declaration) == UI_OK, "Identity type");
            identityApp.loadFleck("<Window xmlns='urn:sunfleck:v1' xmlns:c='urn:typed'><StackPanel><c:Typed Id='first'/><c:Typed Id='second'/></StackPanel></Window>");
            auto first = identityApp.findFleckNode("first"), second = identityApp.findFleckNode("second");
            auto customWidth = first.resolveProperty("Width"), readOnly = first.resolveProperty("Status");
            require(customWidth == second.resolveProperty("Width") && customWidth != registeredWidth, "Same type shares identity and custom name shadows built-in");
            first.setPropertyValue(customWidth, number(25));
            require(first.propertyValue(customWidth).integer == 25 && first.width() == -1 && second.propertyValue(customWidth).integer == 12,
                "Identity shares metadata, not instance storage or built-in slot");
            require(first.describeProperty(readOnly).read_only == 1, "Registered read-only descriptor");
            auto candidate = number(9);
            require(ui_node_set_registered_property(first.nativeHandle(), readOnly.id, UI_PRECEDENCE_LOCAL, &candidate) == UI_ERROR_INVALID_ARGUMENT &&
                ui_node_clear_registered_property(first.nativeHandle(), readOnly.id, UI_PRECEDENCE_LOCAL) == UI_ERROR_INVALID_ARGUMENT,
                "Generic callers cannot overwrite read-only state");
            sunfleck::FrameworkElement owner(first); owner.set("Status", 9);
            require(first.propertyValue(readOnly).integer == 9, "Component owner can publish read-only state");
            ui_registered_property_descriptor descriptor{sizeof(descriptor), 2};
            require(ui_node_describe_registered_property(first.nativeHandle(), customWidth.id, &descriptor) == UI_ERROR_VERSION_MISMATCH,
                "Registered descriptor version checked");
        }
        auto button = app.createNode(UI_BUTTON);
        {
            auto panel = app.createNode(UI_STACK_PANEL);
            double spacing = 4.25, padding = 6.5;
            sunfleck::PropertyModel metrics(app);
            metrics.property("Gap", [&] { return spacing; });
            metrics.property("Inset", [&] { return padding; });
            metrics.bind(panel, UI_PROPERTY_SPACING, "Gap", UI_BIND_ONE_WAY);
            metrics.bind(panel, UI_PROPERTY_PADDING, "Inset", UI_BIND_ONE_WAY);
            require(ui_node_set_layout(panel.nativeHandle(), 8, 65) == UI_ERROR_INVALID_ARGUMENT &&
                panel.propertyValue(UI_PROPERTY_SPACING).number == 4.25 && panel.propertySource(UI_PROPERTY_PADDING) == UI_PROPERTY_SOURCE_BINDING,
                "Invalid paired layout update preserves both values and bindings");
            for (double invalid : {-.5, -2., 65., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
                require(ui_node_set_layout(panel.nativeHandle(), invalid, 2) == UI_ERROR_INVALID_ARGUMENT &&
                    ui_node_set_layout(panel.nativeHandle(), 2, invalid) == UI_ERROR_INVALID_ARGUMENT &&
                    panel.propertyValue(UI_PROPERTY_SPACING).number == 4.25 && panel.propertyValue(UI_PROPERTY_PADDING).number == 6.5,
                    "Invalid fractional pair retains both effective values");
            }
            spacing = 5.75; metrics.notify("Gap");
            require(panel.propertyValue(UI_PROPERTY_SPACING).number == 5.75, "Preserved spacing subscription");
            auto overlay = number(20); panel.setPropertyLayer(UI_PROPERTY_SPACING, UI_PRECEDENCE_ANIMATION, overlay);
            require(ui_node_set_layout(panel.nativeHandle(), 7.125, 9.25) == UI_OK && panel.propertyValue(UI_PROPERTY_SPACING).number == 20 &&
                panel.basePropertyValue(UI_PROPERTY_SPACING).number == 7.125 && panel.propertyValue(UI_PROPERTY_PADDING).number == 9.25,
                "Legacy pair setter obeys precedence and updates both local layers");
            panel.clearPropertyLayer(UI_PROPERTY_SPACING, UI_PRECEDENCE_ANIMATION);
            spacing = 10; metrics.notify("Gap");
            require(panel.propertyValue(UI_PROPERTY_SPACING).number == 7.125, "Pair setter detaches affected bindings");
            panel.clearProperty(UI_PROPERTY_SPACING);
            require(panel.propertyValue(UI_PROPERTY_SPACING).number == -1 && panel.propertySource(UI_PROPERTY_SPACING) == UI_PROPERTY_SOURCE_DEFAULT, "Clear restores theme spacing");
            panel.setWidth(100);
            require(ui_node_set_weight(panel.nativeHandle(), 2) == UI_OK, "Weight setter");
            require(ui_node_set_weight(panel.nativeHandle(), 1025) == UI_ERROR_INVALID_ARGUMENT &&
                panel.width() == 100 && panel.propertyValue(UI_PROPERTY_WEIGHT).integer == 2, "Invalid Weight preserves layout values");
            const std::string metricPage = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='metrics' Spacing='{Binding Gap}' Padding='{Binding Inset}' Width='100' Weight='0'><TextBlock/></StackPanel></Window>";
            require(ui_app_load_fleck_with_model(app.nativeHandle(), metricPage.data(), metricPage.size(), metrics.nativeHandle(), nullptr, &typedDiagnostic) == UI_OK, "Metric XML load");
            auto loaded = app.findFleckNode("metrics");
            require(loaded.propertyValue(UI_PROPERTY_SPACING).number == spacing && loaded.propertySource(UI_PROPERTY_WEIGHT) == UI_PROPERTY_SOURCE_LOCAL, "XML metric binding and explicit default tracking");
            fleck::Page parsed;
            require(fleck::parse(metricPage, parsed, typedDiagnostic), "Metric page parse");
            const auto bytes = fleck::compile_resource(parsed);
            require(ui_app_load_fleck_resource_with_model(app.nativeHandle(), bytes.data(), bytes.size(), metrics.nativeHandle(), nullptr, &typedDiagnostic) == UI_OK, "Metric binary load");
            require(app.findFleckNode("metrics").propertyValue(UI_PROPERTY_PADDING).number == padding, "Metric binary parity");
        }
        ui_command* command = nullptr;
        require(ui_command_create(app.nativeHandle(), [](ui_node*, void*) {}, nullptr, &command) == UI_OK, "Command creation");
        ui_binding_value commandValue{}; commandValue.kind = UI_VALUE_COMMAND; commandValue.command = command;
        button.setPropertyLayer(UI_PROPERTY_COMMAND, UI_PRECEDENCE_STYLE, commandValue);
        commandValue.command = nullptr;
        button.setPropertyValue(UI_PROPERTY_COMMAND, commandValue);
        require(ui_command_destroy(command) == UI_OK, "Destroy hidden command reference");
        button.clearProperty(UI_PROPERTY_COMMAND);
        require(button.propertyValue(UI_PROPERTY_COMMAND).command == nullptr, "Destroyed hidden reference cannot reappear");
        ui_binding_value text{}; text.kind = UI_VALUE_STRING; text.string = "\xf0\x9f\x8c\xb1";
        button.setPropertyValue(UI_PROPERTY_CONTENT, text);
        require(std::string(button.propertyValue(UI_PROPERTY_TEXT).string) == text.string, "Unicode generic getter");
        require(button.propertySource(UI_PROPERTY_TEXT) == UI_PROPERTY_SOURCE_LOCAL, "Content/Text alias source");
        button.clearProperty(UI_PROPERTY_TEXT);
        require(button.text().empty() && button.propertySource(UI_PROPERTY_CONTENT) == UI_PROPERTY_SOURCE_DEFAULT, "Alias clear");
        auto selector = app.createNode(UI_LIST_VIEW); selector.clearProperty(UI_PROPERTY_VALUE);
        require(selector.value() == 0, "Clear empty selector");
        auto split = app.createNode(UI_SPLIT_VIEW); split.setValue(70); split.clearProperty(UI_PROPERTY_VALUE);
        require(split.value() == 50, "Kind-specific default");
        node.setMinWidth(100); node.setMaxWidth(200); node.clearProperty(UI_PROPERTY_MAX_WIDTH);
        require(std::isinf(node.maxWidth()) && node.maxWidth() > 0, "Clear restores the unbounded maximum");

        ui_property_descriptor descriptor{}; descriptor.struct_size = sizeof(descriptor); descriptor.version = 1;
        require(ui_property_describe(UI_TEXT_BOX, UI_PROPERTY_TEXT, &descriptor) == UI_OK &&
            (descriptor.flags & UI_PROPERTY_TWO_WAY), "Editor descriptor");
        descriptor.version = 2;
        require(ui_property_describe(UI_TEXT_BOX, UI_PROPERTY_TEXT, &descriptor) == UI_ERROR_INVALID_ARGUMENT, "Descriptor version check");
        descriptor.version = 1;
        require(ui_property_describe(UI_IMAGE, UI_PROPERTY_TEXT, &descriptor) == UI_ERROR_INVALID_ARGUMENT, "Kind/property check");
        ui_property_source source{}; ui_result threaded{};
        std::thread other([&] { threaded = ui_node_get_property_source(node.nativeHandle(), UI_PROPERTY_WIDTH, &source); }); other.join();
        require(threaded == UI_ERROR_WRONG_THREAD, "Thread affinity");

        const std::string xml = "<Window xmlns='urn:sunfleck:v1'><TextBlock Id='label' Text='hello' Width='Auto' IsEnabled='true'/></Window>";
        app.loadFleck(xml);
        auto label = app.findFleckNode("label");
        require(label.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL && label.propertySource(UI_PROPERTY_IS_ENABLED) == UI_PROPERTY_SOURCE_LOCAL, "Explicit default declarations");
        label.clearProperty(UI_PROPERTY_WIDTH);
        require(label.propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_DEFAULT, "Cleared XML value stays default");
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        require(fleck::parse(xml, page, diagnostic), "Parse fixture");
        const auto bytes = fleck::compile_resource(page);
        sunfleck::App loaded(UI_SOFTWARE, true);
        require(ui_app_load_fleck_resource(loaded.nativeHandle(), bytes.data(), bytes.size(), &diagnostic) == UI_OK, "Load resource");
        require(loaded.findFleckNode("label").propertySource(UI_PROPERTY_WIDTH) == UI_PROPERTY_SOURCE_LOCAL, "Compiled declaration source");
        std::cout << "Property metadata, values, origins, clear and resource parity passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
