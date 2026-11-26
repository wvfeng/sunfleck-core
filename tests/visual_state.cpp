#include <sunfleck/ui.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include <iostream>

static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static ui_binding_value state(int n) { ui_binding_value v{}; v.kind = UI_VALUE_ENUM; v.integer = n; return v; }
static ui_binding_value boolean(bool b) { ui_binding_value v{}; v.kind = UI_VALUE_BOOL; v.integer = b; return v; }
int main() {
    try {
        sunfleck::App app(UI_SOFTWARE,true);
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root' Spacing='10'><TextBox Id='first' Height='30'/><Button Id='second' Height='30' Content='Second'/></StackPanel></Window>");
        app.openWindow(); app.poll(0);
        auto root = app.findFleckNode("root"), first = app.findFleckNode("first"), second = app.findFleckNode("second");
        const int original = second.nativeHandle()->bounds.top;
        check(ui_host_focus(app.nativeHandle(),1) == UI_OK && ui_host_key_event(app.nativeHandle(),UI_HOST_TAB,0) == UI_OK, "Focus editor");
        ui_host_text_state text{}; text.struct_size=sizeof(text); text.version=1;
        check(ui_host_text_read(app.nativeHandle(),&text)==UI_OK && text.active,"Editor session active");
        const auto session=text.session, revision=text.revision;
        first.setPropertyValue(UI_PROPERTY_VISIBILITY,state(UI_HIDDEN));
        check(ui_host_text_read(app.nativeHandle(),&text)==UI_OK && !text.active,"Hidden editor immediately loses focus and text session");
        check(ui_host_text_update(app.nativeHandle(),session,revision,"late",4,4,4,0) != UI_OK,"Hidden editor rejects stale IME update");
        app.poll(0);
        check(second.nativeHandle()->bounds.top == original && first.nativeHandle()->visible.right == first.nativeHandle()->visible.left,"Hidden reserves space without drawing");
        first.setPropertyValue(UI_PROPERTY_VISIBILITY,state(UI_COLLAPSED)); app.poll(0);
        check(second.nativeHandle()->bounds.top == original-40 && first.nativeHandle()->bounds.right == first.nativeHandle()->bounds.left,"Collapsed removes dimensions and spacing");
        first.clearProperty(UI_PROPERTY_VISIBILITY); app.poll(0);
        check(second.nativeHandle()->bounds.top==original,"Clearing visibility restores layout");
        int calls=0; check(ui_node_set_action(second.nativeHandle(),+[](ui_node*,void* context){++*static_cast<int*>(context);},&calls)==UI_OK,"Click callback");
        auto b=second.nativeHandle()->bounds; const int x=(b.left+b.right)/2,y=(b.top+b.bottom)/2;
        root.setPropertyValue(UI_PROPERTY_IS_HIT_TEST_VISIBLE,boolean(false));
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,x,y)==UI_OK && ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,x,y)==UI_OK && calls==0,"Parent hit-test exclusion applies before repaint");
        root.clearProperty(UI_PROPERTY_IS_HIT_TEST_VISIBLE); app.poll(0);
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,x,y)==UI_OK && ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,x,y)==UI_OK && calls==1,"Hit testing restored");
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,x,y)==UI_OK,"Pointer press");
        root.setPropertyValue(UI_PROPERTY_VISIBILITY,state(UI_HIDDEN));
        root.clearProperty(UI_PROPERTY_VISIBILITY);
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,x,y)==UI_OK && calls==1,"Visibility transition cancels pending click");
        // Keyboard focus remains available even when pointer hit testing is disabled.
        root.setPropertyValue(UI_PROPERTY_IS_HIT_TEST_VISIBLE,boolean(false));
        check(ui_host_key_event(app.nativeHandle(),UI_HOST_TAB,0)==UI_OK,"Keyboard navigation with pointer exclusion");
        check(ui_host_text_read(app.nativeHandle(),&text)==UI_OK && text.active,"Hit testing does not disable keyboard editor");
        root.setEnabled(false);
        check(ui_host_text_read(app.nativeHandle(),&text)==UI_OK && !text.active,"Ancestor disable retires editor session");
        root.setEnabled(true); root.clearProperty(UI_PROPERTY_IS_HIT_TEST_VISIBLE);
        ui_visibility mode=UI_HIDDEN; sunfleck::PropertyModel model(app); model.property("Mode",[&]{return mode;});
        model.bind(first,UI_PROPERTY_VISIBILITY,"Mode",UI_BIND_ONE_WAY);
        mode=UI_COLLAPSED; model.notify("Mode"); app.poll(0);
        check(second.nativeHandle()->bounds.top==original-40,"Enum binding changes layout");
        auto invalid=state(99);
        check(ui_node_set_property(first.nativeHandle(),UI_PROPERTY_VISIBILITY,&invalid)!=UI_OK && first.propertyValue(UI_PROPERTY_VISIBILITY).integer==UI_COLLAPSED,"Invalid enum preserves bound state");
        const std::string xml="<Window xmlns='urn:sunfleck:v1'><Grid><Border Id='hidden' Visibility='Collapsed' Width='100'/><Border Id='shown' IsHitTestVisible='false' Background='#123456'/></Grid></Window>";
        fleck::Page page; ui_fleck_diagnostic diagnostic{}; check(fleck::parse(xml,page,diagnostic),"Visual state parses");
        auto resource=fleck::compile_resource(page);
        sunfleck::App compiled(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(compiled.nativeHandle(),resource.data(),resource.size(),&diagnostic)==UI_OK,"Compiled visual state loads");
        compiled.openWindow(); compiled.poll(0);
        auto shown=compiled.findFleckNode("shown");
        check(shown.nativeHandle()->bounds.right-shown.nativeHandle()->bounds.left > 200 && !shown.nativeHandle()->accepts_input(),"Collapsed Grid track removed; hit-test state survives resource");
        for (const char* panel : {"StackPanel","Grid","Flex","WrapPanel","DockPanel","SplitView","Overlay","ScrollView"}) {
            app.refreshFleck(std::string("<Window xmlns='urn:sunfleck:v1'><")+panel+" Id='panel' Spacing='10'><TextBlock Id='gone' Visibility='Collapsed' Width='80' Height='40'/><TextBlock Id='kept' Width='60' Height='30'/></"+panel+"></Window>");
            for (unsigned dpi : {96u,144u,192u}) {
                check(ui_host_resize(app.nativeHandle(),600,400,dpi)==UI_OK,"Visibility DPI resize"); app.poll(0);
                auto* container=app.findFleckNode("panel").nativeHandle(); auto* kept=app.findFleckNode("kept").nativeHandle();
                check(kept->bounds.left==container->bounds.left && kept->bounds.top==container->bounds.top,"Collapsed leading child leaves no track/gap");
            }
        }
        app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><ListView Id='list'><ListItem Id='one' Text='One'/><ListItem Text='Hidden' Visibility='Hidden'/><ListItem Text='Collapsed' Visibility='Collapsed'/><ListItem Id='four' Text='Four'/></ListView></Window>");
        app.poll(0);
        check(ui_host_key_event(app.nativeHandle(),UI_HOST_TAB,0)==UI_OK && ui_host_key_event(app.nativeHandle(),UI_HOST_NEXT,0)==UI_OK,"Selector keyboard navigation");
        check(app.findFleckNode("list").value()==3,"Arrow navigation skips hidden/collapsed items while preserving logical indices");
        const std::string invalidXml="<Window xmlns='urn:sunfleck:v1'><Border Visibility='Invisible'/></Window>";
        auto* retained=app.findFleckNode("list").nativeHandle();
        try { app.refreshFleck(invalidXml); check(false,"Invalid visibility accepted"); } catch(const sunfleck::Error&) {}
        check(app.findFleckNode("list").nativeHandle()==retained,"Invalid visual state refresh retains page");
        compiled.advanceAnimationClock(0);
        auto visual = compiled.findFleckNode("shown");
        const char* state_storyboard = "<Storyboard><Timeline Property='Opacity' DurationMs='100'><KeyFrame Position='0' Value='1'/><KeyFrame Position='1' Value='0.5'/></Timeline></Storyboard>";
        check(ui_node_register_visual_state(visual.nativeHandle(),"Common","Faded",state_storyboard)==UI_OK,"Register visual state");
        check(ui_node_go_to_visual_state(visual.nativeHandle(),"Common","Faded")==UI_OK,"Activate visual state");
        compiled.advanceAnimationClock(50); compiled.poll(0);
        check(std::abs(visual.propertyValue(UI_PROPERTY_OPACITY).number-0.75)<0.03,"State animation midpoint");
        visual.registerVisualState("Common","Normal",nullptr);
        visual.goToVisualState("Common","Normal");
        check(visual.propertyValue(UI_PROPERTY_OPACITY).number==1,"Leaving state restores opacity");
        check(ui_node_go_to_visual_state(visual.nativeHandle(),"Common","Missing")!=UI_OK,"Unknown visual state rejected");
        const std::string statePage = "<Window xmlns='urn:sunfleck:v1'><Border Id='box' Opacity='0.8'><Border.VisualStateGroups>"
            "<VisualStateGroup Name='Input.Pointer'><VisualState Name='Normal'/><VisualState Name='Over'>" +
            std::string(state_storyboard) + "</VisualState></VisualStateGroup></Border.VisualStateGroups></Border></Window>";
        check(fleck::parse(statePage,page,diagnostic),"Parse state groups");
        auto bytes = fleck::compile_resource(page);
        for (bool binary : {false,true}) {
            sunfleck::App host(UI_SOFTWARE,true);
            if (binary) check(ui_app_load_fleck_resource(host.nativeHandle(),bytes.data(),bytes.size(),&diagnostic)==UI_OK,"Load compiled state groups");
            else host.loadFleck(statePage);
            host.advanceAnimationClock(0);
            auto box = host.findFleckNode("box");
            box.goToVisualState("Input.Pointer","Over");
            check(box.propertyValue(UI_PROPERTY_OPACITY).number==1,"Declared state starts");
            box.goToVisualState("Input.Pointer","Normal");
            check(box.propertyValue(UI_PROPERTY_OPACITY).number==0.8,"Empty state restores local value");
            host.openWindow(); host.poll(0);
            box.goToVisualState("Input.Pointer","Over");
            auto duplicate = statePage;
            duplicate.replace(duplicate.find("Name='Over'"),11,"Name='Normal'");
            try { host.refreshFleck(duplicate); check(false,"Duplicate state accepted"); }
            catch (const sunfleck::Error&) {}
            check(host.findFleckNode("box").nativeHandle()==box.nativeHandle(),"Failed state refresh retains node");
            host.advanceAnimationClock(50); host.poll(0);
            check(box.propertyValue(UI_PROPERTY_OPACITY).number==0.75,"Failed state refresh retains playback");
            host.refreshFleck(statePage);
            box = host.findFleckNode("box");
            check(box.propertyValue(UI_PROPERTY_OPACITY).number==0.8,"Successful refresh releases old playback");
            box.goToVisualState("Input.Pointer","Over");
            box.registerVisualState("Overlay","Dim",state_storyboard);
            box.goToVisualState("Overlay","Dim");
            box.goToVisualState("Input.Pointer","Normal");
            check(box.propertyValue(UI_PROPERTY_OPACITY).number==1,"Leaving first group preserves replacement animation");
        }
        const std::string buttonPage = "<Window xmlns='urn:sunfleck:v1'><Button Id='button' Content='Submit' Height='40' Opacity='0.8'>"
            "<Button.VisualStateGroups><VisualStateGroup Name='CommonStates'><VisualState Name='Normal'/><VisualState Name='Pressed'>" +
            std::string(state_storyboard) + "</VisualState></VisualStateGroup></Button.VisualStateGroups></Button></Window>";
        app.refreshFleck(buttonPage); app.poll(0);
        auto button = app.findFleckNode("button");
        const auto bounds = button.nativeHandle()->bounds;
        const auto cx = (bounds.left+bounds.right)/2, cy = (bounds.top+bounds.bottom)/2;
        app.advanceAnimationClock(0);
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,cx,cy)==UI_OK,"Press stateful button");
        app.poll(0);
        check(button.propertyValue(UI_PROPERTY_OPACITY).number==1,"Pointer selects Pressed");
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,cx,cy)==UI_OK,"Release stateful button");
        app.poll(0);
        check(button.propertyValue(UI_PROPERTY_OPACITY).number==0.8,"Release falls back to Normal");
        const std::string face = "<ControlTemplate xmlns='urn:sunfleck:v1' Class='Example.Face' Namespace='urn:test' TargetType='Button'>"
            "<Border Id='frame' Opacity='0.8'><Border.VisualStateGroups><VisualStateGroup Name='CommonStates'>"
            "<VisualState Name='Normal'/><VisualState Name='Pressed'>" + std::string(state_storyboard) +
            "</VisualState></VisualStateGroup></Border.VisualStateGroups><ContentPresenter/></Border></ControlTemplate>";
        ui_fleck_resource_entry entry{"button-face.fleck",face.data(),face.size()};
        check(ui_app_register_fleck_resources(app.nativeHandle(),&entry,1,&diagnostic)==UI_OK,"Register button face");
        app.refreshFleck("<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><Button Id='action' Template='t:Face'><TextBlock Id='caption' Text='Submit'/></Button></Window>");
        app.poll(0);
        auto action = app.findFleckNode("action");
        ui_component* owner{};
        ui_node* frame{};
        check(ui_node_get_component(action.nativeHandle(),&owner)==UI_OK && ui_component_find_node(owner,"frame",&frame)==UI_OK,"Locate template visual");
        auto area = app.findFleckNode("caption").nativeHandle()->bounds;
        const int px=(area.left+area.right)/2, py=(area.top+area.bottom)/2;
        int activations=0;
        check(ui_node_set_action(action.nativeHandle(),+[](ui_node*,void* p){++*static_cast<int*>(p);},&activations)==UI_OK,"Set templated action");
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,px,py)==UI_OK,"Press projected content");
        app.poll(0);
        check(frame->opacity==1,"Template follows host press");
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,px,py)==UI_OK && activations==1,"Projected content activates host");
        app.poll(0);
        check(frame->opacity==0.8,"Template restores local opacity");
        check(ui_host_key_event(app.nativeHandle(),UI_HOST_ACTIVATE,0)==UI_OK && activations==2,"Keyboard activates templated host");
        check(ui_node_register_visual_state(frame,"FocusStates","Focused",state_storyboard)==UI_OK,"Register only the focused appearance");
        action.focus();
        app.poll(0);
        check(frame->opacity==1,"Keyboard focus selects the template focus appearance");
        check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,px,py)==UI_OK &&
              ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,px,py)==UI_OK,"Return to pointer input");
        app.poll(0);
        check(frame->opacity==0.8,"Pointer interaction clears keyboard focus appearance");
        app.refreshFleck("<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><StackPanel>"
            "<Button Id='action' Template='t:Face'><TextBox Id='part'/></Button><Button Id='outside'/>"
            "</StackPanel></Window>");
        app.poll(0);
        action = app.findFleckNode("action");
        check(ui_node_get_component(action.nativeHandle(),&owner)==UI_OK && ui_component_find_node(owner,"frame",&frame)==UI_OK,
            "Locate frame containing the projected editor");
        check(ui_node_register_visual_state(frame,"FocusStates","Focused",state_storyboard)==UI_OK,"Register focus appearance");
        auto part = app.findFleckNode("part");
        part.focus();
        app.poll(0);
        check(frame->opacity==1,"Keyboard focus within projected content reaches the templated button");
        int saves = 0;
        ui_command* save_part{};
        check(ui_command_create(app.nativeHandle(),+[](ui_node*,void* p){ ++*static_cast<int*>(p); },&saves,&save_part)==UI_OK,
            "Create command for the template host");
        action.setCommandBinding("Save",save_part);
        action.setInputBinding(UI_KEY_F5,0,"Save");
        uint32_t part_handled{};
        check(ui_host_key_gesture(app.nativeHandle(),UI_KEY_F5,0,&part_handled)==UI_OK && part_handled && saves==1,
            "Shortcut from projected editor routes to template host");
        app.findFleckNode("outside").focus();
        app.poll(0);
        check(frame->opacity==0.8,"Moving focus outside clears the host focus appearance");
        check(ui_command_destroy(save_part)==UI_OK,"Release template host command");
        for (const std::string kind : {"CheckBox","RadioButton","Switch","ToggleButton"}) {
            const std::string checkFace = "<ControlTemplate xmlns='urn:sunfleck:v1' Class='Example.CheckFace' Namespace='urn:test' TargetType='" + kind +
                "'><Border Id='mark' Opacity='0.8'><Border.VisualStateGroups><VisualStateGroup Name='CheckStates'>"
                "<VisualState Name='Checked'>" + state_storyboard +
                "</VisualState></VisualStateGroup></Border.VisualStateGroups><ContentPresenter/></Border></ControlTemplate>";
            entry = {"check-face.fleck",checkFace.data(),checkFace.size()};
            check(ui_app_register_fleck_resources(app.nativeHandle(),&entry,1,&diagnostic)==UI_OK,"Register selection face");
            app.refreshFleck("<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><" + kind +
                " Id='choice' Template='t:CheckFace' Text='Choice' Height='32'/></Window>");
            app.poll(0);
            auto choice = app.findFleckNode("choice");
            check(ui_node_get_component(choice.nativeHandle(),&owner)==UI_OK && ui_component_find_node(owner,"mark",&frame)==UI_OK,"Locate check mark");
            const auto box=choice.nativeHandle()->bounds;
            const auto x=(box.left+box.right)/2, y=(box.top+box.bottom)/2;
            check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,x,y)==UI_OK &&
                ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,x,y)==UI_OK,"Click templated selector");
            app.poll(0);
            check(choice.nativeHandle()->value==1 && frame->opacity==1,"Activated selector enters Checked");
            check(ui_node_set_value(choice.nativeHandle(),0)==UI_OK,"Clear selection from code");
            app.poll(0);
            check(frame->opacity==0.8,"Clearing value restores Unchecked appearance");
        }
        for (const std::string kind : { "Slider", "SpinBox", "ProgressBar" }) {
            const bool slider = kind == "Slider";
            const std::string sliderFace = "<ControlTemplate xmlns='urn:sunfleck:v1' Class='Example.RangeFace' Namespace='urn:test' TargetType='" + kind + "'>"
                "<StackPanel><ProgressBar Id='track' Value='{TemplateBinding Value}'/><TextBlock Id='label' Text='{TemplateBinding Value}'/></StackPanel></ControlTemplate>";
            entry = {"slider-face.fleck",sliderFace.data(),sliderFace.size()};
            check(ui_app_register_fleck_resources(app.nativeHandle(),&entry,1,&diagnostic)==UI_OK,"Register slider template");
            const std::string range_page = "<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><" + kind + " Id='range' Value='34' Template='t:RangeFace'/></Window>";
            app.refreshFleck(range_page);
            app.poll(0);
            auto range = app.findFleckNode("range");
            ui_node *track{}, *label{};
            check(ui_node_get_component(range.nativeHandle(),&owner)==UI_OK && ui_component_find_node(owner,"track",&track)==UI_OK &&
                  ui_component_find_node(owner,"label",&label)==UI_OK,"Locate range template parts");
            check(track->value==34 && label->display_text()==L"34","Initial host value reaches template");
            check(ui_node_set_value(range.nativeHandle(),61)==UI_OK && track->value==61 && label->display_text()==L"61","Setting host value updates both parts");
            if (kind == "ProgressBar") {
                check(ui_node_focus(range.nativeHandle())==UI_ERROR_INVALID_STATE,"ProgressBar template remains display-only");
                continue;
            }
            range.focus();
            check(ui_host_key_event(app.nativeHandle(),UI_HOST_RIGHT,0)==UI_OK && track->value==(slider ? 66 : 62) && label->display_text()==(slider ? L"66" : L"62"),"Keyboard updates range and template");
            const auto range_box=range.nativeHandle()->visible;
            const int rx=range_box.right-1, ry=(range_box.top+range_box.bottom)/2;
            check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,rx,ry)==UI_OK &&
                  ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,rx,ry)==UI_OK && track->value==(slider ? 100 : 63) && label->display_text()==(slider ? L"100" : L"63"),"Pointer updates range and template");
            const unsigned last_value = track->value;
            app.refreshFleck(range_page);
            range = app.findFleckNode("range");
            check(range.value()==last_value && ui_node_get_component(range.nativeHandle(),&owner)==UI_OK &&
                  ui_component_find_node(owner,"track",&track)==UI_OK && track->value==last_value,"Refresh retains the edited range before binding the new parts");
        }
        const std::string rowFace = "<ControlTemplate xmlns='urn:sunfleck:v1' Class='Example.Row' Namespace='urn:test' TargetType='ListItem'>"
            "<Border Id='frame' Opacity='0.8'><Border.VisualStateGroups><VisualStateGroup Name='SelectionStates'>"
            "<VisualState Name='Unselected'/><VisualState Name='Selected'>" + std::string(state_storyboard) +
            "</VisualState></VisualStateGroup></Border.VisualStateGroups><ContentPresenter/></Border></ControlTemplate>";
        entry = {"row.fleck",rowFace.data(),rowFace.size()};
        check(ui_app_register_fleck_resources(app.nativeHandle(),&entry,1,&diagnostic)==UI_OK,"Register list item template");
        for (const std::string selector : {"ListView","ComboBox"}) {
            app.refreshFleck("<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><"+selector+" Id='items'>"
                "<ListItem Id='one' Text='One' Template='t:Row'/><ListItem Id='two' Text='Two' Template='t:Row'/></"+selector+"></Window>");
            app.poll(0);
            auto items=app.findFleckNode("items");
            if (selector=="ComboBox") {
                items.focus();
                check(ui_host_key_event(app.nativeHandle(),UI_HOST_ACTIVATE,0)==UI_OK,"Open templated choices");
                app.poll(0);
            }
            ui_node *first{}, *second{};
            check(ui_node_get_component(app.findFleckNode("one").nativeHandle(),&owner)==UI_OK &&
                ui_component_find_node(owner,"frame",&first)==UI_OK &&
                ui_node_get_component(app.findFleckNode("two").nativeHandle(),&owner)==UI_OK &&
                ui_component_find_node(owner,"frame",&second)==UI_OK,"Find both item visuals");
            check(first->opacity==1 && second->opacity==0.8,"Initial selection reaches item templates");
            const auto box=app.findFleckNode("two").nativeHandle()->visible;
            check(box.right>box.left && box.bottom>box.top,"Second choice visible");
            const int x=(box.left+box.right)/2, y=(box.top+box.bottom)/2;
            check(ui_host_pointer(app.nativeHandle(),UI_POINTER_DOWN,x,y)==UI_OK &&
                ui_host_pointer(app.nativeHandle(),UI_POINTER_UP,x,y)==UI_OK && items.value()==1,"Templated item selects its parent index");
            app.poll(0);
            check(first->opacity==0.8 && second->opacity==1,"Selection leaves previous item and enters clicked item");
            app.refreshFleck("<Window xmlns='urn:sunfleck:v1' xmlns:t='urn:test'><"+selector+" Id='items'>"
                "<ListItem Id='new' Text='New'/><ListItem Id='one' Text='One' Template='t:Row'/>"
                "<ListItem Id='two' Text='Two' Template='t:Row'/></"+selector+"></Window>");
            app.poll(0);
            check(app.findFleckNode("items").value()==2 && ui_node_get_component(app.findFleckNode("two").nativeHandle(),&owner)==UI_OK &&
                ui_component_find_node(owner,"frame",&second)==UI_OK && second->opacity==1,"Refresh preserves selected item identity after insertion");
        }
        sunfleck::App docked(UI_SOFTWARE,true);
        docked.loadFleck("<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockGroup>"
            "<DockDocument Id='document' Title='Appearance'><CheckBox Id='choice' Opacity='0.8'/></DockDocument>"
            "</DockGroup></DockHost></Window>");
        auto dock_choice = docked.findFleckNode("choice");
        dock_choice.registerVisualState("CheckStates","Checked",state_storyboard);
        docked.advanceAnimationClock(0);
        docked.openWindow();
        docked.poll(0);
        dock_choice.setValue(1);
        docked.poll(0);
        check(dock_choice.nativeHandle()->check_state=="Checked" && dock_choice.nativeHandle()->opacity==1,
              "Dock document enters the hosted check state");
        docked.advanceAnimationClock(50);
        docked.poll(0);
        check(dock_choice.nativeHandle()->opacity==0.75,"Dock animation reaches its midpoint");
        dock_choice.setValue(0);
        docked.poll(0);
        check(dock_choice.nativeHandle()->opacity==0.8,"Dock document clears its previous state");
        sunfleck::App navigation(UI_SOFTWARE,true);
        const std::string tabPage = "<Window xmlns='urn:sunfleck:v1'><StackPanel><Button Id='last'/>"
            "<Button Id='skip' TabIndex='0' IsTabStop='false'/><Button Id='a' TabIndex='1'/>"
            "<Button Id='b' TabIndex='1'/></StackPanel></Window>";
        check(fleck::parse(tabPage,page,diagnostic),"Parse tab navigation");
        const auto tabBytes=fleck::compile_resource(page);
        check(ui_app_load_fleck_resource(navigation.nativeHandle(),tabBytes.data(),tabBytes.size(),&diagnostic)==UI_OK,"Load compiled tab settings");
        navigation.openWindow(); navigation.poll(0);
        std::vector<std::string> visits;
        for (auto id : {"a","b","skip","last"}) {
            check(ui_node_set_action(navigation.findFleckNode(id).nativeHandle(),+[](ui_node* n,void* p){
                static_cast<std::vector<std::string>*>(p)->push_back(n->fleck_id);
            },&visits)==UI_OK,"Attach tab observer");
        }
        for (int reverse : {0,0,0,0,1}) {
            check(ui_host_key_event(navigation.nativeHandle(),UI_HOST_TAB,reverse)==UI_OK &&
                ui_host_key_event(navigation.nativeHandle(),UI_HOST_ACTIVATE,0)==UI_OK,"Navigate and activate");
        }
        check(visits==std::vector<std::string>{"a","b","last","a","last"},"Stable explicit tab order, skipped stop and reverse wrap");
        for (auto direction : {UI_HOST_DOWN,UI_HOST_UP}) {
            check(ui_host_key_event(navigation.nativeHandle(),direction,0)==UI_OK &&
                ui_host_key_event(navigation.nativeHandle(),UI_HOST_ACTIVATE,0)==UI_OK,"Move focus vertically");
        }
        check(visits[5]=="a" && visits[6]=="last","Arrow keys follow screen positions and skip excluded stops");
        navigation.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='outer' IsFocusScope='true'>"
            "<Button Id='outside'/><StackPanel Id='inner' IsFocusScope='true'><Button Id='inside'/></StackPanel></StackPanel></Window>");
        navigation.poll(0);
        auto outer = navigation.findFleckNode("outer"), inner = navigation.findFleckNode("inner");
        auto outside = navigation.findFleckNode("outside"), inside = navigation.findFleckNode("inside");
        outside.focus();
        inside.focus();
        check(outer.logicalFocus().nativeHandle()==outside.nativeHandle() &&
            inner.logicalFocus().nativeHandle()==inside.nativeHandle(),"Nested scopes retain independent logical focus");
        outer.focus();
        visits.clear();
        check(ui_node_set_action(outside.nativeHandle(),+[](ui_node* n,void* p){
            static_cast<std::vector<std::string>*>(p)->push_back(n->fleck_id);
        },&visits)==UI_OK,"Observe restored focus");
        check(ui_host_key_event(navigation.nativeHandle(),UI_HOST_ACTIVATE,0)==UI_OK &&
            visits==std::vector<std::string>{"outside"},"Focus scope restores remembered target");
        inside.setPropertyValue(UI_PROPERTY_VISIBILITY,state(UI_COLLAPSED));
        check(inner.logicalFocus().nativeHandle()==nullptr && ui_node_focus(inner.nativeHandle())==UI_ERROR_INVALID_STATE,"Unavailable logical target is skipped");
        navigation.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='outer' IsFocusScope='true'>"
            "<Button Id='outside'/><StackPanel Id='inner' IsFocusScope='true'><TextBlock Text='Replaced'/></StackPanel></StackPanel></Window>");
        navigation.poll(0);
        check(navigation.findFleckNode("outer").logicalFocus().nativeHandle()==navigation.findFleckNode("outside").nativeHandle() &&
            navigation.findFleckNode("inner").logicalFocus().nativeHandle()==nullptr,"Refresh preserves live logical focus and drops retired targets");
        const std::string routedPage = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='parent'><StackPanel.InputBindings><KeyBinding Gesture='Ctrl+S' Command='Save'/><KeyBinding Gesture='F5' Command='Save'/><KeyBinding Gesture='Shift+F24' Command='Save'/></StackPanel.InputBindings><Button Id='source' Command='{RoutedCommand Save}'/></StackPanel></Window>";
        check(fleck::parse(routedPage,page,diagnostic),"Parse routed command");
        auto routedBytes = fleck::compile_resource(page);
        sunfleck::App routed(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(routed.nativeHandle(),routedBytes.data(),routedBytes.size(),&diagnostic)==UI_OK,"Load compiled routed command");
        auto parent = routed.findFleckNode("parent"), source = routed.findFleckNode("source");
        int parentCalls = 0, localCalls = 0;
        ui_command *outerCommand{}, *innerCommand{};
        auto count = +[](ui_node*,void* context){ ++*static_cast<int*>(context); };
        check(ui_command_create(routed.nativeHandle(),count,&parentCalls,&outerCommand)==UI_OK &&
            ui_command_create(routed.nativeHandle(),count,&localCalls,&innerCommand)==UI_OK,"Create route handlers");
        parent.setCommandBinding("Save",outerCommand);
        source.setCommandBinding("Save",innerCommand);
        routed.openWindow(); routed.poll(0); routed.activateButton();
        check(localCalls==1 && parentCalls==0,"Nearest command handler wins");
        check(ui_command_destroy(innerCommand)==UI_OK,"Remove local command handler");
        routed.activateButton();
        check(localCalls==1 && parentCalls==1,"Destroyed command falls back to ancestor");
        uint32_t handled{};
        check(ui_host_key_gesture(routed.nativeHandle(),'S',UI_KEY_CTRL,&handled)==UI_OK && handled && parentCalls==2,"Compiled key gesture executes command");
        check(ui_host_key_gesture(routed.nativeHandle(),'S',UI_KEY_CTRL|UI_KEY_SHIFT,&handled)==UI_OK && !handled,"Unbound gesture remains available");
        for (auto [key,mods] : {std::pair<unsigned,unsigned>{UI_KEY_F5,0}, {UI_KEY_F24,UI_KEY_SHIFT}}) {
            int before = parentCalls;
            check(ui_host_key_gesture(routed.nativeHandle(),key,mods,&handled)==UI_OK && handled && parentCalls==before+1,"Compiled function key executes once");
        }
        for (const char* invalid : {"F0","F01","F25","F1x","Shift+A","Ctrl+Ctrl+F1"}) {
            unsigned key{}, modifiers{};
            check(!fleck::key_gesture(invalid,key,modifiers),"Reject malformed or unsupported gesture");
        }
        struct PointerLog { std::vector<ui_event_phase> phases; bool consume=false; int x=0,y=0; } pointer;
        auto handler=+[](ui_node*,ui_node*,ui_pointer_action,ui_event_phase phase,int32_t x,int32_t y,void* context) -> int {
            auto& log=*static_cast<PointerLog*>(context);
            log.phases.push_back(phase); log.x=x; log.y=y;
            return log.consume;
        };
        parent.setPointerHandler(UI_POINTER_DOWN,UI_EVENT_PREVIEW,handler,&pointer);
        source.setPointerHandler(UI_POINTER_DOWN,UI_EVENT_DIRECT,handler,&pointer);
        parent.setPointerHandler(UI_POINTER_DOWN,UI_EVENT_BUBBLE,handler,&pointer);
        const auto hit=source.nativeHandle()->visible;
        const int hx=(hit.left+hit.right)/2, hy=(hit.top+hit.bottom)/2;
        auto click=[&] {
            check(ui_host_pointer(routed.nativeHandle(),UI_POINTER_DOWN,hx,hy)==UI_OK &&
                  ui_host_pointer(routed.nativeHandle(),UI_POINTER_UP,hx,hy)==UI_OK,"Send routed pointer events");
        };
        click();
        check(pointer.phases==std::vector<ui_event_phase>{UI_EVENT_PREVIEW,UI_EVENT_DIRECT,UI_EVENT_BUBBLE} &&
              pointer.x==hx && pointer.y==hy && parentCalls==5,"Routed pointer phases, coordinates and default activation");
        pointer.consume=true; pointer.phases.clear(); click();
        check(pointer.phases==std::vector<ui_event_phase>{UI_EVENT_PREVIEW} && parentCalls==5,"Handled press suppresses activation");
        parent.setPointerHandler(UI_POINTER_DOWN,UI_EVENT_PREVIEW,nullptr);
        parent.setPointerHandler(UI_POINTER_CANCEL,UI_EVENT_BUBBLE,handler,&pointer);
        pointer.consume=false;
        check(ui_host_pointer(routed.nativeHandle(),UI_POINTER_DOWN,hx,hy)==UI_OK,"Begin cancellable press");
        pointer.phases.clear();
        check(ui_host_pointer(routed.nativeHandle(),UI_POINTER_CANCEL,-1,-1)==UI_OK &&
              ui_host_pointer(routed.nativeHandle(),UI_POINTER_CANCEL,-1,-1)==UI_OK &&
              ui_host_pointer(routed.nativeHandle(),UI_POINTER_UP,hx,hy)==UI_OK &&
              pointer.phases==std::vector<ui_event_phase>{UI_EVENT_BUBBLE} && parentCalls==5,
              "Cancel reaches the pressed control once outside its bounds and suppresses the click");
        const std::string pointerPage="<Window xmlns='urn:sunfleck:v1'><StackPanel Id='panel'><StackPanel.InputBindings>"
            "<PointerBinding Action='Down' Phase='Preview' Command='Press'/></StackPanel.InputBindings>"
            "<Button Id='source' Content='Press me'/></StackPanel></Window>";
        check(fleck::parse(pointerPage,page,diagnostic),"Parse Fleck pointer binding");
        const auto pointerBytes=fleck::compile_resource(page);
        sunfleck::App input(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(input.nativeHandle(),pointerBytes.data(),pointerBytes.size(),&diagnostic)==UI_OK,"Load binary pointer binding");
        ui_command* press{};
        int presses=0, defaults=0;
        check(ui_command_create(input.nativeHandle(),count,&presses,&press)==UI_OK,"Create pointer command");
        input.findFleckNode("panel").setCommandBinding("Press",press);
        auto target=input.findFleckNode("source");
        check(ui_node_set_action(target.nativeHandle(),count,&defaults)==UI_OK,"Observe default click");
        input.openWindow(); input.poll(0);
        auto tb=target.nativeHandle()->visible;
        const int tx=(tb.left+tb.right)/2, ty=(tb.top+tb.bottom)/2;
        check(ui_host_pointer(input.nativeHandle(),UI_POINTER_DOWN,tx,ty)==UI_OK &&
            ui_host_pointer(input.nativeHandle(),UI_POINTER_UP,tx,ty)==UI_OK && presses==1 && defaults==0,"PointerBinding executes ancestor command once");
        auto panel = input.findFleckNode("panel");
        panel.setInputBinding(UI_KEY_F6,0,"Press");
        target.setRoutedCommand("Press");
        const std::string updatedInput = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='panel'><StackPanel.InputBindings>"
            "<KeyBinding Gesture='F7' Command='Press'/></StackPanel.InputBindings><Button Id='source'/></StackPanel></Window>";
        input.refreshFleck(updatedInput);
        input.poll(0);
        target.focus();
        for (auto key : {UI_KEY_F6,UI_KEY_F7})
            check(ui_host_key_gesture(input.nativeHandle(),key,0,&handled)==UI_OK && handled,"Refresh keeps local shortcut and installs Fleck shortcut");
        input.activateButton();
        check(presses==4,"Refreshed source and local command handler route all three activations");
        input.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='panel'><Button Id='source'/></StackPanel></Window>");
        input.poll(0);
        target.focus();
        check(ui_host_key_gesture(input.nativeHandle(),UI_KEY_F7,0,&handled)==UI_OK && !handled &&
              ui_host_key_gesture(input.nativeHandle(),UI_KEY_F6,0,&handled)==UI_OK && handled && presses==5,
              "Removing a Fleck shortcut during refresh leaves the code registration active");
        check(ui_command_destroy(press)==UI_OK &&
              ui_host_key_gesture(input.nativeHandle(),UI_KEY_F6,0,&handled)==UI_OK && !handled,
              "Destroying the retained command disconnects its shortcut");
        const std::string phasePage = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='outer'><StackPanel.InputBindings>"
            "<KeyBinding Gesture='F8' Phase='Preview' Command='PreviewSave'/></StackPanel.InputBindings>"
            "<Button Id='inner'><Button.InputBindings><KeyBinding Gesture='F8' Phase='Direct' Command='DirectSave'/>"
            "<KeyBinding Gesture='F8' Command='BubbleSave'/></Button.InputBindings></Button></StackPanel></Window>";
        check(fleck::parse(phasePage,page,diagnostic),"Parse phased key bindings");
        auto phaseBytes=fleck::compile_resource(page);
        sunfleck::App phaseApp(UI_SOFTWARE,true);
        check(ui_app_load_fleck_resource(phaseApp.nativeHandle(),phaseBytes.data(),phaseBytes.size(),&diagnostic)==UI_OK,"Load compiled phased gestures");
        auto outerScope=phaseApp.findFleckNode("outer"), innerSource=phaseApp.findFleckNode("inner");
        int counts[3]{};
        ui_command* commands[3]{};
        const char* names[]{"PreviewSave","DirectSave","BubbleSave"};
        for (int i=0;i<3;++i) {
            check(ui_command_create(phaseApp.nativeHandle(),count,&counts[i],&commands[i])==UI_OK,"Create key command");
            outerScope.setCommandBinding(names[i],commands[i]);
        }
        phaseApp.openWindow(); phaseApp.poll(0); innerSource.focus();
        for (int i=0;i<3;++i) {
            check(ui_host_key_gesture(phaseApp.nativeHandle(),UI_KEY_F8,0,&handled)==UI_OK && handled &&
                counts[i]==1 && counts[0]+counts[1]+counts[2]==i+1,"First matching key phase handles the gesture exactly once");
            (i == 0 ? outerScope : innerSource).setInputBinding(UI_KEY_F8,0,static_cast<ui_event_phase>(i),nullptr);
        }
        std::cout << "Visibility layout, enum binding, keyboard/pointer and IME lifecycle passed\n";
    } catch(const std::exception& e) { std::cerr << e.what()<<'\n'; return 1; }
}
