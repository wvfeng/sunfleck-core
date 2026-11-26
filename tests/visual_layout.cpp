#include <sunfleck/ui.hpp>
#include <sunfleck/property_model.hpp>
#include "../src/ui_tree.h"
#include "../src/fleck.h"
#include "../src/dock.h"
#include "../src/dwrite_text.h"
#include <iostream>
#include <cmath>
#include <limits>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void grid_tracks() {
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><Grid Id='grid' RowDefinitions='20,*' ColumnDefinitions='Auto,2*,*' Padding='0' Spacing='2'><Border Id='auto' Width='30' Height='20'/><Border Id='span' Grid.Row='1' Grid.Column='1' Grid.ColumnSpan='2'/></Grid></Window>";
    fleck::Page page; ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml,page,diagnostic),diagnostic.message);
    const auto resource = fleck::compile_resource(page);
    for (bool binary : {false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&diagnostic)==UI_OK,diagnostic.message);
        else app.loadFleck(xml);
        app.openWindow();
        auto grid = app.findFleckNode("grid"), span = app.findFleckNode("span");
        for (unsigned dpi : {96u,144u,192u}) {
            require(ui_host_resize(app.nativeHandle(),600,300,dpi)==UI_OK,"Grid resize");
            app.poll(0);
            const auto bounds = grid.nativeHandle()->bounds, cell = span.nativeHandle()->bounds;
            require(cell.left==bounds.left+int(32*dpi/96) && cell.top==bounds.top+int(22*dpi/96) &&
                    cell.right==bounds.right && cell.bottom==bounds.bottom,"2D grid auto, star and span geometry");
        }
        grid.setGridTracks({{UI_GRID_STAR,1}},{{UI_GRID_PIXELS,16},{UI_GRID_STAR,1}});
        span.setGridCell(0,1);
        app.poll(0);
        require(span.nativeHandle()->bounds.left-grid.nativeHandle()->bounds.left==36,"Code updates remeasure Grid");
        const ui_grid_track invalid{UI_GRID_STAR,0};
        require(ui_grid_set_tracks(grid.nativeHandle(),nullptr,0,&invalid,1)==UI_ERROR_INVALID_ARGUMENT &&
                grid.nativeHandle()->grid.columns.size()==2,"Invalid tracks preserve both axes");
    }
}
static void shared_grid_sizes() {
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel Spacing='0'>"
        "<Grid Id='first' ColumnDefinitions='Auto@Label,*' RowDefinitions='Auto@Field' Spacing='0'><Border Width='18' Height='10'/><Border Id='one' Grid.Column='1'/></Grid>"
        "<Grid Id='second' ColumnDefinitions='Auto@Label,*' RowDefinitions='Auto@Field' Spacing='0'><Border Id='wide' Width='42.5' Height='21'/><Border Id='two' Grid.Column='1'/></Grid>"
        "</StackPanel></Window>";
    fleck::Page page;
    ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml, page, diagnostic), diagnostic.message);
    const auto resource = fleck::compile_resource(page);
    for (bool binary : {false, true}) {
        sunfleck::App app(UI_SOFTWARE, true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(), resource.data(), resource.size(), &diagnostic) == UI_OK, diagnostic.message);
        else app.loadFleck(xml);
        app.openWindow();
        auto first = app.findFleckNode("first"), second = app.findFleckNode("second");
        auto one = app.findFleckNode("one"), two = app.findFleckNode("two");
        for (unsigned dpi : {96u,144u,192u}) {
            require(ui_host_resize(app.nativeHandle(),600,300,dpi) == UI_OK, "Shared grid resize");
            app.poll(0);
            auto bounds = one.nativeHandle()->layout_cache.arranged;
            require(bounds.left - first.nativeHandle()->bounds.left == 42.5*dpi/96 &&
                    bounds.left == two.nativeHandle()->layout_cache.arranged.left, "Shared column uses largest member");
            require(bounds.bottom - bounds.top == 21.*dpi/96, "Shared row uses largest member");
        }
        second.setSharedSizeGroup(UI_HORIZONTAL, 0, "");
        app.poll(0);
        require(one.nativeHandle()->bounds.left-first.nativeHandle()->bounds.left == 36, "Removing member shrinks group");
    }
}

static void sibling_z_order() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Grid><Button Id='a' Panel.ZIndex='2'/><Button Id='b' Panel.ZIndex='-1'/></Grid></Window>");
    app.openWindow();
    auto a = app.findFleckNode("a"), b = app.findFleckNode("b");
    auto* root = a.nativeHandle()->parent;
    auto order = [&] {
        std::vector<FillRectCommand> commands;
        std::vector<ui_node*> hits, scroll;
        render_tree(*root,{0,0,200,100},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
            builtin_themes[UI_BUILTIN_FLUENT_2].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
        return hits;
    };
    const auto hits = order();
    require(hits.size()==2 && hits.back()==a.nativeHandle(),"Higher Z sibling receives the top hit");
    const auto bounds = a.nativeHandle()->bounds;
    a.setZIndex(-1);
    require(order().back()==b.nativeHandle(),"Equal Z preserves declaration order");
    require(root->children.front()==a.nativeHandle() && a.nativeHandle()->bounds.left==bounds.left &&
            a.nativeHandle()->bounds.top==bounds.top,"Z order preserves logical children and slots");
}

static void visual_content() {
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel><ContentControl Padding='2'><ContentPresenter><Button Id='action'><StackPanel Orientation='Horizontal'><Border Id='icon' Width='16' Height='16'/><Label Text='Save'/></StackPanel></Button></ContentPresenter></ContentControl></StackPanel></Window>";
    fleck::Page page;
    ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml, page, diagnostic), diagnostic.message);
    const auto resource = fleck::compile_resource(page);
    for (bool binary : {false, true}) {
        sunfleck::App app(UI_SOFTWARE, true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(), resource.data(), resource.size(), &diagnostic) == UI_OK, diagnostic.message);
        else app.loadFleck(xml);
        app.openWindow();
        app.poll(0);
        const auto button = app.findFleckNode("action"), icon = app.findFleckNode("icon");
        require(icon.nativeHandle()->visible.right - icon.nativeHandle()->visible.left == 16 &&
                icon.nativeHandle()->visible.left >= button.nativeHandle()->visible.left, "Button presents visual content");
        try {
            app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><Button><Border/><Label/></Button></Window>");
            require(false, "Multiple visual children accepted");
        } catch (const sunfleck::Error&) {}
        require(app.findFleckNode("action").nativeHandle() == button.nativeHandle(), "Invalid content refresh preserves live tree");
    }
}

static void overflow_clip() {
    sunfleck::App app(UI_SOFTWARE, true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Canvas><Canvas Id='parent' Width='30' Height='30' ClipToBounds='false'><Button Id='button' Canvas.Left='40' Width='32' Height='18'/></Canvas></Canvas></Window>");
    app.openWindow();
    auto parent = app.findFleckNode("parent"), button = app.findFleckNode("button");
    auto paint = [&] {
        std::vector<FillRectCommand> commands;
        std::vector<ui_node*> hits, scroll;
        render_tree(*parent.nativeHandle()->parent, {0,0,100,80}, builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
            builtin_themes[UI_BUILTIN_FLUENT_2].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
        return std::find(hits.begin(), hits.end(), button.nativeHandle()) != hits.end();
    };
    require(paint() && button.nativeHandle()->visible.right > parent.nativeHandle()->bounds.right, "Overflow hit and paint extend beyond Canvas");
    ui_binding_value clip{};
    clip.kind = UI_VALUE_BOOL;
    clip.integer = 1;
    require(ui_node_set_property(parent.nativeHandle(), UI_PROPERTY_CLIP_TO_BOUNDS, &clip) == UI_OK, "Enable local clipping");
    require(!paint(), "Clipped child has no interactive entry");
}

static void canvas_offsets() {
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><Canvas Id='canvas' Padding='4'><Border Id='box' Canvas.Left='-1.5' Canvas.Top='10.25' Width='20' Height='12'/></Canvas></Window>";
    fleck::Page page;
    ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml, page, diagnostic), diagnostic.message);
    const auto resource = fleck::compile_resource(page);
    for (bool binary : {false, true}) {
        sunfleck::App app(UI_SOFTWARE, true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(), resource.data(), resource.size(), &diagnostic) == UI_OK, diagnostic.message);
        else app.loadFleck(xml);
        app.openWindow();
        auto canvas = app.findFleckNode("canvas"), box = app.findFleckNode("box");
        for (unsigned dpi : {96u, 144u, 192u}) {
            require(ui_host_resize(app.nativeHandle(), 300, 200, dpi) == UI_OK, "Canvas resize");
            app.poll(0);
            const auto parent = canvas.nativeHandle()->layout_cache.arranged;
            const auto child = box.nativeHandle()->layout_cache.arranged;
            require(child.left-parent.left == 2.5*dpi/96 && child.top-parent.top == 14.25*dpi/96, "Canvas fractional coordinates at DPI");
        }
        box.setCanvasPosition(11, 5);
        app.poll(0);
        require(box.nativeHandle()->bounds.left-canvas.nativeHandle()->bounds.left == 30, "Changing Canvas.Left rearranges child");
        require(ui_node_set_canvas_position(box.nativeHandle(), 1, std::numeric_limits<double>::infinity()) == UI_ERROR_INVALID_ARGUMENT &&
                box.nativeHandle()->canvas_left == 11, "Invalid Canvas.Top preserves both coordinates");
    }
}

static void panel_orientation() {
    const std::string xml = "<Window xmlns='urn:sunfleck:v1'><StackPanel Id='row' Orientation='Horizontal' Spacing='2' Padding='0'>"
        "<WrapPanel Id='wrap' Orientation='Vertical' Height='32' Spacing='2' Padding='0'>"
        "<Border Width='7' Height='15'/><Border Width='9' Height='15'/><Border Id='third' Width='12' Height='15'/>"
        "</WrapPanel><Separator Id='line' Orientation='Vertical'/></StackPanel></Window>";
    fleck::Page page;
    ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml, page, diagnostic), diagnostic.message);
    const auto resource = fleck::compile_resource(page);
    for (bool binary : {false, true}) {
        sunfleck::App app(UI_SOFTWARE, true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(), resource.data(), resource.size(), &diagnostic) == UI_OK, diagnostic.message);
        else app.loadFleck(xml);
        app.openWindow();
        require(ui_host_resize(app.nativeHandle(), 250, 200, 96) == UI_OK, "Orientation resize");
        app.poll(0);
        auto row = app.findFleckNode("row");
        auto wrap = app.findFleckNode("wrap");
        auto line = app.findFleckNode("line");
        const auto bounds = wrap.nativeHandle()->bounds;
        require(bounds.right - bounds.left == 23, "Vertical wrap desired width includes second column");
        const auto third = app.findFleckNode("third").nativeHandle()->bounds;
        require(third.left == bounds.left + 11 && third.top == bounds.top, "Vertical wrap starts second column");
        require(line.nativeHandle()->bounds.left == bounds.right + 2 && line.nativeHandle()->vertical(), "Horizontal stack advances by desired width");
        row.setOrientation(UI_VERTICAL);
        line.setOrientation(UI_HORIZONTAL);
        app.poll(0);
        require(line.nativeHandle()->bounds.top == wrap.nativeHandle()->bounds.bottom + 2 && !line.nativeHandle()->vertical(), "Changing orientation rearranges descendants");
    }
}

static void rgba_stretch() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Image Id='img' Stretch='Fill'/></Window>");
    auto img = app.findFleckNode("img");
    const uint8_t source[] = {255,0,0,128,0,0,255,0};
    img.setImageRgba(source,2,1);
    auto draw = [&] {
        std::vector<FillRectCommand> commands;
        std::vector<ui_node*> hits, scroll;
        render_tree(*img.nativeHandle(),{0,0,40,40},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
        std::vector<uint8_t> pixels;
        rasterize(pixels,40,40,commands);
        return pixels;
    };
    auto pixels = draw();
    require(pixels[0] == 128 && pixels[3] == 128 && pixels[4*25+3] == 0, "Fill preserves straight RGBA image alpha");
    img.setStretch(UI_STRETCH_UNIFORM);
    pixels = draw();
    require(pixels[3] == 0 && pixels[4*(40*10)+3] == 128, "Uniform centers aspect-preserving RGBA data");
}

static void drawing_objects() {
    const std::string xml="<Window xmlns='urn:sunfleck:v1'><Canvas Id='root'>"
        "<Rectangle Width='20' Height='12'><Rectangle.Fill><LinearGradientBrush EndPoint='1,0'>"
        "<GradientStop Offset='0' Color='#FF0000'/><GradientStop Offset='1' Color='#0000FF'/>"
        "</LinearGradientBrush></Rectangle.Fill><Rectangle.OpacityMask><SolidColorBrush Color='#80000000'/></Rectangle.OpacityMask></Rectangle>"
        "<Path Canvas.Left='20' Width='12' Height='12' Fill='#00FF00' Data='M0,0 L10,0 L0,10 Z'/>"
        "<Ellipse Canvas.Top='16' Width='10' Height='10'><Ellipse.Fill><ImageBrush Width='1' Height='1' Rgba='ffff00ff'/></Ellipse.Fill>"
        "<Ellipse.Effect><DropShadowEffect Offset='8,0' Radius='0' Color='#FF0000'/></Ellipse.Effect></Ellipse>"
        "<Border Canvas.Left='25' Canvas.Top='20' Width='8' Height='6' Background='#FFFFFF'><Border.Effect><BlurEffect Radius='3'/></Border.Effect></Border>"
        "</Canvas></Window>";
    fleck::Page page; ui_fleck_diagnostic d{};
    require(fleck::parse(xml,page,d),d.message);
    auto resource=fleck::compile_resource(page);
    for(bool binary:{false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if(binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&d)==UI_OK,d.message);
        else app.loadFleck(xml);
        std::vector<FillRectCommand> commands;
        std::vector<ui_node*> hits,scroll;
        render_tree(*app.findFleckNode("root").nativeHandle(),{0,0,40,32},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
                    builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
        std::vector<uint8_t> pixels;
        rasterize(pixels,40,32,commands);
        const auto pixel=[&](int x,int y){return pixels.data()+4*(size_t(y)*40+x);};
        require(pixel(2,4)[0]>pixel(2,4)[2] && pixel(17,4)[2]>pixel(17,4)[0] && pixel(10,4)[3]==128,"Gradient and mask");
        require(pixel(22,2)[1]==255 && pixel(29,9)[3]==0,"Path uses figure coverage");
        require(pixel(5,21)[1]==255 && pixel(14,21)[0]==255 && pixel(14,21)[1]==0,"ImageBrush and shadow offset");
        require(pixel(24,23)[3]>0 && pixel(24,23)[3]<pixel(28,23)[3],"Blur extends beyond source edge");
    }
}

static void layout_transform() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Spacing='0'><Border Id='shape' Width='30' Height='10'><Border.LayoutTransform><RotateTransform Angle='90'/></Border.LayoutTransform></Border><Button Id='next' Height='10'/></StackPanel></Window>");
    auto shape = app.findFleckNode("shape"), next = app.findFleckNode("next");
    app.openWindow();
    for (unsigned dpi : {96u,144u,192u}) {
        require(ui_host_resize(app.nativeHandle(),400,300,dpi)==UI_OK,"LayoutTransform DPI");
        app.poll(0);
        auto rect = shape.nativeHandle()->bounds;
        require(rect.right-rect.left == int(10*dpi/96) && rect.bottom-rect.top == int(30*dpi/96) &&
                next.nativeHandle()->bounds.top == rect.bottom,"LayoutTransform reserves rotated extent");
    }
}

static void render_transform() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Canvas Id='root'><Border Id='shape' Width='10' Height='6' Background='#00FF00'><Border.RenderTransform><TransformGroup><MatrixTransform Matrix='1,0,1,1,0,0'/><TranslateTransform X='5' Y='4'/></TransformGroup></Border.RenderTransform></Border></Canvas></Window>");
    auto shape = app.findFleckNode("shape");
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> hits, scroll;
    render_tree(*app.findFleckNode("root").nativeHandle(),{0,0,30,20},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
                builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
    std::vector<uint8_t> pixels;
    rasterize(pixels,30,20,commands);
    require(pixels[4*(8*30+12)+1] == 255 && pixels[4*(8*30+6)+3] == 0,"Shear preserves polygon coverage");
    const auto window = shape.toWindow({2,3}), local = shape.fromWindow(window);
    require(window.x == 10 && window.y == 7 && local.x == 2 && local.y == 3,"Transform maps both coordinate directions");
    shape.setRenderTransform({0,0,0,1,0,0});
    commands.clear();
    render_tree(*app.findFleckNode("root").nativeHandle(),{0,0,30,20},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
                builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
    ui_point point{};
    require(ui_node_from_window(shape.nativeHandle(),window,&point) == UI_ERROR_INVALID_STATE,"Singular transform has no inverse");
}

static void transformed_clip_and_popup() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Canvas Id='root'>"
        "<Canvas Canvas.Left='12' Canvas.Top='10' Width='20' Height='12' ClipToBounds='true'>"
        "<Canvas.RenderTransform><MatrixTransform Matrix='1,0,1,1,0,0'/></Canvas.RenderTransform>"
        "<Border Id='anchor' Width='35' Height='12' Background='#00FF00'>"
        "<Border.RenderTransform><TranslateTransform X='4'/></Border.RenderTransform></Border>"
        "</Canvas><Popup Id='popup' Anchor='anchor'><TextBlock Text='tip'/></Popup></Canvas></Window>");
    auto* popup = app.findFleckNode("popup").nativeHandle();
    popup->value = 1;
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> hits, scroll;
    render_tree(*app.findFleckNode("root").nativeHandle(),{0,0,100,100},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),
                builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
    std::vector<uint8_t> pixels;
    rasterize(pixels,100,100,commands);
    const auto green = [&](int x,int y) { return pixels[4*(100*y+x)+1] == 255; };
    require(green(28,18) && !green(18,18) && !green(42,18),
            "Inner translation respects the sheared ancestor clip");
    auto* anchor = app.findFleckNode("anchor").nativeHandle();
    require(popup->bounds.top == anchor->visible.bottom && popup->bounds.left == anchor->visible.left,
            "Popup follows transformed and clipped anchor bounds");
}

static void subtree_opacity() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Canvas Id='root' Opacity='0.5'><Border Width='30' Height='20' Background='#FF0000'/><Border Canvas.Left='10' Width='30' Height='20' Background='#0000FF'/></Canvas></Window>");
    auto* node = app.findFleckNode("root").nativeHandle();
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> hits, scroll;
    render_tree(*node,{0,0,40,20},builtin_palette(UI_BUILTIN_FLUENT_2,false,false),builtin_themes[0].shape,96,nullptr,false,false,nullptr,nullptr,commands,hits,scroll);
    std::vector<uint8_t> pixels;
    rasterize(pixels,40,20,commands);
    const auto* overlap = pixels.data()+4*(10*40+15);
    require(overlap[0] == 0 && overlap[2] == 128 && overlap[3] == 128, "Opacity composites overlapping children once");
}

static void viewbox_scale() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><Viewbox Id='view' Width='120' Height='90'><Canvas Width='40' Height='20'><Button Id='button' Canvas.Left='4' Canvas.Top='2' Width='16' Height='10'/></Canvas></Viewbox></Window>");
    app.openWindow();
    auto view = app.findFleckNode("view"), button = app.findFleckNode("button");
    for (unsigned dpi : {96u,192u}) {
        require(ui_host_resize(app.nativeHandle(),400,300,dpi) == UI_OK, "Viewbox DPI");
        app.poll(0);
        auto outer = view.nativeHandle()->bounds, inner = button.nativeHandle()->visible;
        require(inner.left-outer.left == 12*int(dpi)/96 && inner.top-outer.top == 21*int(dpi)/96 &&
                inner.right-inner.left == 48*int(dpi)/96, "Viewbox centers and scales painted hit geometry");
    }
}

static void custom_panel() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel><Panel Id='panel'><Border Id='child' Width='10' Height='5'/></Panel></StackPanel></Window>");
    auto panel = app.findFleckNode("panel"), child = app.findFleckNode("child");
    struct Context { ui_node* node; int releases = 0; ui_result reentry = UI_OK; } context{panel.nativeHandle()};
    ui_panel_callbacks callbacks{sizeof(callbacks),1};
    callbacks.context = &context;
    callbacks.measure = [](void*,ui_size,const ui_size* children,uint32_t count,ui_size* desired) {
        if (count != 1 || children[0].width != 10) return UI_ERROR_INVALID_ARGUMENT;
        *desired = {40,27}; return UI_OK;
    };
    callbacks.arrange = [](void* raw,ui_size,const ui_size*,uint32_t count,ui_layout_rect* slots) {
        if (count != 1) return UI_ERROR_INVALID_ARGUMENT;
        auto& c = *static_cast<Context*>(raw);
        c.reentry = ui_node_set_z_index(c.node,9);
        slots[0] = {4.5,7,10,5};
        return UI_OK;
    };
    callbacks.release = [](void* raw) { ++static_cast<Context*>(raw)->releases; };
    panel.setPanelCallbacks(&callbacks);
    app.openWindow();
    for (unsigned dpi : {96u,144u,192u}) {
        require(ui_host_resize(app.nativeHandle(),250,200,dpi) == UI_OK, "Panel DPI");
        app.poll(0);
        const auto parent = panel.nativeHandle()->layout_cache.arranged, slot = child.nativeHandle()->layout_cache.arranged;
        require(slot.left-parent.left == 4.5*dpi/96 && slot.top-parent.top == 7.*dpi/96 &&
                parent.bottom-parent.top == 27.*dpi/96, "Panel callback coordinates remain logical pixels");
    }
    require(context.reentry == UI_ERROR_INVALID_STATE, "Panel rejects mutation during arrangement");
    panel.setPanelCallbacks(nullptr);
    require(context.releases == 1, "Panel releases registration once");
}

static void two_axis_scroll() {
    sunfleck::App app(UI_SOFTWARE,true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><ScrollView Id='scroll' Width='100' Height='80' HorizontalScrollBarVisibility='Auto'><Canvas Width='200' Height='180'><Button Id='target' Canvas.Left='160' Canvas.Top='140' Width='30' Height='25'/></Canvas></ScrollView></Window>");
    app.openWindow();
    require(ui_host_resize(app.nativeHandle(), 320, 200, 96) == UI_OK, "Resize hosted scroll viewport");
    app.poll(0);
    auto scroll = app.findFleckNode("scroll"), target = app.findFleckNode("target");
    auto* view = scroll.nativeHandle();
    require(view->horizontal_limit > 100 && view->scroll_limit > 100, "Both scrollbars reserve viewport space");
    const int x = view->visible.left + 20, y = view->visible.top + 20;
    require(ui_host_scroll(app.nativeHandle(), x, y, 40) == UI_OK &&
            view->offset == 40 && view->horizontal_offset == 0, "Hosted wheel scrolls the vertical axis");
    require(ui_host_scroll(app.nativeHandle(), x, y, -80) == UI_OK &&
            view->offset == 0, "Hosted wheel clamps the offset at the top");
    target.bringIntoView(); app.poll(0);
    require(view->horizontal_offset > 0 && view->offset > 0 &&
            target.nativeHandle()->visible.right == target.nativeHandle()->bounds.right &&
            target.nativeHandle()->visible.bottom == target.nativeHandle()->bounds.bottom, "BringIntoView reveals both axes");
    scroll.setScrollPolicies(UI_SCROLL_HIDDEN,UI_SCROLL_DISABLED);
    scroll.scrollTo(55.5,60); app.poll(0);
    require(view->offset == 0 && view->horizontal_offset == 55.5 && view->horizontal_bar.right == 0, "Hidden axis scrolls and disabled axis resets");

    sunfleck::App nested(UI_SOFTWARE,true);
    nested.loadFleck("<Window xmlns='urn:sunfleck:v1'><ScrollView Height='85'><Canvas Height='400'>"
                  "<ScrollView Canvas.Top='240' Width='100' Height='68'><Canvas Height='250'>"
                  "<Border Canvas.Top='192' Width='75' Height='45' ClipToBounds='true'>"
                  "<Button Id='deep' Width='30' Height='25'/></Border></Canvas></ScrollView>"
                  "</Canvas></ScrollView></Window>");
    nested.openWindow(); nested.poll(0);
    auto deep = nested.findFleckNode("deep");
    deep.bringIntoView(); nested.poll(0);
    const auto visible = deep.nativeHandle()->visible;
    require(visible.right-visible.left == 30 && visible.bottom-visible.top == 25,
            "BringIntoView arranges a clipped target inside an offscreen nested viewport");
}

static void glass_navigation_appearance() {
    sunfleck::App app(UI_SOFTWARE, true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><NavigationView Id='navigation'>"
                  "<TabItem Id='first' Text='First'/><TabItem Id='second' Text='Second'/></NavigationView></Window>");
    auto& nav = *app.findFleckNode("navigation").nativeHandle();
    const auto* first = app.findFleckNode("first").nativeHandle();
    const auto* second = app.findFleckNode("second").nativeHandle();
    const auto shape = builtin_themes[UI_BUILTIN_LIQUID_GLASS].shape;
    for (bool dark : {false, true}) {
        for (unsigned dpi : {96u, 144u}) {
            const auto palette = builtin_palette(UI_BUILTIN_LIQUID_GLASS, dark, dpi == 144);
            const int width = 420, height = 240, rail = 120 * dpi / 96;
            const layout::Rect viewport{0, 0, width, height};
            const FillRectCommand backdrop{0, 0, width, height, theme_rgb(palette.colors[UI_THEME_BACKGROUND])};
            std::vector<FillRectCommand> reference{backdrop};
            drawing::glass(reference, {0, 0, rail, height}, viewport, theme_rgb(palette.colors[UI_THEME_PANEL]),
                theme_rgb(palette.colors[UI_THEME_DIVIDER]), dpi, (shape.popup_radius * dpi + 48) / 96);
            std::vector<uint8_t> glass_pixels;
            rasterize(glass_pixels, width, height, reference);
            for (const auto* hovered : {static_cast<const ui_node*>(nullptr), second}) {
                std::vector<FillRectCommand> commands{backdrop};
                std::vector<ui_node*> hits, scroll;
                render_tree(nav, viewport, palette, shape, dpi, hovered, hovered != nullptr, false,
                    hovered, nullptr, commands, hits, scroll);
                std::vector<uint8_t> pixels;
                rasterize(pixels, width, height, commands);
                const auto shows_glass = [&](int x, int y) {
                    const auto offset = (y * width + x) * 4;
                    return std::equal(pixels.data() + offset, pixels.data() + offset + 4, glass_pixels.data() + offset);
                };
                for (const auto* row : {first, second}) {
                    const auto bounds = row->bounds;
                    const int center = (bounds.top + bounds.bottom) / 2;
                    require(shows_glass(1, center) && shows_glass(rail - 2, center), "Navigation rows leave the glass rim visible");
                    require(shows_glass(rail - 22, center) == (row == second && !hovered),
                            "Idle row shows glass, and selected or hovered items have a visible highlight");
                }
                const int highlight_right = first->bounds.right - int(6 * dpi / 96);
                const int highlight_top = first->bounds.top + int((3 * dpi + 48) / 96);
                const int shoulder = highlight_right - int(6 * dpi / 96) - 1;
                const auto color = theme_rgb(palette.colors[UI_THEME_SELECTED]);
                const auto offset = ((highlight_top + 1) * width + shoulder) * 4;
                require(shows_glass(highlight_right - 1, highlight_top) &&
                        std::equal(color.begin(), color.end(), pixels.data() + offset),
                        "Glass navigation highlight has small rounded corners");
            }
        }
    }
}

static void scrollbar_theme_appearance() {
    sunfleck::App app(UI_SOFTWARE, true);
    app.loadFleck("<Window xmlns='urn:sunfleck:v1'><ScrollView Id='scroll' HorizontalScrollBarVisibility='Auto'>"
                  "<Canvas Width='240' Height='220'/></ScrollView></Window>");
    auto& view = *app.findFleckNode("scroll").nativeHandle();
    for (unsigned id = 0; id < UI_BUILTIN_THEME_COUNT; ++id) {
        const auto theme = static_cast<ui_builtin_theme>(id);
        const auto track_token = theme == UI_BUILTIN_FLUENT_2 || theme == UI_BUILTIN_SUNFLECK_LITE
            ? UI_THEME_CONTROL : UI_THEME_PANEL;
        const auto pressed_token = theme == UI_BUILTIN_FLUENT_2 ? UI_THEME_TEXT : UI_THEME_PRIMARY;
        const bool translucent = theme == UI_BUILTIN_LIQUID_GLASS;
        for (bool dark : {false, true}) {
            const auto palette = builtin_palette(theme, dark, false);
            auto track = theme_rgb(palette.colors[track_token]);
            auto thumb = theme_rgb(palette.colors[UI_THEME_DIVIDER]);
            if (translucent) { track[3] = 160; thumb[3] = 224; }
            const auto pressed = theme_rgb(palette.colors[pressed_token]);
            for (int drag_axis : {-1, 0, 1}) {
                view.dragging = drag_axis >= 0;
                view.dragging_horizontal = drag_axis == 1;
                std::vector<FillRectCommand> commands;
                std::vector<ui_node*> hits, scroll;
                render_tree(view, {0, 0, 120, 100}, palette, builtin_themes[theme].shape, 96,
                    nullptr, false, false, nullptr, nullptr, commands, hits, scroll);
                const auto painted = [&](int x, int y, const auto& color) {
                    const auto command = std::find_if(commands.rbegin(), commands.rend(), [&](const auto& c) {
                        return c.left <= x && x < c.right && c.top <= y && y < c.bottom;
                    });
                    return command != commands.rend() && command->color == color;
                };
                const auto vertical = view.scrollbar, horizontal = view.horizontal_bar;
                const auto v_thumb = view.scrollbar_thumb, h_thumb = view.horizontal_thumb;
                const int x = (vertical.left + vertical.right) / 2, y = (horizontal.top + horizontal.bottom) / 2;
                require(painted(x, vertical.bottom - 8, track) && painted(horizontal.right - 8, y, track),
                        "Both scrollbar tracks use the selected theme");
                require(painted(x, (v_thumb.top + v_thumb.bottom) / 2, drag_axis == 0 ? pressed : thumb) &&
                        painted((h_thumb.left + h_thumb.right) / 2, y, drag_axis == 1 ? pressed : thumb),
                        "Both scrollbar thumbs use themed colors and only the dragged axis is pressed");
            }
        }
    }
}

static void fractional_scroll() {
    for (unsigned dpi : {96u,144u,192u}) {
        ui_node root{nullptr,UI_SCROLL_VIEW}, child{nullptr,UI_TEXT_BLOCK};
        root.spacing=0;root.children={&child};child.parent=&root;child.height=70.25;
        auto style=builtin_palette(UI_BUILTIN_FLUENT_2,false,false);style.gap=0;
        const int height=50*dpi/96;
        auto draw=[&] {
            std::vector<FillRectCommand> commands;
            std::vector<ui_node*> interactive,scrollable;
            render_tree(root,{0,0,200,height},style,builtin_themes[UI_BUILTIN_FLUENT_2].shape,
                dpi,nullptr,false,false,nullptr,nullptr,commands,interactive,scrollable);
        };
        draw();
        const double limit=20.25*dpi/96.0;
        require(std::abs(root.scroll_limit-limit)<1e-9,"Scroll extent retains fractional layout at every DPI");
        root.offset=0.125;draw();
        require(root.offset==0.125,"Subpixel scroll offset survives rendering");
        root.offset=1000;draw();
        require(root.offset==limit,"Scroll clamps to exact content edge without ceil overscroll");
        require(child.bounds.bottom==height,"Final child edge meets viewport at maximum scroll");
        require(root.scrollbar_thumb.bottom==root.scrollbar.bottom,"Thumb reaches final track edge");
        child.height=10;draw();
        require(root.offset==0 && root.scroll_limit==0,"Shrinking content clears fractional scroll state");
    }
}
static void fractional_dock_minimum() {
    const std::string xml="<Window xmlns='urn:sunfleck:v1'><DockHost Id='dock'><DockSplit Value='1'><DockGroup><DockDocument Id='first' MinWidth='64.25' MinHeight='70.125'/></DockGroup><DockGroup><DockDocument Id='second'/></DockGroup></DockSplit></DockHost></Window>";
    fleck::Page page; ui_fleck_diagnostic error{};
    require(fleck::parse(xml,page,error),"Fractional Dock minimum parse");
    const auto resource=fleck::compile_resource(page);
    for (bool binary:{false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&error)==UI_OK,"Fractional Dock resource");
        else app.loadFleck(xml);
        app.openWindow();
        auto doc=app.findFleckNode("first");
        auto* dock=app.findFleckNode("dock").nativeHandle()->workspace;
        auto* item=docking::find(*dock,"first");
        require(item->min_width==64.25 && item->min_height==70.125,"Dock preserves typed minima from XML and resource");
        for (unsigned dpi:{96u,144u,192u}) {
            require(ui_host_resize(app.nativeHandle(),800,600,dpi)==UI_OK,"Dock minimum DPI"); app.poll(0);
            const auto group=dock->views.at(0).groups.at(docking::group_of(dock->layout,"first"));
            require(group.right-group.left==int(std::ceil(64.25*dpi/96.0)),"Dock minimum rounds once after DPI scaling");
        }
        doc.setMinWidth(65.125); doc.setMinHeight(71.375); app.poll(0);
        require(item->min_width==65.125 && item->min_height==71.375,"Dock property updates preserve fractional minima");
        const auto group=dock->views.at(0).groups.at(docking::group_of(dock->layout,"first"));
        require(group.right-group.left==131,"Updated Dock minimum reaches rendered split boundary");
        app.refreshFleck(xml); app.poll(0);
        dock=app.findFleckNode("dock").nativeHandle()->workspace;
        require(docking::find(*dock,"first")->min_width==64.25,"Transactional refresh restores fractional declaration");
    }
}
static void unbounded_measurement() {
    const double infinity=std::numeric_limits<double>::infinity();
    auto style=builtin_palette(UI_BUILTIN_FLUENT_2,false,false);
    auto shape=builtin_themes[0].shape;
    sunfleck::App app(UI_SOFTWARE,true);
    for (const char* container:{"StackPanel","Flex","Grid","WrapPanel","DockPanel","Border","Overlay","SplitView"}) {
        app.loadFleck("<Window xmlns='urn:sunfleck:v1'><"+std::string(container)+" Id='root' Padding='1.25' Spacing='0.5'><TextBlock Id='text' FontSize='24' Text='Long content needs a genuine unbounded constraint rather than an allocated star track.'/></"+container+"></Window>");
        auto* root=app.findFleckNode("root").nativeHandle();
        const auto measure=[&](double width,unsigned dpi=96) { return measure_tree_height(*root,width,style,shape,dpi); };
        const double unbounded=measure(infinity);
        require(std::isfinite(unbounded) && unbounded>0,"Unbounded container returns finite desired height");
        const double narrow=measure(55.5);
        require(std::isfinite(narrow) && narrow>=unbounded,"Bounded constraint does not reduce wrapped desired height");
        require(measure(infinity)==unbounded,"Changing constraint restores cached unbounded result");
        auto* text=app.findFleckNode("text").nativeHandle();
        app.findFleckNode("text").setMaxWidth(40.25);
        const double capped=measure(infinity);
        require(capped>unbounded,"Explicit maximum still wraps text inside an unbounded container");
        app.findFleckNode("text").setMaxWidth(infinity);
        require(measure(infinity)==unbounded,"Clearing finite maximum invalidates ancestor measurements");
        require(std::isfinite(measure(0)) && std::isfinite(measure(infinity,192)),"Zero and high-DPI unbounded constraints remain finite");
        text->visibility=UI_COLLAPSED;
        require(measure(infinity)<=unbounded,"Collapsed content does not retain old desired height");
    }
}
static void fractional_metrics() {
    const std::string xml="<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root' Spacing='0.25' Padding='1.125'><Border Id='one' Height='10.5'/><Border Id='two' Height='10.5'/></StackPanel></Window>";
    fleck::Page page; ui_fleck_diagnostic error{};
    require(fleck::parse(xml,page,error),"Fractional metrics parse");
    const auto resource=fleck::compile_resource(page);
    for (bool binary:{false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&error)==UI_OK,"Fractional metrics resource");
        else app.loadFleck(xml);
        app.openWindow();
        for (unsigned dpi:{96u,144u,192u}) {
            require(ui_host_resize(app.nativeHandle(),600,400,dpi)==UI_OK,"Metrics DPI resize"); app.poll(0);
            const auto root=app.findFleckNode("root").nativeHandle()->layout_cache.arranged;
            const auto one=app.findFleckNode("one").nativeHandle()->layout_cache.arranged;
            const auto two=app.findFleckNode("two").nativeHandle()->layout_cache.arranged;
            const double scale=dpi/96.;
            require(one.left-root.left==1.125*scale && one.top-root.top==1.125*scale,"Padding retains fractional DPI geometry");
            require(two.top-one.bottom==.25*scale,"Spacing retains fractional DPI geometry");
        }
        auto root=app.findFleckNode("root");
        ui_binding_value value{}; value.kind=UI_VALUE_DOUBLE; value.number=2.625;
        root.setPropertyValue(UI_PROPERTY_SPACING,value);
        require(root.propertyValue(UI_PROPERTY_PADDING).number==1.125,"Single metric preserves other fractional value");
        app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='theme' Spacing='Theme'/></Window>");
        require(app.findFleckNode("theme").propertyValue(UI_PROPERTY_SPACING).number==-1,"Explicit Theme spacing");
    }
    for (const char* value:{"-0.5","-1","NaN","Infinity","64.25"}) {
        require(!fleck::parse("<Window xmlns='urn:sunfleck:v1'><StackPanel Spacing='"+std::string(value)+"'/></Window>",page,error),"Illegal spacing literal rejected");
    }
    auto previous=resource; previous[5]=14;
    sunfleck::App incompatible(UI_SOFTWARE,true);
    require(ui_app_load_fleck_resource(incompatible.nativeHandle(),previous.data(),previous.size(),&error)==UI_ERROR_VERSION_MISMATCH,
        "Integer metric resource version is rejected before decoding new doubles");
}
static void width_contract() {
    const std::string xml="<Window xmlns='urn:sunfleck:v1'><StackPanel Spacing='0'><Flex Id='row' Width='300' Height='40' Spacing='0'><Border Id='fixed' Width='40.25' Weight='99'/><Border Id='grow' MinWidth='60.5' Weight='1'/><Border Id='other' Weight='1'/></Flex><DockHost Id='dock' Width='200' Height='123.5'><DockGroup><DockDocument Id='doc' MinWidth='5000.25'/></DockGroup></DockHost></StackPanel></Window>";
    fleck::Page page; ui_fleck_diagnostic error{};
    require(fleck::parse(xml,page,error),"Width migration XML");
    const auto resource=fleck::compile_resource(page);
    for (bool binary:{false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&error)==UI_OK,"Width migration resource");
        else app.loadFleck(xml);
        app.openWindow(); app.poll(0);
        const auto width=[&](const char* id) { const auto& r=app.findFleckNode(id).nativeHandle()->layout_cache.arranged; return r.right-r.left; };
        require(width("fixed")==40.25,"Explicit Width fixes a Flex track even with nonzero Weight");
        require(width("grow")==160.125 && width("other")==99.625,"MinWidth supplies a Flex basis while Weight distributes extra space");
        const auto dock=app.findFleckNode("dock");
        require(dock.height()==123.5 && dock.width()==200,"DockHost has independent Width and Height");
        require(app.findFleckNode("doc").minWidth()==5000.25,"Large fractional Dock minimum survives resource encoding");
        ui_property_id removed=0;
        require(ui_node_resolve_property(dock.nativeHandle(),"PreferredWidth",&removed)!=UI_OK,"Removed property cannot resolve as a built-in");
        const auto root=app.fleckRoot().nativeHandle();
        try { app.refreshFleck("<Window xmlns='urn:sunfleck:v1'><Border PreferredWidth='80'/></Window>"); require(false,"Removed attribute accepted"); }
        catch (const sunfleck::Error&) {}
        require(app.fleckRoot().nativeHandle()==root && app.findFleckNode("fixed").width()==40.25,"Removed attribute diagnoses without replacing active page");
    }
}
static void fractional_edges() {
    ui_node root{nullptr,UI_STACK_PANEL}; root.spacing=0;
    std::vector<ui_node> items;
    items.reserve(100);
    for (unsigned i=0;i<100;++i) {
        auto& item=items.emplace_back(nullptr,UI_TEXT_BLOCK);
        item.parent=&root; item.height=1; root.children.push_back(&item);
    }
    auto style=builtin_palette(UI_BUILTIN_FLUENT_2,false,false);
    std::vector<FillRectCommand> commands; std::vector<ui_node*> interactive,scrollable;
    render_tree(root,{0,0,400,400},style,builtin_themes[0].shape,144,nullptr,false,false,nullptr,nullptr,commands,interactive,scrollable);
    require(items.back().bounds.bottom==150,"Fractional heights accumulate before rounding, without per-child DPI drift");
    require(items[1].layout_cache.arranged.top==1.5,"Arrangement cache retains subpixel origin");
    for (size_t i=1;i<items.size();++i) require(items[i-1].bounds.bottom==items[i].bounds.top,"Shared edges have no gaps or overlaps after rounding");
    ui_node outer{nullptr,UI_BORDER},inner{nullptr,UI_BORDER},child{nullptr,UI_BUTTON};
    outer.children={&inner}; inner.parent=&outer; inner.children={&child}; child.parent=&inner;
    outer.border.thickness.fill(.25); inner.border.thickness.fill(.25);
    commands.clear();
    render_tree(outer,{0,0,100,100},style,builtin_themes[0].shape,144,nullptr,false,false,nullptr,nullptr,commands,interactive,scrollable);
    require(child.layout_cache.arranged.left==.75 && child.bounds.left==1,"Nested borders retain fractional insets until painting");
    require(!interactive.empty() && child.visible.left==child.bounds.left,"Input geometry uses the same rounded painted boundary");
}
static void fractional_margin() {
    const std::string xml="<Window xmlns='urn:sunfleck:v1'><StackPanel Id='root' Spacing='0'><Border Id='box' Height='12.25' Width='Auto' MinWidth='0.5' MaxWidth='Infinity' MinHeight='1.5' MaxHeight='Infinity' Margin='0.25,0.5,0.75,1.25' Background='#224466'/><Border Id='next' Height='12'/></StackPanel></Window>";
    fleck::Page parsed; ui_fleck_diagnostic diagnostic{};
    require(fleck::parse(xml,parsed,diagnostic),"Fractional Margin XML parses");
    auto resource=fleck::compile_resource(parsed);
    for (bool binary:{false,true}) {
        sunfleck::App app(UI_SOFTWARE,true);
        if (binary) require(ui_app_load_fleck_resource(app.nativeHandle(),resource.data(),resource.size(),&diagnostic)==UI_OK,"Fractional Margin resource loads");
        else app.loadFleck(xml);
        app.openWindow(); app.poll(0);
        auto box=app.findFleckNode("box");
        require(box.height()==12.25 && box.width()==-1 && std::isinf(box.maxWidth()) && box.minHeight()==1.5,"Length values survive XML and resource loading");
        require(box.lengthValue(UI_PROPERTY_WIDTH).unit==UI_LENGTH_AUTO && box.lengthValue(UI_PROPERTY_MAX_HEIGHT).unit==UI_LENGTH_INFINITY,"Size getter preserves explicit Auto/Infinity units");
        int32_t legacy_height=777;
        require(ui_node_get_height(box.nativeHandle(),&legacy_height)==UI_ERROR_INVALID_STATE && legacy_height==777,"Legacy integer getter never truncates fractional height");
        require(box.margin().left==.25 && box.margin().bottom==1.25,"C++ Thickness preserves fractional components");
        require(ui_host_resize(app.nativeHandle(),600,400,144)==UI_OK,"Fractional Margin DPI"); app.poll(0);
        const auto& root=app.findFleckNode("root").nativeHandle()->layout_cache.arranged;
        const auto& rect=box.nativeHandle()->layout_cache.arranged;
        require(rect.left-root.left==.375 && rect.top-root.top==.75,"Margin scales without truncating each component");
        require(rect.bottom-rect.top==18.375,"Fractional explicit height is retained through DPI scaling");
        ui_property_descriptor description{};description.struct_size=sizeof(description);description.version=1;
        require(ui_property_describe(UI_BORDER,UI_PROPERTY_MARGIN,&description)==UI_OK && description.kind==UI_VALUE_THICKNESS && (description.flags&UI_PROPERTY_ANIMATABLE),"Margin has typed, animatable metadata");
        const double animated[]={2.5,3.5,4.5,5.5};
        ui_binding_value value{};value.kind=UI_VALUE_THICKNESS;value.geometry=animated;
        require(ui_node_set_property_layer(box.nativeHandle(),UI_PROPERTY_MARGIN,UI_PRECEDENCE_ANIMATION,&value)==UI_OK,"Margin animation layer");
        box.setMargin({-1.25,2.25,3.25,4.25});
        require(box.margin().left==2.5,"Animation preserves visible value over updated Margin base");
        require(ui_node_clear_property_layer(box.nativeHandle(),UI_PROPERTY_MARGIN,UI_PRECEDENCE_ANIMATION)==UI_OK && box.margin().left==-1.25,"Removing animation restores current base Thickness");
        try {box.setMargin({std::numeric_limits<double>::quiet_NaN(),0,0,0});require(false,"NaN Margin accepted");} catch(const sunfleck::Error&) {}
        require(box.margin().left==-1.25,"Invalid Margin preserves existing effective value");
        double extent=40.125;
        sunfleck::PropertyModel lengths(app); lengths.property("Extent",[&]{return extent;});
        lengths.bind(box,UI_PROPERTY_WIDTH,"Extent",UI_BIND_ONE_WAY);
        require(box.width()==extent,"Scalar double source converts to a typed pixel length");
        extent=42.375; lengths.notify("Extent"); require(box.width()==extent,"Fractional length binding updates");
        try { box.setWidth(std::numeric_limits<double>::quiet_NaN()); require(false,"NaN length accepted"); } catch(const sunfleck::Error&) {}
        require(box.width()==extent,"Invalid length leaves active binding and effective value intact");
        box.setWidth(ui_length{0,UI_LENGTH_AUTO});
        require(box.lengthValue(UI_PROPERTY_WIDTH).unit==UI_LENGTH_AUTO,"Typed C++ Auto setter");
        const auto stale=resource; resource[5]=12;
        sunfleck::App old_resource_app(UI_SOFTWARE,true);
        require(ui_app_load_fleck_resource(old_resource_app.nativeHandle(),resource.data(),resource.size(),&diagnostic)==UI_ERROR_VERSION_MISMATCH,"Pre-Thickness resources are explicitly rejected");
        resource=stale;
    }
}
static void incremental_layout() {
    ui_node root{nullptr, UI_STACK_PANEL}, first{nullptr, UI_BUTTON}, second{nullptr, UI_BUTTON};
    root.children = {&first, &second}; first.parent = second.parent = &root;
    first.text = L"First"; second.text = L"Second";
    first.layout.horizontal = 1; first.layout.horizontal_set = true;
    auto style = builtin_palette(UI_BUILTIN_FLUENT_2, false, false);
    auto shape = builtin_themes[0].shape;
    std::vector<FillRectCommand> commands;
    std::vector<ui_node*> interactive, scrollable;
    unsigned dpi = 96;
    layout::Rect area{0, 0, 400, 300};
    const auto draw = [&] {
        commands.clear();
        render_tree(root, area, style, shape, dpi, nullptr, false, false,
            &first, nullptr, commands, interactive, scrollable);
    };
    draw();
    auto first_measure = first.layout_cache.measures, second_measure = second.layout_cache.measures;
    auto first_arrange = first.layout_cache.arranges;
    const auto first_bounds = first.bounds;
    draw();
    require(first.layout_cache.measures == first_measure && second.layout_cache.measures == second_measure &&
        first.layout_cache.arranges == first_arrange, "Unchanged frame reuses measurement and arrangement");
    first.enabled = false; style.colors[UI_THEME_PRIMARY] ^= 0xffffff;
    draw();
    require(first.layout_cache.measures == first_measure && first.layout_cache.arranges == first_arrange &&
        interactive.size() == 1 && interactive.front() == &second, "Appearance/input updates preserve layout and rebuild hit targets");
    first.layout.horizontal = 3; first.layout.horizontal_set = true;
    draw();
    require(first.layout_cache.measures == first_measure && first.layout_cache.arranges > first_arrange &&
        first.bounds.left > first_bounds.left, "Alignment only recomputes arrangement");
    first.text = L"A much longer first button label";
    draw();
    require(first.layout_cache.measures > first_measure && second.layout_cache.measures == second_measure,
        "Content invalidates its ancestor path without measuring an unchanged sibling");
    auto old_second_top = second.bounds.top;
    first.height = 80;
    draw();
    require(second.bounds.top > old_second_top && second.layout_cache.measures == second_measure,
        "Changed desired size rearranges sibling using its cached measurement");
    auto current = second.layout_cache.measures;
    dpi = 144; draw();
    require(second.layout_cache.measures > current, "DPI change invalidates cached dimensions");
    current = second.layout_cache.measures; style.control_height += 4; draw();
    require(second.layout_cache.measures > current, "Theme metrics invalidate cached dimensions");
    current = second.layout_cache.measures; ++text_resource_revision; draw();
    require(second.layout_cache.measures > current, "Registered font change invalidates cached dimensions");
    current = second.layout_cache.measures; area.right += 50; draw();
    require(second.layout_cache.measures == current, "Window resize reuses unconstrained intrinsic dimensions");
    std::swap(root.children[0], root.children[1]); draw();
    require(second.bounds.top < first.bounds.top, "Child reordering updates arranged geometry");
    current = second.layout_cache.measures;
    invalidate_layout(second, UI_PROPERTY_AFFECTS_MEASURE); draw();
    require(second.layout_cache.measures > current, "Component metadata invalidates measurement without a built-in field change");
}

int main() {
    try {
        grid_tracks();
        shared_grid_sizes();
        sibling_z_order();
        panel_orientation();
        canvas_offsets();
        overflow_clip();
        visual_content();
        fractional_scroll();
        two_axis_scroll();
        glass_navigation_appearance();
        scrollbar_theme_appearance();
        custom_panel();
        viewbox_scale();
        drawing_objects();
        layout_transform();
        render_transform();
        transformed_clip_and_popup();
        subtree_opacity();
        rgba_stretch();
        fractional_edges();
        fractional_margin();
        width_contract();
        fractional_metrics();
        unbounded_measurement();
        fractional_dock_minimum();
        incremental_layout();
        sunfleck::App app(UI_SOFTWARE, true);
        auto xml = [](std::string content) { return "<Window xmlns='urn:sunfleck:v1' Width='400' Height='300'>" + content + "</Window>"; };
        app.loadFleck(xml("<Overlay Id='root' Height='200'><TextBlock Id='text' Width='100' Height='20' Margin='10,20,30,40' HorizontalAlignment='Center' VerticalAlignment='Bottom'/></Overlay>"));
        app.openWindow(); app.poll(0);
        auto bounds = [&](const char* id) { return app.findFleckNode(id).nativeHandle()->bounds; };
        auto root = bounds("root"), text = bounds("text");
        require(text.left == root.left + 10 + (root.right - root.left - 40 - 100)/2, "Center alignment inside asymmetric margin");
        require(text.top == root.bottom - 40 - 20, "Bottom alignment");
        auto node = app.findFleckNode("text");
        node.setHorizontalAlignment("Stretch"); node.setVerticalAlignment("Stretch"); node.setWidth(-1); node.setHeight(-1);
        app.poll(0); text = bounds("text");
        require(text.left == root.left + 10 && text.right == root.right - 30 && text.top == root.top + 20 && text.bottom == root.bottom - 40, "Stretch inside margin");
        node.setMaxWidth(150); node.setMaxHeight(70); app.poll(0); text = bounds("text");
        require(text.right-text.left == 150 && text.bottom-text.top == 70, "Maximum constrains stretched size");
        node.setWidth(10); node.setMinWidth(50); node.setHeight(5); node.setMinHeight(25); app.poll(0); text = bounds("text");
        require(text.right-text.left == 50 && text.bottom-text.top == 25, "Minimum constrains explicit size");
        try { node.setMaxWidth(20); require(false, "Invalid min/max accepted"); } catch(const sunfleck::Error&) {}
        require(node.maxWidth()==150, "Invalid layout update mutated node");
        node.setMargin({-5, -6, -5, -6}); app.poll(0); text=bounds("text");
        require(text.left==root.left-5 && text.top==root.top-6, "Negative margins");

        int width=80; ui_thickness margin{4,4,4,4}; std::string alignment="Right";
        sunfleck::PropertyModel model(app);
        model.property("Width", [&]{return width;});
        model.property("Margin", [&]{return margin;});
        model.property("Alignment", [&]{return alignment;});
        model.bind(node, UI_PROPERTY_WIDTH, "Width", UI_BIND_ONE_WAY);
        model.bind(node, UI_PROPERTY_MARGIN, "Margin", UI_BIND_ONE_WAY);
        model.bind(node, UI_PROPERTY_HORIZONTAL_ALIGNMENT, "Alignment", UI_BIND_ONE_WAY);
        app.poll(0); text=bounds("text");
        require(text.right-text.left==80 && text.right==root.right-4, "Bound layout properties");
        require(ui_node_clear_binding(node.nativeHandle(), UI_PROPERTY_WIDTH)==UI_OK, "Layout binding clear");
        require(ui_node_clear_binding(node.nativeHandle(), UI_PROPERTY_HEIGHT)==UI_OK, "Existing height binding clear");

        app.refreshFleck(xml("<StackPanel Id='root' Spacing='5'><TextBlock Id='one' Height='10' Margin='1,2,3,4'/><TextBlock Id='two' Height='20' Margin='6,7'/></StackPanel>"));
        app.poll(0); root=bounds("root");
        require(bounds("one").top==root.top+2, "Stack top margin");
        require(bounds("two").top==bounds("one").bottom+4+5+7, "Margins participate in stack measurement");
        app.refreshFleck(xml("<Flex Id='root' Height='100' Spacing='5'><TextBlock Id='one' Width='60' Height='20' Margin='4' VerticalAlignment='Center'/><TextBlock Id='two' Width='80' Height='30' VerticalAlignment='Bottom'/></Flex>"));
        app.poll(0); root=bounds("root");
        require(bounds("two").left==root.left+68+5, "Fixed width participates in Flex tracks");
        require(bounds("one").top==root.top+40 && bounds("two").bottom==root.bottom, "Flex vertical alignment");
        require(ui_host_resize(app.nativeHandle(),800,600,192)==UI_OK,"DPI resize"); app.poll(0);
        require(bounds("one").right-bounds("one").left==120, "Width DPI scaling");
        require(ui_host_resize(app.nativeHandle(),400,300,96)==UI_OK,"Restore DPI");
        app.refreshFleck(xml("<StackPanel Id='root'><Button Id='one' Content='Legacy'/><Button Id='two' Content='Stretch' HorizontalAlignment='Stretch'/></StackPanel>")); app.poll(0);
        require(bounds("two").right==bounds("root").right && bounds("one").right<bounds("two").right, "Explicit Stretch overrides legacy button width");
        app.refreshFleck(xml("<StackPanel Spacing='7'><DockPanel Id='root' Spacing='0'><TextBlock Id='one' Dock='Left' Height='30'/><Slider Id='two' Dock='Right' Height='30'/></DockPanel><TextBlock Id='after' Height='20'/></StackPanel>")); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==30, "DockPanel Auto height follows side-by-side children without a minimum floor");
        require(bounds("after").top==bounds("root").bottom+7, "DockPanel leaves no excess space before following sibling");
        app.findFleckNode("one").setHeight(12); app.findFleckNode("two").setHeight(18); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==18, "DockPanel shrinks after child height changes");
        app.findFleckNode("root").setMinHeight(40); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==40, "DockPanel respects explicit minimum height");
        app.refreshFleck(xml("<StackPanel><DockPanel Id='root' Spacing='3' Padding='2' HorizontalAlignment='Left'><TextBlock Dock='Left' Width='10' Height='80'/><TextBlock Dock='Top' Width='40' Height='20'/><TextBlock Dock='Bottom' Width='30' Height='10'/><TextBlock Width='25' Height='15' Margin='1,2'/></DockPanel></StackPanel>")); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==84, "Earlier side dock height is not added to subsequent top/bottom heights");
        require(bounds("root").right-bounds("root").left==57, "DockPanel desired width respects dock order and padding");
        app.refreshFleck(xml("<StackPanel><DockPanel Id='root' Spacing='3' Padding='2'><TextBlock Dock='Top' Height='20'/><TextBlock Dock='Left' Height='50'/><TextBlock Dock='Bottom' Height='10'/><TextBlock Height='15' Margin='1,2'/></DockPanel></StackPanel>")); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==77, "Top dock consumption precedes side dock height");
        require(ui_host_resize(app.nativeHandle(),800,600,192)==UI_OK,"Dock DPI resize"); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==154, "DockPanel Auto measurement scales with DPI");
        require(ui_host_resize(app.nativeHandle(),400,300,96)==UI_OK,"Restore Dock DPI");
        app.refreshFleck(xml("<StackPanel><DockPanel Id='root' Padding='2'/></StackPanel>")); app.poll(0);
        require(bounds("root").bottom-bounds("root").top==4, "Empty DockPanel only requires padding");
        {
            sunfleck::App dockApp(UI_SOFTWARE, true);
            dockApp.loadFleck(xml("<DockHost Id='dock'><DockGroup><DockDocument Id='doc'/></DockGroup></DockHost>"));
            auto doc = dockApp.findFleckNode("doc"); doc.setMinWidth(180); doc.setMinHeight(90);
            auto* workspace = dockApp.findFleckNode("dock").nativeHandle()->workspace;
            require(workspace && workspace->items[0]->min_width == 180 && workspace->items[0]->min_height == 90, "Dock item constraints follow property changes");
        }

        app.refreshFleck(xml("<StackPanel><DockPanel Id='root' Spacing='0'><TextBlock Id='one' Text='70' Height='30'/><Slider Id='two' Height='30'/></DockPanel></StackPanel>")); app.poll(0);
        const int intrinsic = bounds("one").right-bounds("one").left;
#ifdef _WIN32
        require(intrinsic == int(std::lround(windows_text::measure_width(L"70",96))), "Dock TextBlock Auto width rounds measured text at the paint boundary");
#else
        require(intrinsic == text_width(L"70", 96), "Dock TextBlock Auto width equals measured text");
#endif
        require(bounds("two").left == bounds("one").right, "Dock consumes intrinsic text width");
        app.findFleckNode("one").setWidth(60); app.poll(0);
        require(bounds("one").right-bounds("one").left == 60 && bounds("two").left==bounds("one").right, "Explicit Width changes both text bounds and Dock slot");
        app.findFleckNode("one").setWidth(-1); app.poll(0);
        require(bounds("one").right-bounds("one").left == intrinsic, "Setting Width back to Auto restores text measurement");
        app.findFleckNode("one").setText("A longer label"); app.poll(0);
        require(bounds("one").right-bounds("one").left > intrinsic, "Content changes update intrinsic width");
        app.refreshFleck(xml("<StackPanel><WrapPanel><TextBlock Id='one' Text='70' Width='Auto'/></WrapPanel></StackPanel>")); app.poll(0);
        require(bounds("one").right-bounds("one").left == intrinsic, "WrapPanel uses TextBlock content width");

        for (const char* attrs : {"Weight='Auto'", "Width='-2'", "MaxWidth='10000001'", "MinWidth='100' MaxWidth='20'", "MinHeight='40' MaxHeight='10'", "Margin='1,2,3'", "Margin='x'", "Margin='4097'", "HorizontalAlignment='Bottom'", "VerticalAlignment='Right'", "Width='{Binding W, Mode=TwoWay}'"}) {
            fleck::Page page; ui_fleck_diagnostic d{};
            require(!fleck::parse(xml(std::string("<TextBlock ")+attrs+"/>"),page,d),"Invalid layout accepted");
        }
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}


