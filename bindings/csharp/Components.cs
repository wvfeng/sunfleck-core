using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Sunfleck;

[AttributeUsage(AttributeTargets.Assembly)]
public sealed class FleckRegistrationAttribute(Type registryType) : Attribute { public Type RegistryType { get; } = registryType; }


[AttributeUsage(AttributeTargets.Assembly)]
public sealed class FleckManifestAttribute(string json) : Attribute { public string Json { get; } = json; }
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class FleckEventAttribute(string name) : Attribute { public string Name { get; } = name; }
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class FleckTemplatePartAttribute(string name, NodeKind kind) : Attribute {
    public string Name { get; } = name;
    public NodeKind Kind { get; } = kind;
    public bool Required { get; set; } = true;
}
[AttributeUsage(AttributeTargets.Class)]
public sealed class FleckComponentAttribute(string namespaceUri, string name) : Attribute
{
    public string NamespaceUri { get; } = namespaceUri;
    public string Name { get; } = name;
    public string Template { get; set; } = "";
    public string TemplateNamespace { get; set; } = "";
}
[AttributeUsage(AttributeTargets.Property)]
public sealed class FleckPropertyAttribute : Attribute
{
    public object? Default { get; set; }
    public bool ReadOnly { get; set; }
    public PropertyBindingMode Mode { get; set; } = PropertyBindingMode.OneWay;
    /// <summary>-1 infers flags from the type; otherwise these flags are authoritative.</summary>
    public PropertyEffects Effects { get; set; } = (PropertyEffects)(-1);
}
public sealed record TemplatePart(string Name, NodeKind Kind, bool Required = true);
public sealed record ComponentProperty(string Name, Type Type, object? Default = null, bool ReadOnly = false,
    PropertyBindingMode Mode = PropertyBindingMode.OneWay, PropertyEffects? Effects = null,
    Func<object?, bool>? Validate = null, Func<object?, object?>? Coerce = null);

/// <summary>An instance with its own Fleck name scope and bindable properties. Owned by its page.</summary>
public abstract partial class FrameworkElement
{
    public static readonly ComponentProperty DataContextProperty = new("DataContext", typeof(PropertyModel));
    private App? app;
    private nint handle;
    private Node? host;
    private ulong hostId;
    private readonly Dictionary<nint, Native.ComponentEvent> subscriptions = [];
    private readonly Dictionary<nint, GCHandle> browserSubscriptions = [];
    public App App => app ?? node?.App ?? throw new InvalidOperationException("Component has not been initialized.");
    internal nint Handle { get {
        _ = App.Handle; ObjectDisposedException.ThrowIf(handle == 0, this);
        if (host is not null) { SunfleckException.Check(Native.NodeComponent(host.Handle, out var current)); if (current != handle) { throw new ObjectDisposedException(GetType().Name); } }
        return handle;
    } }
    internal void Attach(App owner, nint native) { app = owner; handle = native; SunfleckException.Check(Native.ComponentNode(native, out var node)); host = new Node(owner, node); hostId = host.DebugId; }
    internal bool IsAlive(nint nativeApp) => handle != 0 && Native.FindDebugNode(nativeApp, hostId, out _) == Result.Ok;
    internal void Detach() {
        handle = 0;
        foreach (var subscription in subscriptions.Keys) { SunfleckException.Check(Native.ComponentSubscriptionDestroy(subscription)); }
        subscriptions.Clear();
        foreach (var state in browserSubscriptions.Values) { state.Free(); }
        browserSubscriptions.Clear();
    }
    public Node Node { get { if (this.node is not null) { return this.node; } SunfleckException.Check(Native.ComponentNode(Handle, out var node)); return new Node(App, node); } }
    public Node FindNode(string id) { SunfleckException.Check(Native.ComponentFind(Handle, App.Utf8(id), out var node)); return new Node(App, node); }
    public T Get<T>(string name)
    {
        SunfleckException.Check(Native.ComponentGet(Handle, App.Utf8(name), out var value));
        return (T)ComponentValues.Decode(value, typeof(T), App)!;
    }
    public void Set<T>(string name, T value)
    {
        using var encoded = new ComponentValues(value, typeof(T));
        App.CheckBindingResult(Native.ComponentSet(Handle, App.Utf8(name), in encoded.Value));
    }
    public void Bind(string property, PropertyModel model, string path, PropertyBindingMode mode = PropertyBindingMode.Default)
    {
        App.CheckOwner(model.App);
        App.CheckBindingResult(Native.ComponentBind(Handle, App.Utf8(property), model.Handle, App.Utf8(path), mode));
    }
    public void SetDataContext(PropertyModel? model)
    {
        if (model is not null) { App.CheckOwner(model.App); }
        App.CheckBindingResult(Native.ComponentContext(Handle, model?.Handle ?? 0));
    }
    public unsafe IDisposable Subscribe(string name, Action<FrameworkElement, object?> handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        Native.ComponentEvent callback = (_, payload, _) =>
        {
            try { handler(this, payload == 0 ? null : ComponentValues.Decode(Marshal.PtrToStructure<Native.PropertyValue>(payload), null, App)); }
            catch (Exception error) { App.CaptureBindingError(error); }
        };
        nint token;
        if (OperatingSystem.IsBrowser()) {
            var state = GCHandle.Alloc(callback);
            try {
                SunfleckException.Check(Native.ComponentSubscribe(Handle, App.Utf8(name),
                    (nint)(delegate* unmanaged[Cdecl]<nint, nint, nint, void>)&BrowserEvent, GCHandle.ToIntPtr(state), out token));
                browserSubscriptions.Add(token, state);
            } catch { state.Free(); throw; }
        } else {
            SunfleckException.Check(Native.ComponentSubscribe(Handle, App.Utf8(name), Marshal.GetFunctionPointerForDelegate(callback), 0, out token));
        }
        subscriptions.Add(token, callback);
        return new Subscription(this, token);
    }
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserEvent(nint component, nint value, nint context)
        => ((Native.ComponentEvent)GCHandle.FromIntPtr(context).Target!)(component, value, context);
    private sealed class Subscription(FrameworkElement owner, nint token) : IDisposable
    {
        private FrameworkElement? component = owner;
        public void Dispose()
        {
            if (component is not { } owner) { return; }
            if (owner.subscriptions.ContainsKey(token)) { SunfleckException.Check(Native.ComponentSubscriptionDestroy(token)); owner.subscriptions.Remove(token); }
            if (owner.browserSubscriptions.Remove(token, out var state)) { state.Free(); }
            component = null;
        }
    }
    protected void Raise<T>(string name, T value)
    {
        using var encoded = new ComponentValues(value, typeof(T));
        App.CheckBindingResult(Native.ComponentRaise(Handle, App.Utf8(name), in encoded.Value));
    }
    protected internal virtual void OnInitialize() { }
    protected internal virtual void OnMounted() { }
    protected internal virtual void OnUnmounted() { }
    protected internal virtual void OnDestroyed() { }
    protected internal virtual void OnPropertyChanged(string name) { }
}
public class Control : FrameworkElement
{
    public static readonly ComponentProperty TemplateProperty = new("Template", typeof(string));
    public Node FindTemplatePart(string name) { SunfleckException.Check(Native.ComponentPart(Handle, App.Utf8(name), out var node)); return new Node(App, node); }
    public void ApplyTemplate(string namespaceUri, string name)
    {
        var result = Native.ComponentTemplate(Handle, App.Utf8(namespaceUri), App.Utf8(name), out var diagnostic);
        if (result != Result.Ok) { throw new SunfleckException(result, diagnostic.ToString()); }
        App.InvalidateRetiredFleckObjects(App.Handle);
    }
    protected internal virtual void OnTemplateApplied() { }
}

public class UserControl : Control { }

internal sealed class ComponentValues : IDisposable
{
    internal Native.PropertyValue Value;
    private nint text;
    internal static Native.ValueKind? TypedKind(Type type) =>
        type == typeof(Color) ? Native.ValueKind.Color : type == typeof(Thickness) ? Native.ValueKind.Thickness :
        type == typeof(CornerRadius) ? Native.ValueKind.CornerRadius : type == typeof(Point) ? Native.ValueKind.Point :
        type == typeof(Size) ? Native.ValueKind.Size : type == typeof(Matrix) ? Native.ValueKind.Matrix :
        type == typeof(Length) ? Native.ValueKind.Length : type.IsEnum ? Native.ValueKind.Enum : null;
    private void Geometry(Native.ValueKind kind, params double[] coordinates) {
        Value.Kind = kind;
        Value.Command = text = Marshal.AllocCoTaskMem(coordinates.Length * sizeof(double));
        Marshal.Copy(coordinates, 0, text, coordinates.Length);
    }
    private static double[] Geometry(Native.PropertyValue value, int count) {
        var coordinates = new double[count];
        Marshal.Copy(value.Command, coordinates, 0, count);
        return coordinates;
    }
    internal ComponentValues(object? value, Type type)
    {
        if (value is null && type.IsValueType) { value = Activator.CreateInstance(type); }
        if (type == typeof(string)) { Value.Kind = Native.ValueKind.String; Value.Text = text = Marshal.StringToCoTaskMemUTF8((string?)value ?? ""); }
        else if (type == typeof(bool)) { Value.Kind = Native.ValueKind.Bool; Value.Integer = value is true ? 1 : 0; }
        else if (type == typeof(double) || type == typeof(float)) { Value.Kind = Native.ValueKind.Double; Value.Number = Convert.ToDouble(value); }
        else if (type == typeof(PropertyModel)) { Value.Kind = Native.ValueKind.Model; Value.Command = (value as PropertyModel)?.Handle ?? 0; }
        else if (type == typeof(Command)) { Value.Kind = Native.ValueKind.Command; Value.Command = (value as Command)?.Handle ?? 0; }
        else if (type == typeof(int) || type == typeof(long) || type == typeof(short) || type == typeof(byte) || type == typeof(uint)) { Value.Kind = Native.ValueKind.Int; Value.Integer = Convert.ToInt64(value); }
        else if (type == typeof(Color)) { Value.Kind = Native.ValueKind.Color; Value.Integer = ((Color)value!).Argb; }
        else if (type == typeof(Length)) { var length = (Length)value!; Value.Kind = Native.ValueKind.Length; Value.Number = length.Value; Value.Integer = (long)length.Unit; }
        else if (type.IsEnum) { Value.Kind = Native.ValueKind.Enum; Value.Integer = Convert.ToInt64(value); }
        else if (value is Thickness t) { Geometry(Native.ValueKind.Thickness, t.Left, t.Top, t.Right, t.Bottom); }
        else if (value is CornerRadius r) { Geometry(Native.ValueKind.CornerRadius, r.TopLeft, r.TopRight, r.BottomRight, r.BottomLeft); }
        else if (value is Point p) { Geometry(Native.ValueKind.Point, p.X, p.Y); }
        else if (value is Size z) { Geometry(Native.ValueKind.Size, z.Width, z.Height); }
        else if (value is Matrix m) { Geometry(Native.ValueKind.Matrix, m.M11, m.M12, m.M21, m.M22, m.OffsetX, m.OffsetY); }
        else { throw new ArgumentException("Unsupported component property type.", nameof(type)); }
    }
    internal static object? Decode(Native.PropertyValue value, Type? type, App? app)
    {
        var g = value.Kind switch {
            Native.ValueKind.Thickness or Native.ValueKind.CornerRadius => Geometry(value, 4),
            Native.ValueKind.Point or Native.ValueKind.Size => Geometry(value, 2),
            Native.ValueKind.Matrix => Geometry(value, 6), _ => Array.Empty<double>()
        };
        object? result = value.Kind switch {
            Native.ValueKind.String => Marshal.PtrToStringUTF8(value.Text) ?? "",
            Native.ValueKind.Bool => value.Integer != 0,
            Native.ValueKind.Double => value.Number,
            Native.ValueKind.Int => value.Integer,
            Native.ValueKind.Enum => type?.IsEnum == true ? Enum.ToObject(type, value.Integer) : value.Integer,
            Native.ValueKind.Color => new Color(checked((uint)value.Integer)),
            Native.ValueKind.Length => new Length(value.Number, (LengthUnit)value.Integer),
            Native.ValueKind.Thickness => new Thickness(g[0], g[1], g[2], g[3]),
            Native.ValueKind.CornerRadius => new CornerRadius(g[0], g[1], g[2], g[3]),
            Native.ValueKind.Point => new Point(g[0], g[1]),
            Native.ValueKind.Size => new Size(g[0], g[1]),
            Native.ValueKind.Matrix => new Matrix(g[0], g[1], g[2], g[3], g[4], g[5]),
            Native.ValueKind.Model => value.Command == 0 ? null : app!.ResolveComponentModel(value.Command),
            Native.ValueKind.Command => value.Command == 0 ? null : app!.ResolveComponentCommand(value.Command),
            _ => throw new NotSupportedException("Unsupported component value kind.")
        };
        return result is null || type is null || type.IsInstanceOfType(result) ? result : Convert.ChangeType(result, type);
    }
    public void Dispose() { Marshal.FreeCoTaskMem(text); text = 0; }
}

public sealed partial class App
{
    private readonly List<ComponentRegistration> componentRegistrations = [];
    internal readonly Dictionary<nint, Command> componentCommands = [];
    internal PropertyModel ResolveComponentModel(nint pointer) => propertyModels.FirstOrDefault(model => model.Handle == pointer)
        ?? throw new InvalidOperationException("The model reference has no live managed adapter.");
    internal Command ResolveComponentCommand(nint pointer) => componentCommands.GetValueOrDefault(pointer)
        ?? throw new InvalidOperationException("The command reference has no live managed wrapper.");
    private readonly HashSet<string> componentLibraries = new(StringComparer.Ordinal);
    public bool BeginComponentLibrary(string identity) { _ = Handle; return componentLibraries.Add(identity); }
    private readonly Dictionary<nint, FrameworkElement> componentInstances = [];
    public FrameworkElement FindComponent(string id)
    {
        var node = FindFleckNode(id);
        SunfleckException.Check(Native.NodeComponent(node.Handle, out var component));
        return ResolveComponentInstance(component);
    }
    public FrameworkElement CreateComponent(string namespaceUri, string name, PropertyModel? model = null)
    {
        if (model is not null) { CheckOwner(model.App); }
        var status = Native.CreateComponent(Handle, Utf8(namespaceUri), Utf8(name), model?.Handle ?? 0, out var component, out _);
        CheckBindingResult(status);
        return ResolveComponentInstance(component);
    }
    private FrameworkElement ResolveComponentInstance(nint component)
    {
        if (componentInstances.TryGetValue(component, out var instance)) {
            try { if (instance.Handle == component) { return instance; } }
            catch (SunfleckException) { }
            catch (ObjectDisposedException) { }
            instance.Detach(); componentInstances.Remove(component);
        }
        instance = new UserControl(); instance.Attach(this, component); componentInstances.Add(component, instance);
        return instance;
    }
    public unsafe FleckValidation? ValidateFleckWithComponents(string xml)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(xml);
        fixed (byte* pointer = bytes) {
            var status = Native.ValidateComponentPage(Handle, pointer, (nuint)bytes.Length, out var diagnostic);
            if (status == Result.Ok) { return null; }
            return new FleckValidation(diagnostic.Code, diagnostic.Line, diagnostic.Column, diagnostic.ToString() ?? "Invalid Fleck component");
        }
    }
    public unsafe void RegisterComponentResource(ReadOnlySpan<byte> resource) {
        fixed (byte* bytes = resource) {
            var status = Native.RegisterComponentResource(Handle, bytes, (nuint)resource.Length, out var diagnostic);
            if (status != Result.Ok) { throw new SunfleckException(status, diagnostic.ToString()); }
        }
    }
    public unsafe void RegisterComponentManifest(string json)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(json);
        fixed (byte* pointer = bytes) {
            var result = Native.RegisterComponentManifest(Handle, pointer, (nuint)bytes.Length, out var diagnostic);
            if (result != Result.Ok) { throw new SunfleckException(result, diagnostic.ToString()); }
        }
    }
    public unsafe void QueueFleckRefreshWithResources(string xml, IEnumerable<KeyValuePair<string, string?>> resources, PropertyModel? model = null)
    {
        if (model is not null) { CheckOwner(model.App); }
        SynchronizeQueuedFleckRefresh();
        var previous = ReadFleckRefreshStatus();
        var allocations = new List<nint>();
        try {
            var entries = resources.Select(resource => {
                var uri = Marshal.StringToCoTaskMemUTF8(resource.Key); allocations.Add(uri);
                var text = resource.Value is null ? 0 : Marshal.StringToCoTaskMemUTF8(resource.Value); allocations.Add(text);
                return new Native.ComponentResource { Uri = uri, Text = text, Length = resource.Value is null ? 0 : (nuint)System.Text.Encoding.UTF8.GetByteCount(resource.Value) };
            }).ToArray();
            var bytes = System.Text.Encoding.UTF8.GetBytes(xml);
            fixed (byte* pointer = bytes) { byte empty = 0; SunfleckException.Check(Native.QueueComponentPage(Handle, bytes.Length == 0 ? &empty : pointer, (nuint)bytes.Length, entries, (nuint)entries.Length, model?.Handle ?? 0)); }
            observedRefreshSequence = previous.Sequence; trackQueuedRefresh = true;
        } finally { foreach (var pointer in allocations) { Marshal.FreeCoTaskMem(pointer); } }
    }
    public void RegisterFleckResources(IEnumerable<KeyValuePair<string, string>> resources)
    {
        var allocations = new List<nint>();
        try {
            var entries = resources.Select(resource => {
                var uri = Marshal.StringToCoTaskMemUTF8(resource.Key); allocations.Add(uri);
                var xml = Marshal.StringToCoTaskMemUTF8(resource.Value); allocations.Add(xml);
                return new Native.ComponentResource { Uri = uri, Text = xml, Length = (nuint)System.Text.Encoding.UTF8.GetByteCount(resource.Value) };
            }).ToArray();
            var result = Native.RegisterResources(Handle, entries, (nuint)entries.Length, out var diagnostic);
            if (result != Result.Ok) { throw new SunfleckException(result, diagnostic.ToString()); }
        } finally { foreach (var pointer in allocations) { Marshal.FreeCoTaskMem(pointer); } }
    }
    public void RegisterComponent<T>(string namespaceUri, string name, IEnumerable<ComponentProperty>? properties = null,
        string templateNamespace = "", string template = "", IEnumerable<string>? events = null,
        IEnumerable<TemplatePart>? parts = null) where T : FrameworkElement, new()
    {
        var registration = new ComponentRegistration(this, () => new T());
        try { registration.Register(namespaceUri, name, properties ?? [], templateNamespace, template, events ?? [], parts ?? []); }
        catch { registration.Dispose(); throw; }
        componentRegistrations.Add(registration);
    }
    private sealed class ComponentRegistration : IDisposable
    {
        private readonly List<GCHandle> propertyCallbacks = [];
        private sealed class PropertyCallback(App app, ComponentProperty property) : IDisposable {
            internal readonly App App = app;
            internal readonly ComponentProperty Property = property;
            internal ComponentValues? Coerced;
            public void Dispose() { Coerced?.Dispose(); Coerced = null; }
        }
        public void Dispose() {
            foreach (var callback in propertyCallbacks) { ((PropertyCallback)callback.Target!).Dispose(); callback.Free(); }
            propertyCallbacks.Clear();
        }
        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
        private static unsafe Result ValidateProperty(nint component, Native.PropertyValue* value, nint context) {
            var state = (PropertyCallback)GCHandle.FromIntPtr(context).Target!;
            state.App.EnterBindingCallback();
            try { return state.Property.Validate!(ComponentValues.Decode(*value, state.Property.Type, state.App)) ? Result.Ok : Result.InvalidArgument; }
            catch (Exception error) { state.App.CaptureBindingError(error); return Result.InvalidState; }
            finally { state.App.ExitBindingCallback(); }
        }
        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
        private static unsafe Result CoerceProperty(nint component, Native.PropertyValue* value, Native.PropertyValue* output, nint context) {
            var state = (PropertyCallback)GCHandle.FromIntPtr(context).Target!;
            state.App.EnterBindingCallback();
            try {
                var result = state.Property.Coerce!(ComponentValues.Decode(*value, state.Property.Type, state.App));
                var encoded = new ComponentValues(result, state.Property.Type);
                state.Coerced?.Dispose(); state.Coerced = encoded; *output = encoded.Value;
                return Result.Ok;
            } catch (Exception error) { state.App.CaptureBindingError(error); return Result.InvalidState; }
            finally { state.App.ExitBindingCallback(); }
        }
        private readonly App app;
        private readonly Func<FrameworkElement> factory;
        private readonly Native.ComponentLifecycle lifecycle;
        private readonly Native.ComponentChanged changed;
        internal ComponentRegistration(App app, Func<FrameworkElement> factory)
        {
            this.app = app; this.factory = factory; lifecycle = Lifecycle;
            changed = (component, name, _) => {
                try { app.componentInstances[component].OnPropertyChanged(Marshal.PtrToStringUTF8(name)!); }
                catch (Exception error) { app.CaptureBindingError(error); }
            };
        }
        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
        private static Result BrowserLifecycle(nint component, uint phase, nint context)
            => ((ComponentRegistration)GCHandle.FromIntPtr(context).Target!).Lifecycle(component, phase, context);
        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
        private static void BrowserChanged(nint component, nint name, nint context)
            => ((ComponentRegistration)GCHandle.FromIntPtr(context).Target!).changed(component, name, context);
        private Result Lifecycle(nint component, uint phase, nint context)
        {
            app.EnterBindingCallback();
            try {
                if (phase == 0) { var instance = factory(); instance.Attach(app, component); app.componentInstances.Add(component, instance); instance.OnInitialize(); }
                else if (app.componentInstances.TryGetValue(component, out var instance)) {
                    switch (phase) {
                        case 1: instance.OnMounted(); break;
                        case 2: instance.OnUnmounted(); break;
                        case 3: try { instance.OnDestroyed(); } finally { instance.Detach(); app.componentInstances.Remove(component); } break;
                        case 4: if (instance is Control control) { control.OnTemplateApplied(); } break;
                    }
                }
                return Result.Ok;
            } catch (Exception error) { app.CaptureBindingError(error); return Result.InvalidState; }
            finally { app.ExitBindingCallback(); }
        }
        internal unsafe void Register(string ns, string name, IEnumerable<ComponentProperty> properties, string templateNamespace,
            string template, IEnumerable<string> events, IEnumerable<TemplatePart> parts)
        {
            var strings = new List<nint>(); var values = new List<ComponentValues>(); var pins = new List<GCHandle>();
            nint String(string value) { var pointer = Marshal.StringToCoTaskMemUTF8(value); strings.Add(pointer); return pointer; }
            nint Pin(Array array) { if (array.Length == 0) { return 0; } var pin = GCHandle.Alloc(array, GCHandleType.Pinned); pins.Add(pin); return pin.AddrOfPinnedObject(); }
            try {
                var props = properties.Select(p => {
                    var value = new ComponentValues(p.Default, p.Type); values.Add(value);
                    nint metadata = 0;
                    if (p.Effects is not null || p.Validate is not null || p.Coerce is not null) {
                        var flags = p.Effects ?? (PropertyEffects.Bindable | (p.ReadOnly ? PropertyEffects.None : PropertyEffects.TwoWay) |
                            (value.Value.Kind is Native.ValueKind.Int or Native.ValueKind.Double or Native.ValueKind.Color or Native.ValueKind.Thickness or Native.ValueKind.CornerRadius or Native.ValueKind.Point or Native.ValueKind.Size or Native.ValueKind.Matrix or Native.ValueKind.Length ? PropertyEffects.Animatable : PropertyEffects.None));
                        var descriptor = new Native.ComponentPropertyMetadata { Size = (uint)Marshal.SizeOf<Native.ComponentPropertyMetadata>(), Version = 1, Flags = flags };
                        if (p.Validate is not null || p.Coerce is not null) {
                            var state = GCHandle.Alloc(new PropertyCallback(app, p)); propertyCallbacks.Add(state);
                            descriptor.Context = GCHandle.ToIntPtr(state);
                            if (p.Validate is not null) { descriptor.Validate = (nint)(delegate* unmanaged[Cdecl]<nint, Native.PropertyValue*, nint, Result>)&ValidateProperty; }
                            if (p.Coerce is not null) { descriptor.Coerce = (nint)(delegate* unmanaged[Cdecl]<nint, Native.PropertyValue*, Native.PropertyValue*, nint, Result>)&CoerceProperty; }
                        }
                        metadata = Pin(new[] { descriptor });
                    }
                    return new Native.ComponentProperty { Name = String(p.Name), Value = value.Value, ReadOnly = p.ReadOnly ? 1u : 0u, Mode = p.Mode, Metadata = metadata };
                }).ToArray();
                var eventNames = events.Select(String).ToArray();
                var templateParts = parts.Select(p => new Native.TemplatePart { Name = String(p.Name), Kind = p.Kind, Required = p.Required ? 1u : 0u }).ToArray();
                var descriptor = new Native.ComponentType { Size = (uint)Marshal.SizeOf<Native.ComponentType>(), Version = 2, Namespace = String(ns), Name = String(name),
                    TemplateNamespace = String(templateNamespace.Length == 0 ? ns : templateNamespace), TemplateName = String(template),
                    Properties = Pin(props), PropertyCount = (nuint)props.Length, Events = Pin(eventNames), EventCount = (nuint)eventNames.Length,
                    Parts = Pin(templateParts), PartCount = (nuint)templateParts.Length };
                if (OperatingSystem.IsBrowser()) {
                    var state = GCHandle.Alloc(this);
                    try {
                        descriptor.Lifecycle = (nint)(delegate* unmanaged[Cdecl]<nint, uint, nint, Result>)&BrowserLifecycle;
                        descriptor.Changed = (nint)(delegate* unmanaged[Cdecl]<nint, nint, nint, void>)&BrowserChanged;
                        descriptor.Context = GCHandle.ToIntPtr(state);
                        SunfleckException.Check(Native.RegisterComponent(app.Handle, in descriptor));
                        app.browserCallbacks.Add(state);
                    } catch { state.Free(); throw; }
                } else {
                    descriptor.Lifecycle = Marshal.GetFunctionPointerForDelegate(lifecycle);
                    descriptor.Changed = Marshal.GetFunctionPointerForDelegate(changed);
                    SunfleckException.Check(Native.RegisterComponent(app.Handle, in descriptor));
                }
            } finally { foreach (var pin in pins) { pin.Free(); } foreach (var pointer in strings) { Marshal.FreeCoTaskMem(pointer); } foreach (var value in values) { value.Dispose(); } }
        }
    }
}

internal static partial class Native
{
    [DllImport(Library, EntryPoint="ui_app_register_component_resource", CallingConvention=CallingConvention.Cdecl)] internal static extern unsafe Result RegisterComponentResource(nint app, byte* bytes, nuint length, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint="ui_app_queue_fleck_refresh_with_resources", CallingConvention=CallingConvention.Cdecl)] internal static extern unsafe Result QueueComponentPage(nint app, byte* text, nuint length, [In] ComponentResource[] resources, nuint count, nint model);
    [DllImport(Library, EntryPoint="ui_app_validate_fleck", CallingConvention=CallingConvention.Cdecl)] internal static extern unsafe Result ValidateComponentPage(nint app, byte* text, nuint length, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint="ui_app_register_component_manifest", CallingConvention=CallingConvention.Cdecl)] internal static extern unsafe Result RegisterComponentManifest(nint app, byte* text, nuint length, out Diagnostic diagnostic);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate Result ComponentLifecycle(nint component, uint phase, nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void ComponentChanged(nint component, nint name, nint context);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void ComponentEvent(nint component, nint value, nint context);
    [StructLayout(LayoutKind.Sequential)] internal struct ComponentProperty { internal nint Name; internal PropertyValue Value; internal uint ReadOnly; internal PropertyBindingMode Mode; internal nint Metadata; }
    [StructLayout(LayoutKind.Sequential)] internal struct ComponentPropertyMetadata { internal uint Size, Version; internal PropertyEffects Flags; internal nint Validate, Coerce, Context; }
    [StructLayout(LayoutKind.Sequential)] internal struct TemplatePart { internal nint Name; internal NodeKind Kind; internal uint Required; }
    [StructLayout(LayoutKind.Sequential)] internal struct ComponentResource { internal nint Uri, Text; internal nuint Length; }
    [StructLayout(LayoutKind.Sequential)] internal struct ComponentType {
        internal uint Size, Version; internal nint Namespace, Name, TemplateNamespace, TemplateName, Properties; internal nuint PropertyCount;
        internal nint Events; internal nuint EventCount; internal nint Parts; internal nuint PartCount;
        internal nint Lifecycle, Changed, Context;
    }
    [DllImport(Library, EntryPoint="ui_component_create", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CreateComponent(nint app, [MarshalAs(UnmanagedType.LPUTF8Str)] string namespaceUri,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name, nint model, out nint component, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint="ui_app_register_component_type", CallingConvention=CallingConvention.Cdecl)] internal static extern Result RegisterComponent(nint app, in ComponentType type);
    [DllImport(Library, EntryPoint="ui_app_register_fleck_resources", CallingConvention=CallingConvention.Cdecl)] internal static extern Result RegisterResources(nint app, [In] ComponentResource[] entries, nuint count, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint="ui_node_get_component", CallingConvention=CallingConvention.Cdecl)] internal static extern Result NodeComponent(nint node, out nint component);
    [DllImport(Library, EntryPoint="ui_component_get_node", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentNode(nint component, out nint node);
    [DllImport(Library, EntryPoint="ui_component_find_node", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentFind(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string id, out nint node);
    [DllImport(Library, EntryPoint="ui_component_find_template_part", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentPart(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string id, out nint node);
    [DllImport(Library, EntryPoint="ui_component_get_property", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentGet(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out PropertyValue value);
    [DllImport(Library, EntryPoint="ui_component_set_property", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentSet(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, in PropertyValue value);
    [DllImport(Library, EntryPoint="ui_component_bind_property", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentBind(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, nint model, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, PropertyBindingMode mode);
    [DllImport(Library, EntryPoint="ui_component_set_data_context", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentContext(nint component, nint model);
    [DllImport(Library, EntryPoint="ui_component_apply_template", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentTemplate(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string ns, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out Diagnostic diagnostic);
    [DllImport(Library, EntryPoint="ui_component_subscribe_scoped", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentSubscribe(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, nint handler, nint context, out nint subscription);
    [DllImport(Library, EntryPoint="ui_component_subscription_destroy", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentSubscriptionDestroy(nint subscription);
    [DllImport(Library, EntryPoint="ui_component_raise", CallingConvention=CallingConvention.Cdecl)] internal static extern Result ComponentRaise(nint component, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, in PropertyValue value);
}
