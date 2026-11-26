// Generated from fleckc --schema. Do not edit.
#pragma once
#include <sunfleck/components.hpp>

namespace sunfleck::elements {
template<class T> struct ElementProperty {
    using value_type = T;
    const char *name, *schema_type;
    bool bindable, two_way;
    ui_property target;
    ui_binding_mode mode;
};

class Element : public sunfleck::FrameworkElement {
public:
    explicit Element(Node node) : FrameworkElement(node, nullptr) {}
    template<class T> void bind(const ElementProperty<T>& property, PropertyModel& model,
        const char *path, ui_binding_mode mode = UI_BIND_DEFAULT) {
        if (!property.bindable) { throw Error(UI_ERROR_INVALID_ARGUMENT); }
        model.bind(node(), property.target, path, mode == UI_BIND_DEFAULT ? property.mode : mode);
    }
};

class VisualElement : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> StyleProperty{"Style", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_model *> DataContextProperty{"DataContext", "model", true, false, UI_PROPERTY_DATA_CONTEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> IdProperty{"Id", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> IsEnabledProperty{"IsEnabled", "bool", true, false, UI_PROPERTY_IS_ENABLED, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_visibility> VisibilityProperty{"Visibility", "visibility", true, false, UI_PROPERTY_VISIBILITY, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_matrix> LayoutTransformProperty{"LayoutTransform", "matrix", true, false, UI_PROPERTY_LAYOUT_TRANSFORM, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_matrix> RenderTransformProperty{"RenderTransform", "matrix", true, false, UI_PROPERTY_RENDER_TRANSFORM, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_point> RenderTransformOriginProperty{"RenderTransformOrigin", "point", true, false, UI_PROPERTY_RENDER_TRANSFORM_ORIGIN, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> OpacityProperty{"Opacity", "number", true, false, UI_PROPERTY_OPACITY, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> IsFocusScopeProperty{"IsFocusScope", "bool", true, false, UI_PROPERTY_IS_FOCUS_SCOPE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> TabIndexProperty{"TabIndex", "integer", true, false, UI_PROPERTY_TAB_INDEX, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> IsTabStopProperty{"IsTabStop", "bool", true, false, UI_PROPERTY_IS_TAB_STOP, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> ClipToBoundsProperty{"ClipToBounds", "bool", true, false, UI_PROPERTY_CLIP_TO_BOUNDS, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> IsHitTestVisibleProperty{"IsHitTestVisible", "bool", true, false, UI_PROPERTY_IS_HIT_TEST_VISIBLE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> FontFamilyProperty{"FontFamily", "string", true, false, UI_PROPERTY_FONT_FAMILY, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> FontSizeProperty{"FontSize", "number", true, false, UI_PROPERTY_FONT_SIZE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_font_weight> FontWeightProperty{"FontWeight", "fontWeight", true, false, UI_PROPERTY_FONT_WEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_font_style> FontStyleProperty{"FontStyle", "fontStyle", true, false, UI_PROPERTY_FONT_STYLE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_font_stretch> FontStretchProperty{"FontStretch", "fontStretch", true, false, UI_PROPERTY_FONT_STRETCH, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> ForegroundProperty{"Foreground", "color", true, false, UI_PROPERTY_FOREGROUND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_text_alignment> TextAlignmentProperty{"TextAlignment", "textAlignment", true, false, UI_PROPERTY_TEXT_ALIGNMENT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_text_wrapping> TextWrappingProperty{"TextWrapping", "textWrapping", true, false, UI_PROPERTY_TEXT_WRAPPING, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_text_trimming> TextTrimmingProperty{"TextTrimming", "textTrimming", true, false, UI_PROPERTY_TEXT_TRIMMING, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> LineHeightProperty{"LineHeight", "number", true, false, UI_PROPERTY_LINE_HEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_text_decorations> TextDecorationsProperty{"TextDecorations", "textDecorations", true, false, UI_PROPERTY_TEXT_DECORATIONS, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_flow_direction> FlowDirectionProperty{"FlowDirection", "flowDirection", true, false, UI_PROPERTY_FLOW_DIRECTION, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> HeightProperty{"Height", "length", true, false, UI_PROPERTY_HEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> WidthProperty{"Width", "length", true, false, UI_PROPERTY_WIDTH, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> MinWidthProperty{"MinWidth", "length", true, false, UI_PROPERTY_MIN_WIDTH, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> MaxWidthProperty{"MaxWidth", "length", true, false, UI_PROPERTY_MAX_WIDTH, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> MinHeightProperty{"MinHeight", "length", true, false, UI_PROPERTY_MIN_HEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_length> MaxHeightProperty{"MaxHeight", "length", true, false, UI_PROPERTY_MAX_HEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_thickness> MarginProperty{"Margin", "thickness", true, false, UI_PROPERTY_MARGIN, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> HorizontalAlignmentProperty{"HorizontalAlignment", "string", true, false, UI_PROPERTY_HORIZONTAL_ALIGNMENT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> VerticalAlignmentProperty{"VerticalAlignment", "string", true, false, UI_PROPERTY_VERTICAL_ALIGNMENT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WeightProperty{"Weight", "integer", true, false, UI_PROPERTY_WEIGHT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> DockProperty{"Dock", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> GridRowProperty{"Grid.Row", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> GridColumnProperty{"Grid.Column", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> GridRowSpanProperty{"Grid.RowSpan", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> GridColumnSpanProperty{"Grid.ColumnSpan", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PanelZIndexProperty{"Panel.ZIndex", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> CanvasLeftProperty{"Canvas.Left", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> CanvasTopProperty{"Canvas.Top", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class LayoutElement : public VisualElement {
public:
    using VisualElement::VisualElement;
    inline static constexpr ElementProperty<double> SpacingProperty{"Spacing", "number", true, false, UI_PROPERTY_SPACING, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> PaddingProperty{"Padding", "number", true, false, UI_PROPERTY_PADDING, UI_BIND_ONE_WAY};
};

class StackPanel : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit StackPanel(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(0))) {}
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Flex : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Flex(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(1))) {}
};

class Grid : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Grid(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(2))) {}
    inline static constexpr ElementProperty<std::string> RowDefinitionsProperty{"RowDefinitions", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ColumnDefinitionsProperty{"ColumnDefinitions", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ScrollView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ScrollView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(3))) {}
    inline static constexpr ElementProperty<std::string> HorizontalScrollBarVisibilityProperty{"HorizontalScrollBarVisibility", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> VerticalScrollBarVisibilityProperty{"VerticalScrollBarVisibility", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Panel : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Panel(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(4))) {}
};

class Card : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Card(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(5))) {}
};

class Button : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Button(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(6))) {}
    inline static constexpr ElementProperty<std::string> DragTextProperty{"DragText", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ContentProperty{"Content", "string", true, false, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TextBlock : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit TextBlock(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(7))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Image : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Image(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(8))) {}
    inline static constexpr ElementProperty<std::string> SourceProperty{"Source", "string", true, false, UI_PROPERTY_SOURCE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> StretchProperty{"Stretch", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class CheckBox : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit CheckBox(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(9))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class RadioButton : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit RadioButton(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(10))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Switch : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Switch(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(11))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Slider : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Slider(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(12))) {}
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Separator : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Separator(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(13))) {}
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ProgressBar : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit ProgressBar(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(14))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, false, UI_PROPERTY_VALUE, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Popup : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Popup(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(15))) {}
    inline static constexpr ElementProperty<std::string> AnchorProperty{"Anchor", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ComboBox : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ComboBox(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(16))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class TextBox : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit TextBox(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(17))) {}
    inline static constexpr ElementProperty<bool> AllowDropProperty{"AllowDrop", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, true, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ItemsControl : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ItemsControl(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(18))) {}
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class ListView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ListView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(19))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class ListItem : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit ListItem(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(20))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TabView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit TabView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(21))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TabItem : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit TabItem(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(22))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Overlay : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Overlay(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(23))) {}
};

class WrapPanel : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit WrapPanel(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(24))) {}
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DockPanel : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockPanel(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(25))) {}
};

class SplitView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit SplitView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(26))) {}
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
};

class Label : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Label(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(27))) {}
    inline static constexpr ElementProperty<std::string> AnchorProperty{"Anchor", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Icon : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Icon(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(28))) {}
    inline static constexpr ElementProperty<std::string> StretchProperty{"Stretch", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Badge : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Badge(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(29))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class IconButton : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit IconButton(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(30))) {}
    inline static constexpr ElementProperty<std::string> DragTextProperty{"DragText", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ToggleButton : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit ToggleButton(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(31))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Hyperlink : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Hyperlink(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(32))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class SpinBox : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit SpinBox(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(33))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", true, true, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<std::string> TemplateProperty{"Template", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class PasswordBox : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit PasswordBox(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(34))) {}
    inline static constexpr ElementProperty<bool> AllowDropProperty{"AllowDrop", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, true, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TextArea : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit TextArea(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(35))) {}
    inline static constexpr ElementProperty<bool> AllowDropProperty{"AllowDrop", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, true, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class SearchBox : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit SearchBox(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(36))) {}
    inline static constexpr ElementProperty<bool> AllowDropProperty{"AllowDrop", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, true, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TreeView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit TreeView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(37))) {}
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class DataGrid : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DataGrid(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(38))) {}
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class MenuBar : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit MenuBar(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(39))) {}
};

class ToolBar : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ToolBar(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(40))) {}
};

class NavigationView : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit NavigationView(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(41))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Dialog : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Dialog(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(42))) {}
    inline static constexpr ElementProperty<std::string> AnchorProperty{"Anchor", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Tooltip : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Tooltip(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(43))) {}
    inline static constexpr ElementProperty<std::string> AnchorProperty{"Anchor", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ContextMenu : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ContextMenu(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(44))) {}
    inline static constexpr ElementProperty<std::string> AnchorProperty{"Anchor", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Spinner : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Spinner(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(45))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class InfoBar : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit InfoBar(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(46))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class TreeItem : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit TreeItem(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(47))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_model *> ItemsSourceProperty{"ItemsSource", "model", true, false, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
};

class DockHost : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockHost(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(48))) {}
};

class DockSplit : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockSplit(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(49))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RatioProperty{"Ratio", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DockGroup : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockGroup(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(50))) {}
    inline static constexpr ElementProperty<int> ValueProperty{"Value", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DockDocument : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockDocument(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(51))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TitleProperty{"Title", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> IconProperty{"Icon", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanCloseProperty{"CanClose", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanFloatProperty{"CanFloat", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanAutoHideProperty{"CanAutoHide", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DockTool : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit DockTool(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(52))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TitleProperty{"Title", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> IconProperty{"Icon", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanCloseProperty{"CanClose", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanFloatProperty{"CanFloat", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CanAutoHideProperty{"CanAutoHide", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentView : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit DocumentView(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(53))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_command *> CommandProperty{"Command", "command", true, false, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> CommandModeProperty{"CommandMode", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Border : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Border(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(54))) {}
    inline static constexpr ElementProperty<ui_color> BackgroundProperty{"Background", "color", true, false, UI_PROPERTY_BACKGROUND, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> BorderBrushProperty{"BorderBrush", "color", true, false, UI_PROPERTY_BORDER_BRUSH, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_thickness> BorderThicknessProperty{"BorderThickness", "thickness", true, false, UI_PROPERTY_BORDER_THICKNESS, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_corner_radius> CornerRadiusProperty{"CornerRadius", "cornerRadius", true, false, UI_PROPERTY_CORNER_RADIUS, UI_BIND_ONE_WAY};
};

class Run : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Run(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(55))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Span : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Span(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(56))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Canvas : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Canvas(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(57))) {}
};

class ContentControl : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ContentControl(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(58))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ContentProperty{"Content", "string", true, false, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
};

class ContentPresenter : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit ContentPresenter(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(59))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ContentProperty{"Content", "string", true, false, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
};

class Viewbox : public LayoutElement {
public:
    using LayoutElement::LayoutElement;
    explicit Viewbox(App& app) : LayoutElement(app.createNode(static_cast<ui_node_kind>(60))) {}
    inline static constexpr ElementProperty<std::string> StretchProperty{"Stretch", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Rectangle : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Rectangle(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(61))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> FillProperty{"Fill", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> StrokeProperty{"Stroke", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> StrokeThicknessProperty{"StrokeThickness", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Ellipse : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Ellipse(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(62))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> FillProperty{"Fill", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> StrokeProperty{"Stroke", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> StrokeThicknessProperty{"StrokeThickness", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Line : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Line(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(63))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> FillProperty{"Fill", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> StrokeProperty{"Stroke", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> StrokeThicknessProperty{"StrokeThickness", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class Path : public VisualElement {
public:
    using VisualElement::VisualElement;
    explicit Path(App& app) : VisualElement(app.createNode(static_cast<ui_node_kind>(64))) {}
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", true, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> FillProperty{"Fill", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<ui_color> StrokeProperty{"Stroke", "color", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<double> StrokeThicknessProperty{"StrokeThickness", "number", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> DataProperty{"Data", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentWindow : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> xmlnsProperty{"xmlns", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ModelProperty{"Model", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TitleProperty{"Title", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentUserControl : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> xmlnsProperty{"xmlns", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ClassProperty{"Class", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> NamespaceProperty{"Namespace", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ModelProperty{"Model", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TargetTypeProperty{"TargetType", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentControlTemplate : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> xmlnsProperty{"xmlns", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ClassProperty{"Class", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> NamespaceProperty{"Namespace", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ModelProperty{"Model", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TargetTypeProperty{"TargetType", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentTheme : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> xmlnsProperty{"xmlns", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> VersionProperty{"Version", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> BaseProperty{"Base", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> DarkProperty{"Dark", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<bool> CompactProperty{"Compact", "bool", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> SkinProperty{"Skin", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentResource : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> KeyProperty{"Key", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ValueProperty{"Value", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class DocumentDockTemplate : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> PartProperty{"Part", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> StateProperty{"State", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ThemeFill : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> BackgroundProperty{"Background", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ForegroundProperty{"Foreground", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> RoleProperty{"Role", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RadiusProperty{"Radius", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ThicknessProperty{"Thickness", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PaddingProperty{"Padding", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ThemeBorder : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> BackgroundProperty{"Background", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ForegroundProperty{"Foreground", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> RoleProperty{"Role", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RadiusProperty{"Radius", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ThicknessProperty{"Thickness", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PaddingProperty{"Padding", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ThemeText : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> BackgroundProperty{"Background", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ForegroundProperty{"Foreground", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> RoleProperty{"Role", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RadiusProperty{"Radius", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ThicknessProperty{"Thickness", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PaddingProperty{"Padding", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ThemeButton : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> BackgroundProperty{"Background", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ForegroundProperty{"Foreground", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> RoleProperty{"Role", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RadiusProperty{"Radius", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ThicknessProperty{"Thickness", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PaddingProperty{"Padding", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

class ThemeStack : public Element {
public:
    using Element::Element;
    inline static constexpr ElementProperty<std::string> BackgroundProperty{"Background", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> ForegroundProperty{"Foreground", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> TextProperty{"Text", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> RoleProperty{"Role", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<std::string> OrientationProperty{"Orientation", "string", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> RadiusProperty{"Radius", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> ThicknessProperty{"Thickness", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> WidthProperty{"Width", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> HeightProperty{"Height", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
    inline static constexpr ElementProperty<int> PaddingProperty{"Padding", "integer", false, false, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
};

} // namespace sunfleck::elements
