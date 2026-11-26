using System.Runtime.InteropServices;

namespace Sunfleck;

/// <summary>Runtime identity. Resolve again in a new App; never persist the numeric value.</summary>
public readonly record struct RegisteredProperty(ulong Id);
public sealed record RegisteredPropertyDescriptor(RegisteredProperty Property, string Name,
    string OwnerNamespace, string OwnerName, PropertyValueType Type, PropertyEffects Effects,
    bool ReadOnly, object? DefaultValue);

public sealed partial class Node
{
    public RegisteredProperty ResolveProperty(string name) {
        ArgumentNullException.ThrowIfNull(name);
        SunfleckException.Check(Native.ResolveProperty(Handle, App.Utf8(name), out var id));
        return new(id);
    }
    public RegisteredPropertyDescriptor DescribeProperty(RegisteredProperty property) {
        var descriptor = new Native.RegisteredPropertyDescriptor {
            Size = (uint)Marshal.SizeOf<Native.RegisteredPropertyDescriptor>(), Version = 1
        };
        SunfleckException.Check(Native.DescribeRegisteredProperty(Handle, property.Id, ref descriptor));
        return new(new(descriptor.Id), Marshal.PtrToStringUTF8(descriptor.Name)!,
            Marshal.PtrToStringUTF8(descriptor.OwnerNamespace)!, Marshal.PtrToStringUTF8(descriptor.OwnerName)!,
            (PropertyValueType)descriptor.Kind, descriptor.Effects, descriptor.ReadOnly != 0,
            ComponentValues.Decode(descriptor.Default, null, App));
    }
    public object? GetProperty(RegisteredProperty property) {
        SunfleckException.Check(Native.GetRegisteredProperty(Handle, property.Id, out var value));
        return ComponentValues.Decode(value, null, App);
    }
    public object? GetBaseProperty(RegisteredProperty property) {
        SunfleckException.Check(Native.GetRegisteredBaseProperty(Handle, property.Id, out var value));
        return ComponentValues.Decode(value, null, App);
    }
    public PropertyValueSource GetPropertySource(RegisteredProperty property) {
        SunfleckException.Check(Native.GetRegisteredPropertySource(Handle, property.Id, out var source));
        return source;
    }
    public void SetProperty(RegisteredProperty property, object? value)
        => SetPropertyLayer(property, ValuePrecedence.Local, value);
    public void SetPropertyLayer(RegisteredProperty property, ValuePrecedence precedence, object? value) {
        if (value is Command command) { App.CheckOwner(command.App); }
        if (value is PropertyModel model) { App.CheckOwner(model.App); }
        var type = value?.GetType() ?? (DescribeProperty(property).Type switch {
            PropertyValueType.Model => typeof(PropertyModel), PropertyValueType.Command => typeof(Command),
            PropertyValueType.String => typeof(string), _ => throw new ArgumentNullException(nameof(value))
        });
        using var encoded = new ComponentValues(value, type);
        App.CheckBindingResult(Native.SetRegisteredProperty(Handle, property.Id, precedence, in encoded.Value));
    }
    public void ClearProperty(RegisteredProperty property) => ClearPropertyLayer(property, ValuePrecedence.Local);
    public void ClearPropertyLayer(RegisteredProperty property, ValuePrecedence precedence)
        => App.CheckBindingResult(Native.ClearRegisteredProperty(Handle, property.Id, precedence));
    public void BindProperty(RegisteredProperty property, PropertyModel model, string path,
        PropertyBindingMode mode = PropertyBindingMode.Default,
        UpdateSourceTrigger trigger = UpdateSourceTrigger.PropertyChanged, string? stringFormat = null) {
        ArgumentNullException.ThrowIfNull(model); ArgumentNullException.ThrowIfNull(path);
        App.CheckOwner(model.App);
        var options = new Native.RegisteredBindingOptions {
            Size = (uint)Marshal.SizeOf<Native.RegisteredBindingOptions>(), Version = 1,
            Property = property.Id, Path = App.Utf8(path), Mode = mode, Trigger = trigger, Format = App.Utf8(stringFormat ?? "")
        };
        App.CheckBindingResult(Native.BindRegisteredProperty(Handle, model.Handle, in options));
    }
    public void BindContext(RegisteredProperty property, string path,
        PropertyBindingMode mode = PropertyBindingMode.Default,
        UpdateSourceTrigger trigger = UpdateSourceTrigger.PropertyChanged, string? stringFormat = null) {
        ArgumentNullException.ThrowIfNull(path);
        var options = new Native.RegisteredBindingOptions {
            Size = (uint)Marshal.SizeOf<Native.RegisteredBindingOptions>(), Version = 1,
            Property = property.Id, Path = App.Utf8(path), Mode = mode, Trigger = trigger, Format = App.Utf8(stringFormat ?? "")
        };
        App.CheckBindingResult(Native.BindRegisteredContextProperty(Handle, in options));
    }
}

internal static partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct RegisteredPropertyDescriptor {
        internal uint Size, Version;
        internal ulong Id;
        internal nint Name, OwnerNamespace, OwnerName;
        internal ValueKind Kind;
        internal PropertyEffects Effects;
        internal uint ReadOnly;
        internal PropertyValue Default;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct RegisteredBindingOptions {
        internal uint Size, Version;
        internal ulong Property;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Path;
        internal PropertyBindingMode Mode;
        internal UpdateSourceTrigger Trigger;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Format;
    }
    [DllImport(Library, EntryPoint = "ui_node_resolve_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ResolveProperty(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out ulong property);
    [DllImport(Library, EntryPoint = "ui_node_describe_registered_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DescribeRegisteredProperty(nint node, ulong property, ref RegisteredPropertyDescriptor descriptor);
    [DllImport(Library, EntryPoint = "ui_node_get_registered_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetRegisteredProperty(nint node, ulong property, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_get_registered_base_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetRegisteredBaseProperty(nint node, ulong property, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_get_registered_property_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetRegisteredPropertySource(nint node, ulong property, out PropertyValueSource source);
    [DllImport(Library, EntryPoint = "ui_node_set_registered_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetRegisteredProperty(nint node, ulong property, ValuePrecedence precedence, in PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_clear_registered_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ClearRegisteredProperty(nint node, ulong property, ValuePrecedence precedence);
    [DllImport(Library, EntryPoint = "ui_node_bind_registered_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindRegisteredProperty(nint node, nint model, in RegisteredBindingOptions options);
    [DllImport(Library, EntryPoint = "ui_node_bind_registered_context_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindRegisteredContextProperty(nint node, in RegisteredBindingOptions options);
}
