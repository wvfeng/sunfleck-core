namespace Sunfleck;

public readonly record struct Color(uint Argb);
public readonly record struct Thickness(double Left, double Top, double Right, double Bottom);
public readonly record struct CornerRadius(double TopLeft, double TopRight, double BottomRight, double BottomLeft);
public readonly record struct Point(double X, double Y);
public readonly record struct Size(double Width, double Height);
public readonly record struct Matrix(double M11, double M12, double M21, double M22, double OffsetX, double OffsetY);
public enum LengthUnit { Pixels, Auto, Infinity }
public readonly record struct Length(double Value, LengthUnit Unit = LengthUnit.Pixels)
{
    public static implicit operator Length(double value) => new(value);
    public static Length Auto => new(0, LengthUnit.Auto);
    public static Length Infinity => new(0, LengthUnit.Infinity);
}

/// <summary>Invariant Fleck literal conversion shared with the native property system.</summary>
public static class PropertyValues
{
    public static unsafe T Parse<T>(string literal) {
        ArgumentNullException.ThrowIfNull(literal);
        var kind = ComponentValues.TypedKind(typeof(T)) ?? (typeof(T) == typeof(string) ? Native.ValueKind.String :
            typeof(T) == typeof(bool) ? Native.ValueKind.Bool : typeof(T) == typeof(double) || typeof(T) == typeof(float) ? Native.ValueKind.Double : Native.ValueKind.Int);
        double* geometry = stackalloc double[6];
        var text = System.Runtime.InteropServices.Marshal.StringToCoTaskMemUTF8(literal);
        try {
            SunfleckException.Check(Native.ParseValue(kind, text, out var value, geometry, 6));
            return (T)ComponentValues.Decode(value, typeof(T), null)!;
        } finally { System.Runtime.InteropServices.Marshal.FreeCoTaskMem(text); }
    }
}
internal static unsafe partial class Native
{
    [System.Runtime.InteropServices.DllImport(Library, EntryPoint = "ui_value_parse", CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    internal static extern Result ParseValue(ValueKind kind, nint text, out PropertyValue value, double* geometry, uint count);
}
