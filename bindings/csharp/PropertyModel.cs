using System.ComponentModel;
using System.Globalization;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Diagnostics.CodeAnalysis;

namespace Sunfleck;

/// <summary>A UI-thread model adapter. Dispose before releasing business state.</summary>
public sealed partial class PropertyModel : IDisposable
{
    private sealed class Entry : IDisposable
    {
        internal required Native.PropertyGet Get;
        internal Native.PropertySet? Set;
        internal nint Text;
        internal ComponentValues? TypedValue;
        internal object? ChildObject;
        internal PropertyModel? ChildModel;
        internal GCHandle BrowserState;
        public void Dispose()
        {
            TypedValue?.Dispose(); TypedValue = null;
            ChildModel?.Dispose(); ChildModel = null; ChildObject = null;
            Marshal.FreeCoTaskMem(Text);
            Text = 0;
            if (BrowserState.IsAllocated) { BrowserState.Free(); }
        }
    }
    private readonly Dictionary<string, Entry> entries = new(StringComparer.Ordinal);
    private sealed class ConverterEntry
    {
        internal required Native.BindingConvert Convert;
        internal ComponentValues? Output;
    }
    private readonly List<ConverterEntry> converters = new();

    public unsafe PropertyModel AddConverter<T>(string name, Func<IReadOnlyList<object?>, T> convert)
    {
        ArgumentNullException.ThrowIfNull(convert);
        using var prototype = new ComponentValues(default(T), typeof(T));
        ConverterEntry? entry = null;
        entry = new ConverterEntry
        {
            Convert = (nint _, Native.PropertyValue* inputs, uint count, out Native.PropertyValue value) =>
            {
                value = default;
                App.EnterBindingCallback();
                try
                {
                    var values = new object?[count];
                    for (int i = 0; i < values.Length; ++i)
                        { values[i] = ComponentValues.Decode(inputs[i], null, App); }
                    var result = convert(values);
                    if (result is PropertyModel model) { App.CheckOwner(model.App); }
                    if (result is Command command) { App.CheckOwner(command.App); }
                    var output = new ComponentValues(result, typeof(T));
                    entry!.Output?.Dispose();
                    entry.Output = output;
                    value = output.Value;
                    return Result.Ok;
                }
                catch (Exception error)
                {
                    App.CaptureBindingError(error);
                    return Result.InvalidState;
                }
                finally { App.ExitBindingCallback(); }
            }
        };
        converters.EnsureCapacity(converters.Count + 1);
        SunfleckException.Check(Native.AddConverter(Handle, Sunfleck.App.Utf8(name), prototype.Value.Kind, entry.Convert, 0));
        converters.Add(entry);
        return this;
    }

    public PropertyModel AddMultiBinding(string path, string converter, params string[] inputs)
    {
        ArgumentNullException.ThrowIfNull(inputs);
        var pointers = new nint[inputs.Length];
        try
        {
            for (int i = 0; i < pointers.Length; ++i)
                { pointers[i] = Marshal.StringToCoTaskMemUTF8(Sunfleck.App.Utf8(inputs[i])); }
            App.CheckBindingResult(Native.AddMultiBinding(Handle, Sunfleck.App.Utf8(path),
                Sunfleck.App.Utf8(converter), pointers, (uint)pointers.Length));
        }
        finally { foreach (var pointer in pointers) { Marshal.FreeCoTaskMem(pointer); } }
        return this;
    }
    private readonly INotifyPropertyChanged? observable;
    private nint handle;
    internal App App { get; }
    internal nint Handle
    {
        get
        {
            _ = App.Handle;
            ObjectDisposedException.ThrowIf(handle == 0, this);
            return handle;
        }
    }

    internal PropertyModel(App app, object? model, [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicProperties)] Type? modelType)
    {
        App = app;
        SunfleckException.Check(Native.CreateModel(app.Handle, out handle));
        try
        {
            if (model is not null)
            {
                foreach (var property in modelType!.GetProperties(BindingFlags.Public | BindingFlags.Instance))
                {
                    if (property.GetIndexParameters().Length != 0 || property.GetMethod?.IsPublic != true || !Supported(property.PropertyType))
                    {
                        continue;
                    }
                    AddCore(property.Name, property.PropertyType, () => property.GetValue(model)!,
                        property.SetMethod?.IsPublic == true ? value => property.SetValue(model, value) : null);
                    if (property.PropertyType == typeof(Command) && property.GetCustomAttribute<CommandModesAttribute>() is { } commandModes)
                    {
                        SetCommandModes(property.Name, commandModes.Modes);
                    }
                }
                observable = model as INotifyPropertyChanged;
                if (observable is not null)
                {
                    observable.PropertyChanged += Changed;
                }
            }
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    public PropertyModel Add<T>(string path, Func<T> read, Action<T>? write = null,
        [CallerFilePath] string sourceUri = "", [CallerLineNumber] int sourceLine = 0)
    {
        ArgumentNullException.ThrowIfNull(read);
        AddCore(path, typeof(T), () => read()!, write is null ? null : value => write((T)value));
        SunfleckException.Check(Native.PropertySource(Handle, path, sourceUri, checked((uint)sourceLine), 1));
        return this;
    }

    public PropertyModel AddCommand(string path, Command command,
        [CallerFilePath] string sourceUri = "", [CallerLineNumber] int sourceLine = 0)
    {
        ArgumentNullException.ThrowIfNull(command);
        App.CheckOwner(command.App);
        Add(path, () => command, sourceUri: sourceUri, sourceLine: sourceLine);
        return SetCommandModes(path, command.ExecutionModes);
    }

    public void NotifyPropertyChanged(string? path = null)
    {
        var result = Native.NotifyProperty(Handle, Sunfleck.App.Utf8(path ?? ""));
        App.CheckBindingResult(result);
    }

    public void BeginUpdate() => SunfleckException.Check(Native.BeginModelUpdate(Handle));
    public void EndUpdate() => App.CheckBindingResult(Native.EndModelUpdate(Handle));

    public void Bind(Node node, PropertyTarget property, string path, PropertyBindingMode mode = PropertyBindingMode.Default,
        UpdateSourceTrigger trigger = UpdateSourceTrigger.PropertyChanged, string stringFormat = "")
    {
        ArgumentNullException.ThrowIfNull(node);
        App.CheckOwner(node.App);
        var binding = new Native.PropertyBinding
        {
            Size = (uint)Marshal.SizeOf<Native.PropertyBinding>(), Version = 1, Property = property,
            Path = Sunfleck.App.Utf8(path), Mode = mode, Trigger = trigger, Format = Sunfleck.App.Utf8(stringFormat)
        };
        App.CheckBindingResult(Native.BindProperty(node.Handle, Handle, in binding));
    }

    private void Changed(object? sender, PropertyChangedEventArgs args)
    {
        if (!App.IsUiThread)
        {
            App.Post(() => Changed(sender, args));
            return;
        }
        // INotifyPropertyChanged may announce properties not exported by this adapter.
        if (handle != 0 && (string.IsNullOrEmpty(args.PropertyName) || entries.ContainsKey(args.PropertyName)))
        {
            NotifyPropertyChanged(args.PropertyName);
        }
    }

    private static bool Supported(Type type)
        => ComponentValues.TypedKind(type) is not null || type == typeof(string) || type == typeof(bool) || type == typeof(Command) || type == typeof(PropertyModel) || typeof(INotifyPropertyChanged).IsAssignableFrom(type) ||
           type == typeof(byte) || type == typeof(short) || type == typeof(int) || type == typeof(long) ||
           type == typeof(ushort) || type == typeof(uint) || type == typeof(sbyte) ||
           type == typeof(float) || type == typeof(double);

    private void AddCore(string path, Type type, Func<object> read, Action<object>? write)
    {
        var model = Handle;
        path = Sunfleck.App.Utf8(path);
        if (!Supported(type) || entries.ContainsKey(path))
        {
            throw new ArgumentException("Unsupported property type or duplicate path.", nameof(path));
        }
        var kind = ComponentValues.TypedKind(type) ?? (type == typeof(string) ? Native.ValueKind.String : type == typeof(bool) ? Native.ValueKind.Bool :
            type == typeof(Command) ? Native.ValueKind.Command : type == typeof(PropertyModel) || typeof(INotifyPropertyChanged).IsAssignableFrom(type) ? Native.ValueKind.Model : type == typeof(float) || type == typeof(double)
                ? Native.ValueKind.Double : Native.ValueKind.Int);
        Entry? entry = null;
        entry = new Entry
        {
            Get = (nint _, out Native.PropertyValue value) =>
            {
                value = new Native.PropertyValue { Kind = kind };
                App.EnterBindingCallback();
                try
                {
                    var result = read();
                    if (ComponentValues.TypedKind(type) is not null) {
                        var next = new ComponentValues(result, type);
                        entry!.TypedValue?.Dispose(); entry.TypedValue = next; value = next.Value;
                        return Result.Ok;
                    }
                    switch (kind)
                    {
                        case Native.ValueKind.String:
                            var text = Marshal.StringToCoTaskMemUTF8(Sunfleck.App.Utf8((string)result));
                            Marshal.FreeCoTaskMem(entry!.Text);
                            entry.Text = text;
                            value.Text = text;
                            break;
                        case Native.ValueKind.Bool:
                            value.Integer = (bool)result ? 1 : 0;
                            break;
                        case Native.ValueKind.Command:
                            var command = (Command)result;
                            App.CheckOwner(command.App);
                            value.Command = command.Handle;
                            break;
                        case Native.ValueKind.Model:
                            if (result is PropertyModel explicitModel) { App.CheckOwner(explicitModel.App); value.Command = explicitModel.Handle; }
                            else {
                                if (!ReferenceEquals(entry!.ChildObject, result)) {
                                    var previous = entry.ChildModel;
                                    var next = result is null ? null : App.CreatePropertyModel(result);
                                    entry.ChildObject = result;
                                    entry.ChildModel = next;
                                    if (previous is not null) { App.Post(previous.Dispose); }
                                }
                                value.Command = entry!.ChildModel?.Handle ?? 0;
                            }
                            break;
                        case Native.ValueKind.Double:
                            value.Number = Convert.ToDouble(result, CultureInfo.InvariantCulture);
                            break;
                        default:
                            value.Integer = Convert.ToInt64(result, CultureInfo.InvariantCulture);
                            break;
                    }
                    return Result.Ok;
                }
                catch (Exception error)
                {
                    App.CaptureBindingError(error);
                    return Result.InvalidState;
                }
                finally
                {
                    App.ExitBindingCallback();
                }
            },
            Set = write is null || kind == Native.ValueKind.Model ? null : (nint _, in Native.PropertyValue value) =>
            {
                App.EnterBindingCallback();
                try
                {
                    object converted = ComponentValues.TypedKind(type) is not null ? ComponentValues.Decode(value, type, App)! : kind switch
                    {
                        Native.ValueKind.String => Marshal.PtrToStringUTF8(value.Text)!,
                        Native.ValueKind.Bool => value.Integer != 0,
                        Native.ValueKind.Double => Convert.ChangeType(value.Number, type, CultureInfo.InvariantCulture),
                        _ => Convert.ChangeType(value.Integer, type, CultureInfo.InvariantCulture)
                    };
                    write(converted);
                    return Result.Ok;
                }
                catch (Exception error)
                {
                    App.CaptureBindingError(error);
                    return Result.InvalidState;
                }
                finally
                {
                    App.ExitBindingCallback();
                }
            }
        };
        var descriptor = new Native.ModelProperty
        {
            Size = (uint)Marshal.SizeOf<Native.ModelProperty>(), Version = 1,
            Path = path, Kind = kind, Get = entry.Get, Set = entry.Set
        };
        try
        {
            entries.EnsureCapacity(entries.Count + 1);
            if (OperatingSystem.IsBrowser()) { AddBrowserProperty(model, path, kind, entry); }
            else { SunfleckException.Check(Native.AddProperty(model, in descriptor)); }
            entries.Add(path, entry);
        }
        catch
        {
            entry.Dispose();
            throw;
        }
    }

    public void Dispose()
    {
        if (handle == 0)
        {
            return;
        }
        if (!App.IsDisposed)
        {
            SunfleckException.Check(Native.DestroyModel(Handle));
        }
        handle = 0;
        DetachTextCollection();
        if (observable is not null)
        {
            observable.PropertyChanged -= Changed;
        }
        foreach (var entry in entries.Values)
        {
            entry.Dispose();
        }
        entries.Clear();
        foreach (var converter in converters) { converter.Output?.Dispose(); }
        converters.Clear();
        App.RemovePropertyModel(this);
    }
}
