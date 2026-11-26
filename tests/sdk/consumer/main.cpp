#include <sunfleck/property_model.hpp>
#include <fstream>
#include <stdexcept>
class Counter {
public:
    int Count = 0;
    void Increment() { ++Count; }
};
#include "counter.hpp"
int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    std::ifstream file(argv[1]);
    std::string xml{std::istreambuf_iterator<char>(file), {}};
    sunfleck::App app(UI_SOFTWARE, true);
    sunfleck::PropertyModel rows(app);
    rows.enableTextCollection();
    const char* labels[] = {"first", "second"};
    rows.spliceItems(0, 0, labels);
    auto second = rows.item(1);
    rows.moveItems(1, 1, 0);
    if (rows.itemCount() != 2 || rows.item(0) != second) return 1;
    sunfleck::PropertyModel grouped(app);
    grouped.setCollectionView(rows);
    grouped.setGroupByLabel(true);
    if (grouped.groupCount()!=2 || grouped.group(0).key!="second") return 18;
    auto animated = app.createNode(UI_BORDER);
    ui_binding_value tab{}; tab.kind = UI_VALUE_INT; tab.integer = 7;
    animated.setInputBinding(UI_KEY_F8,0,UI_EVENT_DIRECT,"IncrementCount");
    animated.setInputBinding(UI_KEY_F8,0,UI_EVENT_DIRECT,nullptr);
    animated.setPropertyValue(UI_PROPERTY_TAB_INDEX,tab);
    if (animated.propertyValue(UI_PROPERTY_TAB_INDEX).integer != 7) return 1;
    ui_binding_value fade{}; fade.kind = UI_VALUE_DOUBLE; fade.number = .75;
    animated.setResourceReference(UI_PROPERTY_OPACITY,"Fade");
    app.setResource("Fade",fade);
    if (animated.propertyValue(UI_PROPERTY_OPACITY).number != .75) return 1;
    app.clearResource("Fade");
    ui_key_frame frames[2]{};
    frames[0].value.kind = frames[1].value.kind = UI_VALUE_DOUBLE;
    frames[1].position = frames[1].value.number = 1;
    animated.registerVisualState("Input","Normal",nullptr);
    animated.goToVisualState("Input","Normal");
    animated.startAnimation(UI_PROPERTY_OPACITY,{frames,2,200,0,1,0,0});
    animated.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_SEEK,100);
    if (animated.propertyValue(UI_PROPERTY_OPACITY).number != .5) return 1;
    animated.controlAnimation(UI_PROPERTY_OPACITY,UI_ANIMATION_STOP);
    Counter counter;
    auto page = sunfleck::generated::consumer_page::load(app, counter, xml);
    app.openWindow(); app.poll(0); app.activateButton();
    if (auto error = page->takeError()) std::rethrow_exception(error);
    auto label = app.findFleckNode("count");
    if (label.propertyValue(UI_PROPERTY_LINE_HEIGHT).number != 32 || label.propertyValue(UI_PROPERTY_FOREGROUND).integer != 0x80224466 || label.propertyValue(UI_PROPERTY_FLOW_DIRECTION).integer != UI_FLOW_RIGHT_TO_LEFT) return 15;
    if (label.propertyValue(UI_PROPERTY_FONT_SIZE).number != 23.5 || label.propertyValue(UI_PROPERTY_FONT_WEIGHT).integer != UI_FONT_BOLD || label.propertySource(UI_PROPERTY_FONT_SIZE) != UI_PROPERTY_SOURCE_INHERITED) return 14;
    label.setWidth(120.5);
    const auto registeredWidth = label.resolveProperty("Width");
    if (label.describeProperty(registeredWidth).kind != UI_VALUE_LENGTH || label.propertyValue(registeredWidth).number != 120.5) return 12;
    if (label.propertySource(UI_PROPERTY_WIDTH) != UI_PROPERTY_SOURCE_LOCAL) return 3;
    label.clearProperty(UI_PROPERTY_WIDTH);
    if (label.width() != -1 || sunfleck::describeProperty(UI_TEXT_BLOCK, UI_PROPERTY_WIDTH).default_value.integer != UI_LENGTH_AUTO) return 4;
    const auto point = sunfleck::PropertyModel::parse<ui_point>("3.5,-2");
    if (point.x != 3.5 || point.y != -2) return 5;
    ui_binding_value width{}; width.kind = UI_VALUE_INT; width.integer = 100;
    label.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_STYLE, width);
    width.integer = 150;
    label.setPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION, width);
    if (label.width() != 150 || label.basePropertyValue(UI_PROPERTY_WIDTH).number != 100) return 6;
    label.clearPropertyLayer(UI_PROPERTY_WIDTH, UI_PRECEDENCE_ANIMATION);
    if (label.width() != 100 || label.propertySource(UI_PROPERTY_WIDTH) != UI_PROPERTY_SOURCE_STYLE) return 7;
    auto contextNode = app.createNode(UI_TEXT_BLOCK);
    sunfleck::PropertyModel contextModel(app);
    contextModel.property("Caption", [] { return std::string("installed context"); });
    contextNode.bindContext(contextNode.resolveProperty("Text"), "Caption", UI_BIND_ONE_WAY);
    contextNode.setDataContext(contextModel.nativeHandle());
    if (contextNode.text() != "installed context") return 8;
    contextNode.setDataContext(nullptr);
    if (!contextNode.text().empty() || contextNode.dataContextSource() != UI_PROPERTY_SOURCE_LOCAL) return 9;
    auto parent = app.createNode(UI_STACK_PANEL);
    parent.append(contextNode);
    sunfleck::PropertyModel source(app);
    source.property("Selected", [&] { return contextModel.nativeHandle(); });
    parent.setDataContext(source.nativeHandle());
    contextNode.bindContext(UI_PROPERTY_DATA_CONTEXT, "Selected", UI_BIND_ONE_WAY);
    if (contextNode.text() != "installed context" || contextNode.propertySource(UI_PROPERTY_DATA_CONTEXT) != UI_PROPERTY_SOURCE_BINDING) return 10;
    ui_binding_value gap{}; gap.kind = UI_VALUE_INT; gap.integer = 7;
    parent.setPropertyValue(UI_PROPERTY_SPACING, gap);
    if (parent.propertyValue(UI_PROPERTY_SPACING).number != 7) return 11;
    parent.setLayout(1.25, 2.75);
    if (parent.propertyValue(UI_PROPERTY_SPACING).number != 1.25 || parent.propertyValue(UI_PROPERTY_PADDING).number != 2.75) return 16;
    label.setRenderTransform({1,0,0,1,8,4});
    app.poll(0);
    const auto local = label.fromWindow(label.toWindow({1,2}));
    if (local.x != 1 || local.y != 2) return 17;
    return counter.Count == 1 && app.findFleckNode("count").text() == "1" ? 0 : 2;
}
