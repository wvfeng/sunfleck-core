using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Sunfleck;

internal static unsafe partial class Native
{
    [DllImport(Library, EntryPoint = "ui_model_set_property_source", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result PropertySource(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string uri, uint line, uint column);
    [DllImport(Library, EntryPoint = "ui_model_set_command_modes", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CommandModes(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, CommandExecutionModes modes);
    [DllImport(Library, EntryPoint = "ui_fleck_validate", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ValidateFleck(byte* text, nuint size, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint = "ui_fleck_get_schema", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FleckSchema(byte* text, ref nuint size);
    [DllImport(Library, EntryPoint = "ui_model_describe", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DescribeModel(nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, byte* text, ref nuint size);
    [DllImport(Library, EntryPoint = "ui_app_get_debug_snapshot", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DebugSnapshot(nint app, byte* text, ref nuint size);
    [DllImport(Library, EntryPoint = "ui_app_set_debug_picking", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetDebugPicking(nint app, uint enabled);
    [DllImport(Library, EntryPoint = "ui_app_refresh_fleck", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RefreshFleck(nint app, byte* text, nuint size, nint model, out Diagnostic diagnostic);
    [StructLayout(LayoutKind.Sequential)]
    internal struct FleckRefreshStatus
    {
        internal uint Size, Version;
        internal ulong Sequence;
        internal Result Result;
        internal uint Pending;
        internal long ElapsedMicroseconds;
        internal Diagnostic Diagnostic;
    }
    [DllImport(Library, EntryPoint = "ui_app_queue_fleck_refresh", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result QueueFleckRefresh(nint app, byte* text, nuint size, nint model);
    [DllImport(Library, EntryPoint = "ui_app_get_fleck_refresh_status", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetFleckRefreshStatus(nint app, ref FleckRefreshStatus status);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate Result FleckRefreshStateCallback(nint app, FleckRefreshPhase phase, Result result, nint context);
    [DllImport(Library, EntryPoint = "ui_app_set_fleck_refresh_state_callback", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FleckRefreshStateHandler(nint app, FleckRefreshStateCallback? callback, nint context);
    [DllImport(Library, EntryPoint = "ui_app_get_binding_diagnostics", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BindingDiagnostics(nint app, byte* text, ref nuint size);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void DiagnosticCallback(nint json, nint context);
    [DllImport(Library, EntryPoint = "ui_app_set_diagnostic_callback", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DiagnosticHandler(nint app, DiagnosticCallback? callback, nint context);
    [DllImport(Library, EntryPoint = "ui_app_set_diagnostic_log_callback", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DiagnosticLogHandler(nint app, DiagnosticCallback? callback, nint context);
    [DllImport(Library, EntryPoint = "ui_app_dispatch_diagnostics", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DispatchDiagnostics(nint app);
    [DllImport(Library, EntryPoint = "ui_node_get_debug_id", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DebugId(nint node, out ulong id);
    [DllImport(Library, EntryPoint = "ui_app_find_debug_node", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FindDebugNode(nint app, ulong id, out nint node);
    [DllImport(Library, EntryPoint = "ui_app_set_debug_highlight", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DebugHighlight(nint app, ulong id, uint leaseMilliseconds);
    [DllImport(Library, EntryPoint = "ui_app_get_fleck_root", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetFleckRoot(nint app, out nint root);
    [DllImport(Library, EntryPoint = "ui_app_discard_fleck_page", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result DiscardFleckPage(nint app, nint root);
    [DllImport(Library, EntryPoint = "ui_app_set_fleck_source_uri", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FleckSourceUri(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string uri);
    [DllImport(Library, EntryPoint = "ui_app_attach_fleck_source_map", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result FleckSourceMap(nint app, byte* text, nuint size, out Diagnostic diagnostic);

    internal unsafe delegate Result JsonRead(byte* text, ref nuint size);
    internal static string ReadJson(JsonRead read)
    {
        nuint size = 0;
        SunfleckException.Check(read(null, ref size));
        var bytes = new byte[checked((int)size)];
        fixed (byte* text = bytes)
        {
            SunfleckException.Check(read(text, ref size));
        }
        return Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
    }
}

public sealed partial class App
{
    private ulong fleckRefreshVersion;
    private bool trackQueuedRefresh { get => CurrentFleckContext.TrackQueuedRefresh; set => CurrentFleckContext.TrackQueuedRefresh = value; }
    private ulong observedRefreshSequence { get => CurrentFleckContext.ObservedRefreshSequence; set => CurrentFleckContext.ObservedRefreshSequence = value; }
    internal ulong FleckRefreshVersion
    {
        get
        {
            SynchronizeQueuedFleckRefresh();
            return fleckRefreshVersion;
        }
    }
    private unsafe Native.FleckRefreshStatus ReadFleckRefreshStatus()
    {
        var status = new Native.FleckRefreshStatus { Size = (uint)sizeof(Native.FleckRefreshStatus), Version = 1 };
        SunfleckException.Check(Native.GetFleckRefreshStatus(Handle, ref status));
        return status;
    }
    private void SynchronizeQueuedFleckRefresh()
    {
        if (!trackQueuedRefresh)
        {
            return;
        }
        var status = ReadFleckRefreshStatus();
        if (status.Sequence != observedRefreshSequence)
        {
            observedRefreshSequence = status.Sequence;
            InvalidateRetiredFleckObjects(Handle);
        }
        trackQueuedRefresh = status.Pending != 0;
    }
    public unsafe void QueueFleckRefresh(string? xml, PropertyModel? model = null)
    {
        var app = Handle;
        if (model is not null)
        {
            CheckOwner(model.App);
        }
        SynchronizeQueuedFleckRefresh();
        var previous = ReadFleckRefreshStatus();
        var bytes = xml is null ? null : Encoding.UTF8.GetBytes(xml);
        fixed (byte* text = bytes)
        {
            // An empty string is a syntax request; null explicitly cancels it.
            byte empty = 0;
            SunfleckException.Check(Native.QueueFleckRefresh(app, xml is not null && bytes!.Length == 0 ? &empty : text,
                (nuint)(bytes?.Length ?? 0), model?.Handle ?? 0));
        }
        observedRefreshSequence = previous.Sequence;
        trackQueuedRefresh = xml is not null;
    }
    public unsafe FleckRefreshStatus GetFleckRefreshStatus()
    {
        SynchronizeQueuedFleckRefresh();
        var status = ReadFleckRefreshStatus();
        FleckValidation? diagnostic = null;
        if (status.Diagnostic.Code != FleckError.None)
        {
            var message = new ReadOnlySpan<byte>(status.Diagnostic.Message, 128);
            int end = message.IndexOf((byte)0);
            diagnostic = new FleckValidation(status.Diagnostic.Code, status.Diagnostic.Line, status.Diagnostic.Column,
                Encoding.UTF8.GetString(end < 0 ? message : message[..end]));
        }
        return new FleckRefreshStatus(status.Sequence, status.Result, status.Pending != 0, status.ElapsedMicroseconds, diagnostic);
    }
    public static unsafe FleckValidation? ValidateFleck(string xml)
    {
        var bytes = Encoding.UTF8.GetBytes(xml);
        fixed (byte* text = bytes)
        {
            var status = Native.ValidateFleck(text, (nuint)bytes.Length, out var diagnostic);
            if (status == Result.Ok)
            {
                return null;
            }
            if (diagnostic.Code == FleckError.None)
            {
                SunfleckException.Check(status);
            }
            var message = new ReadOnlySpan<byte>(diagnostic.Message, 128);
            int end = message.IndexOf((byte)0);
            return new FleckValidation(diagnostic.Code, diagnostic.Line, diagnostic.Column,
                Encoding.UTF8.GetString(end < 0 ? message : message[..end]));
        }
    }
    private Native.DiagnosticCallback? diagnosticCallback;
    private Action<string>? diagnosticsChanged;
    public event Action<string> BindingDiagnosticsChanged
    {
        add
        {
            var app = Handle;
            diagnosticCallback ??= (json, _) =>
            {
                ++callbackDepth;
                try
                {
                    diagnosticsChanged?.Invoke(Marshal.PtrToStringUTF8(json)!);
                }
                catch (Exception error)
                {
                    CaptureBindingError(error);
                }
                finally
                {
                    --callbackDepth;
                }
            };
            SunfleckException.Check(Native.DiagnosticHandler(app, diagnosticCallback, 0));
            diagnosticsChanged += value;
        }
        remove
        {
            _ = Handle;
            diagnosticsChanged -= value;
            if (diagnosticsChanged is null)
            {
                SunfleckException.Check(Native.DiagnosticHandler(Handle, null, 0));
            }
        }
    }
    public static unsafe string GetFleckSchema() => Native.ReadJson(Native.FleckSchema);
    private Native.DiagnosticCallback? diagnosticLogCallback;
    private Action<string>? diagnosticLog;
    /// <summary>Changed binding diagnostics as versioned JSON batches, for a structured logging sink.</summary>
    public event Action<string> BindingDiagnosticLog
    {
        add
        {
            var app = Handle;
            diagnosticLogCallback ??= (json, _) =>
            {
                ++callbackDepth;
                try
                {
                    diagnosticLog?.Invoke(Marshal.PtrToStringUTF8(json)!);
                }
                catch (Exception error)
                {
                    CaptureBindingError(error);
                }
                finally
                {
                    --callbackDepth;
                }
            };
            if (diagnosticLog is null)
            {
                SunfleckException.Check(Native.DiagnosticLogHandler(app, diagnosticLogCallback, 0));
            }
            diagnosticLog += value;
        }
        remove
        {
            var app = Handle;
            diagnosticLog -= value;
            if (diagnosticLog is null)
            {
                SunfleckException.Check(Native.DiagnosticLogHandler(app, null, 0));
            }
        }
    }
    public unsafe string GetBindingDiagnostics()
    {
        var app = Handle;
        return Native.ReadJson((byte* text, ref nuint size) => Native.BindingDiagnostics(app, text, ref size));
    }
    public void DispatchBindingDiagnostics()
    {
        SunfleckException.Check(Native.DispatchDiagnostics(Handle));
        if (callbackDepth == 0)
        {
            ThrowCallbackError();
        }
    }
    private Native.FleckRefreshStateCallback? fleckStateCallback { get => CurrentFleckContext.StateCallback; set => CurrentFleckContext.StateCallback = value; }
    /// <summary>Capture application-owned state before staging and restore/release it after each attempted commit.</summary>
    public void SetFleckRefreshStateCallback(Func<FleckRefreshPhase, Result, Result>? callback)
    {
        var app = Handle;
        Native.FleckRefreshStateCallback? handler = callback is null ? null : (nativeApp, phase, result, _) =>
        {
            ++callbackDepth;
            try
            {
                if (phase == FleckRefreshPhase.Complete && result == Result.Ok)
                {
                    InvalidateRetiredFleckObjects(nativeApp);
                }
                return callback(phase, result);
            }
            catch (Exception error)
            {
                CaptureBindingError(error);
                return Result.InvalidState;
            }
            finally
            {
                --callbackDepth;
            }
        };
        SunfleckException.Check(Native.FleckRefreshStateHandler(app, handler, 0));
        fleckStateCallback = handler;
    }
    public void SetDebugPicking(bool enabled) => SunfleckException.Check(Native.SetDebugPicking(Handle, enabled ? 1u : 0u));
    public unsafe void RefreshFleck(string xml, PropertyModel? model = null)
    {
        var app = Handle;
        if (model is not null)
        {
            CheckOwner(model.App);
        }
        var bytes = Encoding.UTF8.GetBytes(xml);
        fixed (byte* text = bytes)
        {
            CheckDockDiagnostic(Native.RefreshFleck(app, text, (nuint)bytes.Length, model?.Handle ?? 0, out var diagnostic), diagnostic);
            InvalidateRetiredFleckObjects(app);
        }
    }

    public Node GetFleckRoot()
    {
        SunfleckException.Check(Native.GetFleckRoot(Handle, out var root));
        return new Node(this, root);
    }

    public void DiscardFleckPage(Node root)
    {
        ArgumentNullException.ThrowIfNull(root);
        CheckOwner(root.App);
        var app = Handle;
        SunfleckException.Check(Native.DiscardFleckPage(app, root.Handle));
        InvalidateRetiredFleckObjects(app);
    }

    internal void InvalidateRetiredFleckObjects(nint app)
    {
        fleckRefreshVersion++;
        foreach (var entry in componentInstances.ToArray()) {
            if (!entry.Value.IsAlive(app)) { entry.Value.Detach(); componentInstances.Remove(entry.Key); }
        }
        foreach (var entry in dockWorkspaces.ToArray())
        {
            if (entry.Value.RetireIfHostRemoved(app))
            {
                dockWorkspaces.Remove(entry.Key);
            }
        }
        foreach (var entry in dropHandlers.ToArray()) {
            if (Native.FindDebugNode(app,entry.Value.Id,out var current) != Result.Ok || current != entry.Key)
            {
                dropHandlers.Remove(entry.Key);
            }
        }
        foreach (var entry in pointerHandlers.ToArray()) {
            if (Native.FindDebugNode(app,entry.Value.Id,out var current) != Result.Ok || current != entry.Key.Node)
                { pointerHandlers.Remove(entry.Key); }
        }
        foreach (var entry in activationHandlers.ToArray()) {
            if (Native.FindDebugNode(app,entry.Value.Id,out var current) != Result.Ok || current != entry.Key.Node)
                { activationHandlers.Remove(entry.Key); }
        }
        foreach (var entry in actions.ToArray())
        {
            if (Native.FindDebugNode(app, entry.Value.Id, out var current) != Result.Ok || current != entry.Key)
            {
                actions.Remove(entry.Key);
            }
        }
    }
    public unsafe string GetDebugSnapshot()
    {
        var app = Handle;
        return Native.ReadJson((byte* text, ref nuint size) => Native.DebugSnapshot(app, text, ref size));
    }
    public void SetFleckSourceUri(string uri) => SunfleckException.Check(Native.FleckSourceUri(Handle, Utf8(uri)));
    /// <summary>Temporarily highlights an object's visible bounds. Zero clears the overlay.</summary>
    public void SetDebugHighlight(ulong objectId, uint leaseMilliseconds = 1000) =>
        SunfleckException.Check(Native.DebugHighlight(Handle, objectId, leaseMilliseconds));
    public unsafe void AttachFleckSourceMap(string json)
    {
        var app = Handle;
        var bytes = Encoding.UTF8.GetBytes(json);
        fixed (byte* text = bytes)
        {
            var status = Native.FleckSourceMap(app, text, (nuint)bytes.Length, out var diagnostic);
            CheckDockDiagnostic(status, diagnostic);
        }
    }
    public Node FindDebugNode(ulong id)
    {
        SunfleckException.Check(Native.FindDebugNode(Handle, id, out var node));
        return new Node(this, node);
    }
}

public sealed record FleckValidation(FleckError Code, uint Line, uint Column, string Message);
public sealed record FleckRefreshStatus(ulong Sequence, Result Result, bool Pending, long ElapsedMicroseconds, FleckValidation? Diagnostic);
public enum FleckRefreshPhase { Capture, Complete }

public sealed partial class PropertyModel
{
    public PropertyModel SetCommandModes(string path, CommandExecutionModes modes)
    {
        SunfleckException.Check(Native.CommandModes(Handle, Sunfleck.App.Utf8(path), modes));
        return this;
    }
    public unsafe string Describe(string name)
    {
        var model = Handle;
        name = Sunfleck.App.Utf8(name);
        return Native.ReadJson((byte* text, ref nuint size) => Native.DescribeModel(model, name, text, ref size));
    }

    /// <summary>Describe supported public properties without constructing a model or calling getters.</summary>
    public static string DescribeType<T>(string? name = null) => DescribeType(typeof(T), name);
    public static string DescribeType(Type type, string? name = null)
    {
        ArgumentNullException.ThrowIfNull(type);
        var properties = type.GetProperties(BindingFlags.Public | BindingFlags.Instance)
            .Where(property => property.GetIndexParameters().Length == 0 && property.GetMethod?.IsPublic == true && Supported(property.PropertyType))
            .OrderBy(property => property.Name, StringComparer.Ordinal)
            .Select(property =>
            {
                var entry = new JsonObject
                {
                    ["path"] = property.Name,
                    ["type"] = property.PropertyType == typeof(string) ? "string" : property.PropertyType == typeof(bool) ? "bool" :
                        property.PropertyType == typeof(Command) ? "command" : property.PropertyType == typeof(PropertyModel) || typeof(System.ComponentModel.INotifyPropertyChanged).IsAssignableFrom(property.PropertyType) ? "model" : property.PropertyType == typeof(float) || property.PropertyType == typeof(double) ? "number" : "integer",
                    ["writable"] = property.SetMethod?.IsPublic == true
                };
                var modes = property.GetCustomAttribute<CommandModesAttribute>()?.Modes ?? CommandExecutionModes.None;
                if (property.PropertyType == typeof(Command) && modes != CommandExecutionModes.None)
                {
                    var values = new JsonArray();
                    if ((modes & CommandExecutionModes.Sync) != 0)
                    {
                        values.Add("Sync");
                    }
                    if ((modes & CommandExecutionModes.Async) != 0)
                    {
                        values.Add("Async");
                    }
                    entry["commandModes"] = values;
                }
                return entry;
            });
        return JsonSerializer.Serialize(new { version = 1, name = name ?? type.FullName, properties });
    }
}

[Flags]
public enum CommandExecutionModes { None = 0, Sync = 1, Async = 2 }

/// <summary>Static command metadata for type export; no property getter is invoked.</summary>
[AttributeUsage(AttributeTargets.Property)]
public sealed class CommandModesAttribute : Attribute
{
    public CommandExecutionModes Modes { get; }
    public CommandModesAttribute(CommandExecutionModes modes)
    {
        if ((modes & ~(CommandExecutionModes.Sync | CommandExecutionModes.Async)) != 0)
        {
            throw new ArgumentOutOfRangeException(nameof(modes));
        }
        Modes = modes;
    }
}
