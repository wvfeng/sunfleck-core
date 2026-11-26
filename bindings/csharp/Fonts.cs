using System.Runtime.InteropServices;
using System.Text;

namespace Sunfleck;

public sealed partial class App
{
    /// <summary>Copies a TTF, OTF or TTC font into this application's collection. Windows only.</summary>
    public ulong RegisterFont(byte[] bytes)
    {
        ArgumentNullException.ThrowIfNull(bytes);
        SunfleckException.Check(FontNative.Register(Handle, bytes, (nuint)bytes.Length, out var id));
        return id;
    }
    public void UnregisterFont(ulong id) => SunfleckException.Check(FontNative.Unregister(Handle, id));
    public IReadOnlyList<string> FontFamilies(ulong id)
    {
        SunfleckException.Check(FontNative.Count(Handle, id, out uint count));
        var families = new List<string>();
        for (uint i = 0; i < count; ++i)
        {
            nuint size = 0;
            SunfleckException.Check(FontNative.Family(Handle, id, i, null, ref size));
            var text = new byte[checked((int)size)];
            SunfleckException.Check(FontNative.Family(Handle, id, i, text, ref size));
            families.Add(Encoding.UTF8.GetString(text, 0, checked((int)size - 1)));
        }
        return families;
    }
}

internal static class FontNative
{
    private const string Library = "sunfleck_native";
    [DllImport(Library, EntryPoint = "ui_app_register_font", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Register(nint app, byte[] bytes, nuint size, out ulong id);
    [DllImport(Library, EntryPoint = "ui_app_unregister_font", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Unregister(nint app, ulong id);
    [DllImport(Library, EntryPoint = "ui_app_font_family_count", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Count(nint app, ulong id, out uint count);
    [DllImport(Library, EntryPoint = "ui_app_font_family", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result Family(nint app, ulong id, uint index, [Out] byte[]? text, ref nuint size);
}
