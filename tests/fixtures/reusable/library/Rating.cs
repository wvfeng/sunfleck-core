using Sunfleck;

namespace Example;

[FleckComponent("urn:example:controls", "Rating", Template = "RatingTemplate")]
[FleckTemplatePart("PART_Value", NodeKind.Slider)]
[FleckEvent("Changed")]
public sealed class Rating : Control
{
    [FleckProperty(Default = 0, Mode = PropertyBindingMode.TwoWay,
        Effects = PropertyEffects.Bindable | PropertyEffects.TwoWay | PropertyEffects.Animatable | PropertyEffects.AffectsRender)]
    public int Value { get => Get<int>(nameof(Value)); set => Set(nameof(Value), value); }
    protected override void OnPropertyChanged(string name) {
        if (name == nameof(Value)) { Raise("Changed", Value); }
    }
}
