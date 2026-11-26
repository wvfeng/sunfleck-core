using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace Sunfleck;

internal static partial class Native
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result WindowCallback(nint app, nint context);
    [DllImport(Library, EntryPoint = "ui_app_create_window", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateWindow(nint app, out nint window);
    [DllImport(Library, EntryPoint = "ui_app_get_current_window", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CurrentWindow(nint app, out nint window);
    [DllImport(Library, EntryPoint = "ui_window_invoke", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result InvokeWindow(nint window, WindowCallback callback, nint context);
    [DllImport(Library, EntryPoint = "ui_window_request_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CloseWindow(nint window);
    [DllImport(Library, EntryPoint = "ui_window_get_id", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result WindowId(nint window, out uint id);
}

/// <summary>An App-owned independent page and Dock window session.</summary>
public sealed class Window
{
    private readonly App app;
    private readonly nint handle;
    internal Window(App app, nint handle)
    {
        this.app = app;
        this.handle = handle;
    }
    public uint Id
    {
        get
        {
            _ = app.Handle;
            SunfleckException.Check(Native.WindowId(handle, out var id));
            return id;
        }
    }
    public void WithContext(Action<App> action)
    {
        ArgumentNullException.ThrowIfNull(action);
        _ = app.Handle;
        ExceptionDispatchInfo? error = null;
        var result = Native.InvokeWindow(handle, (_, _) =>
        {
            app.EnterBindingCallback();
            try
            {
                action(app);
                return Result.Ok;
            }
            catch (Exception failure)
            {
                error = ExceptionDispatchInfo.Capture(failure);
                return Result.InvalidState;
            }
            finally
            {
                app.ExitBindingCallback();
            }
        }, 0);
        error?.Throw();
        app.CheckBindingResult(result);
    }
    public void Open() => WithContext(current => current.OpenWindow());
    public void RequestClose()
    {
        _ = app.Handle;
        SunfleckException.Check(Native.CloseWindow(handle));
    }
    internal void SynchronizeRefresh() => WithContext(current => current.SynchronizeWindowRefresh());
}

public sealed partial class App
{
    private sealed class FleckWindowContext
    {
        public bool TrackQueuedRefresh;
        public ulong ObservedRefreshSequence;
        public Native.FleckRefreshStateCallback? StateCallback;
    }
    private readonly Dictionary<nint, FleckWindowContext> fleckWindows = [];
    private readonly List<Window> windows = [];
    private FleckWindowContext CurrentFleckContext
    {
        get
        {
            SunfleckException.Check(Native.CurrentWindow(Handle, out var window));
            if (!fleckWindows.TryGetValue(window, out var context))
            {
                context = new FleckWindowContext();
                fleckWindows.Add(window, context);
            }
            return context;
        }
    }
    public Window CreateWindow()
    {
        SunfleckException.Check(Native.CreateWindow(Handle, out var handle));
        var window = new Window(this, handle);
        windows.Add(window);
        return window;
    }
    internal void SynchronizeWindowRefresh() => SynchronizeQueuedFleckRefresh();
}
