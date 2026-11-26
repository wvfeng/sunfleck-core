#include <sunfleck/ui.hpp>
#include <sunfleck/elements.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include <iostream>
#include <limits>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static ui_binding_value color(uint32_t argb) { ui_binding_value v{}; v.kind = UI_VALUE_COLOR; v.integer = argb; return v; }
static ui_binding_value geometry(ui_value_kind kind, const double* data) { ui_binding_value v{}; v.kind = kind; v.geometry = data; return v; }

static void pixels_and_layout() {
    BorderStyle s; s.background = 0xff102030; s.brush = 0xffaabbcc; s.thickness = {2,3,4,5};
    std::vector<FillRectCommand> commands;
    border::paint(s, {0,0,30,30}, {0,0,30,30}, 96, commands);
    auto pixel = [&](int x, int y) { std::array<uint8_t,4> result{}; for (auto& c : commands)
        if (x >= c.left && x < c.right && y >= c.top && y < c.bottom) result = c.color; return result; };
    check(pixel(0,15) == std::array<uint8_t,4>{170,187,204,255}, "Left border color");
    check(pixel(2,3) == std::array<uint8_t,4>{16,32,48,255}, "Asymmetric content starts at thickness");
    check(pixel(26,10)[0] == 170 && pixel(10,25)[0] == 170, "Right and bottom thickness");
    s.radius = {10,0,8,0}; commands.clear();
    border::paint(s, {0,0,30,30}, {0,0,30,30}, 96, commands);
    check(pixel(0,0)[3] == 0 && pixel(29,0)[3] == 255 && pixel(29,29)[3] == 0, "Independent rounded corners");
    check(pixel(3,2)[3] > 0 && pixel(3,2)[3] < 255, "Rounded edge coverage");
    s.radius = {}; s.background = 0x80102030; s.brush = 0x40aabbcc; commands.clear();
    border::paint(s, {0,0,30,30}, {0,0,30,30}, 96, commands);
    check(pixel(10,10)[3] == 128 && pixel(0,10)[3] == 64, "Alpha preserved without double painting border");
    commands.clear(); border::paint(s, {0,0,30,30}, {5,6,15,16}, 96, commands);
    for (const auto& c : commands) check(c.left >= 5 && c.top >= 6 && c.right <= 15 && c.bottom <= 16, "Clipped spans");
    s.thickness = {100,100,100,100}; commands.clear();
    border::paint(s, {0,0,30,30}, {0,0,30,30}, 96, commands);
    check(pixel(15,15)[3] == 64, "Oversized thickness has no negative inner region");

    ui_node root{nullptr,UI_BORDER}, child{nullptr,UI_TEXT_BLOCK};
    root.children = {&child}; child.parent = &root; root.padding = 2; root.border.thickness = {2,4,6,8};
    child.layout.width = 20; child.height = 10; child.layout.horizontal = 1; child.layout.horizontal_set = true;
    auto style = builtin_palette(UI_BUILTIN_FLUENT_2,false,false); auto shape = builtin_themes[0].shape;
    std::vector<ui_node*> interactive, scrollable;
    for (unsigned dpi : {96u,144u,192u}) {
        commands.clear(); render_tree(root,{0,0,300,200},style,shape,dpi,nullptr,false,false,nullptr,nullptr,commands,interactive,scrollable);
        check(child.bounds.left == root.bounds.left + int(4*dpi/96) && child.bounds.top == root.bounds.top + int(6*dpi/96), "DPI scales padding and border content inset");
        auto invalidations = root.layout_cache.invalidations; root.border.thickness[0] += 2;
        render_tree(root,{0,0,300,200},style,shape,dpi,nullptr,false,false,nullptr,nullptr,commands,interactive,scrollable);
        check(root.layout_cache.invalidations > invalidations, "Thickness invalidates cached measurement"); root.border.thickness[0] -= 2;
    }
}

int main() {
    try {
        pixels_and_layout();
        sunfleck::App app(UI_SOFTWARE,true);
        sunfleck::elements::Border typed(app); auto node = typed.node();
        check(node.propertyValue(UI_PROPERTY_BACKGROUND).integer == 0 && node.propertyValue(UI_PROPERTY_BORDER_BRUSH).integer == 0xff000000, "Border defaults");
        node.setPropertyValue(UI_PROPERTY_BACKGROUND,color(0xff102030));
        node.setPropertyLayer(UI_PROPERTY_BACKGROUND,UI_PRECEDENCE_ANIMATION,color(0xff203040));
        check(node.propertyValue(UI_PROPERTY_BACKGROUND).integer == 0xff203040, "Animation layer overrides local");
        node.clearPropertyLayer(UI_PROPERTY_BACKGROUND,UI_PRECEDENCE_ANIMATION);
        check(node.propertyValue(UI_PROPERTY_BACKGROUND).integer == 0xff102030, "Removing animation reveals base");
        double thickness[]{1,2,3,4}; node.setPropertyValue(UI_PROPERTY_BORDER_THICKNESS,geometry(UI_VALUE_THICKNESS,thickness));
        thickness[0] = -1;
        auto invalid = geometry(UI_VALUE_THICKNESS,thickness);
        check(ui_node_set_property(node.nativeHandle(),UI_PROPERTY_BORDER_THICKNESS,&invalid) != UI_OK && node.propertyValue(UI_PROPERTY_BORDER_THICKNESS).geometry[0] == 1, "Invalid geometry preserves value");
        thickness[0] = std::numeric_limits<double>::infinity();
        check(ui_node_set_property(node.nativeHandle(),UI_PROPERTY_BORDER_THICKNESS,&invalid) != UI_OK, "Nonfinite thickness rejected");
        node.clearProperty(UI_PROPERTY_BACKGROUND);
        check(node.propertySource(UI_PROPERTY_BACKGROUND) == UI_PROPERTY_SOURCE_DEFAULT, "Clear local restores default source");
        auto first = app.createNode(UI_TEXT_BLOCK), second = app.createNode(UI_TEXT_BLOCK); node.append(first);
        check(ui_node_append(node.nativeHandle(),second.nativeHandle()) != UI_OK && !second.nativeHandle()->parent, "Second child rejected atomically");

        ui_color fill{0xff102030}; sunfleck::PropertyModel model(app);
        model.property("Fill",[&]{return fill;}); model.bind(node,UI_PROPERTY_BACKGROUND,"Fill",UI_BIND_ONE_WAY);
        fill.argb = 0xffabcdef; model.notify("Fill");
        check(node.propertyValue(UI_PROPERTY_BACKGROUND).integer == fill.argb && node.propertySource(UI_PROPERTY_BACKGROUND) == UI_PROPERTY_SOURCE_BINDING, "Typed color binding notification");
        const std::string xml = "<Window xmlns='urn:sunfleck:v1'><Border Id='frame' Background='#80402010' BorderBrush='#ff123456' BorderThickness='1,2,3,4' CornerRadius='5,6,7,8' Padding='2'><TextBlock Text='Content'/></Border></Window>";
        fleck::Page page; ui_fleck_diagnostic diagnostic{};
        check(fleck::parse(xml,page,diagnostic), "Border Fleck parses");
        const auto bytes = fleck::compile_resource(page);
        app.loadFleck(xml); auto frame = app.findFleckNode("frame");
        check(frame.propertyValue(UI_PROPERTY_CORNER_RADIUS).geometry[3] == 8, "XML appearance applied");
        check(ui_app_load_fleck_resource(app.nativeHandle(),bytes.data(),bytes.size(),&diagnostic) == UI_OK, "Compiled Border loads");
        frame = app.findFleckNode("frame");
        check(frame.propertyValue(UI_PROPERTY_BACKGROUND).integer == 0x80402010 && frame.propertyValue(UI_PROPERTY_BORDER_THICKNESS).geometry[2] == 3, "Compiled appearance parity");
        const std::string bad = "<Window xmlns='urn:sunfleck:v1'><Border><TextBlock/><TextBlock/></Border></Window>";
        check(ui_app_load_fleck(app.nativeHandle(),bad.data(),bad.size(),&diagnostic) != UI_OK && app.findFleckNode("frame").nativeHandle() == frame.nativeHandle(), "Invalid refresh retains page");
        std::cout << "Border geometry, rendering, properties, binding and resource tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
