using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Sunfleck;

public sealed partial class App : IDisposable
{
    private readonly int ownerThread = Environment.CurrentManagedThreadId;
    private readonly Dictionary<nint, (ulong Id, Native.ActionCallback Callback)> actions = [];
    private readonly Dictionary<nint, Native.ActionCallback> commands = [];
    private readonly Dictionary<nint, object> asyncCommands = [];
    private nint handle;
    private GCHandle lifetime;
    private int callbackDepth;
    private ExceptionDispatchInfo? callbackError;
    private readonly List<PropertyModel> propertyModels = [];
    private readonly object postingLock = new();
    private readonly CancellationTokenSource shutdown = new();
    private bool disposing;
    internal bool IsUiThread => Environment.CurrentManagedThreadId == ownerThread;
    private static readonly Native.PostCallback postedWork = context =>
    {
        var (app, action) = ((App, Action))GCHandle.FromIntPtr(context).Target!;
        ++app.callbackDepth;
        try
        {
            action();
        }
        catch (Exception error)
        {
            app.CaptureBindingError(error);
        }
        finally
        {
            --app.callbackDepth;
        }
    };
    private static readonly Native.PostCallback postedRelease = context => GCHandle.FromIntPtr(context).Free();

    public void Post(Action action)
    {
        ArgumentNullException.ThrowIfNull(action);
        lock (postingLock)
        {
            var app = handle;
            ObjectDisposedException.ThrowIf(app == 0 || disposing, this);
            var cancellation = commandCancellation.Value;
            cancellation.ThrowIfCancellationRequested();
            Action guarded = () => { if (!cancellation.IsCancellationRequested && !shutdown.IsCancellationRequested) { action(); } };
            var task = GCHandle.Alloc((this, guarded));
            var result = PostWork(app, GCHandle.ToIntPtr(task));
            if (result != Result.Ok)
            {
                task.Free();
                if (result == Result.Cancelled) { throw new OperationCanceledException("The application is closing."); }
                SunfleckException.Check(result);
            }
        }
    }

    public unsafe void RegisterImageRgba(string key, ReadOnlySpan<byte> rgba, uint width, uint height)
    {
        ArgumentException.ThrowIfNullOrEmpty(key);
        if (width is 0 or > 4096 || height is 0 or > 4096 || rgba.Length != (int)(width * height * 4))
        {
            throw new ArgumentException("The RGBA buffer must contain four bytes per image pixel.", nameof(rgba));
        }
        fixed (byte* pixels = rgba)
        {
            SunfleckException.Check(Native.RegisterImageRgba(Handle, key, pixels, width, height));
        }
    }

    public PropertyModel CreatePropertyModel() => CreatePropertyModelCore(null, null);

    public PropertyModel CreatePropertyModel<[System.Diagnostics.CodeAnalysis.DynamicallyAccessedMembers(System.Diagnostics.CodeAnalysis.DynamicallyAccessedMemberTypes.PublicProperties)] T>(T model) where T : class
        => CreatePropertyModelCore(model, typeof(T));

    [System.Diagnostics.CodeAnalysis.RequiresUnreferencedCode("Runtime model types need preserved public properties. Prefer the generic overload or Add<T> registrations when trimming.")]
    public PropertyModel CreatePropertyModel(object? model) => CreatePropertyModelCore(model, model?.GetType());

    private PropertyModel CreatePropertyModelCore(object? model,
        [System.Diagnostics.CodeAnalysis.DynamicallyAccessedMembers(System.Diagnostics.CodeAnalysis.DynamicallyAccessedMemberTypes.PublicProperties)] Type? modelType)
    {
        _ = Handle;
        propertyModels.EnsureCapacity(propertyModels.Count + 1);
        var adapter = new PropertyModel(this, model, modelType);
        propertyModels.Add(adapter);
        return adapter;
    }
    internal void RemovePropertyModel(PropertyModel model) => propertyModels.Remove(model);
    internal void EnterBindingCallback() => ++callbackDepth;
    internal void ExitBindingCallback() => --callbackDepth;
    internal void CaptureBindingError(Exception error)
        => callbackError ??= ExceptionDispatchInfo.Capture(error is System.Reflection.TargetInvocationException { InnerException: not null } wrapped ? wrapped.InnerException! : error);
    internal void CheckBindingResult(Result result)
    {
        if (callbackDepth == 0)
        {
            ThrowCallbackError();
        }
        SunfleckException.Check(result);
    }

    public App(Renderer renderer = Renderer.Auto, bool headless = false)
        : this(renderer, headless, showRenderer: false)
    {
    }

    public App(Renderer renderer, bool headless, bool showRenderer)
    {
        var options = new Native.AppOptions
        {
            Size = (uint)Marshal.SizeOf<Native.AppOptions>(),
            Version = 5,
            Renderer = (uint)renderer,
            Flags = (headless ? 1u : 0u) | (showRenderer ? 2u : 0u)
        };
        lifetime = GCHandle.Alloc(this);
        try
        {
            try
            {
                if (Native.ApiVersion() != options.Version) { throw new SunfleckException(Result.VersionMismatch); }
            }
            catch (EntryPointNotFoundException)
            {
                throw new SunfleckException(Result.VersionMismatch);
            }
            SunfleckException.Check(Native.Create(in options, out handle));
            EnableDebugServer(Environment.GetEnvironmentVariable("SUNFLECK_DEBUG_PIPE"));
        }
        catch
        {
            if (handle != 0)
            {
                Native.Destroy(handle);
                handle = 0;
            }
            lifetime.Free();
            throw;
        }
    }

    internal bool IsDisposed => handle == 0;

    internal nint Handle
    {
        get
        {
            CheckThread();
            ObjectDisposedException.ThrowIf(IsDisposed, this);
            return handle;
        }
    }

    private void CheckThread()
    {
        if (Environment.CurrentManagedThreadId != ownerThread)
        {
            throw new SunfleckException(Result.WrongThread);
        }
    }

    internal void CheckOwner(App other)
    {
        _ = Handle;
        if (!ReferenceEquals(this, other))
        {
            throw new SunfleckException(Result.InvalidArgument, "Objects must belong to the same App.");
        }
    }

    public void SetWindowOptions(string title, uint width, uint height)
        => SunfleckException.Check(Native.SetWindowOptions(Handle, Utf8(title), width, height));

    public void OpenWindow() => SunfleckException.Check(Native.OpenWindow(Handle));
    public void RequestClose() => SunfleckException.Check(Native.RequestClose(Handle));

    public void SetStyle(bool dark, bool compact = false)
        => SunfleckException.Check(Native.SetStyle(Handle, dark ? 1u : 0u, compact ? 1u : 0u));

    public void SetTheme(BuiltinTheme theme, bool dark = false, bool compact = false)
    {
        SunfleckException.Check(Native.SetBuiltinTheme(Handle, theme, dark ? 1u : 0u, compact ? 1u : 0u));
    }

    public void SetTheme(bool dark, bool compact, string overrides)
    {
        var app = Handle;
        SunfleckException.Check(Native.InitializeTheme(out var theme, dark ? 1u : 0u, compact ? 1u : 0u));
        SunfleckException.Check(Native.ApplyThemeOverrides(ref theme, Utf8(overrides)));
        SunfleckException.Check(Native.SetTheme(app, in theme));
    }

    public Node CreateNode(NodeKind kind)
    {
        SunfleckException.Check(Native.CreateNode(Handle, kind, out var node));
        return new Node(this, node);
    }

    public void SetRoot(Node root)
    {
        ArgumentNullException.ThrowIfNull(root);
        CheckOwner(root.App);
        SunfleckException.Check(Native.SetRoot(Handle, root.Handle));
    }

    public Node FindFleckNode(string id)
    {
        SunfleckException.Check(Native.FindNode(Handle, Utf8(id), out var node));
        return new Node(this, node);
    }

    public TextSource CreateTextSource(string initialValue)
    {
        SunfleckException.Check(Native.CreateTextSource(Handle, Utf8(initialValue), out var source));
        return new TextSource(this, source);
    }

    public Command CreateCommand(Action<Node> action)
    {
        ArgumentNullException.ThrowIfNull(action);
        var app = Handle;
        if (OperatingSystem.IsBrowser()) { return CreateBrowserCommand(action); }
        commands.EnsureCapacity(commands.Count + 1);
        var callback = Wrap(action);
        SunfleckException.Check(Native.CreateCommand(app, callback, 0, out var command));
        commands.Add(command, callback);
        return new Command(this, command);
    }

    internal void RemoveCommand(nint command)
    {
        _ = Handle;
        SunfleckException.Check(Native.DestroyCommand(command));
        commands.Remove(command);
        if (browserCommands.Remove(command, out var browserState)) { browserState.Free(); }
        if (asyncCommands.TryGetValue(command, out var state) && state is IDisposable disposable) { disposable.Dispose(); }
        asyncCommands.Remove(command);
    }
    internal Command RegisterAsyncCommand(in Native.AsyncCommandOptions options, object state)
    {
        var app = Handle;
        asyncCommands.EnsureCapacity(asyncCommands.Count + 1);
        SunfleckException.Check(Native.CreateAsyncCommand(app, in options, out var command));
        asyncCommands.Add(command, state);
        return new Command(this, command, asynchronous: true);
    }

    private readonly Dictionary<(nint Node, EventPhase Phase),(ulong Id, Native.ActivationHandler Callback)> activationHandlers = [];

    private readonly Dictionary<(nint Node, PointerAction Action, EventPhase Phase),(ulong Id,Native.PointerHandler Callback)> pointerHandlers = [];

    internal void SetPointerHandler(nint node,PointerAction action,EventPhase phase,Func<Node,Node,int,int,bool>? handler) {
        _ = Handle;
        Native.PointerHandler? callback = handler is null ? null : (sender,source,_,_,x,y,_) => {
            ++callbackDepth;
            try { return handler(new Node(this,sender),new Node(this,source),x,y) ? 1 : 0; }
            catch (Exception error) { CaptureBindingError(error); return 1; }
            finally { --callbackDepth; }
        };
        SunfleckException.Check(Native.DebugId(node,out var id));
        pointerHandlers.EnsureCapacity(pointerHandlers.Count+1);
        SunfleckException.Check(Native.SetPointerHandler(node,action,phase,callback,0));
        if (callback is null) { pointerHandlers.Remove((node,action,phase)); }
        else { pointerHandlers[(node,action,phase)] = (id,callback); }
    }

    internal void SetActivationHandler(nint node, EventPhase phase, Func<Node,Node,bool>? handler)
    {
        _ = Handle;
        Native.ActivationHandler? callback = handler is null ? null : (sender,source,_,_) => {
            ++callbackDepth;
            try { return handler(new Node(this,sender),new Node(this,source)) ? 1 : 0; }
            catch (Exception error) { CaptureBindingError(error); return 1; }
            finally { --callbackDepth; }
        };
        SunfleckException.Check(Native.DebugId(node,out var id));
        activationHandlers.EnsureCapacity(activationHandlers.Count+1);
        SunfleckException.Check(Native.SetActivationHandler(node,phase,callback,0));
        if (callback is null) { activationHandlers.Remove((node,phase)); }
        else { activationHandlers[(node,phase)] = (id,callback); }
    }

    internal void SetAction(nint node, Action<Node>? action, ActionConcurrency concurrency)
    {
        if (OperatingSystem.IsBrowser()) { SetBrowserAction(node, action, concurrency); return; }
        _ = Handle;
        var callback = action is null ? null : Wrap(action);
        ulong id = 0;
        if (callback is not null)
        {
            actions.EnsureCapacity(actions.Count + 1);
            SunfleckException.Check(Native.DebugId(node, out id));
        }
        SunfleckException.Check(Native.SetAction(node, callback, 0, concurrency));
        if (callback is null)
        {
            actions.Remove(node);
        }
        else
        {
            actions[node] = (id, callback);
        }
    }

    internal void BindCommand(nint node, nint command)
    {
        _ = Handle;
        SunfleckException.Check(Native.BindCommand(node, command));
        if (command != 0)
        {
            actions.Remove(node);
        }
    }

    private Native.ActionCallback Wrap(Action<Node> action)
    {
        return (node, _) =>
        {
            ++callbackDepth;
            try
            {
                action(new Node(this, node));
            }
            catch (Exception error)
            {
                callbackError ??= ExceptionDispatchInfo.Capture(error);
            }
            finally
            {
                --callbackDepth;
            }
        };
    }

    private void ThrowCallbackError()
    {
        var error = callbackError;
        callbackError = null;
        error?.Throw();
    }

    public Result Poll(uint timeoutMilliseconds = 0)
    {
        var result = Native.Poll(Handle, timeoutMilliseconds);
        if (result == Result.Exited) { CancelShutdown(); }
        SynchronizeQueuedFleckRefresh();
        foreach (var window in windows)
        {
            window.SynchronizeRefresh();
        }
        if (callbackDepth == 0)
        {
            ThrowCallbackError();
        }
        if (result is not (Result.Timeout or Result.Exited))
        {
            SunfleckException.Check(result);
        }
        return result;
    }

    public void Run()
    {
        if (OperatingSystem.IsBrowser()) { throw new PlatformNotSupportedException("Use HostedSurface and Poll(0) from the browser event loop."); }
        while (Poll(1000) != Result.Exited)
        {
        }
    }

    public void ActivateButton()
    {
        var result = Native.ActivateButton(Handle);
        if (callbackDepth == 0)
        {
            ThrowCallbackError();
        }
        SunfleckException.Check(result);
    }

    public PixelFrame ReadPixels()
    {
        var pixels = Native.Pixels(Handle, out var width, out var height);
        if (pixels == 0)
        {
            throw new SunfleckException(Result.InvalidState, "No rendered headless frame is available.");
        }
        var rgba = new byte[checked((int)(width * (ulong)height * 4))];
        Marshal.Copy(pixels, rgba, 0, rgba.Length);
        return new PixelFrame(width, height, rgba);
    }

    public void LoadFleck(string xml, FleckDataContext? context = null)
        => LoadPage(Encoding.UTF8.GetBytes(xml), context, binary: false);

    public void LoadFleckResource(ReadOnlySpan<byte> bytes, FleckDataContext? context = null)
        => LoadPage(bytes, context, binary: true);

    public void LoadFleck(string xml, PropertyModel model)
    {
        ArgumentNullException.ThrowIfNull(model);
        CheckOwner(model.App);
        LoadPage(Encoding.UTF8.GetBytes(xml), null, binary: false, model);
    }
    public void LoadFleckResource(ReadOnlySpan<byte> bytes, PropertyModel model)
    {
        ArgumentNullException.ThrowIfNull(model);
        CheckOwner(model.App);
        LoadPage(bytes, null, binary: true, model);
    }

    private unsafe void LoadPage(ReadOnlySpan<byte> bytes, FleckDataContext? context, bool binary, PropertyModel? model = null)
    {
        var app = Handle;
        if (bytes.IsEmpty)
        {
            throw new SunfleckException(Result.InvalidArgument);
        }

        using (var exports = context?.MarshalExports(this))
        {
            var text = exports?.Text ?? [];
            var commands = exports?.Commands ?? [];
            fixed (byte* data = bytes)
            {
                fixed (Native.Export* textPointer = text, commandPointer = commands)
                {
                    var nativeContext = new Native.Context
                    {
                        Text = textPointer,
                        TextCount = (nuint)text.Length,
                        Commands = commandPointer,
                        CommandCount = (nuint)commands.Length
                    };
                    Native.Context* pointer = context is null ? null : &nativeContext;
                    Native.Diagnostic diagnostic;
                    var result = model is not null
                        ? binary
                            ? Native.LoadResourceModel(app, data, (nuint)bytes.Length, model.Handle, pointer, out diagnostic)
                            : Native.LoadFleckModel(app, data, (nuint)bytes.Length, model.Handle, pointer, out diagnostic)
                        : binary
                            ? Native.LoadResource(app, data, (nuint)bytes.Length, pointer, out diagnostic)
                            : Native.LoadFleck(app, data, (nuint)bytes.Length, pointer, out diagnostic);
                    if (callbackDepth == 0)
                    {
                        ThrowCallbackError();
                    }
                    if (result != Result.Ok)
                    {
                        if (diagnostic.Code == FleckError.None)
                        {
                            SunfleckException.Check(result);
                        }
                        var message = new ReadOnlySpan<byte>(diagnostic.Message, 128);
                        var end = message.IndexOf((byte)0);
                        throw new FleckException(result, diagnostic.Code, diagnostic.Line, diagnostic.Column,
                            Encoding.UTF8.GetString(end < 0 ? message : message[..end]));
                    }
                    InvalidateRetiredFleckObjects(app);
                }
            }
        }
    }

    internal static string Utf8(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (value.Contains('\0'))
        {
            throw new ArgumentException("Text cannot contain a NUL character.", nameof(value));
        }
        return value;
    }

    private void CancelShutdown()
    {
        EnterBindingCallback();
        try { shutdown.Cancel(); }
        catch (Exception error) { CaptureBindingError(error); }
        finally { ExitBindingCallback(); }
    }

    public void Dispose()
    {
        CheckThread();
        if (handle == 0)
        {
            return;
        }
        if (callbackDepth != 0)
        {
            throw new SunfleckException(Result.InvalidState, "Request closure from inside a callback.");
        }
        lock (postingLock) { disposing = true; }
        debugServer?.Dispose();
        debugServer = null;
        CancelShutdown();
        var cancelResult = Native.CancelCommands(handle);
        if (cancelResult != Result.Ok) { CaptureBindingError(new SunfleckException(cancelResult)); }
        foreach (var model in propertyModels.ToArray())
        {
            model.Dispose();
        }
        // Never hold postingLock while joining workers: a worker may still be
        // entering Post/InvokeAsync and must be able to observe disposing.
        Native.Destroy(handle);
        lock (postingLock) { handle = 0; }
        actions.Clear();
        activationHandlers.Clear();
        dropHandlers.Clear();
        pointerHandlers.Clear();
        commands.Clear();
        componentCommands.Clear();
        foreach (var component in componentInstances.Values) { component.Detach(); }
        componentInstances.Clear();
        foreach (var registration in componentRegistrations) { registration.Dispose(); }
        componentRegistrations.Clear();
        componentLibraries.Clear();
        foreach (var state in browserCommands.Values) { state.Free(); }
        browserCommands.Clear();
        foreach (var state in browserActions.Values) { state.Free(); }
        browserActions.Clear();
        foreach (var state in browserCallbacks) { state.Free(); }
        browserCallbacks.Clear();
        foreach (var state in browserSkins.Values) { state.Free(); }
        browserSkins.Clear();
        foreach (var state in asyncCommands.Values) { if (state is IDisposable disposable) { disposable.Dispose(); } }
        asyncCommands.Clear();
        dockWorkspaces.Clear();
        dockSkins.Clear();
        fleckWindows.Clear();
        windows.Clear();
        if (lifetime.IsAllocated) { lifetime.Free(); }
        ThrowCallbackError();
    }
}
