// Generated from fleckc --schema. Do not edit.
#pragma once
#include <sunfleck/component_attributes.h>

typedef struct ui_element_property {
    const char *name, *schema_type;
    ui_value_kind kind;
    uint32_t bindable, two_way;
    ui_property target;
    ui_binding_mode mode;
} ui_element_property;

static inline ui_result ui_element_bind(ui_node *node, const ui_element_property *property,
    ui_model *model, const char *path, ui_binding_mode mode) {
    if (!property || !property->bindable) { return UI_ERROR_INVALID_ARGUMENT; }
    ui_binding_options options = {sizeof(options), 1, property->target, path,
        mode == UI_BIND_DEFAULT ? property->mode : mode, UI_UPDATE_PROPERTY_CHANGED, ""};
    return ui_node_bind_property(node, model, &options);
}

typedef struct ui_VisualElement { ui_framework_element base; ui_node *node; } ui_VisualElement;
static const ui_element_property ui_VisualElement_StyleProperty = {"Style", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_DataContextProperty = {"DataContext", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_DATA_CONTEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_IdProperty = {"Id", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_IsEnabledProperty = {"IsEnabled", "bool", UI_VALUE_BOOL, 1, 0, UI_PROPERTY_IS_ENABLED, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_VisibilityProperty = {"Visibility", "visibility", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_VISIBILITY, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_LayoutTransformProperty = {"LayoutTransform", "matrix", UI_VALUE_MATRIX, 1, 0, UI_PROPERTY_LAYOUT_TRANSFORM, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_RenderTransformProperty = {"RenderTransform", "matrix", UI_VALUE_MATRIX, 1, 0, UI_PROPERTY_RENDER_TRANSFORM, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_RenderTransformOriginProperty = {"RenderTransformOrigin", "point", UI_VALUE_POINT, 1, 0, UI_PROPERTY_RENDER_TRANSFORM_ORIGIN, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_OpacityProperty = {"Opacity", "number", UI_VALUE_DOUBLE, 1, 0, UI_PROPERTY_OPACITY, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_IsFocusScopeProperty = {"IsFocusScope", "bool", UI_VALUE_BOOL, 1, 0, UI_PROPERTY_IS_FOCUS_SCOPE, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_TabIndexProperty = {"TabIndex", "integer", UI_VALUE_INT, 1, 0, UI_PROPERTY_TAB_INDEX, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_IsTabStopProperty = {"IsTabStop", "bool", UI_VALUE_BOOL, 1, 0, UI_PROPERTY_IS_TAB_STOP, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_ClipToBoundsProperty = {"ClipToBounds", "bool", UI_VALUE_BOOL, 1, 0, UI_PROPERTY_CLIP_TO_BOUNDS, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_IsHitTestVisibleProperty = {"IsHitTestVisible", "bool", UI_VALUE_BOOL, 1, 0, UI_PROPERTY_IS_HIT_TEST_VISIBLE, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FontFamilyProperty = {"FontFamily", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_FONT_FAMILY, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FontSizeProperty = {"FontSize", "number", UI_VALUE_DOUBLE, 1, 0, UI_PROPERTY_FONT_SIZE, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FontWeightProperty = {"FontWeight", "fontWeight", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_FONT_WEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FontStyleProperty = {"FontStyle", "fontStyle", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_FONT_STYLE, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FontStretchProperty = {"FontStretch", "fontStretch", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_FONT_STRETCH, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_ForegroundProperty = {"Foreground", "color", UI_VALUE_COLOR, 1, 0, UI_PROPERTY_FOREGROUND, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_TextAlignmentProperty = {"TextAlignment", "textAlignment", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_TEXT_ALIGNMENT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_TextWrappingProperty = {"TextWrapping", "textWrapping", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_TEXT_WRAPPING, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_TextTrimmingProperty = {"TextTrimming", "textTrimming", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_TEXT_TRIMMING, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_LineHeightProperty = {"LineHeight", "number", UI_VALUE_DOUBLE, 1, 0, UI_PROPERTY_LINE_HEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_TextDecorationsProperty = {"TextDecorations", "textDecorations", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_TEXT_DECORATIONS, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_FlowDirectionProperty = {"FlowDirection", "flowDirection", UI_VALUE_ENUM, 1, 0, UI_PROPERTY_FLOW_DIRECTION, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_HeightProperty = {"Height", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_HEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_WidthProperty = {"Width", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_WIDTH, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_MinWidthProperty = {"MinWidth", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_MIN_WIDTH, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_MaxWidthProperty = {"MaxWidth", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_MAX_WIDTH, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_MinHeightProperty = {"MinHeight", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_MIN_HEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_MaxHeightProperty = {"MaxHeight", "length", UI_VALUE_LENGTH, 1, 0, UI_PROPERTY_MAX_HEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_MarginProperty = {"Margin", "thickness", UI_VALUE_THICKNESS, 1, 0, UI_PROPERTY_MARGIN, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_HorizontalAlignmentProperty = {"HorizontalAlignment", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_HORIZONTAL_ALIGNMENT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_VerticalAlignmentProperty = {"VerticalAlignment", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_VERTICAL_ALIGNMENT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_WeightProperty = {"Weight", "integer", UI_VALUE_INT, 1, 0, UI_PROPERTY_WEIGHT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_DockProperty = {"Dock", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_GridRowProperty = {"Grid.Row", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_GridColumnProperty = {"Grid.Column", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_GridRowSpanProperty = {"Grid.RowSpan", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_GridColumnSpanProperty = {"Grid.ColumnSpan", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_PanelZIndexProperty = {"Panel.ZIndex", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_CanvasLeftProperty = {"Canvas.Left", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_VisualElement_CanvasTopProperty = {"Canvas.Top", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_LayoutElement { ui_VisualElement base; } ui_LayoutElement;
static const ui_element_property ui_LayoutElement_SpacingProperty = {"Spacing", "number", UI_VALUE_DOUBLE, 1, 0, UI_PROPERTY_SPACING, UI_BIND_ONE_WAY};
static const ui_element_property ui_LayoutElement_PaddingProperty = {"Padding", "number", UI_VALUE_DOUBLE, 1, 0, UI_PROPERTY_PADDING, UI_BIND_ONE_WAY};
#define ui_LayoutElement_StyleProperty ui_VisualElement_StyleProperty
#define ui_LayoutElement_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_LayoutElement_IdProperty ui_VisualElement_IdProperty
#define ui_LayoutElement_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_LayoutElement_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_LayoutElement_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_LayoutElement_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_LayoutElement_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_LayoutElement_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_LayoutElement_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_LayoutElement_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_LayoutElement_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_LayoutElement_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_LayoutElement_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_LayoutElement_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_LayoutElement_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_LayoutElement_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_LayoutElement_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_LayoutElement_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_LayoutElement_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_LayoutElement_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_LayoutElement_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_LayoutElement_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_LayoutElement_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_LayoutElement_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_LayoutElement_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_LayoutElement_HeightProperty ui_VisualElement_HeightProperty
#define ui_LayoutElement_WidthProperty ui_VisualElement_WidthProperty
#define ui_LayoutElement_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_LayoutElement_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_LayoutElement_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_LayoutElement_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_LayoutElement_MarginProperty ui_VisualElement_MarginProperty
#define ui_LayoutElement_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_LayoutElement_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_LayoutElement_WeightProperty ui_VisualElement_WeightProperty
#define ui_LayoutElement_DockProperty ui_VisualElement_DockProperty
#define ui_LayoutElement_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_LayoutElement_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_LayoutElement_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_LayoutElement_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_LayoutElement_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_LayoutElement_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_LayoutElement_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_StackPanel { ui_LayoutElement base; } ui_StackPanel;
static inline ui_node *ui_StackPanel_node(const ui_StackPanel *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_StackPanel_create(ui_app *app, ui_StackPanel *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)0, &out->base.base.node);
}
#define ui_StackPanel_IdProperty ui_VisualElement_IdProperty
#define ui_StackPanel_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_StackPanel_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_StackPanel_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_StackPanel_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_StackPanel_StyleProperty ui_VisualElement_StyleProperty
#define ui_StackPanel_HeightProperty ui_VisualElement_HeightProperty
#define ui_StackPanel_WidthProperty ui_VisualElement_WidthProperty
#define ui_StackPanel_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_StackPanel_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_StackPanel_MarginProperty ui_VisualElement_MarginProperty
#define ui_StackPanel_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_StackPanel_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_StackPanel_WeightProperty ui_VisualElement_WeightProperty
#define ui_StackPanel_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_StackPanel_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_StackPanel_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_StackPanel_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_StackPanel_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_StackPanel_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_StackPanel_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_StackPanel_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_StackPanel_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_StackPanel_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_StackPanel_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_StackPanel_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_StackPanel_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_StackPanel_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_StackPanel_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_StackPanel_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_StackPanel_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_StackPanel_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_StackPanel_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_StackPanel_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_StackPanel_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_StackPanel_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_StackPanel_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_StackPanel_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_StackPanel_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_StackPanel_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_StackPanel_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_StackPanel_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_StackPanel_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_StackPanel_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_StackPanel_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_StackPanel_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_StackPanel_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Flex { ui_LayoutElement base; } ui_Flex;
static inline ui_node *ui_Flex_node(const ui_Flex *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Flex_create(ui_app *app, ui_Flex *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)1, &out->base.base.node);
}
#define ui_Flex_IdProperty ui_VisualElement_IdProperty
#define ui_Flex_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Flex_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Flex_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Flex_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Flex_StyleProperty ui_VisualElement_StyleProperty
#define ui_Flex_HeightProperty ui_VisualElement_HeightProperty
#define ui_Flex_WidthProperty ui_VisualElement_WidthProperty
#define ui_Flex_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Flex_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Flex_MarginProperty ui_VisualElement_MarginProperty
#define ui_Flex_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Flex_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Flex_WeightProperty ui_VisualElement_WeightProperty
#define ui_Flex_DockProperty ui_VisualElement_DockProperty
#define ui_Flex_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Flex_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Flex_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Flex_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Flex_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Flex_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Flex_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Flex_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Flex_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Flex_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Flex_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Flex_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Flex_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Flex_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Flex_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Flex_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Flex_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Flex_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Flex_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Flex_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Flex_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Flex_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Flex_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Flex_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Flex_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Flex_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Flex_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Flex_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Flex_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Flex_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Flex_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Grid { ui_LayoutElement base; } ui_Grid;
static inline ui_node *ui_Grid_node(const ui_Grid *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Grid_create(ui_app *app, ui_Grid *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)2, &out->base.base.node);
}
#define ui_Grid_IdProperty ui_VisualElement_IdProperty
#define ui_Grid_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Grid_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Grid_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Grid_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Grid_StyleProperty ui_VisualElement_StyleProperty
#define ui_Grid_HeightProperty ui_VisualElement_HeightProperty
#define ui_Grid_WidthProperty ui_VisualElement_WidthProperty
#define ui_Grid_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Grid_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Grid_MarginProperty ui_VisualElement_MarginProperty
#define ui_Grid_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Grid_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Grid_WeightProperty ui_VisualElement_WeightProperty
#define ui_Grid_DockProperty ui_VisualElement_DockProperty
#define ui_Grid_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Grid_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Grid_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Grid_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Grid_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Grid_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Grid_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Grid_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Grid_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Grid_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Grid_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Grid_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Grid_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Grid_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Grid_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Grid_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Grid_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Grid_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Grid_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Grid_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Grid_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Grid_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Grid_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Grid_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
static const ui_element_property ui_Grid_RowDefinitionsProperty = {"RowDefinitions", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Grid_ColumnDefinitionsProperty = {"ColumnDefinitions", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Grid_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Grid_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Grid_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Grid_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Grid_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Grid_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Grid_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ScrollView { ui_LayoutElement base; } ui_ScrollView;
static inline ui_node *ui_ScrollView_node(const ui_ScrollView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ScrollView_create(ui_app *app, ui_ScrollView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)3, &out->base.base.node);
}
#define ui_ScrollView_IdProperty ui_VisualElement_IdProperty
#define ui_ScrollView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ScrollView_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_ScrollView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ScrollView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ScrollView_StyleProperty ui_VisualElement_StyleProperty
#define ui_ScrollView_HeightProperty ui_VisualElement_HeightProperty
#define ui_ScrollView_WidthProperty ui_VisualElement_WidthProperty
#define ui_ScrollView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ScrollView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ScrollView_MarginProperty ui_VisualElement_MarginProperty
#define ui_ScrollView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ScrollView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ScrollView_WeightProperty ui_VisualElement_WeightProperty
#define ui_ScrollView_DockProperty ui_VisualElement_DockProperty
#define ui_ScrollView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ScrollView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ScrollView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ScrollView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ScrollView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ScrollView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ScrollView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ScrollView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ScrollView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ScrollView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ScrollView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ScrollView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ScrollView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ScrollView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ScrollView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ScrollView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ScrollView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ScrollView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ScrollView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ScrollView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ScrollView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ScrollView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ScrollView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ScrollView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ScrollView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ScrollView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ScrollView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ScrollView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ScrollView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ScrollView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ScrollView_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_ScrollView_HorizontalScrollBarVisibilityProperty = {"HorizontalScrollBarVisibility", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ScrollView_VerticalScrollBarVisibilityProperty = {"VerticalScrollBarVisibility", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Panel { ui_LayoutElement base; } ui_Panel;
static inline ui_node *ui_Panel_node(const ui_Panel *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Panel_create(ui_app *app, ui_Panel *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)4, &out->base.base.node);
}
#define ui_Panel_IdProperty ui_VisualElement_IdProperty
#define ui_Panel_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Panel_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Panel_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Panel_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Panel_StyleProperty ui_VisualElement_StyleProperty
#define ui_Panel_HeightProperty ui_VisualElement_HeightProperty
#define ui_Panel_WidthProperty ui_VisualElement_WidthProperty
#define ui_Panel_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Panel_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Panel_MarginProperty ui_VisualElement_MarginProperty
#define ui_Panel_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Panel_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Panel_WeightProperty ui_VisualElement_WeightProperty
#define ui_Panel_DockProperty ui_VisualElement_DockProperty
#define ui_Panel_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Panel_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Panel_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Panel_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Panel_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Panel_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Panel_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Panel_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Panel_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Panel_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Panel_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Panel_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Panel_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Panel_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Panel_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Panel_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Panel_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Panel_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Panel_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Panel_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Panel_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Panel_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Panel_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Panel_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Panel_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Panel_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Panel_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Panel_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Panel_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Panel_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Panel_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Card { ui_LayoutElement base; } ui_Card;
static inline ui_node *ui_Card_node(const ui_Card *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Card_create(ui_app *app, ui_Card *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)5, &out->base.base.node);
}
#define ui_Card_IdProperty ui_VisualElement_IdProperty
#define ui_Card_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Card_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Card_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Card_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Card_StyleProperty ui_VisualElement_StyleProperty
#define ui_Card_HeightProperty ui_VisualElement_HeightProperty
#define ui_Card_WidthProperty ui_VisualElement_WidthProperty
#define ui_Card_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Card_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Card_MarginProperty ui_VisualElement_MarginProperty
#define ui_Card_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Card_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Card_WeightProperty ui_VisualElement_WeightProperty
#define ui_Card_DockProperty ui_VisualElement_DockProperty
#define ui_Card_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Card_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Card_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Card_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Card_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Card_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Card_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Card_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Card_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Card_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Card_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Card_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Card_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Card_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Card_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Card_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Card_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Card_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Card_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Card_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Card_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Card_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Card_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Card_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Card_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Card_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Card_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Card_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Card_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Card_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Card_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Button { ui_LayoutElement base; } ui_Button;
static inline ui_node *ui_Button_node(const ui_Button *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Button_create(ui_app *app, ui_Button *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)6, &out->base.base.node);
}
#define ui_Button_IdProperty ui_VisualElement_IdProperty
#define ui_Button_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_Button_DragTextProperty = {"DragText", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Button_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Button_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Button_ContentProperty = {"Content", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Button_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_Button_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Button_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Button_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Button_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Button_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_Button_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Button_HeightProperty ui_VisualElement_HeightProperty
#define ui_Button_WidthProperty ui_VisualElement_WidthProperty
#define ui_Button_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Button_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Button_MarginProperty ui_VisualElement_MarginProperty
#define ui_Button_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Button_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Button_WeightProperty ui_VisualElement_WeightProperty
#define ui_Button_DockProperty ui_VisualElement_DockProperty
#define ui_Button_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Button_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Button_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Button_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Button_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Button_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Button_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Button_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Button_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Button_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Button_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Button_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Button_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Button_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Button_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Button_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Button_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Button_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Button_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Button_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Button_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Button_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Button_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Button_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Button_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Button_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Button_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Button_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Button_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Button_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Button_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TextBlock { ui_VisualElement base; } ui_TextBlock;
static inline ui_node *ui_TextBlock_node(const ui_TextBlock *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_TextBlock_create(ui_app *app, ui_TextBlock *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)7, &out->base.node);
}
#define ui_TextBlock_IdProperty ui_VisualElement_IdProperty
#define ui_TextBlock_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_TextBlock_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TextBlock_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TextBlock_StyleProperty ui_VisualElement_StyleProperty
#define ui_TextBlock_HeightProperty ui_VisualElement_HeightProperty
#define ui_TextBlock_WidthProperty ui_VisualElement_WidthProperty
#define ui_TextBlock_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TextBlock_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TextBlock_MarginProperty ui_VisualElement_MarginProperty
#define ui_TextBlock_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TextBlock_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TextBlock_WeightProperty ui_VisualElement_WeightProperty
#define ui_TextBlock_DockProperty ui_VisualElement_DockProperty
#define ui_TextBlock_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TextBlock_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TextBlock_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TextBlock_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TextBlock_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TextBlock_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TextBlock_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TextBlock_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TextBlock_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TextBlock_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TextBlock_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TextBlock_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TextBlock_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TextBlock_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TextBlock_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TextBlock_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TextBlock_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TextBlock_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TextBlock_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TextBlock_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TextBlock_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TextBlock_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TextBlock_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TextBlock_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TextBlock_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TextBlock_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TextBlock_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TextBlock_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TextBlock_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TextBlock_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TextBlock_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Image { ui_VisualElement base; } ui_Image;
static inline ui_node *ui_Image_node(const ui_Image *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Image_create(ui_app *app, ui_Image *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)8, &out->base.node);
}
#define ui_Image_IdProperty ui_VisualElement_IdProperty
#define ui_Image_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Image_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Image_StyleProperty ui_VisualElement_StyleProperty
#define ui_Image_HeightProperty ui_VisualElement_HeightProperty
#define ui_Image_WidthProperty ui_VisualElement_WidthProperty
#define ui_Image_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Image_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Image_MarginProperty ui_VisualElement_MarginProperty
#define ui_Image_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Image_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Image_WeightProperty ui_VisualElement_WeightProperty
#define ui_Image_DockProperty ui_VisualElement_DockProperty
#define ui_Image_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Image_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Image_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Image_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Image_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Image_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Image_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Image_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Image_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Image_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Image_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Image_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Image_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Image_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Image_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Image_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Image_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Image_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Image_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Image_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Image_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Image_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Image_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Image_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Image_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Image_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Image_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Image_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Image_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Image_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Image_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Image_SourceProperty = {"Source", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_SOURCE, UI_BIND_ONE_WAY};
static const ui_element_property ui_Image_StretchProperty = {"Stretch", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_CheckBox { ui_VisualElement base; } ui_CheckBox;
static inline ui_node *ui_CheckBox_node(const ui_CheckBox *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_CheckBox_create(ui_app *app, ui_CheckBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)9, &out->base.node);
}
#define ui_CheckBox_IdProperty ui_VisualElement_IdProperty
#define ui_CheckBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_CheckBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_CheckBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_CheckBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_CheckBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_CheckBox_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_CheckBox_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_CheckBox_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_CheckBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_CheckBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_CheckBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_CheckBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_CheckBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_CheckBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_CheckBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_CheckBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_CheckBox_DockProperty ui_VisualElement_DockProperty
#define ui_CheckBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_CheckBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_CheckBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_CheckBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_CheckBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_CheckBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_CheckBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_CheckBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_CheckBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_CheckBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_CheckBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_CheckBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_CheckBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_CheckBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_CheckBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_CheckBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_CheckBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_CheckBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_CheckBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_CheckBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_CheckBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_CheckBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_CheckBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_CheckBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_CheckBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_CheckBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_CheckBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_CheckBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_CheckBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_CheckBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_CheckBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_RadioButton { ui_VisualElement base; } ui_RadioButton;
static inline ui_node *ui_RadioButton_node(const ui_RadioButton *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_RadioButton_create(ui_app *app, ui_RadioButton *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)10, &out->base.node);
}
#define ui_RadioButton_IdProperty ui_VisualElement_IdProperty
#define ui_RadioButton_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_RadioButton_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_RadioButton_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_RadioButton_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_RadioButton_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_RadioButton_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_RadioButton_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_RadioButton_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_RadioButton_HeightProperty ui_VisualElement_HeightProperty
#define ui_RadioButton_WidthProperty ui_VisualElement_WidthProperty
#define ui_RadioButton_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_RadioButton_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_RadioButton_MarginProperty ui_VisualElement_MarginProperty
#define ui_RadioButton_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_RadioButton_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_RadioButton_WeightProperty ui_VisualElement_WeightProperty
#define ui_RadioButton_DockProperty ui_VisualElement_DockProperty
#define ui_RadioButton_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_RadioButton_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_RadioButton_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_RadioButton_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_RadioButton_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_RadioButton_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_RadioButton_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_RadioButton_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_RadioButton_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_RadioButton_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_RadioButton_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_RadioButton_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_RadioButton_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_RadioButton_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_RadioButton_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_RadioButton_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_RadioButton_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_RadioButton_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_RadioButton_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_RadioButton_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_RadioButton_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_RadioButton_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_RadioButton_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_RadioButton_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_RadioButton_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_RadioButton_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_RadioButton_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_RadioButton_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_RadioButton_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_RadioButton_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_RadioButton_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Switch { ui_VisualElement base; } ui_Switch;
static inline ui_node *ui_Switch_node(const ui_Switch *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Switch_create(ui_app *app, ui_Switch *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)11, &out->base.node);
}
#define ui_Switch_IdProperty ui_VisualElement_IdProperty
#define ui_Switch_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Switch_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Switch_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Switch_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_Switch_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Switch_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_Switch_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_Switch_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Switch_HeightProperty ui_VisualElement_HeightProperty
#define ui_Switch_WidthProperty ui_VisualElement_WidthProperty
#define ui_Switch_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Switch_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Switch_MarginProperty ui_VisualElement_MarginProperty
#define ui_Switch_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Switch_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Switch_WeightProperty ui_VisualElement_WeightProperty
#define ui_Switch_DockProperty ui_VisualElement_DockProperty
#define ui_Switch_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Switch_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Switch_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Switch_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Switch_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Switch_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Switch_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Switch_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Switch_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Switch_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Switch_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Switch_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Switch_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Switch_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Switch_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Switch_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Switch_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Switch_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Switch_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Switch_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Switch_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Switch_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Switch_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Switch_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Switch_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Switch_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Switch_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Switch_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Switch_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Switch_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Switch_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Slider { ui_VisualElement base; } ui_Slider;
static inline ui_node *ui_Slider_node(const ui_Slider *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Slider_create(ui_app *app, ui_Slider *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)12, &out->base.node);
}
#define ui_Slider_IdProperty ui_VisualElement_IdProperty
#define ui_Slider_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Slider_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Slider_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_Slider_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Slider_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_Slider_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_Slider_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Slider_HeightProperty ui_VisualElement_HeightProperty
#define ui_Slider_WidthProperty ui_VisualElement_WidthProperty
#define ui_Slider_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Slider_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Slider_MarginProperty ui_VisualElement_MarginProperty
#define ui_Slider_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Slider_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Slider_WeightProperty ui_VisualElement_WeightProperty
#define ui_Slider_DockProperty ui_VisualElement_DockProperty
#define ui_Slider_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Slider_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Slider_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Slider_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Slider_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Slider_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Slider_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Slider_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Slider_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Slider_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Slider_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Slider_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Slider_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Slider_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Slider_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Slider_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Slider_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Slider_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Slider_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Slider_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Slider_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Slider_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Slider_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Slider_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Slider_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Slider_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Slider_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Slider_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Slider_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Slider_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Slider_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Separator { ui_VisualElement base; } ui_Separator;
static inline ui_node *ui_Separator_node(const ui_Separator *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Separator_create(ui_app *app, ui_Separator *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)13, &out->base.node);
}
#define ui_Separator_IdProperty ui_VisualElement_IdProperty
#define ui_Separator_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Separator_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Separator_StyleProperty ui_VisualElement_StyleProperty
#define ui_Separator_HeightProperty ui_VisualElement_HeightProperty
#define ui_Separator_WidthProperty ui_VisualElement_WidthProperty
#define ui_Separator_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Separator_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Separator_MarginProperty ui_VisualElement_MarginProperty
#define ui_Separator_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Separator_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Separator_WeightProperty ui_VisualElement_WeightProperty
#define ui_Separator_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_Separator_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Separator_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Separator_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Separator_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Separator_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Separator_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Separator_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Separator_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Separator_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Separator_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Separator_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Separator_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Separator_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Separator_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Separator_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Separator_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Separator_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Separator_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Separator_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Separator_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Separator_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Separator_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Separator_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Separator_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Separator_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Separator_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Separator_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Separator_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Separator_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Separator_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Separator_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Separator_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ProgressBar { ui_VisualElement base; } ui_ProgressBar;
static inline ui_node *ui_ProgressBar_node(const ui_ProgressBar *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_ProgressBar_create(ui_app *app, ui_ProgressBar *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)14, &out->base.node);
}
#define ui_ProgressBar_IdProperty ui_VisualElement_IdProperty
#define ui_ProgressBar_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ProgressBar_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ProgressBar_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 0, UI_PROPERTY_VALUE, UI_BIND_ONE_WAY};
#define ui_ProgressBar_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ProgressBar_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ProgressBar_HeightProperty ui_VisualElement_HeightProperty
#define ui_ProgressBar_WidthProperty ui_VisualElement_WidthProperty
#define ui_ProgressBar_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ProgressBar_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ProgressBar_MarginProperty ui_VisualElement_MarginProperty
#define ui_ProgressBar_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ProgressBar_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ProgressBar_WeightProperty ui_VisualElement_WeightProperty
#define ui_ProgressBar_DockProperty ui_VisualElement_DockProperty
#define ui_ProgressBar_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ProgressBar_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ProgressBar_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ProgressBar_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ProgressBar_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ProgressBar_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ProgressBar_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ProgressBar_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ProgressBar_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ProgressBar_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ProgressBar_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ProgressBar_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ProgressBar_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ProgressBar_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ProgressBar_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ProgressBar_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ProgressBar_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ProgressBar_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ProgressBar_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ProgressBar_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ProgressBar_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ProgressBar_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ProgressBar_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ProgressBar_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ProgressBar_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ProgressBar_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ProgressBar_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ProgressBar_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ProgressBar_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ProgressBar_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ProgressBar_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Popup { ui_LayoutElement base; } ui_Popup;
static inline ui_node *ui_Popup_node(const ui_Popup *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Popup_create(ui_app *app, ui_Popup *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)15, &out->base.base.node);
}
#define ui_Popup_IdProperty ui_VisualElement_IdProperty
static const ui_element_property ui_Popup_AnchorProperty = {"Anchor", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Popup_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Popup_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Popup_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Popup_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Popup_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Popup_StyleProperty ui_VisualElement_StyleProperty
#define ui_Popup_HeightProperty ui_VisualElement_HeightProperty
#define ui_Popup_WidthProperty ui_VisualElement_WidthProperty
#define ui_Popup_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Popup_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Popup_MarginProperty ui_VisualElement_MarginProperty
#define ui_Popup_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Popup_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Popup_WeightProperty ui_VisualElement_WeightProperty
#define ui_Popup_DockProperty ui_VisualElement_DockProperty
#define ui_Popup_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Popup_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Popup_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Popup_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Popup_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Popup_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Popup_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Popup_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Popup_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Popup_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Popup_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Popup_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Popup_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Popup_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Popup_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Popup_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Popup_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Popup_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Popup_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Popup_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Popup_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Popup_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Popup_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Popup_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Popup_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Popup_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Popup_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Popup_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Popup_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Popup_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Popup_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ComboBox { ui_LayoutElement base; } ui_ComboBox;
static inline ui_node *ui_ComboBox_node(const ui_ComboBox *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ComboBox_create(ui_app *app, ui_ComboBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)16, &out->base.base.node);
}
#define ui_ComboBox_IdProperty ui_VisualElement_IdProperty
#define ui_ComboBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ComboBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ComboBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ComboBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_ComboBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ComboBox_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ComboBox_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ComboBox_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ComboBox_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ComboBox_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_ComboBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_ComboBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_ComboBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ComboBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ComboBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_ComboBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ComboBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ComboBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_ComboBox_DockProperty ui_VisualElement_DockProperty
#define ui_ComboBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ComboBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ComboBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ComboBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ComboBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ComboBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ComboBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ComboBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ComboBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ComboBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ComboBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ComboBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ComboBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ComboBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ComboBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ComboBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ComboBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ComboBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ComboBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ComboBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ComboBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ComboBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ComboBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ComboBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ComboBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ComboBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ComboBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ComboBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ComboBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ComboBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ComboBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TextBox { ui_VisualElement base; } ui_TextBox;
static inline ui_node *ui_TextBox_node(const ui_TextBox *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_TextBox_create(ui_app *app, ui_TextBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)17, &out->base.node);
}
#define ui_TextBox_IdProperty ui_VisualElement_IdProperty
#define ui_TextBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_TextBox_AllowDropProperty = {"AllowDrop", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TextBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TextBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 1, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
static const ui_element_property ui_TextBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_TextBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TextBox_StyleProperty ui_VisualElement_StyleProperty
#define ui_TextBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_TextBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_TextBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TextBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TextBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_TextBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TextBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TextBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_TextBox_DockProperty ui_VisualElement_DockProperty
#define ui_TextBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TextBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TextBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TextBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TextBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TextBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TextBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TextBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TextBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TextBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TextBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TextBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TextBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TextBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TextBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TextBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TextBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TextBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TextBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TextBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TextBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TextBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TextBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TextBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TextBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TextBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TextBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TextBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TextBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TextBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TextBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ItemsControl { ui_LayoutElement base; } ui_ItemsControl;
static inline ui_node *ui_ItemsControl_node(const ui_ItemsControl *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ItemsControl_create(ui_app *app, ui_ItemsControl *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)18, &out->base.base.node);
}
#define ui_ItemsControl_IdProperty ui_VisualElement_IdProperty
#define ui_ItemsControl_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ItemsControl_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_ItemsControl_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ItemsControl_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ItemsControl_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ItemsControl_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_ItemsControl_HeightProperty ui_VisualElement_HeightProperty
#define ui_ItemsControl_WidthProperty ui_VisualElement_WidthProperty
#define ui_ItemsControl_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ItemsControl_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ItemsControl_MarginProperty ui_VisualElement_MarginProperty
#define ui_ItemsControl_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ItemsControl_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ItemsControl_WeightProperty ui_VisualElement_WeightProperty
#define ui_ItemsControl_DockProperty ui_VisualElement_DockProperty
#define ui_ItemsControl_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ItemsControl_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ItemsControl_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ItemsControl_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ItemsControl_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ItemsControl_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ItemsControl_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ItemsControl_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ItemsControl_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ItemsControl_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ItemsControl_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ItemsControl_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ItemsControl_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ItemsControl_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ItemsControl_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ItemsControl_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ItemsControl_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ItemsControl_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ItemsControl_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ItemsControl_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ItemsControl_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ItemsControl_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ItemsControl_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ItemsControl_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ItemsControl_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ItemsControl_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ItemsControl_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ItemsControl_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ItemsControl_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ItemsControl_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ItemsControl_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ListView { ui_LayoutElement base; } ui_ListView;
static inline ui_node *ui_ListView_node(const ui_ListView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ListView_create(ui_app *app, ui_ListView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)19, &out->base.base.node);
}
#define ui_ListView_IdProperty ui_VisualElement_IdProperty
#define ui_ListView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ListView_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ListView_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ListView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ListView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ListView_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ListView_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_ListView_HeightProperty ui_VisualElement_HeightProperty
#define ui_ListView_WidthProperty ui_VisualElement_WidthProperty
#define ui_ListView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ListView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ListView_MarginProperty ui_VisualElement_MarginProperty
#define ui_ListView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ListView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ListView_WeightProperty ui_VisualElement_WeightProperty
#define ui_ListView_DockProperty ui_VisualElement_DockProperty
#define ui_ListView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ListView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ListView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ListView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ListView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ListView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ListView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ListView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ListView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ListView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ListView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ListView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ListView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ListView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ListView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ListView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ListView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ListView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ListView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ListView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ListView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ListView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ListView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ListView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ListView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ListView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ListView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ListView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ListView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ListView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ListView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ListItem { ui_VisualElement base; } ui_ListItem;
static inline ui_node *ui_ListItem_node(const ui_ListItem *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_ListItem_create(ui_app *app, ui_ListItem *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)20, &out->base.node);
}
#define ui_ListItem_IdProperty ui_VisualElement_IdProperty
#define ui_ListItem_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ListItem_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ListItem_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ListItem_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_ListItem_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ListItem_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ListItem_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ListItem_HeightProperty ui_VisualElement_HeightProperty
#define ui_ListItem_WidthProperty ui_VisualElement_WidthProperty
#define ui_ListItem_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ListItem_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ListItem_MarginProperty ui_VisualElement_MarginProperty
#define ui_ListItem_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ListItem_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ListItem_WeightProperty ui_VisualElement_WeightProperty
#define ui_ListItem_DockProperty ui_VisualElement_DockProperty
#define ui_ListItem_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ListItem_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ListItem_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ListItem_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ListItem_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ListItem_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ListItem_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ListItem_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ListItem_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ListItem_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ListItem_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ListItem_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ListItem_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ListItem_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ListItem_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ListItem_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ListItem_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ListItem_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ListItem_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ListItem_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ListItem_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ListItem_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ListItem_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ListItem_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ListItem_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ListItem_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ListItem_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ListItem_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ListItem_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ListItem_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ListItem_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TabView { ui_LayoutElement base; } ui_TabView;
static inline ui_node *ui_TabView_node(const ui_TabView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_TabView_create(ui_app *app, ui_TabView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)21, &out->base.base.node);
}
#define ui_TabView_IdProperty ui_VisualElement_IdProperty
#define ui_TabView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_TabView_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TabView_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TabView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_TabView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_TabView_StyleProperty ui_VisualElement_StyleProperty
#define ui_TabView_HeightProperty ui_VisualElement_HeightProperty
#define ui_TabView_WidthProperty ui_VisualElement_WidthProperty
#define ui_TabView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TabView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TabView_MarginProperty ui_VisualElement_MarginProperty
#define ui_TabView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TabView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TabView_WeightProperty ui_VisualElement_WeightProperty
#define ui_TabView_DockProperty ui_VisualElement_DockProperty
#define ui_TabView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TabView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TabView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TabView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TabView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TabView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TabView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TabView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TabView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TabView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TabView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TabView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TabView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TabView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TabView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TabView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TabView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TabView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TabView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TabView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TabView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TabView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TabView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TabView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TabView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TabView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TabView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TabView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TabView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TabView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TabView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TabItem { ui_LayoutElement base; } ui_TabItem;
static inline ui_node *ui_TabItem_node(const ui_TabItem *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_TabItem_create(ui_app *app, ui_TabItem *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)22, &out->base.base.node);
}
#define ui_TabItem_IdProperty ui_VisualElement_IdProperty
#define ui_TabItem_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_TabItem_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TabItem_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_TabItem_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_TabItem_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TabItem_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_TabItem_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_TabItem_StyleProperty ui_VisualElement_StyleProperty
#define ui_TabItem_HeightProperty ui_VisualElement_HeightProperty
#define ui_TabItem_WidthProperty ui_VisualElement_WidthProperty
#define ui_TabItem_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TabItem_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TabItem_MarginProperty ui_VisualElement_MarginProperty
#define ui_TabItem_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TabItem_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TabItem_WeightProperty ui_VisualElement_WeightProperty
#define ui_TabItem_DockProperty ui_VisualElement_DockProperty
#define ui_TabItem_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TabItem_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TabItem_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TabItem_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TabItem_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TabItem_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TabItem_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TabItem_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TabItem_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TabItem_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TabItem_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TabItem_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TabItem_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TabItem_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TabItem_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TabItem_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TabItem_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TabItem_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TabItem_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TabItem_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TabItem_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TabItem_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TabItem_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TabItem_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TabItem_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TabItem_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TabItem_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TabItem_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TabItem_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TabItem_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TabItem_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Overlay { ui_LayoutElement base; } ui_Overlay;
static inline ui_node *ui_Overlay_node(const ui_Overlay *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Overlay_create(ui_app *app, ui_Overlay *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)23, &out->base.base.node);
}
#define ui_Overlay_IdProperty ui_VisualElement_IdProperty
#define ui_Overlay_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Overlay_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Overlay_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Overlay_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Overlay_StyleProperty ui_VisualElement_StyleProperty
#define ui_Overlay_HeightProperty ui_VisualElement_HeightProperty
#define ui_Overlay_WidthProperty ui_VisualElement_WidthProperty
#define ui_Overlay_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Overlay_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Overlay_MarginProperty ui_VisualElement_MarginProperty
#define ui_Overlay_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Overlay_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Overlay_WeightProperty ui_VisualElement_WeightProperty
#define ui_Overlay_DockProperty ui_VisualElement_DockProperty
#define ui_Overlay_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Overlay_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Overlay_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Overlay_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Overlay_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Overlay_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Overlay_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Overlay_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Overlay_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Overlay_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Overlay_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Overlay_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Overlay_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Overlay_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Overlay_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Overlay_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Overlay_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Overlay_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Overlay_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Overlay_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Overlay_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Overlay_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Overlay_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Overlay_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Overlay_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Overlay_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Overlay_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Overlay_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Overlay_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Overlay_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Overlay_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_WrapPanel { ui_LayoutElement base; } ui_WrapPanel;
static inline ui_node *ui_WrapPanel_node(const ui_WrapPanel *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_WrapPanel_create(ui_app *app, ui_WrapPanel *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)24, &out->base.base.node);
}
#define ui_WrapPanel_IdProperty ui_VisualElement_IdProperty
#define ui_WrapPanel_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_WrapPanel_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_WrapPanel_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_WrapPanel_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_WrapPanel_StyleProperty ui_VisualElement_StyleProperty
#define ui_WrapPanel_HeightProperty ui_VisualElement_HeightProperty
#define ui_WrapPanel_WidthProperty ui_VisualElement_WidthProperty
#define ui_WrapPanel_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_WrapPanel_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_WrapPanel_MarginProperty ui_VisualElement_MarginProperty
#define ui_WrapPanel_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_WrapPanel_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_WrapPanel_WeightProperty ui_VisualElement_WeightProperty
#define ui_WrapPanel_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_WrapPanel_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_WrapPanel_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_WrapPanel_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_WrapPanel_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_WrapPanel_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_WrapPanel_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_WrapPanel_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_WrapPanel_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_WrapPanel_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_WrapPanel_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_WrapPanel_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_WrapPanel_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_WrapPanel_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_WrapPanel_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_WrapPanel_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_WrapPanel_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_WrapPanel_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_WrapPanel_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_WrapPanel_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_WrapPanel_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_WrapPanel_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_WrapPanel_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_WrapPanel_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_WrapPanel_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_WrapPanel_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_WrapPanel_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_WrapPanel_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_WrapPanel_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_WrapPanel_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_WrapPanel_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_WrapPanel_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_WrapPanel_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockPanel { ui_LayoutElement base; } ui_DockPanel;
static inline ui_node *ui_DockPanel_node(const ui_DockPanel *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockPanel_create(ui_app *app, ui_DockPanel *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)25, &out->base.base.node);
}
#define ui_DockPanel_IdProperty ui_VisualElement_IdProperty
#define ui_DockPanel_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockPanel_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_DockPanel_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockPanel_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockPanel_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockPanel_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockPanel_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockPanel_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockPanel_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockPanel_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockPanel_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockPanel_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockPanel_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockPanel_DockProperty ui_VisualElement_DockProperty
#define ui_DockPanel_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockPanel_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockPanel_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockPanel_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockPanel_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockPanel_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockPanel_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockPanel_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockPanel_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockPanel_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockPanel_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockPanel_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockPanel_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockPanel_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockPanel_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockPanel_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockPanel_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockPanel_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockPanel_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockPanel_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockPanel_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockPanel_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockPanel_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockPanel_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockPanel_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockPanel_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockPanel_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockPanel_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockPanel_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockPanel_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockPanel_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_SplitView { ui_LayoutElement base; } ui_SplitView;
static inline ui_node *ui_SplitView_node(const ui_SplitView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_SplitView_create(ui_app *app, ui_SplitView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)26, &out->base.base.node);
}
#define ui_SplitView_IdProperty ui_VisualElement_IdProperty
#define ui_SplitView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_SplitView_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_SplitView_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_SplitView_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_SplitView_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_SplitView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_SplitView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_SplitView_StyleProperty ui_VisualElement_StyleProperty
#define ui_SplitView_HeightProperty ui_VisualElement_HeightProperty
#define ui_SplitView_WidthProperty ui_VisualElement_WidthProperty
#define ui_SplitView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_SplitView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_SplitView_MarginProperty ui_VisualElement_MarginProperty
#define ui_SplitView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_SplitView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_SplitView_WeightProperty ui_VisualElement_WeightProperty
#define ui_SplitView_DockProperty ui_VisualElement_DockProperty
#define ui_SplitView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_SplitView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_SplitView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_SplitView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_SplitView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_SplitView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_SplitView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_SplitView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_SplitView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_SplitView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_SplitView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_SplitView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_SplitView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_SplitView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_SplitView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_SplitView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_SplitView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_SplitView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_SplitView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_SplitView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_SplitView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_SplitView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_SplitView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_SplitView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_SplitView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_SplitView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_SplitView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_SplitView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_SplitView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_SplitView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_SplitView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Label { ui_VisualElement base; } ui_Label;
static inline ui_node *ui_Label_node(const ui_Label *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Label_create(ui_app *app, ui_Label *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)27, &out->base.node);
}
#define ui_Label_IdProperty ui_VisualElement_IdProperty
static const ui_element_property ui_Label_AnchorProperty = {"Anchor", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Label_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Label_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Label_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Label_StyleProperty ui_VisualElement_StyleProperty
#define ui_Label_HeightProperty ui_VisualElement_HeightProperty
#define ui_Label_WidthProperty ui_VisualElement_WidthProperty
#define ui_Label_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Label_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Label_MarginProperty ui_VisualElement_MarginProperty
#define ui_Label_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Label_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Label_WeightProperty ui_VisualElement_WeightProperty
#define ui_Label_DockProperty ui_VisualElement_DockProperty
#define ui_Label_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Label_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Label_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Label_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Label_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Label_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Label_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Label_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Label_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Label_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Label_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Label_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Label_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Label_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Label_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Label_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Label_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Label_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Label_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Label_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Label_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Label_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Label_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Label_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Label_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Label_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Label_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Label_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Label_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Label_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Label_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Icon { ui_VisualElement base; } ui_Icon;
static inline ui_node *ui_Icon_node(const ui_Icon *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Icon_create(ui_app *app, ui_Icon *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)28, &out->base.node);
}
#define ui_Icon_IdProperty ui_VisualElement_IdProperty
#define ui_Icon_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Icon_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Icon_StyleProperty ui_VisualElement_StyleProperty
#define ui_Icon_HeightProperty ui_VisualElement_HeightProperty
#define ui_Icon_WidthProperty ui_VisualElement_WidthProperty
#define ui_Icon_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Icon_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Icon_MarginProperty ui_VisualElement_MarginProperty
#define ui_Icon_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Icon_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Icon_WeightProperty ui_VisualElement_WeightProperty
#define ui_Icon_DockProperty ui_VisualElement_DockProperty
#define ui_Icon_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Icon_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Icon_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Icon_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Icon_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Icon_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Icon_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Icon_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Icon_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Icon_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Icon_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Icon_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Icon_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Icon_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Icon_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Icon_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Icon_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Icon_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Icon_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Icon_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Icon_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Icon_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Icon_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Icon_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Icon_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Icon_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Icon_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Icon_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Icon_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Icon_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Icon_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Icon_StretchProperty = {"Stretch", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Badge { ui_VisualElement base; } ui_Badge;
static inline ui_node *ui_Badge_node(const ui_Badge *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Badge_create(ui_app *app, ui_Badge *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)29, &out->base.node);
}
#define ui_Badge_IdProperty ui_VisualElement_IdProperty
#define ui_Badge_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Badge_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Badge_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Badge_StyleProperty ui_VisualElement_StyleProperty
#define ui_Badge_HeightProperty ui_VisualElement_HeightProperty
#define ui_Badge_WidthProperty ui_VisualElement_WidthProperty
#define ui_Badge_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Badge_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Badge_MarginProperty ui_VisualElement_MarginProperty
#define ui_Badge_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Badge_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Badge_WeightProperty ui_VisualElement_WeightProperty
#define ui_Badge_DockProperty ui_VisualElement_DockProperty
#define ui_Badge_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Badge_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Badge_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Badge_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Badge_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Badge_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Badge_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Badge_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Badge_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Badge_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Badge_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Badge_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Badge_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Badge_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Badge_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Badge_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Badge_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Badge_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Badge_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Badge_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Badge_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Badge_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Badge_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Badge_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Badge_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Badge_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Badge_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Badge_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Badge_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Badge_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Badge_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_IconButton { ui_VisualElement base; } ui_IconButton;
static inline ui_node *ui_IconButton_node(const ui_IconButton *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_IconButton_create(ui_app *app, ui_IconButton *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)30, &out->base.node);
}
#define ui_IconButton_IdProperty ui_VisualElement_IdProperty
#define ui_IconButton_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_IconButton_DragTextProperty = {"DragText", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_IconButton_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_IconButton_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_IconButton_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_IconButton_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_IconButton_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_IconButton_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_IconButton_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_IconButton_HeightProperty ui_VisualElement_HeightProperty
#define ui_IconButton_WidthProperty ui_VisualElement_WidthProperty
#define ui_IconButton_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_IconButton_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_IconButton_MarginProperty ui_VisualElement_MarginProperty
#define ui_IconButton_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_IconButton_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_IconButton_WeightProperty ui_VisualElement_WeightProperty
#define ui_IconButton_DockProperty ui_VisualElement_DockProperty
#define ui_IconButton_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_IconButton_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_IconButton_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_IconButton_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_IconButton_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_IconButton_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_IconButton_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_IconButton_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_IconButton_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_IconButton_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_IconButton_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_IconButton_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_IconButton_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_IconButton_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_IconButton_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_IconButton_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_IconButton_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_IconButton_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_IconButton_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_IconButton_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_IconButton_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_IconButton_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_IconButton_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_IconButton_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_IconButton_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_IconButton_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_IconButton_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_IconButton_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_IconButton_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_IconButton_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_IconButton_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ToggleButton { ui_VisualElement base; } ui_ToggleButton;
static inline ui_node *ui_ToggleButton_node(const ui_ToggleButton *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_ToggleButton_create(ui_app *app, ui_ToggleButton *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)31, &out->base.node);
}
#define ui_ToggleButton_IdProperty ui_VisualElement_IdProperty
#define ui_ToggleButton_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ToggleButton_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ToggleButton_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ToggleButton_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_ToggleButton_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ToggleButton_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_ToggleButton_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_ToggleButton_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ToggleButton_HeightProperty ui_VisualElement_HeightProperty
#define ui_ToggleButton_WidthProperty ui_VisualElement_WidthProperty
#define ui_ToggleButton_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ToggleButton_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ToggleButton_MarginProperty ui_VisualElement_MarginProperty
#define ui_ToggleButton_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ToggleButton_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ToggleButton_WeightProperty ui_VisualElement_WeightProperty
#define ui_ToggleButton_DockProperty ui_VisualElement_DockProperty
#define ui_ToggleButton_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ToggleButton_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ToggleButton_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ToggleButton_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ToggleButton_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ToggleButton_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ToggleButton_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ToggleButton_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ToggleButton_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ToggleButton_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ToggleButton_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ToggleButton_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ToggleButton_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ToggleButton_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ToggleButton_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ToggleButton_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ToggleButton_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ToggleButton_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ToggleButton_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ToggleButton_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ToggleButton_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ToggleButton_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ToggleButton_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ToggleButton_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ToggleButton_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ToggleButton_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ToggleButton_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ToggleButton_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ToggleButton_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ToggleButton_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ToggleButton_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Hyperlink { ui_VisualElement base; } ui_Hyperlink;
static inline ui_node *ui_Hyperlink_node(const ui_Hyperlink *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Hyperlink_create(ui_app *app, ui_Hyperlink *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)32, &out->base.node);
}
#define ui_Hyperlink_IdProperty ui_VisualElement_IdProperty
#define ui_Hyperlink_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Hyperlink_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Hyperlink_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Hyperlink_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_Hyperlink_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Hyperlink_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Hyperlink_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_Hyperlink_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Hyperlink_HeightProperty ui_VisualElement_HeightProperty
#define ui_Hyperlink_WidthProperty ui_VisualElement_WidthProperty
#define ui_Hyperlink_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Hyperlink_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Hyperlink_MarginProperty ui_VisualElement_MarginProperty
#define ui_Hyperlink_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Hyperlink_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Hyperlink_WeightProperty ui_VisualElement_WeightProperty
#define ui_Hyperlink_DockProperty ui_VisualElement_DockProperty
#define ui_Hyperlink_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Hyperlink_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Hyperlink_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Hyperlink_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Hyperlink_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Hyperlink_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Hyperlink_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Hyperlink_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Hyperlink_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Hyperlink_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Hyperlink_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Hyperlink_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Hyperlink_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Hyperlink_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Hyperlink_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Hyperlink_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Hyperlink_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Hyperlink_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Hyperlink_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Hyperlink_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Hyperlink_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Hyperlink_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Hyperlink_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Hyperlink_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Hyperlink_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Hyperlink_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Hyperlink_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Hyperlink_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Hyperlink_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Hyperlink_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Hyperlink_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_SpinBox { ui_VisualElement base; } ui_SpinBox;
static inline ui_node *ui_SpinBox_node(const ui_SpinBox *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_SpinBox_create(ui_app *app, ui_SpinBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)33, &out->base.node);
}
#define ui_SpinBox_IdProperty ui_VisualElement_IdProperty
#define ui_SpinBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_SpinBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_SpinBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_SpinBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_SpinBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_SpinBox_ValueProperty = {"Value", "integer", UI_VALUE_INT, 1, 1, UI_PROPERTY_VALUE, UI_BIND_TWO_WAY};
#define ui_SpinBox_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_SpinBox_TemplateProperty = {"Template", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_SpinBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_SpinBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_SpinBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_SpinBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_SpinBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_SpinBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_SpinBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_SpinBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_SpinBox_DockProperty ui_VisualElement_DockProperty
#define ui_SpinBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_SpinBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_SpinBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_SpinBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_SpinBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_SpinBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_SpinBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_SpinBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_SpinBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_SpinBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_SpinBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_SpinBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_SpinBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_SpinBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_SpinBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_SpinBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_SpinBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_SpinBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_SpinBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_SpinBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_SpinBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_SpinBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_SpinBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_SpinBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_SpinBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_SpinBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_SpinBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_SpinBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_SpinBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_SpinBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_SpinBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_PasswordBox { ui_VisualElement base; } ui_PasswordBox;
static inline ui_node *ui_PasswordBox_node(const ui_PasswordBox *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_PasswordBox_create(ui_app *app, ui_PasswordBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)34, &out->base.node);
}
#define ui_PasswordBox_IdProperty ui_VisualElement_IdProperty
#define ui_PasswordBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_PasswordBox_AllowDropProperty = {"AllowDrop", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_PasswordBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_PasswordBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 1, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
static const ui_element_property ui_PasswordBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_PasswordBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_PasswordBox_StyleProperty ui_VisualElement_StyleProperty
#define ui_PasswordBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_PasswordBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_PasswordBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_PasswordBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_PasswordBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_PasswordBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_PasswordBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_PasswordBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_PasswordBox_DockProperty ui_VisualElement_DockProperty
#define ui_PasswordBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_PasswordBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_PasswordBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_PasswordBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_PasswordBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_PasswordBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_PasswordBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_PasswordBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_PasswordBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_PasswordBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_PasswordBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_PasswordBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_PasswordBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_PasswordBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_PasswordBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_PasswordBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_PasswordBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_PasswordBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_PasswordBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_PasswordBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_PasswordBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_PasswordBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_PasswordBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_PasswordBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_PasswordBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_PasswordBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_PasswordBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_PasswordBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_PasswordBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_PasswordBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_PasswordBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TextArea { ui_VisualElement base; } ui_TextArea;
static inline ui_node *ui_TextArea_node(const ui_TextArea *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_TextArea_create(ui_app *app, ui_TextArea *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)35, &out->base.node);
}
#define ui_TextArea_IdProperty ui_VisualElement_IdProperty
#define ui_TextArea_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_TextArea_AllowDropProperty = {"AllowDrop", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TextArea_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TextArea_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 1, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
static const ui_element_property ui_TextArea_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_TextArea_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TextArea_StyleProperty ui_VisualElement_StyleProperty
#define ui_TextArea_HeightProperty ui_VisualElement_HeightProperty
#define ui_TextArea_WidthProperty ui_VisualElement_WidthProperty
#define ui_TextArea_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TextArea_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TextArea_MarginProperty ui_VisualElement_MarginProperty
#define ui_TextArea_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TextArea_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TextArea_WeightProperty ui_VisualElement_WeightProperty
#define ui_TextArea_DockProperty ui_VisualElement_DockProperty
#define ui_TextArea_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TextArea_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TextArea_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TextArea_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TextArea_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TextArea_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TextArea_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TextArea_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TextArea_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TextArea_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TextArea_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TextArea_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TextArea_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TextArea_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TextArea_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TextArea_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TextArea_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TextArea_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TextArea_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TextArea_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TextArea_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TextArea_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TextArea_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TextArea_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TextArea_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TextArea_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TextArea_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TextArea_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TextArea_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TextArea_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TextArea_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_SearchBox { ui_VisualElement base; } ui_SearchBox;
static inline ui_node *ui_SearchBox_node(const ui_SearchBox *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_SearchBox_create(ui_app *app, ui_SearchBox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)36, &out->base.node);
}
#define ui_SearchBox_IdProperty ui_VisualElement_IdProperty
#define ui_SearchBox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
static const ui_element_property ui_SearchBox_AllowDropProperty = {"AllowDrop", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_SearchBox_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_SearchBox_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 1, UI_PROPERTY_TEXT, UI_BIND_TWO_WAY};
static const ui_element_property ui_SearchBox_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_SearchBox_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_SearchBox_StyleProperty ui_VisualElement_StyleProperty
#define ui_SearchBox_HeightProperty ui_VisualElement_HeightProperty
#define ui_SearchBox_WidthProperty ui_VisualElement_WidthProperty
#define ui_SearchBox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_SearchBox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_SearchBox_MarginProperty ui_VisualElement_MarginProperty
#define ui_SearchBox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_SearchBox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_SearchBox_WeightProperty ui_VisualElement_WeightProperty
#define ui_SearchBox_DockProperty ui_VisualElement_DockProperty
#define ui_SearchBox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_SearchBox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_SearchBox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_SearchBox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_SearchBox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_SearchBox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_SearchBox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_SearchBox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_SearchBox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_SearchBox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_SearchBox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_SearchBox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_SearchBox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_SearchBox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_SearchBox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_SearchBox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_SearchBox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_SearchBox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_SearchBox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_SearchBox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_SearchBox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_SearchBox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_SearchBox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_SearchBox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_SearchBox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_SearchBox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_SearchBox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_SearchBox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_SearchBox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_SearchBox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_SearchBox_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TreeView { ui_LayoutElement base; } ui_TreeView;
static inline ui_node *ui_TreeView_node(const ui_TreeView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_TreeView_create(ui_app *app, ui_TreeView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)37, &out->base.base.node);
}
#define ui_TreeView_IdProperty ui_VisualElement_IdProperty
#define ui_TreeView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_TreeView_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_TreeView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_TreeView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_TreeView_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_TreeView_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_TreeView_HeightProperty ui_VisualElement_HeightProperty
#define ui_TreeView_WidthProperty ui_VisualElement_WidthProperty
#define ui_TreeView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TreeView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TreeView_MarginProperty ui_VisualElement_MarginProperty
#define ui_TreeView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TreeView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TreeView_WeightProperty ui_VisualElement_WeightProperty
#define ui_TreeView_DockProperty ui_VisualElement_DockProperty
#define ui_TreeView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TreeView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TreeView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TreeView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TreeView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TreeView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TreeView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TreeView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TreeView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TreeView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TreeView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TreeView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TreeView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TreeView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TreeView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TreeView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TreeView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TreeView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TreeView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TreeView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TreeView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TreeView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TreeView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TreeView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TreeView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TreeView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TreeView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TreeView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TreeView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TreeView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TreeView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DataGrid { ui_LayoutElement base; } ui_DataGrid;
static inline ui_node *ui_DataGrid_node(const ui_DataGrid *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DataGrid_create(ui_app *app, ui_DataGrid *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)38, &out->base.base.node);
}
#define ui_DataGrid_IdProperty ui_VisualElement_IdProperty
#define ui_DataGrid_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DataGrid_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_DataGrid_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DataGrid_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DataGrid_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_DataGrid_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_DataGrid_HeightProperty ui_VisualElement_HeightProperty
#define ui_DataGrid_WidthProperty ui_VisualElement_WidthProperty
#define ui_DataGrid_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DataGrid_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DataGrid_MarginProperty ui_VisualElement_MarginProperty
#define ui_DataGrid_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DataGrid_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DataGrid_WeightProperty ui_VisualElement_WeightProperty
#define ui_DataGrid_DockProperty ui_VisualElement_DockProperty
#define ui_DataGrid_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DataGrid_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DataGrid_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DataGrid_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DataGrid_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DataGrid_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DataGrid_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DataGrid_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DataGrid_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DataGrid_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DataGrid_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DataGrid_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DataGrid_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DataGrid_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DataGrid_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DataGrid_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DataGrid_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DataGrid_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DataGrid_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DataGrid_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DataGrid_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DataGrid_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DataGrid_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DataGrid_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DataGrid_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DataGrid_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DataGrid_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DataGrid_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DataGrid_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DataGrid_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DataGrid_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_MenuBar { ui_LayoutElement base; } ui_MenuBar;
static inline ui_node *ui_MenuBar_node(const ui_MenuBar *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_MenuBar_create(ui_app *app, ui_MenuBar *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)39, &out->base.base.node);
}
#define ui_MenuBar_IdProperty ui_VisualElement_IdProperty
#define ui_MenuBar_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_MenuBar_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_MenuBar_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_MenuBar_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_MenuBar_StyleProperty ui_VisualElement_StyleProperty
#define ui_MenuBar_HeightProperty ui_VisualElement_HeightProperty
#define ui_MenuBar_WidthProperty ui_VisualElement_WidthProperty
#define ui_MenuBar_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_MenuBar_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_MenuBar_MarginProperty ui_VisualElement_MarginProperty
#define ui_MenuBar_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_MenuBar_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_MenuBar_WeightProperty ui_VisualElement_WeightProperty
#define ui_MenuBar_DockProperty ui_VisualElement_DockProperty
#define ui_MenuBar_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_MenuBar_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_MenuBar_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_MenuBar_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_MenuBar_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_MenuBar_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_MenuBar_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_MenuBar_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_MenuBar_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_MenuBar_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_MenuBar_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_MenuBar_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_MenuBar_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_MenuBar_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_MenuBar_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_MenuBar_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_MenuBar_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_MenuBar_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_MenuBar_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_MenuBar_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_MenuBar_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_MenuBar_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_MenuBar_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_MenuBar_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_MenuBar_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_MenuBar_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_MenuBar_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_MenuBar_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_MenuBar_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_MenuBar_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_MenuBar_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ToolBar { ui_LayoutElement base; } ui_ToolBar;
static inline ui_node *ui_ToolBar_node(const ui_ToolBar *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ToolBar_create(ui_app *app, ui_ToolBar *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)40, &out->base.base.node);
}
#define ui_ToolBar_IdProperty ui_VisualElement_IdProperty
#define ui_ToolBar_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ToolBar_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_ToolBar_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ToolBar_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ToolBar_StyleProperty ui_VisualElement_StyleProperty
#define ui_ToolBar_HeightProperty ui_VisualElement_HeightProperty
#define ui_ToolBar_WidthProperty ui_VisualElement_WidthProperty
#define ui_ToolBar_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ToolBar_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ToolBar_MarginProperty ui_VisualElement_MarginProperty
#define ui_ToolBar_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ToolBar_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ToolBar_WeightProperty ui_VisualElement_WeightProperty
#define ui_ToolBar_DockProperty ui_VisualElement_DockProperty
#define ui_ToolBar_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ToolBar_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ToolBar_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ToolBar_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ToolBar_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ToolBar_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ToolBar_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ToolBar_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ToolBar_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ToolBar_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ToolBar_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ToolBar_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ToolBar_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ToolBar_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ToolBar_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ToolBar_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ToolBar_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ToolBar_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ToolBar_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ToolBar_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ToolBar_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ToolBar_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ToolBar_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ToolBar_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ToolBar_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ToolBar_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ToolBar_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ToolBar_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ToolBar_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ToolBar_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ToolBar_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_NavigationView { ui_LayoutElement base; } ui_NavigationView;
static inline ui_node *ui_NavigationView_node(const ui_NavigationView *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_NavigationView_create(ui_app *app, ui_NavigationView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)41, &out->base.base.node);
}
#define ui_NavigationView_IdProperty ui_VisualElement_IdProperty
#define ui_NavigationView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_NavigationView_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_NavigationView_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_NavigationView_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_NavigationView_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_NavigationView_StyleProperty ui_VisualElement_StyleProperty
#define ui_NavigationView_HeightProperty ui_VisualElement_HeightProperty
#define ui_NavigationView_WidthProperty ui_VisualElement_WidthProperty
#define ui_NavigationView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_NavigationView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_NavigationView_MarginProperty ui_VisualElement_MarginProperty
#define ui_NavigationView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_NavigationView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_NavigationView_WeightProperty ui_VisualElement_WeightProperty
#define ui_NavigationView_DockProperty ui_VisualElement_DockProperty
#define ui_NavigationView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_NavigationView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_NavigationView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_NavigationView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_NavigationView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_NavigationView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_NavigationView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_NavigationView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_NavigationView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_NavigationView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_NavigationView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_NavigationView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_NavigationView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_NavigationView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_NavigationView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_NavigationView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_NavigationView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_NavigationView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_NavigationView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_NavigationView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_NavigationView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_NavigationView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_NavigationView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_NavigationView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_NavigationView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_NavigationView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_NavigationView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_NavigationView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_NavigationView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_NavigationView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_NavigationView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Dialog { ui_LayoutElement base; } ui_Dialog;
static inline ui_node *ui_Dialog_node(const ui_Dialog *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Dialog_create(ui_app *app, ui_Dialog *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)42, &out->base.base.node);
}
#define ui_Dialog_IdProperty ui_VisualElement_IdProperty
static const ui_element_property ui_Dialog_AnchorProperty = {"Anchor", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Dialog_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Dialog_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Dialog_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Dialog_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Dialog_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Dialog_StyleProperty ui_VisualElement_StyleProperty
#define ui_Dialog_HeightProperty ui_VisualElement_HeightProperty
#define ui_Dialog_WidthProperty ui_VisualElement_WidthProperty
#define ui_Dialog_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Dialog_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Dialog_MarginProperty ui_VisualElement_MarginProperty
#define ui_Dialog_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Dialog_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Dialog_WeightProperty ui_VisualElement_WeightProperty
#define ui_Dialog_DockProperty ui_VisualElement_DockProperty
#define ui_Dialog_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Dialog_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Dialog_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Dialog_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Dialog_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Dialog_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Dialog_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Dialog_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Dialog_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Dialog_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Dialog_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Dialog_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Dialog_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Dialog_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Dialog_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Dialog_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Dialog_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Dialog_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Dialog_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Dialog_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Dialog_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Dialog_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Dialog_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Dialog_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Dialog_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Dialog_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Dialog_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Dialog_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Dialog_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Dialog_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Dialog_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Tooltip { ui_LayoutElement base; } ui_Tooltip;
static inline ui_node *ui_Tooltip_node(const ui_Tooltip *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Tooltip_create(ui_app *app, ui_Tooltip *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)43, &out->base.base.node);
}
#define ui_Tooltip_IdProperty ui_VisualElement_IdProperty
static const ui_element_property ui_Tooltip_AnchorProperty = {"Anchor", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Tooltip_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Tooltip_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Tooltip_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Tooltip_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Tooltip_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Tooltip_StyleProperty ui_VisualElement_StyleProperty
#define ui_Tooltip_HeightProperty ui_VisualElement_HeightProperty
#define ui_Tooltip_WidthProperty ui_VisualElement_WidthProperty
#define ui_Tooltip_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Tooltip_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Tooltip_MarginProperty ui_VisualElement_MarginProperty
#define ui_Tooltip_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Tooltip_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Tooltip_WeightProperty ui_VisualElement_WeightProperty
#define ui_Tooltip_DockProperty ui_VisualElement_DockProperty
#define ui_Tooltip_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Tooltip_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Tooltip_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Tooltip_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Tooltip_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Tooltip_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Tooltip_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Tooltip_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Tooltip_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Tooltip_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Tooltip_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Tooltip_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Tooltip_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Tooltip_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Tooltip_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Tooltip_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Tooltip_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Tooltip_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Tooltip_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Tooltip_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Tooltip_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Tooltip_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Tooltip_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Tooltip_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Tooltip_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Tooltip_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Tooltip_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Tooltip_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Tooltip_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Tooltip_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Tooltip_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ContextMenu { ui_LayoutElement base; } ui_ContextMenu;
static inline ui_node *ui_ContextMenu_node(const ui_ContextMenu *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ContextMenu_create(ui_app *app, ui_ContextMenu *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)44, &out->base.base.node);
}
#define ui_ContextMenu_IdProperty ui_VisualElement_IdProperty
static const ui_element_property ui_ContextMenu_AnchorProperty = {"Anchor", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ContextMenu_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ContextMenu_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ContextMenu_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_ContextMenu_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ContextMenu_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ContextMenu_StyleProperty ui_VisualElement_StyleProperty
#define ui_ContextMenu_HeightProperty ui_VisualElement_HeightProperty
#define ui_ContextMenu_WidthProperty ui_VisualElement_WidthProperty
#define ui_ContextMenu_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ContextMenu_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ContextMenu_MarginProperty ui_VisualElement_MarginProperty
#define ui_ContextMenu_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ContextMenu_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ContextMenu_WeightProperty ui_VisualElement_WeightProperty
#define ui_ContextMenu_DockProperty ui_VisualElement_DockProperty
#define ui_ContextMenu_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ContextMenu_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ContextMenu_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ContextMenu_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ContextMenu_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ContextMenu_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ContextMenu_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ContextMenu_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ContextMenu_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ContextMenu_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ContextMenu_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ContextMenu_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ContextMenu_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ContextMenu_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ContextMenu_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ContextMenu_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ContextMenu_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ContextMenu_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ContextMenu_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ContextMenu_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ContextMenu_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ContextMenu_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ContextMenu_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ContextMenu_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ContextMenu_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ContextMenu_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ContextMenu_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ContextMenu_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ContextMenu_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ContextMenu_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ContextMenu_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Spinner { ui_VisualElement base; } ui_Spinner;
static inline ui_node *ui_Spinner_node(const ui_Spinner *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Spinner_create(ui_app *app, ui_Spinner *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)45, &out->base.node);
}
#define ui_Spinner_IdProperty ui_VisualElement_IdProperty
#define ui_Spinner_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Spinner_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Spinner_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Spinner_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Spinner_StyleProperty ui_VisualElement_StyleProperty
#define ui_Spinner_HeightProperty ui_VisualElement_HeightProperty
#define ui_Spinner_WidthProperty ui_VisualElement_WidthProperty
#define ui_Spinner_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Spinner_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Spinner_MarginProperty ui_VisualElement_MarginProperty
#define ui_Spinner_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Spinner_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Spinner_WeightProperty ui_VisualElement_WeightProperty
#define ui_Spinner_DockProperty ui_VisualElement_DockProperty
#define ui_Spinner_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Spinner_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Spinner_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Spinner_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Spinner_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Spinner_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Spinner_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Spinner_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Spinner_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Spinner_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Spinner_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Spinner_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Spinner_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Spinner_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Spinner_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Spinner_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Spinner_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Spinner_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Spinner_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Spinner_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Spinner_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Spinner_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Spinner_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Spinner_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Spinner_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Spinner_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Spinner_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Spinner_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Spinner_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Spinner_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Spinner_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_InfoBar { ui_VisualElement base; } ui_InfoBar;
static inline ui_node *ui_InfoBar_node(const ui_InfoBar *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_InfoBar_create(ui_app *app, ui_InfoBar *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)46, &out->base.node);
}
#define ui_InfoBar_IdProperty ui_VisualElement_IdProperty
#define ui_InfoBar_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_InfoBar_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_InfoBar_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_InfoBar_StyleProperty ui_VisualElement_StyleProperty
#define ui_InfoBar_HeightProperty ui_VisualElement_HeightProperty
#define ui_InfoBar_WidthProperty ui_VisualElement_WidthProperty
#define ui_InfoBar_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_InfoBar_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_InfoBar_MarginProperty ui_VisualElement_MarginProperty
#define ui_InfoBar_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_InfoBar_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_InfoBar_WeightProperty ui_VisualElement_WeightProperty
#define ui_InfoBar_DockProperty ui_VisualElement_DockProperty
#define ui_InfoBar_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_InfoBar_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_InfoBar_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_InfoBar_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_InfoBar_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_InfoBar_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_InfoBar_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_InfoBar_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_InfoBar_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_InfoBar_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_InfoBar_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_InfoBar_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_InfoBar_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_InfoBar_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_InfoBar_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_InfoBar_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_InfoBar_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_InfoBar_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_InfoBar_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_InfoBar_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_InfoBar_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_InfoBar_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_InfoBar_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_InfoBar_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_InfoBar_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_InfoBar_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_InfoBar_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_InfoBar_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_InfoBar_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_InfoBar_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_InfoBar_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_TreeItem { ui_LayoutElement base; } ui_TreeItem;
static inline ui_node *ui_TreeItem_node(const ui_TreeItem *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_TreeItem_create(ui_app *app, ui_TreeItem *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)47, &out->base.base.node);
}
#define ui_TreeItem_IdProperty ui_VisualElement_IdProperty
#define ui_TreeItem_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_TreeItem_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_TreeItem_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_TreeItem_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_TreeItem_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_TreeItem_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_TreeItem_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_TreeItem_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_TreeItem_StyleProperty ui_VisualElement_StyleProperty
static const ui_element_property ui_TreeItem_ItemsSourceProperty = {"ItemsSource", "model", UI_VALUE_MODEL, 1, 0, UI_PROPERTY_ITEMS_SOURCE, UI_BIND_ONE_WAY};
#define ui_TreeItem_HeightProperty ui_VisualElement_HeightProperty
#define ui_TreeItem_WidthProperty ui_VisualElement_WidthProperty
#define ui_TreeItem_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_TreeItem_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_TreeItem_MarginProperty ui_VisualElement_MarginProperty
#define ui_TreeItem_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_TreeItem_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_TreeItem_WeightProperty ui_VisualElement_WeightProperty
#define ui_TreeItem_DockProperty ui_VisualElement_DockProperty
#define ui_TreeItem_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_TreeItem_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_TreeItem_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_TreeItem_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_TreeItem_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_TreeItem_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_TreeItem_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_TreeItem_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_TreeItem_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_TreeItem_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_TreeItem_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_TreeItem_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_TreeItem_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_TreeItem_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_TreeItem_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_TreeItem_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_TreeItem_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_TreeItem_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_TreeItem_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_TreeItem_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_TreeItem_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_TreeItem_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_TreeItem_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_TreeItem_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_TreeItem_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_TreeItem_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_TreeItem_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_TreeItem_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_TreeItem_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_TreeItem_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_TreeItem_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockHost { ui_LayoutElement base; } ui_DockHost;
static inline ui_node *ui_DockHost_node(const ui_DockHost *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockHost_create(ui_app *app, ui_DockHost *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)48, &out->base.base.node);
}
#define ui_DockHost_IdProperty ui_VisualElement_IdProperty
#define ui_DockHost_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockHost_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_DockHost_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockHost_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockHost_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockHost_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockHost_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockHost_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockHost_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockHost_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockHost_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockHost_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockHost_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockHost_DockProperty ui_VisualElement_DockProperty
#define ui_DockHost_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockHost_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockHost_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockHost_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockHost_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockHost_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockHost_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockHost_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockHost_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockHost_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockHost_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockHost_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockHost_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockHost_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockHost_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockHost_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockHost_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockHost_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockHost_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockHost_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockHost_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockHost_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockHost_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockHost_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockHost_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockHost_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockHost_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockHost_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockHost_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockHost_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockHost_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockSplit { ui_LayoutElement base; } ui_DockSplit;
static inline ui_node *ui_DockSplit_node(const ui_DockSplit *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockSplit_create(ui_app *app, ui_DockSplit *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)49, &out->base.base.node);
}
#define ui_DockSplit_IdProperty ui_VisualElement_IdProperty
#define ui_DockSplit_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockSplit_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_DockSplit_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockSplit_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockSplit_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockSplit_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockSplit_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockSplit_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockSplit_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockSplit_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockSplit_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockSplit_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockSplit_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockSplit_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockSplit_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_DockSplit_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockSplit_RatioProperty = {"Ratio", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockSplit_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockSplit_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockSplit_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockSplit_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockSplit_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockSplit_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockSplit_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockSplit_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockSplit_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockSplit_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockSplit_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockSplit_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockSplit_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockSplit_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockSplit_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockSplit_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockSplit_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockSplit_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockSplit_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockSplit_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockSplit_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockSplit_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockSplit_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockSplit_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockSplit_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockSplit_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockSplit_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockSplit_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockSplit_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockSplit_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockSplit_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockGroup { ui_LayoutElement base; } ui_DockGroup;
static inline ui_node *ui_DockGroup_node(const ui_DockGroup *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockGroup_create(ui_app *app, ui_DockGroup *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)50, &out->base.base.node);
}
#define ui_DockGroup_IdProperty ui_VisualElement_IdProperty
#define ui_DockGroup_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockGroup_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_DockGroup_ValueProperty = {"Value", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockGroup_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockGroup_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockGroup_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockGroup_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockGroup_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockGroup_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockGroup_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockGroup_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockGroup_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockGroup_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockGroup_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockGroup_DockProperty ui_VisualElement_DockProperty
#define ui_DockGroup_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockGroup_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockGroup_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockGroup_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockGroup_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockGroup_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockGroup_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockGroup_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockGroup_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockGroup_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockGroup_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockGroup_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockGroup_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockGroup_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockGroup_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockGroup_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockGroup_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockGroup_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockGroup_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockGroup_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockGroup_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockGroup_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockGroup_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockGroup_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockGroup_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockGroup_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockGroup_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockGroup_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockGroup_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockGroup_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockGroup_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockDocument { ui_LayoutElement base; } ui_DockDocument;
static inline ui_node *ui_DockDocument_node(const ui_DockDocument *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockDocument_create(ui_app *app, ui_DockDocument *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)51, &out->base.base.node);
}
#define ui_DockDocument_IdProperty ui_VisualElement_IdProperty
#define ui_DockDocument_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockDocument_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_DockDocument_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockDocument_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockDocument_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockDocument_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockDocument_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockDocument_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockDocument_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockDocument_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockDocument_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockDocument_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockDocument_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockDocument_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockDocument_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_DockDocument_TitleProperty = {"Title", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockDocument_IconProperty = {"Icon", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockDocument_CanCloseProperty = {"CanClose", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockDocument_CanFloatProperty = {"CanFloat", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockDocument_CanAutoHideProperty = {"CanAutoHide", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockDocument_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockDocument_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockDocument_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockDocument_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockDocument_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockDocument_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockDocument_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockDocument_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockDocument_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockDocument_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockDocument_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockDocument_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockDocument_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockDocument_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockDocument_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockDocument_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockDocument_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockDocument_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockDocument_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockDocument_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockDocument_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockDocument_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockDocument_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockDocument_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockDocument_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockDocument_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockDocument_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockDocument_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockDocument_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockDocument_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockDocument_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DockTool { ui_LayoutElement base; } ui_DockTool;
static inline ui_node *ui_DockTool_node(const ui_DockTool *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_DockTool_create(ui_app *app, ui_DockTool *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)52, &out->base.base.node);
}
#define ui_DockTool_IdProperty ui_VisualElement_IdProperty
#define ui_DockTool_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DockTool_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_DockTool_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockTool_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_DockTool_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_DockTool_StyleProperty ui_VisualElement_StyleProperty
#define ui_DockTool_HeightProperty ui_VisualElement_HeightProperty
#define ui_DockTool_WidthProperty ui_VisualElement_WidthProperty
#define ui_DockTool_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DockTool_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DockTool_MarginProperty ui_VisualElement_MarginProperty
#define ui_DockTool_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DockTool_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DockTool_WeightProperty ui_VisualElement_WeightProperty
#define ui_DockTool_DockProperty ui_VisualElement_DockProperty
static const ui_element_property ui_DockTool_TitleProperty = {"Title", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockTool_IconProperty = {"Icon", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockTool_CanCloseProperty = {"CanClose", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockTool_CanFloatProperty = {"CanFloat", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DockTool_CanAutoHideProperty = {"CanAutoHide", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DockTool_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DockTool_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DockTool_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DockTool_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DockTool_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DockTool_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DockTool_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DockTool_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DockTool_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DockTool_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DockTool_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DockTool_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DockTool_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DockTool_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DockTool_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DockTool_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DockTool_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DockTool_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DockTool_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DockTool_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DockTool_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DockTool_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DockTool_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DockTool_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DockTool_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DockTool_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DockTool_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DockTool_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DockTool_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DockTool_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DockTool_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_DocumentView { ui_VisualElement base; } ui_DocumentView;
static inline ui_node *ui_DocumentView_node(const ui_DocumentView *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_DocumentView_create(ui_app *app, ui_DocumentView *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)53, &out->base.node);
}
#define ui_DocumentView_IdProperty ui_VisualElement_IdProperty
#define ui_DocumentView_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_DocumentView_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_DocumentView_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentView_CommandProperty = {"Command", "command", UI_VALUE_COMMAND, 1, 0, UI_PROPERTY_COMMAND, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentView_CommandModeProperty = {"CommandMode", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_DocumentView_StyleProperty ui_VisualElement_StyleProperty
#define ui_DocumentView_HeightProperty ui_VisualElement_HeightProperty
#define ui_DocumentView_WidthProperty ui_VisualElement_WidthProperty
#define ui_DocumentView_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_DocumentView_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_DocumentView_MarginProperty ui_VisualElement_MarginProperty
#define ui_DocumentView_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_DocumentView_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_DocumentView_WeightProperty ui_VisualElement_WeightProperty
#define ui_DocumentView_DockProperty ui_VisualElement_DockProperty
#define ui_DocumentView_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_DocumentView_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_DocumentView_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_DocumentView_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_DocumentView_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_DocumentView_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_DocumentView_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_DocumentView_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_DocumentView_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_DocumentView_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_DocumentView_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_DocumentView_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_DocumentView_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_DocumentView_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_DocumentView_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_DocumentView_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_DocumentView_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_DocumentView_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_DocumentView_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_DocumentView_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_DocumentView_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_DocumentView_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_DocumentView_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_DocumentView_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_DocumentView_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_DocumentView_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_DocumentView_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_DocumentView_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_DocumentView_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_DocumentView_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_DocumentView_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Border { ui_LayoutElement base; } ui_Border;
static inline ui_node *ui_Border_node(const ui_Border *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Border_create(ui_app *app, ui_Border *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)54, &out->base.base.node);
}
#define ui_Border_IdProperty ui_VisualElement_IdProperty
#define ui_Border_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Border_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Border_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Border_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Border_StyleProperty ui_VisualElement_StyleProperty
#define ui_Border_HeightProperty ui_VisualElement_HeightProperty
#define ui_Border_WidthProperty ui_VisualElement_WidthProperty
#define ui_Border_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Border_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Border_MarginProperty ui_VisualElement_MarginProperty
#define ui_Border_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Border_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Border_WeightProperty ui_VisualElement_WeightProperty
#define ui_Border_DockProperty ui_VisualElement_DockProperty
#define ui_Border_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Border_MinHeightProperty ui_VisualElement_MinHeightProperty
static const ui_element_property ui_Border_BackgroundProperty = {"Background", "color", UI_VALUE_COLOR, 1, 0, UI_PROPERTY_BACKGROUND, UI_BIND_ONE_WAY};
static const ui_element_property ui_Border_BorderBrushProperty = {"BorderBrush", "color", UI_VALUE_COLOR, 1, 0, UI_PROPERTY_BORDER_BRUSH, UI_BIND_ONE_WAY};
static const ui_element_property ui_Border_BorderThicknessProperty = {"BorderThickness", "thickness", UI_VALUE_THICKNESS, 1, 0, UI_PROPERTY_BORDER_THICKNESS, UI_BIND_ONE_WAY};
static const ui_element_property ui_Border_CornerRadiusProperty = {"CornerRadius", "cornerRadius", UI_VALUE_CORNER_RADIUS, 1, 0, UI_PROPERTY_CORNER_RADIUS, UI_BIND_ONE_WAY};
#define ui_Border_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Border_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Border_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Border_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Border_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Border_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Border_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Border_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Border_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Border_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Border_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Border_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Border_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Border_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Border_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Border_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Border_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Border_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Border_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Border_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Border_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Border_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Border_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Border_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Border_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Border_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Border_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Border_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Border_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Run { ui_VisualElement base; } ui_Run;
static inline ui_node *ui_Run_node(const ui_Run *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Run_create(ui_app *app, ui_Run *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)55, &out->base.node);
}
#define ui_Run_IdProperty ui_VisualElement_IdProperty
#define ui_Run_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Run_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Run_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Run_StyleProperty ui_VisualElement_StyleProperty
#define ui_Run_HeightProperty ui_VisualElement_HeightProperty
#define ui_Run_WidthProperty ui_VisualElement_WidthProperty
#define ui_Run_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Run_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Run_MarginProperty ui_VisualElement_MarginProperty
#define ui_Run_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Run_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Run_WeightProperty ui_VisualElement_WeightProperty
#define ui_Run_DockProperty ui_VisualElement_DockProperty
#define ui_Run_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Run_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Run_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Run_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Run_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Run_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Run_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Run_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Run_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Run_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Run_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Run_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Run_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Run_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Run_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Run_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Run_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Run_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Run_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Run_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Run_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Run_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Run_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Run_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Run_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Run_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Run_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Run_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Run_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Run_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Run_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Span { ui_VisualElement base; } ui_Span;
static inline ui_node *ui_Span_node(const ui_Span *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Span_create(ui_app *app, ui_Span *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)56, &out->base.node);
}
#define ui_Span_IdProperty ui_VisualElement_IdProperty
#define ui_Span_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Span_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Span_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Span_StyleProperty ui_VisualElement_StyleProperty
#define ui_Span_HeightProperty ui_VisualElement_HeightProperty
#define ui_Span_WidthProperty ui_VisualElement_WidthProperty
#define ui_Span_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Span_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Span_MarginProperty ui_VisualElement_MarginProperty
#define ui_Span_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Span_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Span_WeightProperty ui_VisualElement_WeightProperty
#define ui_Span_DockProperty ui_VisualElement_DockProperty
#define ui_Span_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Span_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Span_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Span_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Span_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Span_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Span_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Span_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Span_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Span_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Span_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Span_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Span_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Span_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Span_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Span_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Span_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Span_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Span_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Span_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Span_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Span_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Span_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Span_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Span_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Span_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Span_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Span_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Span_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Span_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Span_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Canvas { ui_LayoutElement base; } ui_Canvas;
static inline ui_node *ui_Canvas_node(const ui_Canvas *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Canvas_create(ui_app *app, ui_Canvas *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)57, &out->base.base.node);
}
#define ui_Canvas_IdProperty ui_VisualElement_IdProperty
#define ui_Canvas_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Canvas_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Canvas_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Canvas_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Canvas_StyleProperty ui_VisualElement_StyleProperty
#define ui_Canvas_HeightProperty ui_VisualElement_HeightProperty
#define ui_Canvas_WidthProperty ui_VisualElement_WidthProperty
#define ui_Canvas_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Canvas_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Canvas_MarginProperty ui_VisualElement_MarginProperty
#define ui_Canvas_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Canvas_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Canvas_WeightProperty ui_VisualElement_WeightProperty
#define ui_Canvas_DockProperty ui_VisualElement_DockProperty
#define ui_Canvas_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Canvas_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Canvas_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Canvas_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Canvas_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Canvas_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Canvas_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Canvas_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Canvas_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Canvas_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Canvas_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Canvas_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Canvas_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Canvas_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Canvas_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Canvas_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Canvas_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Canvas_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Canvas_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Canvas_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Canvas_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Canvas_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Canvas_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Canvas_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Canvas_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Canvas_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Canvas_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Canvas_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Canvas_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Canvas_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Canvas_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ContentControl { ui_LayoutElement base; } ui_ContentControl;
static inline ui_node *ui_ContentControl_node(const ui_ContentControl *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ContentControl_create(ui_app *app, ui_ContentControl *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)58, &out->base.base.node);
}
#define ui_ContentControl_IdProperty ui_VisualElement_IdProperty
#define ui_ContentControl_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ContentControl_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ContentControl_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ContentControl_ContentProperty = {"Content", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
#define ui_ContentControl_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ContentControl_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ContentControl_StyleProperty ui_VisualElement_StyleProperty
#define ui_ContentControl_HeightProperty ui_VisualElement_HeightProperty
#define ui_ContentControl_WidthProperty ui_VisualElement_WidthProperty
#define ui_ContentControl_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ContentControl_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ContentControl_MarginProperty ui_VisualElement_MarginProperty
#define ui_ContentControl_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ContentControl_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ContentControl_WeightProperty ui_VisualElement_WeightProperty
#define ui_ContentControl_DockProperty ui_VisualElement_DockProperty
#define ui_ContentControl_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ContentControl_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ContentControl_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ContentControl_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ContentControl_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ContentControl_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ContentControl_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ContentControl_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ContentControl_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ContentControl_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ContentControl_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ContentControl_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ContentControl_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ContentControl_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ContentControl_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ContentControl_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ContentControl_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ContentControl_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ContentControl_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ContentControl_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ContentControl_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ContentControl_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ContentControl_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ContentControl_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ContentControl_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ContentControl_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ContentControl_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ContentControl_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ContentControl_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ContentControl_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ContentControl_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_ContentPresenter { ui_LayoutElement base; } ui_ContentPresenter;
static inline ui_node *ui_ContentPresenter_node(const ui_ContentPresenter *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_ContentPresenter_create(ui_app *app, ui_ContentPresenter *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)59, &out->base.base.node);
}
#define ui_ContentPresenter_IdProperty ui_VisualElement_IdProperty
#define ui_ContentPresenter_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_ContentPresenter_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_ContentPresenter_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ContentPresenter_ContentProperty = {"Content", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_CONTENT, UI_BIND_ONE_WAY};
#define ui_ContentPresenter_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_ContentPresenter_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_ContentPresenter_StyleProperty ui_VisualElement_StyleProperty
#define ui_ContentPresenter_HeightProperty ui_VisualElement_HeightProperty
#define ui_ContentPresenter_WidthProperty ui_VisualElement_WidthProperty
#define ui_ContentPresenter_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_ContentPresenter_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_ContentPresenter_MarginProperty ui_VisualElement_MarginProperty
#define ui_ContentPresenter_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_ContentPresenter_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_ContentPresenter_WeightProperty ui_VisualElement_WeightProperty
#define ui_ContentPresenter_DockProperty ui_VisualElement_DockProperty
#define ui_ContentPresenter_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_ContentPresenter_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_ContentPresenter_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_ContentPresenter_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_ContentPresenter_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_ContentPresenter_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_ContentPresenter_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_ContentPresenter_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_ContentPresenter_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_ContentPresenter_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_ContentPresenter_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_ContentPresenter_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_ContentPresenter_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_ContentPresenter_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_ContentPresenter_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_ContentPresenter_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_ContentPresenter_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_ContentPresenter_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_ContentPresenter_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_ContentPresenter_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_ContentPresenter_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_ContentPresenter_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_ContentPresenter_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_ContentPresenter_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_ContentPresenter_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_ContentPresenter_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_ContentPresenter_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_ContentPresenter_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_ContentPresenter_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_ContentPresenter_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_ContentPresenter_CanvasTopProperty ui_VisualElement_CanvasTopProperty

typedef struct ui_Viewbox { ui_LayoutElement base; } ui_Viewbox;
static inline ui_node *ui_Viewbox_node(const ui_Viewbox *element) { return element ? element->base.base.node : NULL; }
static inline ui_result ui_Viewbox_create(ui_app *app, ui_Viewbox *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.base.component = NULL; out->base.base.node = NULL;
    return ui_node_create(app, (ui_node_kind)60, &out->base.base.node);
}
#define ui_Viewbox_IdProperty ui_VisualElement_IdProperty
#define ui_Viewbox_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Viewbox_DataContextProperty ui_VisualElement_DataContextProperty
#define ui_Viewbox_SpacingProperty ui_LayoutElement_SpacingProperty
#define ui_Viewbox_PaddingProperty ui_LayoutElement_PaddingProperty
#define ui_Viewbox_StyleProperty ui_VisualElement_StyleProperty
#define ui_Viewbox_HeightProperty ui_VisualElement_HeightProperty
#define ui_Viewbox_WidthProperty ui_VisualElement_WidthProperty
#define ui_Viewbox_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Viewbox_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Viewbox_MarginProperty ui_VisualElement_MarginProperty
#define ui_Viewbox_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Viewbox_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Viewbox_WeightProperty ui_VisualElement_WeightProperty
#define ui_Viewbox_DockProperty ui_VisualElement_DockProperty
#define ui_Viewbox_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Viewbox_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Viewbox_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Viewbox_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Viewbox_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Viewbox_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Viewbox_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Viewbox_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Viewbox_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Viewbox_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Viewbox_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Viewbox_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Viewbox_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Viewbox_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Viewbox_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Viewbox_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Viewbox_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Viewbox_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Viewbox_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Viewbox_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Viewbox_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Viewbox_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Viewbox_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Viewbox_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Viewbox_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Viewbox_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Viewbox_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Viewbox_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Viewbox_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Viewbox_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Viewbox_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Viewbox_StretchProperty = {"Stretch", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Rectangle { ui_VisualElement base; } ui_Rectangle;
static inline ui_node *ui_Rectangle_node(const ui_Rectangle *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Rectangle_create(ui_app *app, ui_Rectangle *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)61, &out->base.node);
}
#define ui_Rectangle_IdProperty ui_VisualElement_IdProperty
#define ui_Rectangle_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Rectangle_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Rectangle_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Rectangle_StyleProperty ui_VisualElement_StyleProperty
#define ui_Rectangle_HeightProperty ui_VisualElement_HeightProperty
#define ui_Rectangle_WidthProperty ui_VisualElement_WidthProperty
#define ui_Rectangle_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Rectangle_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Rectangle_MarginProperty ui_VisualElement_MarginProperty
#define ui_Rectangle_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Rectangle_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Rectangle_WeightProperty ui_VisualElement_WeightProperty
#define ui_Rectangle_DockProperty ui_VisualElement_DockProperty
#define ui_Rectangle_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Rectangle_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Rectangle_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Rectangle_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Rectangle_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Rectangle_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Rectangle_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Rectangle_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Rectangle_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Rectangle_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Rectangle_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Rectangle_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Rectangle_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Rectangle_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Rectangle_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Rectangle_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Rectangle_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Rectangle_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Rectangle_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Rectangle_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Rectangle_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Rectangle_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Rectangle_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Rectangle_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Rectangle_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Rectangle_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Rectangle_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Rectangle_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Rectangle_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Rectangle_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Rectangle_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Rectangle_FillProperty = {"Fill", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Rectangle_StrokeProperty = {"Stroke", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Rectangle_StrokeThicknessProperty = {"StrokeThickness", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Ellipse { ui_VisualElement base; } ui_Ellipse;
static inline ui_node *ui_Ellipse_node(const ui_Ellipse *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Ellipse_create(ui_app *app, ui_Ellipse *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)62, &out->base.node);
}
#define ui_Ellipse_IdProperty ui_VisualElement_IdProperty
#define ui_Ellipse_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Ellipse_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Ellipse_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Ellipse_StyleProperty ui_VisualElement_StyleProperty
#define ui_Ellipse_HeightProperty ui_VisualElement_HeightProperty
#define ui_Ellipse_WidthProperty ui_VisualElement_WidthProperty
#define ui_Ellipse_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Ellipse_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Ellipse_MarginProperty ui_VisualElement_MarginProperty
#define ui_Ellipse_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Ellipse_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Ellipse_WeightProperty ui_VisualElement_WeightProperty
#define ui_Ellipse_DockProperty ui_VisualElement_DockProperty
#define ui_Ellipse_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Ellipse_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Ellipse_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Ellipse_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Ellipse_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Ellipse_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Ellipse_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Ellipse_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Ellipse_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Ellipse_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Ellipse_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Ellipse_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Ellipse_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Ellipse_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Ellipse_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Ellipse_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Ellipse_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Ellipse_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Ellipse_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Ellipse_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Ellipse_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Ellipse_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Ellipse_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Ellipse_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Ellipse_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Ellipse_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Ellipse_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Ellipse_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Ellipse_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Ellipse_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Ellipse_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Ellipse_FillProperty = {"Fill", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Ellipse_StrokeProperty = {"Stroke", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Ellipse_StrokeThicknessProperty = {"StrokeThickness", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Line { ui_VisualElement base; } ui_Line;
static inline ui_node *ui_Line_node(const ui_Line *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Line_create(ui_app *app, ui_Line *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)63, &out->base.node);
}
#define ui_Line_IdProperty ui_VisualElement_IdProperty
#define ui_Line_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Line_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Line_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Line_StyleProperty ui_VisualElement_StyleProperty
#define ui_Line_HeightProperty ui_VisualElement_HeightProperty
#define ui_Line_WidthProperty ui_VisualElement_WidthProperty
#define ui_Line_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Line_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Line_MarginProperty ui_VisualElement_MarginProperty
#define ui_Line_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Line_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Line_WeightProperty ui_VisualElement_WeightProperty
#define ui_Line_DockProperty ui_VisualElement_DockProperty
#define ui_Line_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Line_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Line_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Line_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Line_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Line_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Line_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Line_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Line_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Line_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Line_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Line_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Line_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Line_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Line_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Line_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Line_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Line_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Line_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Line_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Line_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Line_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Line_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Line_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Line_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Line_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Line_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Line_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Line_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Line_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Line_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Line_FillProperty = {"Fill", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Line_StrokeProperty = {"Stroke", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Line_StrokeThicknessProperty = {"StrokeThickness", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_Path { ui_VisualElement base; } ui_Path;
static inline ui_node *ui_Path_node(const ui_Path *element) { return element ? element->base.node : NULL; }
static inline ui_result ui_Path_create(ui_app *app, ui_Path *out) {
    if (!out) { return UI_ERROR_INVALID_ARGUMENT; }
    out->base.base.component = NULL; out->base.node = NULL;
    return ui_node_create(app, (ui_node_kind)64, &out->base.node);
}
#define ui_Path_IdProperty ui_VisualElement_IdProperty
#define ui_Path_IsEnabledProperty ui_VisualElement_IsEnabledProperty
#define ui_Path_DataContextProperty ui_VisualElement_DataContextProperty
static const ui_element_property ui_Path_TextProperty = {"Text", "string", UI_VALUE_STRING, 1, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
#define ui_Path_StyleProperty ui_VisualElement_StyleProperty
#define ui_Path_HeightProperty ui_VisualElement_HeightProperty
#define ui_Path_WidthProperty ui_VisualElement_WidthProperty
#define ui_Path_MaxWidthProperty ui_VisualElement_MaxWidthProperty
#define ui_Path_MaxHeightProperty ui_VisualElement_MaxHeightProperty
#define ui_Path_MarginProperty ui_VisualElement_MarginProperty
#define ui_Path_HorizontalAlignmentProperty ui_VisualElement_HorizontalAlignmentProperty
#define ui_Path_VerticalAlignmentProperty ui_VisualElement_VerticalAlignmentProperty
#define ui_Path_WeightProperty ui_VisualElement_WeightProperty
#define ui_Path_DockProperty ui_VisualElement_DockProperty
#define ui_Path_MinWidthProperty ui_VisualElement_MinWidthProperty
#define ui_Path_MinHeightProperty ui_VisualElement_MinHeightProperty
#define ui_Path_VisibilityProperty ui_VisualElement_VisibilityProperty
#define ui_Path_LayoutTransformProperty ui_VisualElement_LayoutTransformProperty
#define ui_Path_RenderTransformProperty ui_VisualElement_RenderTransformProperty
#define ui_Path_RenderTransformOriginProperty ui_VisualElement_RenderTransformOriginProperty
#define ui_Path_OpacityProperty ui_VisualElement_OpacityProperty
#define ui_Path_TabIndexProperty ui_VisualElement_TabIndexProperty
#define ui_Path_IsFocusScopeProperty ui_VisualElement_IsFocusScopeProperty
#define ui_Path_IsTabStopProperty ui_VisualElement_IsTabStopProperty
#define ui_Path_ClipToBoundsProperty ui_VisualElement_ClipToBoundsProperty
#define ui_Path_IsHitTestVisibleProperty ui_VisualElement_IsHitTestVisibleProperty
#define ui_Path_FontFamilyProperty ui_VisualElement_FontFamilyProperty
#define ui_Path_FontSizeProperty ui_VisualElement_FontSizeProperty
#define ui_Path_FontWeightProperty ui_VisualElement_FontWeightProperty
#define ui_Path_FontStyleProperty ui_VisualElement_FontStyleProperty
#define ui_Path_FontStretchProperty ui_VisualElement_FontStretchProperty
#define ui_Path_ForegroundProperty ui_VisualElement_ForegroundProperty
#define ui_Path_TextAlignmentProperty ui_VisualElement_TextAlignmentProperty
#define ui_Path_TextWrappingProperty ui_VisualElement_TextWrappingProperty
#define ui_Path_TextTrimmingProperty ui_VisualElement_TextTrimmingProperty
#define ui_Path_LineHeightProperty ui_VisualElement_LineHeightProperty
#define ui_Path_TextDecorationsProperty ui_VisualElement_TextDecorationsProperty
#define ui_Path_FlowDirectionProperty ui_VisualElement_FlowDirectionProperty
#define ui_Path_GridRowProperty ui_VisualElement_GridRowProperty
#define ui_Path_GridColumnProperty ui_VisualElement_GridColumnProperty
#define ui_Path_GridRowSpanProperty ui_VisualElement_GridRowSpanProperty
#define ui_Path_GridColumnSpanProperty ui_VisualElement_GridColumnSpanProperty
#define ui_Path_PanelZIndexProperty ui_VisualElement_PanelZIndexProperty
#define ui_Path_CanvasLeftProperty ui_VisualElement_CanvasLeftProperty
#define ui_Path_CanvasTopProperty ui_VisualElement_CanvasTopProperty
static const ui_element_property ui_Path_FillProperty = {"Fill", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Path_StrokeProperty = {"Stroke", "color", UI_VALUE_COLOR, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Path_StrokeThicknessProperty = {"StrokeThickness", "number", UI_VALUE_DOUBLE, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_Path_DataProperty = {"Data", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentWindow { ui_framework_element base; ui_node *node; } ui_DocumentWindow;
static const ui_element_property ui_DocumentWindow_xmlnsProperty = {"xmlns", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentWindow_ModelProperty = {"Model", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentWindow_TitleProperty = {"Title", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentWindow_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentWindow_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentUserControl { ui_framework_element base; ui_node *node; } ui_DocumentUserControl;
static const ui_element_property ui_DocumentUserControl_xmlnsProperty = {"xmlns", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentUserControl_ClassProperty = {"Class", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentUserControl_NamespaceProperty = {"Namespace", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentUserControl_ModelProperty = {"Model", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentUserControl_TargetTypeProperty = {"TargetType", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentControlTemplate { ui_framework_element base; ui_node *node; } ui_DocumentControlTemplate;
static const ui_element_property ui_DocumentControlTemplate_xmlnsProperty = {"xmlns", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentControlTemplate_ClassProperty = {"Class", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentControlTemplate_NamespaceProperty = {"Namespace", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentControlTemplate_ModelProperty = {"Model", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentControlTemplate_TargetTypeProperty = {"TargetType", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentTheme { ui_framework_element base; ui_node *node; } ui_DocumentTheme;
static const ui_element_property ui_DocumentTheme_xmlnsProperty = {"xmlns", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentTheme_VersionProperty = {"Version", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentTheme_BaseProperty = {"Base", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentTheme_DarkProperty = {"Dark", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentTheme_CompactProperty = {"Compact", "bool", UI_VALUE_BOOL, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentTheme_SkinProperty = {"Skin", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentResource { ui_framework_element base; ui_node *node; } ui_DocumentResource;
static const ui_element_property ui_DocumentResource_KeyProperty = {"Key", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentResource_ValueProperty = {"Value", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_DocumentDockTemplate { ui_framework_element base; ui_node *node; } ui_DocumentDockTemplate;
static const ui_element_property ui_DocumentDockTemplate_PartProperty = {"Part", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_DocumentDockTemplate_StateProperty = {"State", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_ThemeFill { ui_framework_element base; ui_node *node; } ui_ThemeFill;
static const ui_element_property ui_ThemeFill_BackgroundProperty = {"Background", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_ForegroundProperty = {"Foreground", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_TextProperty = {"Text", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_RoleProperty = {"Role", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_RadiusProperty = {"Radius", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_ThicknessProperty = {"Thickness", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeFill_PaddingProperty = {"Padding", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_ThemeBorder { ui_framework_element base; ui_node *node; } ui_ThemeBorder;
static const ui_element_property ui_ThemeBorder_BackgroundProperty = {"Background", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_ForegroundProperty = {"Foreground", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_TextProperty = {"Text", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_RoleProperty = {"Role", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_RadiusProperty = {"Radius", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_ThicknessProperty = {"Thickness", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeBorder_PaddingProperty = {"Padding", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_ThemeText { ui_framework_element base; ui_node *node; } ui_ThemeText;
static const ui_element_property ui_ThemeText_BackgroundProperty = {"Background", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_ForegroundProperty = {"Foreground", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_TextProperty = {"Text", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_RoleProperty = {"Role", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_RadiusProperty = {"Radius", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_ThicknessProperty = {"Thickness", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeText_PaddingProperty = {"Padding", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_ThemeButton { ui_framework_element base; ui_node *node; } ui_ThemeButton;
static const ui_element_property ui_ThemeButton_BackgroundProperty = {"Background", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_ForegroundProperty = {"Foreground", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_TextProperty = {"Text", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_RoleProperty = {"Role", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_RadiusProperty = {"Radius", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_ThicknessProperty = {"Thickness", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeButton_PaddingProperty = {"Padding", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};

typedef struct ui_ThemeStack { ui_framework_element base; ui_node *node; } ui_ThemeStack;
static const ui_element_property ui_ThemeStack_BackgroundProperty = {"Background", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_ForegroundProperty = {"Foreground", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_TextProperty = {"Text", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_RoleProperty = {"Role", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_OrientationProperty = {"Orientation", "string", UI_VALUE_STRING, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_RadiusProperty = {"Radius", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_ThicknessProperty = {"Thickness", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_WidthProperty = {"Width", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_HeightProperty = {"Height", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
static const ui_element_property ui_ThemeStack_PaddingProperty = {"Padding", "integer", UI_VALUE_INT, 0, 0, UI_PROPERTY_TEXT, UI_BIND_ONE_WAY};
