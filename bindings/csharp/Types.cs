namespace Sunfleck;

[Flags]
public enum KeyModifiers : uint { None = 0, Ctrl = 1, Shift = 2, Alt = 4 }
public enum FunctionKey : uint {
    F1 = 0xA0, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24
}

public enum EventPhase { Preview, Direct, Bubble }

public enum BuiltinTheme
{
    Fluent2 = 0,
    Material3 = 1,
    RadixThemes = 2,
    AppleLiquidGlass = 3,
    SunfleckLite = 4,
}

public enum Result
{
    Ok, InvalidArgument, OutOfMemory, PlatformError, Unsupported, WrongThread, InvalidState, Timeout, Exited, Cancelled, VersionMismatch
}

public enum Renderer { Auto, Gles3, Software, D3D11, Metal, Angle }
public enum DockEdge { Left, Top, Right, Bottom }
public enum BindingMode { OneWay, OneTime, TwoWay }
public enum ActionConcurrency { SingleFlight, AllowOverlap }
public enum FleckError { None, Syntax, Limit, Forbidden, Element, Attribute, Value, Runtime }

public enum NodeKind
{
    StackPanel, Flex, Grid, ScrollView, Panel, Card, Button, TextBlock, Image, CheckBox, RadioButton,
    Switch, Slider, Separator, ProgressBar, Popup, ComboBox, TextBox, ItemsControl, ListView,
    ListItem, TabView, TabItem, Overlay, WrapPanel, DockPanel, SplitView, Label, Icon, Badge,
    IconButton, ToggleButton, Hyperlink, SpinBox, PasswordBox, TextArea, SearchBox, TreeView,
    DataGrid, MenuBar, ToolBar, NavigationView, Dialog, Tooltip, ContextMenu, Spinner, InfoBar, TreeItem,
    DockHost, DockSplit, DockGroup, DockDocument, DockTool, DocumentView, Border, Run, Span, Canvas, ContentControl, ContentPresenter, Viewbox, Rectangle, Ellipse, Line, Path
}

public class SunfleckException(Result result, string? message = null)
    : InvalidOperationException(message ?? (result == Result.VersionMismatch
        ? "Sunfleck SDK/runtime version mismatch. Rebuild the application and Fleck resources with the same SDK."
        : $"Sunfleck operation failed: {result}."))
{
    public Result Result { get; } = result;

    internal static void Check(Result result)
    {
        if (result != Result.Ok)
        {
            throw new SunfleckException(result);
        }
    }
}

public sealed class FleckException(Result result, FleckError error, uint line, uint column, string message)
    : SunfleckException(result, $"Fleck {error} ({line},{column}): {message}")
{
    public FleckError Error { get; } = error;
    public uint Line { get; } = line;
    public uint Column { get; } = column;
}

public sealed record PixelFrame(uint Width, uint Height, byte[] Rgba);

public enum FontWeight { Thin=100, ExtraLight=200, Light=300, SemiLight=350, Normal=400, Medium=500, SemiBold=600, Bold=700, ExtraBold=800, Black=900, ExtraBlack=950 }
public enum FontStyle { Normal, Oblique, Italic }
public enum FontStretch { UltraCondensed=1, ExtraCondensed, Condensed, SemiCondensed, Normal, SemiExpanded, Expanded, ExtraExpanded, UltraExpanded }

public enum TextAlignment { Left, Center, Right, Justify }
public enum TextWrapping { NoWrap, Wrap }
public enum TextTrimming { None, CharacterEllipsis, WordEllipsis }
[Flags] public enum TextDecorations { None=0, Underline=1, Strikethrough=2 }
public enum FlowDirection { LeftToRight, RightToLeft }

public enum Visibility { Visible, Hidden, Collapsed }
