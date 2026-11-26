using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed partial class PropertyModel
{
    private static unsafe void AddBrowserProperty(nint model, string path, Native.ValueKind kind, Entry entry)
    {
        entry.BrowserState = GCHandle.Alloc(entry);
        var property = new Native.BrowserModelProperty {
            Size = (uint)Marshal.SizeOf<Native.BrowserModelProperty>(), Version = 1, Path = path, Kind = kind,
            Get = (nint)(delegate* unmanaged[Cdecl]<nint, Native.PropertyValue*, Result>)&BrowserGet,
            Set = entry.Set is null ? 0 : (nint)(delegate* unmanaged[Cdecl]<nint, Native.PropertyValue*, Result>)&BrowserSet,
            Context = GCHandle.ToIntPtr(entry.BrowserState),
        };
        SunfleckException.Check(Native.AddBrowserProperty(model, in property));
    }
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe Result BrowserGet(nint context, Native.PropertyValue* value)
        => ((Entry)GCHandle.FromIntPtr(context).Target!).Get(context, out *value);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe Result BrowserSet(nint context, Native.PropertyValue* value)
        => ((Entry)GCHandle.FromIntPtr(context).Target!).Set!(context, in *value);
}

internal static partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct BrowserModelProperty
    {
        internal uint Size, Version;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Path;
        internal ValueKind Kind;
        internal nint Get, Set, Context;
    }
    [DllImport(Library, EntryPoint = "ui_model_add_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result AddBrowserProperty(nint model, in BrowserModelProperty property);
}
