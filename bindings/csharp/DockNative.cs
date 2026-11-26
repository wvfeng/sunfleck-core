using System.Runtime.InteropServices;

namespace Sunfleck;

internal static unsafe partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct ThemeDescriptor
    {
        internal uint Size, Version;
        internal BuiltinTheme Base;
        internal uint Dark, Compact;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string? Skin;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Resources;
        internal nuint ResourcesSize;
    }
    [DllImport(Library, EntryPoint = "ui_app_apply_theme", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ApplyTheme(nint app, in ThemeDescriptor theme, out Diagnostic diagnostic);
    [StructLayout(LayoutKind.Sequential)]
    internal struct DockOptions
    {
        internal uint Size, Version;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Id;
        [MarshalAs(UnmanagedType.LPUTF8Str)] internal string Title;
        internal nint Content;
        internal DockKind Kind;
        internal DockCapabilities Capabilities;
        internal uint MinWidth, MinHeight;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct DockPaintData
    {
        internal uint Size, Version;
        internal DockPart Part;
        internal DockPartState State;
        internal DockCapabilities Capabilities;
        internal uint Dpi;
        internal DockRectangle Bounds;
        internal nint Title, Id;
        internal uint Background, Foreground, Border;
        internal nint Icon;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate uint DockCallback(nint dock, nint item, DockEventKind kind, nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate uint PaintCallback(nint draw, in DockPaintData paint, nint context);

    [DllImport(Library, EntryPoint = "ui_dock_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockCreate(nint host, out nint dock);
    [DllImport(Library, EntryPoint = "ui_dock_register", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockRegister(nint dock, in DockOptions options, out nint item);
    [DllImport(Library, EntryPoint = "ui_dock_find", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockFind(nint dock, [MarshalAs(UnmanagedType.LPUTF8Str)] string id, out nint item);
    [DllImport(Library, EntryPoint = "ui_dock_item_get_debug_id", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockItemDebugId(nint item, out ulong id);
    [DllImport(Library, EntryPoint = "ui_dock_set_event", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockSetEvent(nint dock, DockCallback callback, nint context);
    [DllImport(Library, EntryPoint = "ui_dock_move", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockMove(nint item, nint target, DockPosition position, uint index);
    [DllImport(Library, EntryPoint = "ui_dock_move_group", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockMoveGroup(nint item, nint target, DockPosition position);
    [DllImport(Library, EntryPoint = "ui_dock_activate", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockActivate(nint item);
    [DllImport(Library, EntryPoint = "ui_dock_hide", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockHide(nint item);
    [DllImport(Library, EntryPoint = "ui_dock_show", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockShow(nint item);
    [DllImport(Library, EntryPoint = "ui_dock_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockClose(nint item);
    [DllImport(Library, EntryPoint = "ui_dock_float", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockFloat(nint item, in DockRectangle bounds, uint wholeGroup);
    [DllImport(Library, EntryPoint = "ui_dock_auto_hide", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockAutoHide(nint item, DockEdge edge);
    [DllImport(Library, EntryPoint = "ui_dock_pin", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockPin(nint item);
    [DllImport(Library, EntryPoint = "ui_dock_get_state", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockGetState(nint item, out DockState state);
    [DllImport(Library, EntryPoint = "ui_dock_get_id", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockGetId(nint item, byte* text, ref uint size);
    [DllImport(Library, EntryPoint = "ui_dock_set_title", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockSetTitle(nint item, [MarshalAs(UnmanagedType.LPUTF8Str)] string title);
    [DllImport(Library, EntryPoint = "ui_dock_set_icon", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockSetIcon(nint item, [MarshalAs(UnmanagedType.LPUTF8Str)] string icon);
    [DllImport(Library, EntryPoint = "ui_dock_capture_default", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockCaptureDefault(nint dock);
    [DllImport(Library, EntryPoint = "ui_dock_reset", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockReset(nint dock);
    [DllImport(Library, EntryPoint = "ui_dock_save", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockSave(nint dock, byte* text, ref uint size);
    [DllImport(Library, EntryPoint = "ui_dock_restore", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockRestore(nint dock, byte* text, nuint size, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint = "ui_dock_get_windows", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockGetWindows(nint dock, uint* ids, ref uint count);
    [DllImport(Library, EntryPoint = "ui_dock_pointer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DockPointer(nint dock, uint window, PointerAction action, int x, int y);
    [DllImport(Library, EntryPoint = "ui_dock_pixels", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint DockPixels(nint dock, uint window, out uint width, out uint height);
    [DllImport(Library, EntryPoint = "ui_app_load_theme", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result LoadTheme(nint app, byte* text, nuint size, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint = "ui_app_register_dock_skin", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RegisterDockSkin(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, PaintCallback? callback, nint context);
    [DllImport(Library, EntryPoint = "ui_draw_fill", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawFill(nint draw, DockRectangle bounds, uint rgb, uint opacity, uint radius);
    [DllImport(Library, EntryPoint = "ui_draw_stroke", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawStroke(nint draw, DockRectangle bounds, uint rgb, uint width, uint radius);
    [DllImport(Library, EntryPoint = "ui_draw_text", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawText(nint draw, DockRectangle bounds, [MarshalAs(UnmanagedType.LPUTF8Str)] string text, uint rgb);
    [DllImport(Library, EntryPoint = "ui_draw_image", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawImage(nint draw, DockRectangle bounds, byte* rgba, uint width, uint height);
    [DllImport(Library, EntryPoint = "ui_draw_push_clip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawPushClip(nint draw, DockRectangle bounds);
    [DllImport(Library, EntryPoint = "ui_draw_pop_clip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DrawPopClip(nint draw);
}
