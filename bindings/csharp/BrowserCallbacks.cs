using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed partial class App
{
    private readonly Dictionary<nint, GCHandle> browserCommands = [];
    private readonly Dictionary<nint, GCHandle> browserActions = [];
    private readonly List<GCHandle> browserCallbacks = [];
    private readonly Dictionary<string, GCHandle> browserSkins = new(StringComparer.Ordinal);

    private unsafe void SetBrowserSkin(string name, Native.PaintCallback? callback)
    {
        browserSkins.EnsureCapacity(browserSkins.Count + 1);
        var state = callback is null ? default : GCHandle.Alloc(callback);
        try {
            SunfleckException.Check(Native.RegisterBrowserSkin(Handle, name,
                callback is null ? 0 : (nint)(delegate* unmanaged[Cdecl]<nint, Native.DockPaintData*, nint, uint>)&BrowserPaint,
                state.IsAllocated ? GCHandle.ToIntPtr(state) : 0));
        } catch { if (state.IsAllocated) { state.Free(); } throw; }
        if (browserSkins.Remove(name, out var previous)) { previous.Free(); }
        if (state.IsAllocated) { browserSkins.Add(name, state); }
    }
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe uint BrowserPaint(nint draw, Native.DockPaintData* paint, nint context)
        => ((Native.PaintCallback)GCHandle.FromIntPtr(context).Target!)(draw, in *paint, context);

    private unsafe void SetBrowserAction(nint node, Action<Node>? action, ActionConcurrency concurrency)
    {
        browserActions.EnsureCapacity(browserActions.Count + 1);
        var state = action is null ? default : GCHandle.Alloc(Wrap(action));
        try {
            SunfleckException.Check(Native.SetBrowserAction(node,
                action is null ? 0 : (nint)(delegate* unmanaged[Cdecl]<nint, nint, void>)&BrowserAction,
                state.IsAllocated ? GCHandle.ToIntPtr(state) : 0, concurrency));
        } catch { if (state.IsAllocated) { state.Free(); } throw; }
        if (browserActions.Remove(node, out var previous)) { previous.Free(); }
        if (state.IsAllocated) { browserActions.Add(node, state); }
    }

    internal unsafe void SetBrowserDockEvent(nint dock, Native.DockCallback callback)
    {
        var state = GCHandle.Alloc(callback);
        browserCallbacks.EnsureCapacity(browserCallbacks.Count + 1);
        try {
            SunfleckException.Check(Native.SetBrowserDockEvent(dock,
                (nint)(delegate* unmanaged[Cdecl]<nint, nint, DockEventKind, nint, uint>)&BrowserDockEvent, GCHandle.ToIntPtr(state)));
            browserCallbacks.Add(state);
        } catch { state.Free(); throw; }
    }
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint BrowserDockEvent(nint dock, nint item, DockEventKind kind, nint context)
        => ((Native.DockCallback)GCHandle.FromIntPtr(context).Target!)(dock, item, kind, context);

    // Wasm reverse P/Invoke uses statically discoverable exports. Keep closures
    // in a GCHandle and pass an explicit context instead of marshaling delegates.
    private unsafe Command CreateBrowserCommand(Action<Node> action)
    {
        browserCommands.EnsureCapacity(browserCommands.Count + 1);
        var state = GCHandle.Alloc(Wrap(action));
        try
        {
            SunfleckException.Check(Native.CreateBrowserCommand(Handle,
                (nint)(delegate* unmanaged[Cdecl]<nint, nint, void>)&BrowserAction,
                GCHandle.ToIntPtr(state), out var command));
            browserCommands.Add(command, state);
            return new Command(this, command);
        }
        catch { state.Free(); throw; }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserAction(nint node, nint context)
        => ((Native.ActionCallback)GCHandle.FromIntPtr(context).Target!)(node, context);

    private static unsafe Result PostWork(nint app, nint context)
        => OperatingSystem.IsBrowser()
            ? Native.PostBrowser(app,
                (nint)(delegate* unmanaged[Cdecl]<nint, void>)&BrowserPostedWork, context,
                (nint)(delegate* unmanaged[Cdecl]<nint, void>)&BrowserPostedRelease)
            : Native.Post(app, postedWork, context, postedRelease);

    private static unsafe Result PostInvocation(nint app, nint context)
        => OperatingSystem.IsBrowser()
            ? Native.PostBrowser(app,
                (nint)(delegate* unmanaged[Cdecl]<nint, void>)&BrowserInvokeWork, context,
                (nint)(delegate* unmanaged[Cdecl]<nint, void>)&BrowserInvokeRelease)
            : Native.Post(app, invokeWork, context, invokeRelease);

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserPostedWork(nint context) => postedWork(context);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserPostedRelease(nint context) => postedRelease(context);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserInvokeWork(nint context) => invokeWork(context);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserInvokeRelease(nint context) => invokeRelease(context);
}

internal static partial class Native
{
    [DllImport(Library, EntryPoint = "ui_app_register_dock_skin", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RegisterBrowserSkin(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, nint callback, nint context);
    [DllImport(Library, EntryPoint = "ui_node_set_action_with_concurrency", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetBrowserAction(nint node, nint callback, nint context, ActionConcurrency concurrency);
    [DllImport(Library, EntryPoint = "ui_dock_set_event", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetBrowserDockEvent(nint dock, nint callback, nint context);
    [DllImport(Library, EntryPoint = "ui_command_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateBrowserCommand(nint app, nint callback, nint context, out nint command);
    [DllImport(Library, EntryPoint = "ui_app_post", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result PostBrowser(nint app, nint callback, nint context, nint release);
}
