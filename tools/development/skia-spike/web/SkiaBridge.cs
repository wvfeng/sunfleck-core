using System.Runtime.InteropServices;
using System.Runtime.InteropServices.JavaScript;
using System.Runtime.Versioning;

[SupportedOSPlatform("browser")]
public static partial class SkiaBridge
{
    [DllImport("skia_bridge")] private static extern int skia_web_init();
    [DllImport("skia_bridge")] private static extern int skia_web_font_version();
    [DllImport("skia_bridge")] private static extern unsafe int skia_web_features(byte* font, int size);
    [DllImport("skia_bridge")] private static extern unsafe int skia_web_present(byte* commands, int size, int width, int height);
    [DllImport("skia_bridge")] private static extern void skia_web_lost();
    [DllImport("skia_bridge")] private static extern int skia_web_restore();
    [DllImport("skia_bridge")] private static extern void skia_web_close();
    [JSExport] public static int Init() => skia_web_init();
    [JSExport] public static int FontVersion() => skia_web_font_version();
    [JSExport] public static unsafe int Features(byte[] font)
    {
        fixed(byte* pointer = font) return skia_web_features(pointer, font.Length);
    }
    [JSExport] public static void Lost() => skia_web_lost();
    [JSExport] public static int Restore() => skia_web_restore();
    [JSExport] public static void Close() => skia_web_close();
    [JSExport] public static unsafe int Present(byte[] bytes, int width, int height)
    {
        fixed(byte* pointer = bytes) return skia_web_present(pointer, bytes.Length, width, height);
    }
}
