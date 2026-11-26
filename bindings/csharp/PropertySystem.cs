using System.Runtime.InteropServices;

namespace Sunfleck;

[Flags]
public enum PropertyEffects
{
    None = 0, Bindable = 1, TwoWay = 2, AffectsMeasure = 4,
    AffectsArrange = 8, AffectsRender = 16, AffectsInput = 32, Animatable = 64, Inherits = 128
}
public enum PropertyValueSource { Default, Local, Binding, Inherited, Theme, Style, Trigger, Template, Animation }
public enum ValuePrecedence { Inherited, Theme, Style, Trigger, Template, Local, Animation }
public enum PropertyValueType { String, Bool, Int, Double, Command, Model, Color, Thickness, CornerRadius, Point, Size, Matrix, Length, Enum }

/// <summary>Native metadata for an existing built-in mutable property.</summary>
public sealed record BuiltInPropertyDescriptor(PropertyTarget Property, string Name,
    PropertyValueType Type, PropertyEffects Effects, object? DefaultValue)
{
    public static BuiltInPropertyDescriptor Describe(NodeKind kind, PropertyTarget property)
    {
        var descriptor = new Native.BuiltInPropertyDescriptor {
            Size = (uint)Marshal.SizeOf<Native.BuiltInPropertyDescriptor>(), Version = 1
        };
        SunfleckException.Check(Native.DescribeProperty(kind, property, ref descriptor));
        object? value = ComponentValues.Decode(descriptor.Default, null, null);
        return new(descriptor.Property, Marshal.PtrToStringUTF8(descriptor.Name)!,
            (PropertyValueType)descriptor.Kind, descriptor.Effects, value);
    }
}

public sealed partial class App
{
    public void SetResource(string key, object value) => UpdateResource(key,value,false);
    public void ClearResource(string key) => UpdateResource(key,null,false);
    public void SetThemeResource(string key, object value) => UpdateResource(key,value,true);
    public void ClearThemeResource(string key) => UpdateResource(key,null,true);
    private unsafe void UpdateResource(string key, object? value, bool theme) {
        using var encoded = value is null ? null : new ComponentValues(value,value.GetType());
        var native = encoded?.Value ?? default;
        var status = theme ? Native.SetThemeResource(Handle,Utf8(key),value is null ? null : &native)
                           : Native.SetResource(Handle,Utf8(key),value is null ? null : &native);
        SunfleckException.Check(status);
    }
}

internal static unsafe partial class Native
{
    [DllImport(Library, EntryPoint = "ui_node_set_resource_reference", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetResourceReference(nint node, PropertyTarget property, ValuePrecedence layer, [MarshalAs(UnmanagedType.LPUTF8Str)] string key);
    [DllImport(Library, EntryPoint = "ui_node_set_resource", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetNodeResource(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string key, PropertyValue* value);
    [DllImport(Library, EntryPoint = "ui_app_set_resource", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetResource(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string key, PropertyValue* value);
    [DllImport(Library, EntryPoint = "ui_app_set_theme_resource", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetThemeResource(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string key, PropertyValue* value);
}

public sealed partial class Node
{
    public void SetResourceReference(PropertyTarget property, string key, ValuePrecedence layer = ValuePrecedence.Local)
        => SunfleckException.Check(Native.SetResourceReference(Handle,property,layer,App.Utf8(key)));
    public unsafe void SetResource(string key, object value) {
        ArgumentNullException.ThrowIfNull(value);
        using var encoded = new ComponentValues(value,value.GetType());
        var native = encoded.Value;
        SunfleckException.Check(Native.SetNodeResource(Handle,App.Utf8(key),&native));
    }
    public unsafe void ClearResource(string key) => SunfleckException.Check(Native.SetNodeResource(Handle,App.Utf8(key),null));
    public PropertyModel? DataContext {
        get {
            SunfleckException.Check(Native.GetNodeDataContext(Handle, out var model));
            return model == 0 ? null : App.ResolveComponentModel(model);
        }
        set {
            if (value is not null) { App.CheckOwner(value.App); }
            App.CheckBindingResult(Native.SetNodeDataContext(Handle, value?.Handle ?? 0));
        }
    }
    public PropertyValueSource DataContextSource {
        get { SunfleckException.Check(Native.GetNodeDataContextSource(Handle, out var source)); return source; }
    }
    public void ClearDataContext() => App.CheckBindingResult(Native.ClearNodeDataContext(Handle));
    public (Result Status,string Code,string Message) GetValidation(PropertyTarget property) {
        SunfleckException.Check(Native.GetValidation(Handle,property,out var result));
        return (result.Status,Marshal.PtrToStringUTF8(result.Code)!,Marshal.PtrToStringUTF8(result.Message)!);
    }
    public void BindElement(PropertyTarget property, Node source, string path, PropertyBindingMode mode = PropertyBindingMode.Default) {
        App.CheckOwner(source.App);
        var binding = new Native.PropertyBinding {
            Size = (uint)Marshal.SizeOf<Native.PropertyBinding>(), Version = 1, Property = property,
            Path = App.Utf8(path), Mode = mode, Trigger = UpdateSourceTrigger.PropertyChanged, Format = ""
        };
        App.CheckBindingResult(Native.BindElementProperty(Handle,source.Handle,in binding));
    }
    public void BindContext(PropertyTarget property, string path, PropertyBindingMode mode = PropertyBindingMode.Default,
                            UpdateSourceTrigger trigger = UpdateSourceTrigger.PropertyChanged, string? stringFormat = null) {
        var binding = new Native.PropertyBinding {
            Size = (uint)Marshal.SizeOf<Native.PropertyBinding>(), Version = 1, Property = property,
            Path = App.Utf8(path), Mode = mode, Trigger = trigger, Format = App.Utf8(stringFormat ?? "")
        };
        App.CheckBindingResult(Native.BindContextProperty(Handle, in binding));
    }
    public PropertyValueSource GetPropertySource(PropertyTarget property)
    {
        SunfleckException.Check(Native.GetPropertySource(Handle, property, out var source));
        return source;
    }

    /// <summary>Removes the local value or binding and restores the native default.</summary>
    public void ClearProperty(PropertyTarget property)
        => SunfleckException.Check(Native.ClearProperty(Handle, property));

    /// <summary>Copies strings; integer values are returned as Int64. Commands retain App ownership.</summary>
    public object? GetProperty(PropertyTarget property)
    {
        SunfleckException.Check(Native.GetProperty(Handle, property, out var value));
        return ComponentValues.Decode(value, null, App);
    }

    public object? GetBaseProperty(PropertyTarget property) {
        SunfleckException.Check(Native.GetBaseProperty(Handle, property, out var value));
        return ComponentValues.Decode(value, null, App);
    }
    public void ClearPropertyLayer(PropertyTarget property, ValuePrecedence precedence)
        => SunfleckException.Check(Native.ClearPropertyLayer(Handle, property, precedence));
    public void SetProperty(PropertyTarget property, object? value)
        => SetPropertyLayer(property, ValuePrecedence.Local, value);
    public void SetPropertyLayer(PropertyTarget property, ValuePrecedence precedence, object? value) {
        if (value is Command command) { App.CheckOwner(command.App); }
        if (value is PropertyModel model) { App.CheckOwner(model.App); }
        var type = value?.GetType() ?? (property == PropertyTarget.Command ? typeof(Command) : property is PropertyTarget.DataContext or PropertyTarget.ItemsSource ? typeof(PropertyModel) : throw new ArgumentNullException(nameof(value)));
        using var encoded = new ComponentValues(value, type);
        SunfleckException.Check(Native.SetPropertyLayer(Handle, property, precedence, in encoded.Value));
    }
}

public sealed record ComponentPropertyDescriptor(string Name, PropertyValueType Type, PropertyEffects Effects, bool ReadOnly, object? DefaultValue);

public abstract partial class FrameworkElement
{
    public void ClearDataContext() => Node.ClearDataContext();
    public T GetBaseValue<T>(string name) {
        SunfleckException.Check(Native.ComponentGetBase(Handle, App.Utf8(name), out var value));
        return (T)ComponentValues.Decode(value, typeof(T), App)!;
    }
    public void SetLayer<T>(string name, ValuePrecedence precedence, T value) {
        if (value is Command command) { App.CheckOwner(command.App); }
        if (value is PropertyModel model) { App.CheckOwner(model.App); }
        using var encoded = new ComponentValues(value, typeof(T));
        App.CheckBindingResult(Native.ComponentSetLayer(Handle, App.Utf8(name), precedence, in encoded.Value));
    }
    public void ClearLayer(string name, ValuePrecedence precedence = ValuePrecedence.Local)
        => App.CheckBindingResult(Native.ComponentClearLayer(Handle, App.Utf8(name), precedence));
    public PropertyValueSource GetValueSource(string name) {
        SunfleckException.Check(Native.ComponentSource(Handle, App.Utf8(name), out var source)); return source;
    }
    public ComponentPropertyDescriptor DescribeProperty(string name) {
        var descriptor = new Native.ComponentPropertyDescriptor { Size = (uint)Marshal.SizeOf<Native.ComponentPropertyDescriptor>(), Version = 1 };
        SunfleckException.Check(Native.ComponentDescribe(Handle, App.Utf8(name), ref descriptor));
        return new(Marshal.PtrToStringUTF8(descriptor.Name)!, (PropertyValueType)descriptor.Kind, descriptor.Effects, descriptor.ReadOnly != 0,
            ComponentValues.Decode(descriptor.Default, null, App));
    }

    public void ClearValue<T>(ElementProperty<T> property)
        => Node.ClearProperty(property.Target ?? throw new ArgumentException("This attribute has no mutable property slot.", nameof(property)));
    public PropertyValueSource GetValueSource<T>(ElementProperty<T> property)
        => Node.GetPropertySource(property.Target ?? throw new ArgumentException("This attribute has no mutable property slot.", nameof(property)));
}

internal static partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct ComponentPropertyDescriptor {
        internal uint Size, Version;
        internal nint Name;
        internal ValueKind Kind;
        internal PropertyEffects Effects;
        internal uint ReadOnly;
        internal PropertyValue Default;
    }
    [DllImport(Library, EntryPoint = "ui_component_get_base_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ComponentGetBase(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_component_get_property_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ComponentSource(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out PropertyValueSource source);
    [DllImport(Library, EntryPoint = "ui_component_describe_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ComponentDescribe(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, ref ComponentPropertyDescriptor descriptor);
    [DllImport(Library, EntryPoint = "ui_component_set_property_layer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ComponentSetLayer(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, ValuePrecedence precedence, in PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_component_clear_property_layer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ComponentClearLayer(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, ValuePrecedence precedence);
    [StructLayout(LayoutKind.Sequential)]
    internal struct BuiltInPropertyDescriptor
    {
        internal uint Size, Version;
        internal PropertyTarget Property;
        internal nint Name;
        internal ValueKind Kind;
        internal PropertyEffects Effects;
        internal PropertyValue Default;
    }
    [DllImport(Library, EntryPoint = "ui_node_get_base_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetBaseProperty(nint node, PropertyTarget property, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_set_property_layer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetPropertyLayer(nint node, PropertyTarget property, ValuePrecedence precedence, in PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_clear_property_layer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ClearPropertyLayer(nint node, PropertyTarget property, ValuePrecedence precedence);
    [DllImport(Library, EntryPoint = "ui_property_describe", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DescribeProperty(NodeKind kind, PropertyTarget property, ref BuiltInPropertyDescriptor descriptor);
    [DllImport(Library, EntryPoint = "ui_node_get_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetProperty(nint node, PropertyTarget property, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_set_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetProperty(nint node, PropertyTarget property, in PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_clear_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ClearProperty(nint node, PropertyTarget property);
    [DllImport(Library, EntryPoint = "ui_node_get_property_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetPropertySource(nint node, PropertyTarget property, out PropertyValueSource source);
}
