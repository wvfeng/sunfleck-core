using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed class TextSource : IDisposable
{
    private nint handle;
    internal App App { get; }

    internal TextSource(App app, nint handle)
    {
        App = app;
        this.handle = handle;
    }

    internal nint Handle
    {
        get
        {
            _ = App.Handle;
            ObjectDisposedException.ThrowIf(handle == 0, this);
            return handle;
        }
    }

    public void Set(string value)
        => SunfleckException.Check(Native.SetSourceText(Handle, App.Utf8(value)));

    public void Dispose()
    {
        if (handle != 0 && !App.IsDisposed)
        {
            SunfleckException.Check(Native.DestroyTextSource(Handle));
        }
        handle = 0;
    }
}

public sealed class Command : IDisposable
{
    private nint handle;
    internal App App { get; }

    public CommandExecutionModes ExecutionModes { get; }
    internal Command(App app, nint handle, bool asynchronous = false)
    {
        App = app;
        this.handle = handle;
        app.componentCommands.Add(handle, this);
        ExecutionModes = asynchronous ? CommandExecutionModes.Async : CommandExecutionModes.Sync;
    }

    internal nint Handle
    {
        get
        {
            _ = App.Handle;
            ObjectDisposedException.ThrowIf(handle == 0, this);
            return handle;
        }
    }

    public bool IsRunning
    {
        get
        {
            SunfleckException.Check(Native.CommandIsRunning(Handle, out var running));
            return running != 0;
        }
    }

    /// <summary>Requests cooperative cancellation of the current execution. Call on the UI thread.</summary>
    public void Cancel() => SunfleckException.Check(Native.CancelCommand(Handle));

    public void Dispose()
    {
        if (handle != 0 && !App.IsDisposed)
        {
            App.RemoveCommand(handle);
            App.componentCommands.Remove(handle);
        }
        handle = 0;
    }
}

public sealed class FleckDataContext
{
    private readonly Dictionary<string, TextSource> sources = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Command> commands = new(StringComparer.Ordinal);

    public FleckDataContext AddText(string name, TextSource source)
    {
        ArgumentNullException.ThrowIfNull(source);
        sources.Add(App.Utf8(name), source);
        return this;
    }

    public FleckDataContext AddCommand(string name, Command command)
    {
        ArgumentNullException.ThrowIfNull(command);
        commands.Add(App.Utf8(name), command);
        return this;
    }

    internal Exports MarshalExports(App app)
    {
        var exports = new Exports(sources.Count, commands.Count);
        try
        {
            int index = 0;
            foreach (var (name, source) in sources)
            {
                app.CheckOwner(source.App);
                exports.Text[index++] = MakeExport(name, source.Handle);
            }
            index = 0;
            foreach (var (name, command) in commands)
            {
                app.CheckOwner(command.App);
                exports.Commands[index++] = MakeExport(name, command.Handle);
            }
            return exports;
        }
        catch
        {
            exports.Dispose();
            throw;
        }
    }

    private static Native.Export MakeExport(string name, nint value)
        => new() { Name = Marshal.StringToCoTaskMemUTF8(name), Value = value };

    internal sealed class Exports(int texts, int commands) : IDisposable
    {
        internal Native.Export[] Text { get; } = new Native.Export[texts];
        internal Native.Export[] Commands { get; } = new Native.Export[commands];

        public void Dispose()
        {
            foreach (var entry in Text)
            {
                Marshal.FreeCoTaskMem(entry.Name);
            }
            foreach (var entry in Commands)
            {
                Marshal.FreeCoTaskMem(entry.Name);
            }
        }
    }
}
