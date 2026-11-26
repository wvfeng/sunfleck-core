namespace Sunfleck;

/// <summary>A typed Fleck property. Binding metadata is available without constructing an element.</summary>
public sealed record ElementProperty<T>(string Name, PropertyTarget? Target = null,
    PropertyBindingMode Mode = PropertyBindingMode.OneWay)
{
    public bool IsBindable => Target is not null;
}

/// <summary>Base for typed elements. Existing Node handles remain supported.</summary>
public abstract partial class FrameworkElement
{
    private readonly Node? node;
    protected FrameworkElement() { }
    protected FrameworkElement(Node node) { this.node = node; }

    public Length Height { get => Node.Height; set => Node.Height = value; }
    public Length Width { get => Node.Width; set => Node.Width = value; }
    public Length MinWidth { get => Node.MinWidth; set => Node.MinWidth = value; }
    public Length MaxWidth { get => Node.MaxWidth; set => Node.MaxWidth = value; }
    public Length MinHeight { get => Node.MinHeight; set => Node.MinHeight = value; }
    public Length MaxHeight { get => Node.MaxHeight; set => Node.MaxHeight = value; }
    public Thickness Margin { get => Node.Margin; set => Node.Margin = value; }
    public string HorizontalAlignment { get => Node.HorizontalAlignment; set => Node.HorizontalAlignment = value; }
    public string VerticalAlignment { get => Node.VerticalAlignment; set => Node.VerticalAlignment = value; }

    public T GetValue<T>(ElementProperty<T> property)
    {
        if (property.Target is >= PropertyTarget.FontFamily and <= PropertyTarget.FlowDirection) {
            var raw = Node.GetProperty(property.Target.Value);
            return typeof(T).IsEnum ? (T)Enum.ToObject(typeof(T), raw!) : (T)raw!;
        }
        if (property.Target == PropertyTarget.Visibility) { return (T)(object)(Visibility)(long)Node.GetProperty(PropertyTarget.Visibility)!; }
        if (property.Target is PropertyTarget.IsFocusScope or PropertyTarget.IsTabStop or PropertyTarget.IsHitTestVisible or PropertyTarget.ClipToBounds or PropertyTarget.Opacity or PropertyTarget.RenderTransform or PropertyTarget.RenderTransformOrigin or PropertyTarget.LayoutTransform) { return (T)Node.GetProperty(property.Target.Value)!; }
        if (property.Target is >= PropertyTarget.Background and <= PropertyTarget.CornerRadius) { return (T)Node.GetProperty(property.Target.Value)!; }
        if (property.Target is PropertyTarget.Spacing or PropertyTarget.Padding) { return (T)Node.GetProperty(property.Target.Value)!; }
        if (property.Target is PropertyTarget.TabIndex or PropertyTarget.Weight) { return (T)(object)checked((int)(long)Node.GetProperty(property.Target.Value)!); }
        if (property.Target is >= PropertyTarget.Width and <= PropertyTarget.VerticalAlignment) { return (T)Node.GetLayoutValue(property.Target.Value); }
        object? value = property.Target switch {
            PropertyTarget.ItemsSource => Node.GetProperty(PropertyTarget.ItemsSource),
            PropertyTarget.Source => Node.GetProperty(PropertyTarget.Source),
            PropertyTarget.DataContext => Node.DataContext,
            PropertyTarget.Text or PropertyTarget.Content => Node.Text,
            PropertyTarget.Height => Node.Height,
            PropertyTarget.IsEnabled => Node.IsEnabled,
            PropertyTarget.Value => typeof(T) == typeof(bool) ? (object)(Node.Value != 0) : checked((int)Node.Value),
            _ => throw new NotSupportedException($"{property.Name} has no native value getter.")
        };
        return (T)value!;
    }
    public void SetValue<T>(ElementProperty<T> property, T value)
    {
        if (property.Target is >= PropertyTarget.Background and <= PropertyTarget.IsFocusScope) { Node.SetProperty(property.Target.Value, value); return; }
        if (property.Target is >= PropertyTarget.Spacing and <= PropertyTarget.Weight) { Node.SetProperty(property.Target.Value, value); return; }
        if (property.Target is >= PropertyTarget.Width and <= PropertyTarget.VerticalAlignment) { Node.SetLayoutValue(property.Target.Value, value!); return; }
        switch (property.Target) {
            case PropertyTarget.ItemsSource:
            case PropertyTarget.Source:
                Node.SetProperty(property.Target.Value, value);
                break;
            case PropertyTarget.DataContext: Node.DataContext = (PropertyModel?)(object?)value; break;
            case PropertyTarget.Text: case PropertyTarget.Content: Node.Text = (string)(object)value!; break;
            case PropertyTarget.Height: Node.SetProperty(PropertyTarget.Height,value); break;
            case PropertyTarget.IsEnabled: Node.IsEnabled = (bool)(object)value!; break;
            case PropertyTarget.Value: Node.Value = value is bool boolean ? (boolean ? 1u : 0u) : checked(Convert.ToUInt32(value)); break;
            case PropertyTarget.Command: Node.BindCommand((Command?)(object?)value); break;
            default: throw new NotSupportedException($"{property.Name} is a Fleck declaration attribute, not a mutable native property.");
        }
    }
    public void Bind<T>(ElementProperty<T> property, PropertyModel model, string path,
        PropertyBindingMode mode = PropertyBindingMode.Default)
    {
        if (property.Target is not { } target) { throw new ArgumentException("This attribute cannot be bound.", nameof(property)); }
        model.Bind(Node, target, path, mode == PropertyBindingMode.Default ? property.Mode : mode);
    }
}
