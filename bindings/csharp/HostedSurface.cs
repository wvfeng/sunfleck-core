using System.Runtime.InteropServices;

namespace Sunfleck;

/// <summary>A surface driven by an external event loop. Uses a Headless App.</summary>
public sealed class HostedSurface
{
    private readonly App app;
    public HostedSurface(App app) => this.app = app ?? throw new ArgumentNullException(nameof(app));
    public void SelectView(uint id) => SunfleckException.Check(Native.HostSelectView(app.Handle, id));
    public void SemanticAction(ulong id, bool activate) => SunfleckException.Check(Native.HostSemanticAction(app.Handle, id, activate ? 1u : 0u));
    public unsafe string Snapshot()
    {
        nuint size = 0;
        SunfleckException.Check(Native.HostSnapshot(app.Handle, null, ref size));
        var bytes = new byte[checked((int)size)];
        fixed (byte* data = bytes) { SunfleckException.Check(Native.HostSnapshot(app.Handle, data, ref size)); }
        return System.Text.Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
    }
    public void Resize(uint width, uint height, uint dpi = 96)
        => SunfleckException.Check(Native.HostResize(app.Handle, width, height, dpi));
    public void Pointer(PointerAction action, int x, int y)
        => SunfleckException.Check(Native.HostPointer(app.Handle, action, x, y));
    /// <summary>Inspects a view at a device-pixel position while debug picking is active.</summary>
    public Result DebugPointer(uint view, PointerAction action, int x, int y)
        => Native.HostDebugPointer(app.Handle, view, action, x, y);
    public void Key(HostedKey key, bool shift = false)
        => SunfleckException.Check(Native.HostKey(app.Handle, key, shift ? 1u : 0u));
    public void Focus(bool focused)
        => SunfleckException.Check(Native.HostFocus(app.Handle, focused ? 1u : 0u));
    public void Scroll(int x, int y, int dy)
        => SunfleckException.Check(Native.HostScroll(app.Handle, x, y, dy));
    public static HostedCapabilities Capabilities => (HostedCapabilities)Native.HostCapabilities();
    public void ContextMenu(int x, int y) => SunfleckException.Check(Native.HostContextMenu(app.Handle, x, y));
    public HostedTextState? ReadText()
    {
        var state = new Native.HostTextState { Size = (uint)Marshal.SizeOf<Native.HostTextState>(), Version = 1 };
        SunfleckException.Check(Native.HostTextRead(app.Handle, ref state));
        return state.Active == 0 ? null : new HostedTextState(state.Session, state.Revision, state.Multiline, state.Password,
            state.Left, state.Top, state.Right, state.Bottom, state.ClipLeft, state.ClipTop, state.ClipRight, state.ClipBottom,
            state.SelectionStart, state.SelectionEnd, state.SelectionBackward, state.Foreground, state.Background, state.FontSize,
            state.TextLeft, state.TextTop, state.TextRight, state.TextBottom, state.LineHeight,
            state.CaretX, state.CaretY, state.ScrollX, state.ScrollY,
            Marshal.PtrToStringUTF8(state.Text, checked((int)state.TextSize))!);
    }
    /// <summary>Commits a DOM edit. InvalidState means its session/revision became stale; reread before retrying.</summary>
    public Result UpdateText(uint session, uint revision, string text, uint first, uint last, bool backward = false)
    {
        ArgumentNullException.ThrowIfNull(text);
        var bytes = System.Text.Encoding.UTF8.GetBytes(text);
        return Native.HostTextUpdate(app.Handle, session, revision, bytes, checked((uint)bytes.Length), first, last, backward ? 1u : 0u);
    }
    /// <summary>Displays an IME draft without publishing it to the model.</summary>
    public Result PreviewText(uint session, uint revision, string text, uint first, uint last, bool backward, uint compositionFirst, uint compositionLast)
    {
        ArgumentNullException.ThrowIfNull(text);
        var bytes = System.Text.Encoding.UTF8.GetBytes(text);
        var range = new Native.HostCompositionRange {
            Size = (uint)Marshal.SizeOf<Native.HostCompositionRange>(), Version = 1,
            First = first, Last = last, Backward = backward ? 1u : 0u,
            CompositionFirst = compositionFirst, CompositionLast = compositionLast,
        };
        return Native.HostTextPreview(app.Handle, session, revision, bytes, checked((uint)bytes.Length), ref range);
    }
    public Result HitText(uint session, uint revision, int x, int y, out uint offset)
        => Native.HostTextHit(app.Handle, session, revision, x, y, out offset);
    public Result ScrollText(uint session, uint revision, int dx, int dy)
        => Native.HostTextScroll(app.Handle, session, revision, dx, dy);
    /// <summary>Select CPU pixels (false) or ordered rectangle commands (true) for all hosted views.</summary>
    public void SetCommandRendering(bool enabled)
        => SunfleckException.Check(Native.HostSetRenderMode(app.Handle, enabled ? 1u : 0u));
    /// <summary>Copies the selected view's v1 command bytes after Poll. See ui_host_draw_commands.</summary>
    public unsafe byte[] CopyDrawCommands()
    {
        SunfleckException.Check(Native.HostDrawCommands(app.Handle, out var pointer, out var size));
        return new ReadOnlySpan<byte>((void*)pointer, checked((int)size)).ToArray();
    }
    public unsafe byte[] CopyPixels()
    {
        var pointer = Native.HostPixels(app.Handle, out var width, out var height);
        return pointer == 0 ? [] : new ReadOnlySpan<byte>((void*)pointer, checked((int)(width * height * 4))).ToArray();
    }
}

public enum HostedKey { Tab, Activate, Previous, Next, Escape, Left, Right, Up, Down }
[Flags]
public enum HostedCapabilities { Text = 1, Scroll = 2, DockViews = 4, Semantics = 8, ExternalCommands = 16, DrawCommands = 32 }

public sealed record HostedTextState(uint Session, uint Revision, uint Multiline, uint Password,
    int Left, int Top, int Right, int Bottom, int ClipLeft, int ClipTop, int ClipRight, int ClipBottom,
    uint SelectionStart, uint SelectionEnd, uint SelectionBackward, uint Foreground, uint Background, uint FontSize,
    int TextLeft, int TextTop, int TextRight, int TextBottom, uint LineHeight,
    int CaretX, int CaretY, int ScrollX, int ScrollY, string Text);

internal static partial class Native
{
    [DllImport(Library, EntryPoint = "ui_host_capabilities", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint HostCapabilities();
    [DllImport(Library, EntryPoint = "ui_host_context_menu", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostContextMenu(nint app, int x, int y);
    [DllImport(Library, EntryPoint = "ui_host_select_view", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostSelectView(nint app, uint id);
    [DllImport(Library, EntryPoint = "ui_host_set_render_mode", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostSetRenderMode(nint app, uint mode);
    [DllImport(Library, EntryPoint = "ui_host_draw_commands", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostDrawCommands(nint app, out nint data, out uint size);
    [DllImport(Library, EntryPoint = "ui_host_pixels", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint HostPixels(nint app, out uint width, out uint height);
    [DllImport(Library, EntryPoint = "ui_host_snapshot", CallingConvention = CallingConvention.Cdecl)]
    internal static extern unsafe Result HostSnapshot(nint app, byte* data, ref nuint size);
    [DllImport(Library, EntryPoint = "ui_host_semantic_action", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostSemanticAction(nint app, ulong id, uint action);
    [DllImport(Library, EntryPoint = "ui_host_scroll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostScroll(nint app, int x, int y, int dy);
    [StructLayout(LayoutKind.Sequential)]
    internal struct HostCompositionRange
    {
        internal uint Size, Version, First, Last, Backward, CompositionFirst, CompositionLast;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct HostTextState
    {
        internal uint Size, Version, Active, Session, Revision, Multiline, Password;
        internal int Left, Top, Right, Bottom, ClipLeft, ClipTop, ClipRight, ClipBottom;
        internal uint SelectionStart, SelectionEnd, SelectionBackward, Foreground, Background, FontSize;
        internal int TextLeft, TextTop, TextRight, TextBottom;
        internal uint LineHeight;
        internal int CaretX, CaretY, ScrollX, ScrollY;
        internal nint Text;
        internal uint TextSize;
    }
    [DllImport(Library, EntryPoint = "ui_host_text_read", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostTextRead(nint app, ref HostTextState state);
    [DllImport(Library, EntryPoint = "ui_host_text_update", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostTextUpdate(nint app, uint session, uint revision, byte[] text, uint size, uint first, uint last, uint backward);
    [DllImport(Library, EntryPoint = "ui_host_text_preview", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostTextPreview(nint app, uint session, uint revision, byte[] text, uint size, ref HostCompositionRange range);
    [DllImport(Library, EntryPoint = "ui_host_text_hit_test", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostTextHit(nint app, uint session, uint revision, int x, int y, out uint offset);
    [DllImport(Library, EntryPoint = "ui_host_text_scroll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostTextScroll(nint app, uint session, uint revision, int dx, int dy);
    [DllImport(Library, EntryPoint = "ui_host_resize", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostResize(nint app, uint width, uint height, uint dpi);
    [DllImport(Library, EntryPoint = "ui_host_pointer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostPointer(nint app, PointerAction action, int x, int y);
    [DllImport(Library, EntryPoint = "ui_host_debug_pointer", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostDebugPointer(nint app, uint view, PointerAction action, int x, int y);
    [DllImport(Library, EntryPoint = "ui_host_key_event", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostKey(nint app, HostedKey key, uint shift);
    [DllImport(Library, EntryPoint = "ui_host_focus", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result HostFocus(nint app, uint focused);
}
