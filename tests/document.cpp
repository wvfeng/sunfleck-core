#include <sunfleck/ui.hpp>
#include "document.h"
#include "ui_tree.h"
#include "fleck.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <cstdio>
#undef assert
#define assert(test) do { if(!(test)) { std::fprintf(stderr,"Assertion at line %d: %s\n",__LINE__,#test); return 1; } } while(0)

extern "C" ui_result load_document(ui_app* app, ui_fleck_diagnostic* diagnostic);
int main() {
    const ui_app_options options{sizeof(options),UI_API_VERSION,UI_SOFTWARE,UI_HEADLESS};
    ui_app* app=nullptr;assert(ui_app_create(&options,&app)==UI_OK);
    const char page[]=R"(<Window xmlns="urn:sunfleck:v1" Width="600" Height="400"><Grid><DocumentView Id="article"/></Grid></Window>)";
    ui_fleck_diagnostic diagnostic{};assert(ui_app_load_fleck(app,page,strlen(page),&diagnostic)==UI_OK);
    ui_node* node=nullptr;assert(ui_app_find_fleck_node(app,"article",&node)==UI_OK);
    const std::string json=R"({"version":1,"blocks":[{"kind":"heading","id":"start","level":1,"spans":[{"text":"Heading"}]},{"kind":"paragraph","spans":[{"text":"A clickable link","href":"#/target"}]},{"kind":"code","language":"cpp","raw":"int x = 1;\n","spans":[{"text":"int x = 1;\n","flags":4}]},{"kind":"paragraph","spans":[{"text":")"+std::string(2000,'x')+R"("}]},{"kind":"heading","id":"end","level":2,"spans":[{"text":"End"}]}]})";
    assert(ui_document_load(node,json.data(),json.size())==UI_OK);
    assert(ui_app_open_window(app)==UI_OK);assert(ui_app_poll(app,0)==UI_OK);
    assert(node->document->height>400);const auto original=node->document;
    assert(ui_document_load(node,"{}",2)==UI_ERROR_INVALID_ARGUMENT);assert(node->document==original);
    assert(ui_document_load(node,"{\"version\":2,\"blocks\":[]}",25)==UI_ERROR_INVALID_ARGUMENT);
    ui_result wrong=UI_OK;std::thread worker([&]{wrong=ui_document_set_scroll(node,10);});worker.join();assert(wrong==UI_ERROR_WRONG_THREAD);
    assert(ui_document_scroll_to(node,"missing")==UI_ERROR_INVALID_ARGUMENT);
    assert(ui_document_scroll_to(node,"end")==UI_OK);ui_app_poll(app,0);int32_t offset=0;assert(ui_document_get_scroll(node,&offset)==UI_OK&&offset>0);
    assert(ui_document_set_scroll(node,0)==UI_OK);ui_app_poll(app,0);
    auto glyph=std::find_if(node->document->glyphs.begin(),node->document->glyphs.end(),[](const auto& g){return !g.href.empty();});assert(glyph!=node->document->glyphs.end());
    const int x=node->bounds.left+glyph->bounds.left+1,y=node->bounds.top+glyph->bounds.top+2;
    assert(ui_host_pointer(app,UI_POINTER_DOWN,x,y)==UI_OK);assert(ui_host_pointer(app,UI_POINTER_UP,x,y)==UI_OK);
    ui_document_event event=UI_DOCUMENT_NONE;size_t size=0;
    assert(ui_document_take_event(node,&event,nullptr,&size)==UI_OK&&event==UI_DOCUMENT_LINK);
    std::string value(size,'\0');assert(ui_document_take_event(node,&event,value.data(),&size)==UI_OK);assert(value.c_str()==std::string("#/target"));
    assert(ui_document_take_event(node,&event,nullptr,&size)==UI_OK&&event==UI_DOCUMENT_NONE);
    assert(ui_document_select_all(node)==UI_OK);assert(ui_document_selection(node,nullptr,&size)==UI_OK);value.resize(size);assert(ui_document_selection(node,value.data(),&size)==UI_OK);assert(value.find("clickable link")!=std::string::npos);
    const auto old_height=node->document->height;assert(ui_host_resize(app,300,400,96)==UI_OK);ui_app_poll(app,0);assert(node->document->height>old_height);
    // Local image pixels and independent horizontal scrolling share the same
    // clip/geometry path as the visible document.
    const std::string rich=R"({"version":1,"blocks":[{"kind":"image","image":{"width":2,"height":2,"rows":["ff0000ff0000","ff0000ff0000"]},"spans":[]},{"kind":"table","spans":[{"text":"Column A\tColumn B\nA\tB\n","flags":4}]},{"kind":"code","raw":"copy exact\n","spans":[{"text":")"+std::string(200,'x')+R"(","flags":4}]}]})";
    assert(ui_document_load(node,rich.data(),rich.size())==UI_OK);ui_app_poll(app,0);
    const auto image_box=node->document->boxes[0].bounds;
    uint32_t width=0,height=0;const auto* pixels=ui_host_pixels(app,&width,&height);assert(pixels);
    const size_t pixel=(size_t(node->bounds.top+image_box.top)*width+node->bounds.left+image_box.left)*4;
    assert(pixels[pixel]==255&&pixels[pixel+1]==0&&pixels[pixel+2]==0);
    const auto code_box=node->document->boxes.back();
    const int cx=node->bounds.left+40,cy=node->bounds.top+code_box.bounds.top+5;
    assert(ui_document_scroll_horizontal(node,cx,cy,100)==UI_OK);assert(node->document->horizontal[code_box.block]==100);
    document::pointer(*node,0,cx,cy);document::pointer(*node,2,cx,cy);
    assert(ui_document_take_event(node,&event,nullptr,&size)==UI_OK&&event==UI_DOCUMENT_COPY);value.resize(size);ui_document_take_event(node,&event,value.data(),&size);assert(std::string(value.c_str())=="copy exact\n");
#ifdef _WIN32
    const std::string bidi=R"({"version":1,"blocks":[{"kind":"paragraph","spans":[{"text":"e"},{"text":"\u0301 mixed \u05d0\u05d1\u05d2 \ud83d\udc69\u200d\ud83d\udcbb text"}]}]})";
    assert(ui_document_load(node,bidi.data(),bidi.size())==UI_OK);
    ui_binding_value direction{};direction.kind=UI_VALUE_ENUM;direction.integer=UI_FLOW_RIGHT_TO_LEFT;
    assert(ui_node_set_property(node,UI_PROPERTY_FLOW_DIRECTION,&direction)==UI_OK);ui_app_poll(app,0);
    assert(node->document->paragraphs.size()==1);
    assert(std::none_of(node->document->glyphs.begin(),node->document->glyphs.end(),[](const auto& g){return g.index==1;}));
    auto rtl=std::find_if(node->document->glyphs.begin(),node->document->glyphs.end(),[](const auto& g){return g.rtl && g.bounds.right-g.bounds.left>2;});
    assert(rtl!=node->document->glyphs.end());
    document::pointer(*node,0,node->bounds.left+rtl->bounds.right-1,node->bounds.top+rtl->bounds.top+(rtl->bounds.bottom-rtl->bounds.top)/2);
    assert(node->document->first==rtl->index);
    document::pointer(*node,3,0,0);
    const std::string table=R"({"version":1,"blocks":[{"kind":"table","spans":[{"text":"e"},{"text":"\u0301\t\u05d0\u05d1\u05d2\nWide ","href":"#/cell"},{"text":"\ud83d\udc69\u200d\ud83d\udcbb\tLast cell without newline","flags":1}]}]})";
    assert(ui_document_load(node,table.data(),table.size())==UI_OK);ui_app_poll(app,0);
    const auto& table_state=*node->document;
    assert(table_state.paragraphs.size()==4);
    assert(table_state.plain==L"e\u0301\t\u05d0\u05d1\u05d2\nWide \U0001f469\u200d\U0001f4bb\tLast cell without newline\n");
    assert(std::none_of(table_state.glyphs.begin(),table_state.glyphs.end(),[](const auto& g){return g.index==1;}));
    auto cell=table_state.paragraphs.begin();
    const auto first_cell=cell++->second.bounds,second_cell=cell++->second.bounds,third_cell=cell++->second.bounds;
    assert(first_cell.top==second_cell.top && third_cell.top>first_cell.top);
    assert(second_cell.left>first_cell.right && third_cell.left==first_cell.left);
    const auto table_rtl=std::find_if(table_state.glyphs.begin(),table_state.glyphs.end(),[](const auto& g){return g.rtl && g.bounds.right-g.bounds.left>2;});
    assert(table_rtl!=table_state.glyphs.end());
    document::pointer(*node,0,node->bounds.left+table_rtl->bounds.right-1,node->bounds.top+table_rtl->bounds.top+2);
    assert(table_state.first==table_rtl->index);
    document::pointer(*node,3,0,0);
    assert(ui_document_select_all(node)==UI_OK);
    assert(document::selected(table_state).find("Last cell without newline")!=std::string::npos);
    // Horizontal scrolling moves all cells as one block, including hit regions.
    assert(ui_document_scroll_horizontal(node,node->bounds.left+40,node->bounds.top+first_cell.top+2,40)==UI_OK);
    assert(table_state.horizontal.at(0)>0);ui_app_poll(app,0);
    const auto shift=table_state.horizontal.at(0);
    document::pointer(*node,0,node->bounds.left+table_rtl->bounds.right-1-shift,node->bounds.top+table_rtl->bounds.top+2);
    assert(table_state.first==table_rtl->index);
    document::pointer(*node,3,0,0);
#endif
    ui_app_destroy(app);
    assert(ui_app_create(&options,&app)==UI_OK);assert(load_document(app,&diagnostic)==UI_OK);assert(ui_app_find_fleck_node(app,"article",&node)==UI_OK);ui_app_destroy(app);
    // The new node also survives generated/binary Fleck paths.
    fleck::Page parsed;assert(fleck::parse(page,parsed,diagnostic));
    auto binary=fleck::compile_resource(parsed);assert(binary[5]==UI_FLECK_RESOURCE_VERSION);
    assert(ui_app_create(&options,&app)==UI_OK);assert(ui_app_load_fleck_resource(app,binary.data(),binary.size(),&diagnostic)==UI_OK);
    assert(ui_app_find_fleck_node(app,"article",&node)==UI_OK);assert(node->kind==UI_DOCUMENT_VIEW);ui_app_destroy(app);
    binary[5]=5;assert(ui_app_create(&options,&app)==UI_OK);assert(ui_app_load_fleck_resource(app,binary.data(),binary.size(),&diagnostic)==UI_ERROR_VERSION_MISMATCH);ui_app_destroy(app);
    fleck::Page legacy;assert(fleck::parse("<Window xmlns='urn:sunfleck:v1'><Grid><TextBlock/></Grid></Window>",legacy,diagnostic));
    binary=fleck::compile_resource(legacy);binary[5]=5;
    assert(ui_app_create(&options,&app)==UI_OK);assert(ui_app_load_fleck_resource(app,binary.data(),binary.size(),&diagnostic)==UI_ERROR_VERSION_MISMATCH);ui_app_destroy(app);
}
