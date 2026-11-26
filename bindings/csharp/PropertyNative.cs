using System.Runtime.InteropServices;

namespace Sunfleck;

public enum PropertyTarget { Text, Content, IsEnabled, Value, Command, Height, Width, MinWidth, MaxWidth, MinHeight, MaxHeight, Margin, HorizontalAlignment, VerticalAlignment, Spacing, Padding, Weight, DataContext, Background, BorderBrush, BorderThickness, CornerRadius, Visibility, IsHitTestVisible, FontFamily, FontSize, FontWeight, FontStyle, FontStretch, Foreground, TextAlignment, TextWrapping, TextTrimming, LineHeight, TextDecorations, FlowDirection, ClipToBounds, Opacity, RenderTransform, RenderTransformOrigin, LayoutTransform, IsTabStop, TabIndex, IsFocusScope, ItemsSource, Source }
public enum UpdateSourceTrigger { PropertyChanged, LostFocus }
public enum PropertyBindingMode { OneWay, OneTime, TwoWay, Default }
public enum CommandMode { Default, Sync, Async }

internal static unsafe partial class Native
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result AsyncWork(nint context, nint cancellation);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void AsyncCancel(nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void AsyncCompleted(nint context, Result result);
    [StructLayout(LayoutKind.Sequential)]
    internal struct AsyncCommandOptions
    {
        internal uint Size, Version;
        internal AsyncWork Execute;
        internal AsyncCancel Cancel;
        internal AsyncCompleted Completed;
        internal nint Context;
    }
    [DllImport(Library, EntryPoint = "ui_command_create_async", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateAsyncCommand(nint app, in AsyncCommandOptions options, out nint command);
    [DllImport(Library, EntryPoint = "ui_cancellation_requested", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint CancellationRequested(nint token);
    [DllImport(Library, EntryPoint = "ui_command_cancel", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CancelCommand(nint command);
    [DllImport(Library, EntryPoint = "ui_app_cancel_commands", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CancelCommands(nint app);
    [DllImport(Library, EntryPoint = "ui_command_is_running", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CommandIsRunning(nint command, out uint running);
    [DllImport(Library, EntryPoint = "ui_node_set_command_mode", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetCommandMode(nint node, CommandMode mode);
    [DllImport(Library, EntryPoint = "ui_node_set_layout_value", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetLayoutValue(nint node, PropertyTarget property, in PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_node_get_layout_value", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetLayoutValue(nint node, PropertyTarget property, out PropertyValue value);
    internal enum ValueKind { String, Bool, Int, Double, Command, Model, Color, Thickness, CornerRadius, Point, Size, Matrix, Length, Enum }
    [StructLayout(LayoutKind.Sequential)]
    internal struct PropertyValue
    {
        internal ValueKind Kind;
        internal long Integer;
        internal double Number;
        internal nint Text, Command;
    }
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result PropertyGet(nint context, out PropertyValue value);
    [StructLayout(LayoutKind.Sequential)]
    internal struct Validation { internal Result Status; internal nint Code, Message; }
    [DllImport(Library, EntryPoint = "ui_node_get_validation", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetValidation(nint node, PropertyTarget property, out Validation result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result BindingConvert(nint context, PropertyValue* inputs, uint count, out PropertyValue value);
    [DllImport(Library, EntryPoint = "ui_model_add_converter", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result AddConverter(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, ValueKind kind, BindingConvert convert, nint context);
    [DllImport(Library, EntryPoint = "ui_model_add_multi_binding", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result AddMultiBinding(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string converter, nint[] inputs, uint count);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result PropertySet(nint context, in PropertyValue value);
    [StructLayout(LayoutKind.Sequential)]
    internal struct ModelProperty
    {
        internal uint Size, Version;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Path;
        internal ValueKind Kind;
        internal PropertyGet Get;
        internal PropertySet? Set;
        internal nint Context;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct PropertyBinding
    {
        internal uint Size, Version;
        internal PropertyTarget Property;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Path;
        internal PropertyBindingMode Mode;
        internal UpdateSourceTrigger Trigger;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Format;
    }
    [DllImport(Library, EntryPoint = "ui_model_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateModel(nint app, out nint model);
    [DllImport(Library, EntryPoint = "ui_model_destroy", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DestroyModel(nint model);
    [DllImport(Library, EntryPoint = "ui_model_add_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result AddProperty(nint model, in ModelProperty property);
    [DllImport(Library, EntryPoint = "ui_model_notify_property_changed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result NotifyProperty(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Library, EntryPoint = "ui_model_begin_update", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BeginModelUpdate(nint model);
    [DllImport(Library, EntryPoint = "ui_model_end_update", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result EndModelUpdate(nint model);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void PostCallback(nint context);
    [DllImport(Library, EntryPoint = "ui_app_post", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Post(nint app, PostCallback work, nint context, PostCallback release);
    [DllImport(Library, EntryPoint = "ui_node_bind_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindProperty(nint node, nint model, in PropertyBinding binding);
    [DllImport(Library, EntryPoint = "ui_node_bind_context_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindContextProperty(nint node, in PropertyBinding binding);
    [DllImport(Library, EntryPoint = "ui_node_bind_element_property", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindElementProperty(nint node, nint source, in PropertyBinding binding);
    [DllImport(Library, EntryPoint = "ui_node_set_data_context", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetNodeDataContext(nint node, nint model);
    [DllImport(Library, EntryPoint = "ui_node_clear_data_context", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ClearNodeDataContext(nint node);
    [DllImport(Library, EntryPoint = "ui_node_get_data_context", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetNodeDataContext(nint node, out nint model);
    [DllImport(Library, EntryPoint = "ui_node_get_data_context_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetNodeDataContextSource(nint node, out PropertyValueSource source);
    [DllImport(Library, EntryPoint = "ui_node_clear_binding", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ClearBinding(nint node, PropertyTarget property);
    [DllImport(Library, EntryPoint = "ui_node_update_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result UpdateSource(nint node, PropertyTarget property);
    [DllImport(Library, EntryPoint = "ui_node_set_enabled", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetEnabled(nint node, uint enabled);
    [DllImport(Library, EntryPoint = "ui_node_get_enabled", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetEnabled(nint node, out uint enabled);
    [DllImport(Library, EntryPoint = "ui_app_load_fleck_with_model", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result LoadFleckModel(nint app, byte* xml, nuint size, nint model, Context* context, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint = "ui_app_load_fleck_resource_with_model", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result LoadResourceModel(nint app, byte* bytes, nuint size, nint model, Context* context, out Diagnostic diagnostic);
}
