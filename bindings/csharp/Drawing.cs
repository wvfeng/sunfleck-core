using System.Runtime.InteropServices;

namespace Sunfleck;

public enum BrushTarget { Background, Outline, OpacityMask }
public enum BrushKind { Solid, Linear, Radial, Image }
public enum EffectKind { None, Blur, DropShadow }
public readonly record struct GradientStop(double Offset, Color Color);
public readonly record struct Effect(EffectKind Kind, double Radius, Point Offset, Color Color);

public sealed record Brush(BrushKind Kind)
{
    public Color Color { get; init; }
    public Point Start { get; init; }
    public Point End { get; init; } = new(1, 1);
    public GradientStop[] Stops { get; init; } = [];
    public byte[] Rgba { get; init; } = [];
    public uint Width { get; init; }
    public uint Height { get; init; }
    public Stretch Stretch { get; init; } = Stretch.Fill;
}

internal static unsafe partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct BrushDescriptor
    {
        internal BrushKind Kind;
        internal Color Color;
        internal Point Start, End;
        internal GradientStop* Stops;
        internal uint StopCount;
        internal byte* Pixels;
        internal uint Width, Height;
        internal Stretch Stretch;
    }

    [DllImport(Library, EntryPoint = "ui_path_set_data", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetPathData(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string data);

    [DllImport(Library, EntryPoint = "ui_shape_set_stroke_thickness", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetStrokeThickness(nint node, double width);

    [DllImport(Library, EntryPoint = "ui_node_set_brush", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetBrush(nint node, BrushTarget target, BrushDescriptor* brush);

    [DllImport(Library, EntryPoint = "ui_node_set_effect", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetEffect(nint node, in Effect effect);
}

public sealed partial class Node
{
    public void SetPathData(string data) => SunfleckException.Check(Native.SetPathData(Handle, data));
    public void SetStrokeThickness(double width) => SunfleckException.Check(Native.SetStrokeThickness(Handle, width));
    public void SetEffect(Effect effect) => SunfleckException.Check(Native.SetEffect(Handle, in effect));

    public unsafe void SetBrush(BrushTarget target, Brush? brush)
    {
        if (brush is null)
        {
            SunfleckException.Check(Native.SetBrush(Handle, target, null));
            return;
        }
        if (brush.Kind == BrushKind.Image && (brush.Width is 0 or > 4096 || brush.Height is 0 or > 4096 ||
            brush.Rgba.Length != brush.Width * brush.Height * 4))
        {
            throw new SunfleckException(Result.InvalidArgument, "Image brush requires packed RGBA pixels.");
        }
        fixed (GradientStop* stops = brush.Stops)
        fixed (byte* pixels = brush.Rgba)
        {
            Native.BrushDescriptor desc = new()
            {
                Kind = brush.Kind, Color = brush.Color, Start = brush.Start, End = brush.End,
                Stops = stops, StopCount = (uint)brush.Stops.Length, Pixels = pixels,
                Width = brush.Width, Height = brush.Height, Stretch = brush.Stretch
            };
            SunfleckException.Check(Native.SetBrush(Handle, target, &desc));
        }
    }
}
