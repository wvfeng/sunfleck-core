using System.Runtime.InteropServices;
using System.Text;

namespace Sunfleck;

public static class DocumentFonts
{
    public static void Register(uint role, byte[] bytes)
    {
        ArgumentNullException.ThrowIfNull(bytes);
        SunfleckException.Check(DocumentNative.Font(role, bytes, (nuint)bytes.Length));
    }
}

public enum DocumentEvent { None, Link, Copy }

public sealed partial class Node
{
    public void LoadDocument(string json)
    {
        ArgumentNullException.ThrowIfNull(json);
        var bytes = Encoding.UTF8.GetBytes(json);
        SunfleckException.Check(DocumentNative.Load(Handle, bytes, (nuint)bytes.Length));
    }
    public void ScrollToHeading(string id) => SunfleckException.Check(DocumentNative.Heading(Handle, App.Utf8(id)));
    public int DocumentScroll
    {
        get { SunfleckException.Check(DocumentNative.GetScroll(Handle, out var value)); return value; }
        set => SunfleckException.Check(DocumentNative.SetScroll(Handle, value));
    }
    public void ScrollDocumentHorizontal(int x, int y, int delta) => SunfleckException.Check(DocumentNative.Horizontal(Handle, x, y, delta));
    public void SelectDocument() => SunfleckException.Check(DocumentNative.Select(Handle));
    public string DocumentSelection
    {
        get
        {
            nuint size = 0;
            SunfleckException.Check(DocumentNative.Selection(Handle, null, ref size));
            var bytes = new byte[checked((int)size)];
            SunfleckException.Check(DocumentNative.Selection(Handle, bytes, ref size));
            return Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1));
        }
    }
    public (DocumentEvent Event, string Value) TakeDocumentEvent()
    {
        nuint size = 0;
        SunfleckException.Check(DocumentNative.Event(Handle, out var kind, null, ref size));
        var bytes = new byte[checked((int)size)];
        SunfleckException.Check(DocumentNative.Event(Handle, out kind, bytes, ref size));
        return (kind, Encoding.UTF8.GetString(bytes, 0, checked((int)size - 1)));
    }
}

internal static class DocumentNative
{
    private const string Library = "sunfleck_native";
    [DllImport(Library, EntryPoint = "ui_document_set_font", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Font(uint role, byte[] bytes, nuint size);
    [DllImport(Library, EntryPoint = "ui_document_scroll_horizontal", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Horizontal(nint node, int x, int y, int delta);
    [DllImport(Library, EntryPoint = "ui_document_load", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Load(nint node, byte[] data, nuint size);
    [DllImport(Library, EntryPoint = "ui_document_scroll_to", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Heading(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string id);
    [DllImport(Library, EntryPoint = "ui_document_set_scroll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetScroll(nint node, int value);
    [DllImport(Library, EntryPoint = "ui_document_get_scroll", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GetScroll(nint node, out int value);
    [DllImport(Library, EntryPoint = "ui_document_selection", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Selection(nint node, [Out] byte[]? bytes, ref nuint size);
    [DllImport(Library, EntryPoint = "ui_document_select_all", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Select(nint node);
    [DllImport(Library, EntryPoint = "ui_document_take_event", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Event(nint node, out DocumentEvent kind, [Out] byte[]? bytes, ref nuint size);
}
