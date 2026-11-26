using System.Runtime.InteropServices;

namespace Sunfleck;

internal static unsafe partial class Native
{
    private const string Library = "sunfleck_native";

    [StructLayout(LayoutKind.Sequential)]
    internal struct AppOptions
    {
        internal uint Size, Version, Renderer, Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Diagnostic
    {
        internal FleckError Code;
        internal uint Line, Column;
        internal fixed byte Message[128];
        public override string ToString() {
            fixed (byte* message = Message) { return Marshal.PtrToStringUTF8((nint)message) ?? ""; }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Export
    {
        internal nint Name, Value;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Context
    {
        internal Export* Text;
        internal nuint TextCount;
        internal Export* Commands;
        internal nuint CommandCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct Theme
    {
        internal uint Size;
        internal fixed uint Colors[11];
        internal uint Gap, ControlHeight, WindowPadding, GalleryGap;
    }

    [DllImport(Library, EntryPoint = "ui_theme_init", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result InitializeTheme(out Theme theme, uint dark, uint compact);

    [DllImport(Library, EntryPoint = "ui_theme_apply_overrides", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ApplyThemeOverrides(ref Theme theme, [MarshalAs(UnmanagedType.LPUTF8Str)] string overrides);

    [DllImport(Library, EntryPoint = "ui_app_set_theme", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetTheme(nint app, in Theme theme);

    [DllImport(Library, EntryPoint = "ui_app_set_builtin_theme", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetBuiltinTheme(nint app, BuiltinTheme theme, uint dark, uint compact);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void ActionCallback(nint node, nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int PointerHandler(nint sender,nint source,PointerAction action,EventPhase phase,int x,int y,nint context);
    [DllImport(Library,EntryPoint="ui_node_set_pointer_handler",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetPointerHandler(nint node,PointerAction action,EventPhase phase,PointerHandler? handler,nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int ActivationHandler(nint sender,nint source,EventPhase phase,nint context);
    [DllImport(Library,EntryPoint="ui_node_set_key_binding",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetKeyBinding(nint node,uint key,KeyModifiers modifiers,EventPhase phase,[MarshalAs(UnmanagedType.LPUTF8Str)] string? command);
    [DllImport(Library,EntryPoint="ui_node_set_input_binding",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetInputBinding(nint node,uint key,KeyModifiers modifiers,[MarshalAs(UnmanagedType.LPUTF8Str)] string? command);
    [DllImport(Library,EntryPoint="ui_node_set_routed_command",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetRoutedCommand(nint node,[MarshalAs(UnmanagedType.LPUTF8Str)] string? name);
    [DllImport(Library,EntryPoint="ui_node_set_command_binding",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetCommandBinding(nint node,[MarshalAs(UnmanagedType.LPUTF8Str)] string name,nint command);
    [DllImport(Library,EntryPoint="ui_node_set_activation_handler",CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetActivationHandler(nint node,EventPhase phase,ActivationHandler? handler,nint context);


    [DllImport(Library, EntryPoint = "ui_app_create", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Create(in AppOptions options, out nint app);
    [DllImport(Library, EntryPoint = "ui_api_version", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint ApiVersion();

    [DllImport(Library, EntryPoint = "ui_app_destroy", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void Destroy(nint app);

    [DllImport(Library, EntryPoint = "ui_app_set_window_options", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetWindowOptions(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string title, uint width, uint height);

    [DllImport(Library, EntryPoint = "ui_app_open_window", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result OpenWindow(nint app);

    [DllImport(Library, EntryPoint = "ui_app_poll", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Poll(nint app, uint timeout);

    [DllImport(Library, EntryPoint = "ui_app_request_close", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RequestClose(nint app);

    [DllImport(Library, EntryPoint = "ui_app_activate_button", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ActivateButton(nint app);

    [DllImport(Library, EntryPoint = "ui_app_pixels", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint Pixels(nint app, out uint width, out uint height);

    [DllImport(Library, EntryPoint = "ui_app_set_root", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetRoot(nint app, nint root);

    [DllImport(Library, EntryPoint = "ui_app_set_style", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetStyle(nint app, uint dark, uint compact);

    [DllImport(Library, EntryPoint = "ui_node_create", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateNode(nint app, NodeKind kind, out nint node);

    [DllImport(Library, EntryPoint = "ui_node_append", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Append(nint parent, nint child);

    [DllImport(Library, EntryPoint = "ui_node_set_layout", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetLayout(nint node, double spacing, double padding);

    [DllImport(Library, EntryPoint = "ui_node_set_height", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetHeight(nint node, int height);
    [DllImport(Library, EntryPoint = "ui_node_get_height", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetHeight(nint node, out int height);
    [DllImport(Library, EntryPoint = "ui_grid_set_tracks", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetGridTracks(nint node, [In] GridTrack[] rows, uint rowCount, [In] GridTrack[] columns, uint columnCount);
    [DllImport(Library, EntryPoint = "ui_node_set_orientation", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetOrientation(nint node, Orientation orientation);
    [DllImport(Library, EntryPoint = "ui_scroll_set_policies", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetScrollPolicies(nint node, ScrollPolicy horizontal, ScrollPolicy vertical);
    [DllImport(Library, EntryPoint = "ui_scroll_to", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ScrollTo(nint node, double left, double top);
    [DllImport(Library, EntryPoint = "ui_node_bring_into_view", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BringIntoView(nint node);
    [DllImport(Library, EntryPoint = "ui_grid_set_shared_size_group", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetSharedSizeGroup(nint node, Orientation axis, uint index, [MarshalAs(UnmanagedType.LPUTF8Str)] string group);
    [DllImport(Library, EntryPoint = "ui_node_focus", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FocusNode(nint node);
    [DllImport(Library, EntryPoint = "ui_node_get_logical_focus", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetLogicalFocus(nint scope, out nint target);
    [DllImport(Library, EntryPoint = "ui_node_get_parent", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetParent(nint node, TreeKind tree, out nint parent);
    [DllImport(Library, EntryPoint = "ui_node_get_child_count", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetChildCount(nint node, TreeKind tree, out uint count);
    [DllImport(Library, EntryPoint = "ui_node_get_child", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetChild(nint node, TreeKind tree, uint index, out nint child);
    [DllImport(Library, EntryPoint = "ui_node_set_canvas_position", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetCanvasPosition(nint node, double left, double top);
    [DllImport(Library, EntryPoint = "ui_node_set_z_index", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetZIndex(nint node, int value);
    [DllImport(Library, EntryPoint = "ui_node_set_grid_cell", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetGridCell(nint node, uint row, uint column, uint rows, uint columns);
    [DllImport(Library, EntryPoint = "ui_node_set_weight", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetWeight(nint node, uint weight);

    [DllImport(Library, EntryPoint = "ui_node_set_dock", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetDock(nint node, DockEdge edge);

    [DllImport(Library, EntryPoint = "ui_node_set_anchor", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetAnchor(nint node, nint anchor);

    [DllImport(Library, EntryPoint = "ui_node_set_items", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetItems(nint node, nint* labels, uint count);

    [DllImport(Library, EntryPoint = "ui_node_set_text", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetText(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);

    [DllImport(Library, EntryPoint = "ui_node_get_text", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetText(nint node, byte* bytes, ref uint size);

    [DllImport(Library, EntryPoint = "ui_node_set_render_transform", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetRenderTransform(nint node, Matrix matrix, Point origin);
    [DllImport(Library, EntryPoint = "ui_node_to_window", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ToWindow(nint node, Point local, out Point window);
    [DllImport(Library, EntryPoint = "ui_node_from_window", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FromWindow(nint node, Point window, out Point local);
    [DllImport(Library, EntryPoint = "ui_node_set_stretch", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetStretch(nint node, Stretch stretch);
    [DllImport(Library, EntryPoint = "ui_node_set_image_rgba", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetImageRgba(nint node, byte* rgba, uint width, uint height);
    [DllImport(Library, EntryPoint = "ui_app_register_image_rgba", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RegisterImageRgba(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string key,
        byte* rgba, uint width, uint height);
    [DllImport(Library, EntryPoint = "ui_node_set_image", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetImage(nint node, byte* rgb, uint width, uint height);

    [DllImport(Library, EntryPoint = "ui_node_set_value", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetValue(nint node, uint value);

    [DllImport(Library, EntryPoint = "ui_node_get_value", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetValue(nint node, out uint value);

    [DllImport(Library, EntryPoint = "ui_node_set_action_with_concurrency", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetAction(nint node, ActionCallback? callback, nint context, ActionConcurrency concurrency);

    [DllImport(Library, EntryPoint = "ui_node_defer_action", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DeferAction(nint node);

    [DllImport(Library, EntryPoint = "ui_node_complete_action", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CompleteAction(nint node);

    [DllImport(Library, EntryPoint = "ui_text_source_create", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateTextSource(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string text, out nint source);

    [DllImport(Library, EntryPoint = "ui_text_source_set", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetSourceText(nint source, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);

    [DllImport(Library, EntryPoint = "ui_text_source_destroy", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DestroyTextSource(nint source);

    [DllImport(Library, EntryPoint = "ui_node_bind_text", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindText(nint node, nint source, BindingMode mode);

    [DllImport(Library, EntryPoint = "ui_command_create", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateCommand(nint app, ActionCallback callback, nint context, out nint command);

    [DllImport(Library, EntryPoint = "ui_command_destroy", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DestroyCommand(nint command);

    [DllImport(Library, EntryPoint = "ui_node_bind_command", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindCommand(nint node, nint command);

    [DllImport(Library, EntryPoint = "ui_app_find_fleck_node", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FindNode(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string id, out nint node);

    [DllImport(Library, EntryPoint = "ui_app_load_fleck_with_context", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result LoadFleck(nint app, byte* bytes, nuint length, Context* context, out Diagnostic diagnostic);

    [DllImport(Library, EntryPoint = "ui_app_load_fleck_resource_with_context", ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result LoadResource(nint app, byte* bytes, nuint length, Context* context, out Diagnostic diagnostic);
}
