using System.Runtime.InteropServices;
using System.Text;

namespace Sunfleck;

public enum Orientation { Horizontal, Vertical }
public enum Stretch { None, Fill, Uniform, UniformToFill }
public enum ScrollPolicy { Disabled, Auto, Hidden, Visible }
public enum TreeKind { Logical, Visual }
public enum GridUnit { Pixels, Auto, Star }
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public readonly record struct GridTrack(GridUnit Unit, double Value = 1);

public sealed partial class Node
{
    public ulong DebugId
    {
        get
        {
            _ = Handle;
            return debugId;
        }
    }
    private readonly nint handle;
    private readonly ulong debugId;
    public ulong ItemIdentity { get; }
    private ulong checkedRefreshVersion;
    internal App App { get; }

    internal Node(App app, nint handle)
    {
        App = app;
        this.handle = handle;
        checkedRefreshVersion = app.FleckRefreshVersion;
        SunfleckException.Check(Native.DebugId(handle, out debugId));
        SunfleckException.Check(Native.GetItemIdentity(handle, out var identity));
        ItemIdentity = identity;
    }

    internal nint Handle
    {
        get
        {
            var app = App.Handle;
            if (ItemIdentity != 0 || checkedRefreshVersion != App.FleckRefreshVersion)
            {
                if (Native.FindDebugNode(app, debugId, out var current) != Result.Ok || current != handle)
                {
                    throw new SunfleckException(Result.InvalidState, "The Fleck node was removed or replaced.");
                }
                checkedRefreshVersion = App.FleckRefreshVersion;
            }
            return handle;
        }
    }

    public void Focus() => SunfleckException.Check(Native.FocusNode(Handle));
    public Node? LogicalFocus {
        get {
            SunfleckException.Check(Native.GetLogicalFocus(Handle,out var target));
            return target == 0 ? null : new Node(App,target);
        }
    }

    public unsafe string Text
    {
        get
        {
            var node = Handle;
            uint size = 0;
            SunfleckException.Check(Native.GetText(node, null, ref size));
            var bytes = new byte[checked((int)size)];
            fixed (byte* pointer = bytes)
            {
                SunfleckException.Check(Native.GetText(node, pointer, ref size));
            }
            return Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
        }
        set => SunfleckException.Check(Native.SetText(Handle, Sunfleck.App.Utf8(value)));
    }

    public uint Value
    {
        get
        {
            SunfleckException.Check(Native.GetValue(Handle, out var value));
            return value;
        }
        set => SunfleckException.Check(Native.SetValue(Handle, value));
    }

    public bool IsEnabled
    {
        get
        {
            SunfleckException.Check(Native.GetEnabled(Handle, out var enabled));
            return enabled != 0;
        }
        set => SunfleckException.Check(Native.SetEnabled(Handle, value ? 1u : 0u));
    }

    public void ClearBinding(PropertyTarget property) => SunfleckException.Check(Native.ClearBinding(Handle, property));
    public void SetCommandMode(CommandMode mode) => SunfleckException.Check(Native.SetCommandMode(Handle, mode));
    public void UpdateSource(PropertyTarget property) => App.CheckBindingResult(Native.UpdateSource(Handle, property));

    public void Append(Node child)
    {
        ArgumentNullException.ThrowIfNull(child);
        App.CheckOwner(child.App);
        SunfleckException.Check(Native.Append(Handle, child.Handle));
    }

    public void SetLayout(double spacing, double padding)
        => SunfleckException.Check(Native.SetLayout(Handle, spacing, padding));

    /// <summary>Logical height with explicit Auto support.</summary>
    public Length Height {
        get => (Length)GetProperty(PropertyTarget.Height)!;
        set => SetProperty(PropertyTarget.Height,value);
    }
    internal object GetLayoutValue(PropertyTarget property) {
        if (property is >= PropertyTarget.Width and <= PropertyTarget.Margin) { return GetProperty(property)!; }
        SunfleckException.Check(Native.GetLayoutValue(Handle, property, out var value));
        return value.Kind == Native.ValueKind.Int ? (object)checked((int)value.Integer) : System.Runtime.InteropServices.Marshal.PtrToStringUTF8(value.Text)!;
    }
    internal void SetLayoutValue(PropertyTarget property, object value) {
        if (property is >= PropertyTarget.Width and <= PropertyTarget.Margin) { SetProperty(property,value); return; }
        nint text = 0;
        try {
            var native = new Native.PropertyValue();
            if (value is string literal) {
                text = System.Runtime.InteropServices.Marshal.StringToCoTaskMemUTF8(literal);
                native.Kind = Native.ValueKind.String; native.Text = text;
            } else { native.Kind = Native.ValueKind.Int; native.Integer = Convert.ToInt32(value); }
            SunfleckException.Check(Native.SetLayoutValue(Handle, property, in native));
        } finally { if (text != 0) { System.Runtime.InteropServices.Marshal.FreeCoTaskMem(text); } }
    }
    public Length Width { get => (Length)GetLayoutValue(PropertyTarget.Width); set => SetLayoutValue(PropertyTarget.Width, value); }
    public Length MinWidth { get => (Length)GetLayoutValue(PropertyTarget.MinWidth); set => SetLayoutValue(PropertyTarget.MinWidth, value); }
    public Length MaxWidth { get => (Length)GetLayoutValue(PropertyTarget.MaxWidth); set => SetLayoutValue(PropertyTarget.MaxWidth, value); }
    public Length MinHeight { get => (Length)GetLayoutValue(PropertyTarget.MinHeight); set => SetLayoutValue(PropertyTarget.MinHeight, value); }
    public Length MaxHeight { get => (Length)GetLayoutValue(PropertyTarget.MaxHeight); set => SetLayoutValue(PropertyTarget.MaxHeight, value); }
    public Thickness Margin { get => (Thickness)GetLayoutValue(PropertyTarget.Margin); set => SetLayoutValue(PropertyTarget.Margin, value); }
    public string HorizontalAlignment { get => (string)GetLayoutValue(PropertyTarget.HorizontalAlignment); set => SetLayoutValue(PropertyTarget.HorizontalAlignment, value); }
    public string VerticalAlignment { get => (string)GetLayoutValue(PropertyTarget.VerticalAlignment); set => SetLayoutValue(PropertyTarget.VerticalAlignment, value); }
    public void SetGridTracks(GridTrack[] rows, GridTrack[] columns)
        => SunfleckException.Check(Native.SetGridTracks(Handle, rows, (uint)rows.Length, columns, (uint)columns.Length));
    public void SetScrollPolicies(ScrollPolicy horizontal, ScrollPolicy vertical) => SunfleckException.Check(Native.SetScrollPolicies(Handle, horizontal, vertical));
    public void ScrollTo(double left, double top) => SunfleckException.Check(Native.ScrollTo(Handle, left, top));
    public void BringIntoView() => SunfleckException.Check(Native.BringIntoView(Handle));
    public void SetSharedSizeGroup(Orientation axis, uint index, string group) => SunfleckException.Check(Native.SetSharedSizeGroup(Handle, axis, index, group));
    public void SetOrientation(Orientation value) => SunfleckException.Check(Native.SetOrientation(Handle, value));
    public Node? GetParent(TreeKind tree)
    {
        SunfleckException.Check(Native.GetParent(Handle, tree, out var parent));
        return parent == 0 ? null : new Node(App, parent);
    }
    public uint GetChildCount(TreeKind tree)
    {
        SunfleckException.Check(Native.GetChildCount(Handle, tree, out var count));
        return count;
    }
    public Node GetChild(TreeKind tree, uint index)
    {
        SunfleckException.Check(Native.GetChild(Handle, tree, index, out var child));
        return new Node(App, child);
    }
    public void SetCanvasPosition(double left, double top) => SunfleckException.Check(Native.SetCanvasPosition(Handle, left, top));
    public void SetZIndex(int value) => SunfleckException.Check(Native.SetZIndex(Handle, value));
    public void SetGridCell(uint row, uint column, uint rowSpan = 1, uint columnSpan = 1)
        => SunfleckException.Check(Native.SetGridCell(Handle, row, column, rowSpan, columnSpan));
    public void SetWeight(uint weight)
        => SunfleckException.Check(Native.SetWeight(Handle, weight));

    public void SetDock(DockEdge edge)
        => SunfleckException.Check(Native.SetDock(Handle, edge));

    public void SetAnchor(Node anchor)
    {
        ArgumentNullException.ThrowIfNull(anchor);
        App.CheckOwner(anchor.App);
        SunfleckException.Check(Native.SetAnchor(Handle, anchor.Handle));
    }

    public unsafe void SetItems(params string[] items)
    {
        ArgumentNullException.ThrowIfNull(items);
        var node = Handle;
        var names = new nint[items.Length];
        try
        {
            for (int index = 0; index < items.Length; index++)
            {
                names[index] = Marshal.StringToCoTaskMemUTF8(Sunfleck.App.Utf8(items[index]));
            }
            fixed (nint* pointer = names)
            {
                SunfleckException.Check(Native.SetItems(node, pointer, (uint)names.Length));
            }
        }
        finally
        {
            foreach (var name in names)
            {
                Marshal.FreeCoTaskMem(name);
            }
        }
    }

    public void SetRenderTransform(Matrix matrix, Point origin = default) => SunfleckException.Check(Native.SetRenderTransform(Handle,matrix,origin));
    public Point ToWindow(Point local) { SunfleckException.Check(Native.ToWindow(Handle,local,out var result)); return result; }
    public Point FromWindow(Point window) { SunfleckException.Check(Native.FromWindow(Handle,window,out var result)); return result; }
    public void SetStretch(Stretch value) => SunfleckException.Check(Native.SetStretch(Handle,value));
    public unsafe void SetImageRgba(ReadOnlySpan<byte> rgba, uint width, uint height)
    {
        if (width is 0 or > 4096 || height is 0 or > 4096 || rgba.Length != width * height * 4)
        {
            throw new SunfleckException(Result.InvalidArgument, "Expected a 1–4096 pixel RGBA8 image.");
        }
        fixed (byte* bytes = rgba)
        {
            SunfleckException.Check(Native.SetImageRgba(Handle, bytes, width, height));
        }
    }

    public unsafe void SetImage(ReadOnlySpan<byte> rgb, uint width, uint height)
    {
        if (width is 0 or > 4096 || height is 0 or > 4096 || rgb.Length != width * height * 3)
        {
            throw new SunfleckException(Result.InvalidArgument, "Expected a 1–4096 pixel RGB8 image.");
        }
        fixed (byte* bytes = rgb)
        {
            SunfleckException.Check(Native.SetImage(Handle, bytes, width, height));
        }
    }

    public void SetInputBinding(char key, KeyModifiers modifiers, string? commandName) =>
        SunfleckException.Check(Native.SetInputBinding(Handle,key,modifiers,commandName));
    public void SetInputBinding(FunctionKey key, KeyModifiers modifiers, string? commandName) =>
        SunfleckException.Check(Native.SetInputBinding(Handle,(uint)key,modifiers,commandName));
    public void SetInputBinding(char key, KeyModifiers modifiers, EventPhase phase, string? commandName) =>
        SunfleckException.Check(Native.SetKeyBinding(Handle,key,modifiers,phase,commandName));
    public void SetInputBinding(FunctionKey key, KeyModifiers modifiers, EventPhase phase, string? commandName) =>
        SunfleckException.Check(Native.SetKeyBinding(Handle,(uint)key,modifiers,phase,commandName));
    public void SetRoutedCommand(string? name) => SunfleckException.Check(Native.SetRoutedCommand(Handle,name));
    public void SetCommandBinding(string name, Command? command) =>
        SunfleckException.Check(Native.SetCommandBinding(Handle,name,command?.Handle ?? 0));

    public void SetPointerHandler(PointerAction action, EventPhase phase, Func<Node,Node,int,int,bool>? handler) =>
        App.SetPointerHandler(Handle,action,phase,handler);
    public void SetActivationHandler(EventPhase phase, Func<Node,Node,bool>? handler) =>
        App.SetActivationHandler(Handle,phase,handler);

    public void SetAction(Action<Node>? action, ActionConcurrency concurrency = ActionConcurrency.SingleFlight)
        => App.SetAction(Handle, action, concurrency);

    public void DeferAction() => SunfleckException.Check(Native.DeferAction(Handle));
    public void CompleteAction() => SunfleckException.Check(Native.CompleteAction(Handle));

    public void BindText(TextSource? source, BindingMode mode = BindingMode.OneWay)
    {
        if (source is not null)
        {
            App.CheckOwner(source.App);
        }
        SunfleckException.Check(Native.BindText(Handle, source?.Handle ?? 0, mode));
    }

    public void BindCommand(Command? command)
    {
        if (command is not null)
        {
            App.CheckOwner(command.App);
        }
        App.BindCommand(Handle, command?.Handle ?? 0);
    }
}
