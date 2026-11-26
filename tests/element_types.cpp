#include <sunfleck/elements.hpp>
#include <cstdio>
#include "../src/ui_tree.h"
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Check failed at line %d\n", __LINE__); return 1; } } while (0)
#include <type_traits>
int main() {
    using namespace sunfleck;
    static_assert(std::is_base_of_v<FrameworkElement, elements::Slider>);
    static_assert(std::is_base_of_v<elements::VisualElement, elements::TextBlock>);
    static_assert(std::is_base_of_v<elements::LayoutElement, elements::StackPanel>);
    static_assert(!std::is_base_of_v<elements::LayoutElement, elements::TextBlock>);
    static_assert(&elements::TextBlock::IdProperty == &elements::Image::IdProperty);
    static_assert(&elements::StackPanel::PaddingProperty == &elements::Grid::PaddingProperty);
    static_assert(std::is_same_v<decltype(elements::Slider::ValueProperty)::value_type, int>);
    static_assert(std::is_base_of_v<elements::LayoutElement, elements::Border>);
    static_assert(std::is_same_v<decltype(elements::Border::BackgroundProperty)::value_type, ui_color>);
    static_assert(std::is_same_v<decltype(elements::Border::CornerRadiusProperty)::value_type, ui_corner_radius>);
    static_assert(std::is_same_v<decltype(elements::TextBlock::FontSizeProperty)::value_type, double>);
    static_assert(std::is_same_v<decltype(elements::TextBlock::FontWeightProperty)::value_type, ui_font_weight>);
    static_assert(std::is_same_v<decltype(elements::TextBlock::TextAlignmentProperty)::value_type, ui_text_alignment>);
    static_assert(std::is_same_v<decltype(elements::TextBlock::ForegroundProperty)::value_type, ui_color>);
    static_assert(std::is_same_v<decltype(elements::TextBlock::LineHeightProperty)::value_type, double>);
    App app(UI_SOFTWARE, true);
    elements::Slider slider(app);
    int score = 42;
    PropertyModel model(app);
    model.property("Score", [&] { return score; }, [&](int value) { score = value; });
    slider.bind(elements::Slider::ValueProperty, model, "Score");
    CHECK(slider.node().value() == 42);
    slider.node().nativeHandle()->value = 80; // Simulate a user edit without detaching the binding.
    slider.node().updateSource(UI_PROPERTY_VALUE);
    CHECK(score == 80);
    elements::TextBox box(app);
    box.node().setText("native types");
    CHECK(box.node().text() == "native types");
    box.bind(elements::TextBox::HeightProperty, model, "Score");
    CHECK(box.node().height() == 80);
    try {
        slider.bind(elements::StackPanel::PaddingProperty, model, "Score");
        CHECK(false);
    } catch (const Error& error) { CHECK(error.result() == UI_ERROR_INVALID_ARGUMENT); }
}
