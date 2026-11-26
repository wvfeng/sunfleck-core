using System.Runtime.InteropServices;
internal static class Program {
    [DllImport("skia_evaluation", CallingConvention=CallingConvention.Cdecl)]
    private static extern uint skia_evaluation_version();
    [DllImport("skia_evaluation", CallingConvention=CallingConvention.Cdecl)]
    private static extern int skia_evaluation_render([MarshalAs(UnmanagedType.LPUTF8Str)] string directory, uint dpi);
    [DllImport("skia_evaluation", CallingConvention=CallingConvention.Cdecl)]
    private static extern nint skia_evaluation_error();
    private static int Main(string[] args) {
        if(args.Length!=1 || skia_evaluation_version()!=1)return 1;
        if(skia_evaluation_render(args[0],96)!=0)throw new Exception(Marshal.PtrToStringUTF8(skia_evaluation_error()));
        Console.WriteLine("Installed C# consumer passed.");return 0;
    }
}
