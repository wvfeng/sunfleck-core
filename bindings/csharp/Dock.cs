using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Sunfleck;

public enum DockKind { Document, Tool }
public enum DockPosition { Center, Left, Top, Right, Bottom }
public enum DockState { Docked, Floating, AutoHidden, Hidden, Closed }
public enum DockEventKind { Closing, Changed, Activated }
public enum PointerAction { Down, Move, Up, Cancel }
public enum DockPart { Title, Tab, Close, Pin, Float, Splitter, Indicator, Preview, AutoHide, Menu, MenuItem, WindowTitle, WindowMinimize, WindowMaximize, WindowClose }
[Flags]
public enum DockCapabilities { None = 0, CanClose = 1, CanFloat = 2, CanAutoHide = 4, Default = 7 }
[Flags]
public enum DockPartState { Normal = 0, Active = 1, Hover = 2, Pressed = 4, Disabled = 8 }
[StructLayout(LayoutKind.Sequential)]
public readonly record struct DockRectangle(int X, int Y, int Width, int Height);
public sealed record DockPaint(DockPart Part, DockPartState State, DockCapabilities Capabilities, uint Dpi,
    DockRectangle Bounds, string Title, string ItemId, uint Background, uint Foreground, uint Border, string Icon);

public sealed class DockEventArgs(DockItem? item, DockEventKind kind) : EventArgs
{
    public DockItem? Item { get; } = item;
    public DockEventKind Kind { get; } = kind;
    public bool Cancel { get; set; }
}

/// <summary>Borrowed workspace. Its lifetime and UI thread are those of its App.</summary>
public sealed class DockWorkspace
{
    internal App App { get; }
    private readonly nint handle;
    private readonly ulong hostId;
    private readonly nint hostHandle;
    private bool retired;
    private readonly Native.DockCallback callback;
    internal nint Handle
    {
        get
        {
            _ = App.Handle;
            if (retired)
            {
                throw new ObjectDisposedException(nameof(DockWorkspace));
            }
            return handle;
        }
    }
    internal DockWorkspace(App app, nint value, nint host)
    {
        App = app;
        handle = value;
        hostHandle = host;
        SunfleckException.Check(Native.DebugId(host, out hostId));
        callback = (_, item, kind, _) => App.DispatchDockEvent(this, item, kind);
        if (OperatingSystem.IsBrowser()) { App.SetBrowserDockEvent(Handle, callback); }
        else { SunfleckException.Check(Native.DockSetEvent(Handle, callback, 0)); }
    }
    public event EventHandler<DockEventArgs>? Event;
    internal bool RetireIfHostRemoved(nint app)
    {
        if (Native.FindDebugNode(app, hostId, out var current) == Result.Ok && current == hostHandle)
        {
            return false;
        }
        retired = true;
        Event = null;
        return true;
    }
    internal bool Notify(nint item, DockEventKind kind)
    {
        var args = new DockEventArgs(item == 0 ? null : new DockItem(this, item), kind);
        Event?.Invoke(this, args);
        return args.Cancel;
    }
    public DockItem Register(string id, string title, Node content, DockKind kind = DockKind.Document,
        DockCapabilities capabilities = DockCapabilities.Default, uint minWidth = 96, uint minHeight = 64)
    {
        ArgumentNullException.ThrowIfNull(content);
        App.CheckOwner(content.App);
        var options = new Native.DockOptions
        {
            Size = (uint)Marshal.SizeOf<Native.DockOptions>(), Version = 1,
            Id = App.Utf8(id), Title = App.Utf8(title), Content = content.Handle,
            Kind = kind, Capabilities = capabilities, MinWidth = minWidth, MinHeight = minHeight
        };
        App.CheckDockResult(Native.DockRegister(Handle, in options, out var item));
        return new DockItem(this, item);
    }
    public DockItem Find(string id)
    {
        SunfleckException.Check(Native.DockFind(Handle, App.Utf8(id), out var item));
        return new DockItem(this, item);
    }
    public void CaptureDefault() => App.CheckDockResult(Native.DockCaptureDefault(Handle));
    public void Reset() => App.CheckDockResult(Native.DockReset(Handle));
    public unsafe string Save()
    {
        uint size = 0;
        SunfleckException.Check(Native.DockSave(Handle, null, ref size));
        var bytes = new byte[checked((int)size)];
        fixed (byte* data = bytes)
        {
            SunfleckException.Check(Native.DockSave(Handle, data, ref size));
        }
        return Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
    }
    public unsafe void Restore(string layout)
    {
        var bytes = Encoding.UTF8.GetBytes(App.Utf8(layout));
        fixed (byte* data = bytes)
        {
            var status = Native.DockRestore(Handle, data, (nuint)bytes.Length, out var diagnostic);
            App.CheckDockDiagnostic(status, diagnostic);
        }
    }
    public unsafe uint[] Windows()
    {
        uint count = 0;
        SunfleckException.Check(Native.DockGetWindows(Handle, null, ref count));
        var ids = new uint[checked((int)count)];
        fixed (uint* data = ids)
        {
            SunfleckException.Check(Native.DockGetWindows(Handle, data, ref count));
        }
        return ids;
    }
    public void Pointer(uint window, PointerAction action, int x, int y)
        => App.CheckDockResult(Native.DockPointer(Handle, window, action, x, y));
    public PixelFrame ReadPixels(uint window = 0)
    {
        var pixels = Native.DockPixels(Handle, window, out var width, out var height);
        if (pixels == 0)
        {
            throw new SunfleckException(Result.InvalidState, "No rendered headless Dock surface is available.");
        }
        var bytes = new byte[checked((int)(width * (ulong)height * 4))];
        Marshal.Copy(pixels, bytes, 0, bytes.Length);
        return new PixelFrame(width, height, bytes);
    }
}

public sealed class DockItem
{
    internal DockWorkspace Workspace { get; }
    private readonly nint handle;
    private readonly string id;
    private readonly ulong debugId;
    private ulong refreshVersion;
    internal nint Handle
    {
        get
        {
            var dock = Workspace.Handle;
            if (refreshVersion != Workspace.App.FleckRefreshVersion)
            {
                if (Native.DockFind(dock, App.Utf8(id), out var current) != Result.Ok || current != handle ||
                    Native.DockItemDebugId(current, out var currentId) != Result.Ok || currentId != debugId)
                {
                    throw new SunfleckException(Result.InvalidState, "The Fleck Dock item was removed.");
                }
                refreshVersion = Workspace.App.FleckRefreshVersion;
            }
            return handle;
        }
    }
    internal DockItem(DockWorkspace workspace, nint value)
    {
        Workspace = workspace;
        handle = value;
        refreshVersion = workspace.App.FleckRefreshVersion;
        id = Id;
        SunfleckException.Check(Native.DockItemDebugId(handle, out debugId));
    }
    public unsafe string Id
    {
        get
        {
            uint size = 0;
            SunfleckException.Check(Native.DockGetId(Handle, null, ref size));
            var bytes = new byte[checked((int)size)];
            fixed (byte* data = bytes)
            {
                SunfleckException.Check(Native.DockGetId(Handle, data, ref size));
            }
            return Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
        }
    }
    public DockState State
    {
        get
        {
            SunfleckException.Check(Native.DockGetState(Handle, out var state));
            return state;
        }
    }
    public void SetTitle(string title) => Workspace.App.CheckDockResult(Native.DockSetTitle(Handle, App.Utf8(title)));
    public void SetIcon(string icon) => Workspace.App.CheckDockResult(Native.DockSetIcon(Handle, App.Utf8(icon)));
    public void Move(DockItem? target = null, DockPosition position = DockPosition.Center, uint tabIndex = uint.MaxValue)
    {
        if (target is not null && !ReferenceEquals(Workspace, target.Workspace))
        {
            throw new SunfleckException(Result.InvalidArgument, "Dock items must belong to the same workspace.");
        }
        Workspace.App.CheckDockResult(Native.DockMove(Handle, target?.Handle ?? 0, position, tabIndex));
    }
    public void Activate() => Workspace.App.CheckDockResult(Native.DockActivate(Handle));
    public void MoveGroup(DockItem? target, DockPosition position = DockPosition.Center)
    {
        if (target is not null && !ReferenceEquals(Workspace, target.Workspace))
        {
            throw new SunfleckException(Result.InvalidArgument, "Dock items must belong to the same workspace.");
        }
        Workspace.App.CheckDockResult(Native.DockMoveGroup(Handle, target?.Handle ?? 0, position));
    }
    public void Hide() => Workspace.App.CheckDockResult(Native.DockHide(Handle));
    public void Show() => Workspace.App.CheckDockResult(Native.DockShow(Handle));
    public bool Close()
    {
        var result = Native.DockClose(Handle);
        Workspace.App.CheckDockResult(result == Result.InvalidState ? Result.Ok : result);
        return result == Result.Ok;
    }
    public void Float(DockRectangle bounds, bool wholeGroup = false)
        => Workspace.App.CheckDockResult(Native.DockFloat(Handle, in bounds, wholeGroup ? 1u : 0u));
    public void AutoHide(DockEdge edge = DockEdge.Left) => Workspace.App.CheckDockResult(Native.DockAutoHide(Handle, edge));
    public void Pin() => Workspace.App.CheckDockResult(Native.DockPin(Handle));
}

/// <summary>Drawing coordinates and bounds are window pixels. Valid only during the paint callback.</summary>
public sealed class DrawContext
{
    private nint handle;
    private readonly int thread = Environment.CurrentManagedThreadId;
    internal DrawContext(nint value) => handle = value;
    private nint Handle
    {
        get
        {
            if (Environment.CurrentManagedThreadId != thread)
            {
                throw new SunfleckException(Result.WrongThread);
            }
            ObjectDisposedException.ThrowIf(handle == 0, this);
            return handle;
        }
    }
    internal void Invalidate() => handle = 0;
    public void Fill(DockRectangle bounds, uint rgb, uint opacity = 255, uint radius = 0)
        => SunfleckException.Check(Native.DrawFill(Handle, bounds, rgb, opacity, radius));
    public void Stroke(DockRectangle bounds, uint rgb, uint width = 1, uint radius = 0)
        => SunfleckException.Check(Native.DrawStroke(Handle, bounds, rgb, width, radius));
    public void Text(DockRectangle bounds, string text, uint rgb)
        => SunfleckException.Check(Native.DrawText(Handle, bounds, App.Utf8(text), rgb));
    public unsafe void Image(DockRectangle bounds, ReadOnlySpan<byte> rgba, uint width, uint height)
    {
        if ((ulong)width * height * 4 != (ulong)rgba.Length)
        {
            throw new SunfleckException(Result.InvalidArgument);
        }
        fixed (byte* data = rgba)
        {
            SunfleckException.Check(Native.DrawImage(Handle, bounds, data, width, height));
        }
    }
    public void PushClip(DockRectangle bounds) => SunfleckException.Check(Native.DrawPushClip(Handle, bounds));
    public void PopClip() => SunfleckException.Check(Native.DrawPopClip(Handle));
}

public sealed partial class App
{
    private readonly Dictionary<nint, DockWorkspace> dockWorkspaces = [];
    private readonly Dictionary<string, Native.PaintCallback> dockSkins = new(StringComparer.Ordinal);

    public DockWorkspace CreateDock(Node host)
    {
        ArgumentNullException.ThrowIfNull(host);
        CheckOwner(host.App);
        SunfleckException.Check(Native.DockCreate(host.Handle, out var dock));
        if (!dockWorkspaces.TryGetValue(dock, out var workspace))
        {
            workspace = new DockWorkspace(this, dock, host.Handle);
            dockWorkspaces.Add(dock, workspace);
        }
        return workspace;
    }
    internal uint DispatchDockEvent(DockWorkspace workspace, nint item, DockEventKind kind)
    {
        ++callbackDepth;
        try
        {
            return workspace.Notify(item, kind) ? 1u : 0u;
        }
        catch (Exception error)
        {
            callbackError ??= ExceptionDispatchInfo.Capture(error);
            return 1;
        }
        finally
        {
            --callbackDepth;
        }
    }
    internal void CheckDockResult(Result result)
    {
        if (callbackDepth == 0)
        {
            ThrowCallbackError();
        }
        SunfleckException.Check(result);
    }
    internal unsafe void CheckDockDiagnostic(Result result, Native.Diagnostic diagnostic)
    {
        if (result != Result.Ok && diagnostic.Code != FleckError.None)
        {
            var message = new ReadOnlySpan<byte>(diagnostic.Message, 128);
            var end = message.IndexOf((byte)0);
            throw new FleckException(result, diagnostic.Code, diagnostic.Line, diagnostic.Column,
                Encoding.UTF8.GetString(end < 0 ? message : message[..end]));
        }
        CheckDockResult(result);
    }
    public unsafe void LoadTheme(string resource)
    {
        var bytes = Encoding.UTF8.GetBytes(Utf8(resource));
        LoadTheme(bytes);
    }
    public unsafe void LoadTheme(ReadOnlySpan<byte> resource)
    {
        fixed (byte* data = resource)
        {
            var status = Native.LoadTheme(Handle, data, (nuint)resource.Length, out var diagnostic);
            CheckDockDiagnostic(status, diagnostic);
        }
    }
    public void ApplyTheme(BuiltinTheme baseTheme, string resources = "", bool dark = false, bool compact = false, string? skin = null)
    {
        resources = Utf8(resources);
        var theme = new Native.ThemeDescriptor
        {
            Size = (uint)Marshal.SizeOf<Native.ThemeDescriptor>(), Version = 1, Base = baseTheme,
            Dark = dark ? 1u : 0u, Compact = compact ? 1u : 0u, Skin = skin is null ? null : Utf8(skin),
            Resources = resources, ResourcesSize = (nuint)Encoding.UTF8.GetByteCount(resources)
        };
        var result = Native.ApplyTheme(Handle, in theme, out var diagnostic);
        CheckDockDiagnostic(result, diagnostic);
    }
    public void RegisterDockSkin(string name, Func<DrawContext, DockPaint, bool>? painter)
    {
        name = Utf8(name);
        if (painter is null)
        {
            if (OperatingSystem.IsBrowser()) { SetBrowserSkin(name, null); }
            else { SunfleckException.Check(Native.RegisterDockSkin(Handle, name, null, 0)); }
            dockSkins.Remove(name);
            return;
        }
        Native.PaintCallback callback = (nint context, in Native.DockPaintData part, nint _) =>
        {
            var draw = new DrawContext(context);
            ++callbackDepth;
            try
            {
                var data = new DockPaint(part.Part, part.State, part.Capabilities, part.Dpi, part.Bounds,
                    Marshal.PtrToStringUTF8(part.Title) ?? "", Marshal.PtrToStringUTF8(part.Id) ?? "",
                    part.Background, part.Foreground, part.Border, Marshal.PtrToStringUTF8(part.Icon) ?? "");
                return painter(draw, data) ? 1u : 0u;
            }
            catch (Exception error)
            {
                callbackError ??= ExceptionDispatchInfo.Capture(error);
                return 0;
            }
            finally
            {
                draw.Invalidate();
                --callbackDepth;
            }
        };
        if (OperatingSystem.IsBrowser()) { SetBrowserSkin(name, callback); }
        else { SunfleckException.Check(Native.RegisterDockSkin(Handle, name, callback, 0)); }
        dockSkins[name] = callback;
    }
}
