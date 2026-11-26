#include <sunfleck/ui.h>
#include <cstring>

static int start_payload(ui_node* node, ui_node*, ui_pointer_action, ui_event_phase, int32_t, int32_t, void* context)
{
    auto* data=static_cast<ui_drop_data*>(context);
    return ui_node_start_drag(node,data)==UI_OK;
}

static int accept_payload(ui_node* node,const ui_drop_data* data,void*)
{
    if (data->kind==UI_DROP_OBJECT && data->object==42)
        return ui_node_set_text(node,"object 42")==UI_OK;
    if (data->kind==UI_DROP_FILES && data->count==2 &&
        !std::strcmp(data->paths[0],"C:/Sunfleck/one.txt") && !std::strcmp(data->paths[1],"C:/Sunfleck/two.txt"))
        return ui_node_set_text(node,"two paths")==UI_OK;
    return 0;
}

int main()
{
    constexpr char page[] = R"(<Window xmlns="urn:sunfleck:v1" Title="Sunfleck UIA acceptance" Width="480" Height="560">
      <StackPanel>
        <Button Id="apply" Content="Apply" DragText="dropped text"/>
        <CheckBox Id="enabled" Text="Enabled"/>
        <TextBox Id="editor" Text="initial" AllowDrop="true"/>
        <PasswordBox Id="secret" Text="sensitive"/>
        <Slider Id="level" Value="15"/>
        <ComboBox Id="choices"><ListItem Text="first"/><ListItem Text="second"/></ComboBox>
        <Button Id="fileDrag" Content="Drag two paths"/>
        <Button Id="objectDrag" Content="Drag object"/>
        <Button Id="dropTarget" Content="Drop here"/>
      </StackPanel></Window>)";
    const ui_app_options options{sizeof(options), UI_API_VERSION, UI_SOFTWARE, 0};
    ui_app* app=nullptr;
    if (ui_app_create(&options,&app)!=UI_OK) return 1;
    auto status=ui_app_load_fleck(app,page,std::strlen(page),nullptr);
    const char* paths[]={"C:/Sunfleck/one.txt","C:/Sunfleck/two.txt"};
    ui_drop_data files{UI_DROP_FILES,nullptr,paths,2};
    ui_drop_data object{UI_DROP_OBJECT,nullptr,nullptr,0,app,42};
    ui_node *file_node=nullptr,*object_node=nullptr,*target=nullptr;
    if(status==UI_OK) status=ui_app_find_fleck_node(app,"fileDrag",&file_node);
    if(status==UI_OK) status=ui_app_find_fleck_node(app,"objectDrag",&object_node);
    if(status==UI_OK) status=ui_app_find_fleck_node(app,"dropTarget",&target);
    if(status==UI_OK) status=ui_node_set_pointer_handler(file_node,UI_POINTER_DOWN,UI_EVENT_DIRECT,start_payload,&files);
    if(status==UI_OK) status=ui_node_set_pointer_handler(object_node,UI_POINTER_DOWN,UI_EVENT_DIRECT,start_payload,&object);
    if(status==UI_OK) status=ui_node_set_drop_handler(target,UI_DROP_FILES|UI_DROP_OBJECT,accept_payload,nullptr);
    if(status==UI_OK) status=ui_app_open_window(app);
    if(status==UI_OK) status=ui_app_run(app);
    ui_app_destroy(app);
    return status==UI_EXITED ? 0 : 1;
}

