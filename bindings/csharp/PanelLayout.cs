using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Sunfleck;

public readonly record struct LayoutRect(double X, double Y, double Width, double Height);
public delegate Size PanelMeasure(Size available, ReadOnlySpan<Size> children);
public delegate void PanelArrange(Size finalSize, ReadOnlySpan<Size> children, Span<LayoutRect> slots);

public sealed partial class Node
{
    public unsafe void SetPanelCallbacks(PanelMeasure measure, PanelArrange arrange)
    {
        ArgumentNullException.ThrowIfNull(measure);
        ArgumentNullException.ThrowIfNull(arrange);
        var node = Handle;
        var callbacks = GCHandle.Alloc((measure, arrange));
        try
        {
            var registration = new Native.PanelCallbacks
            {
                StructSize = (uint)sizeof(Native.PanelCallbacks), Version = 1,
                Measure = &MeasurePanel, Arrange = &ArrangePanel, Release = &ReleasePanel,
                Context = GCHandle.ToIntPtr(callbacks)
            };
            SunfleckException.Check(Native.SetPanelCallbacks(node, &registration));
        }
        catch
        {
            callbacks.Free();
            throw;
        }
    }

    public unsafe void ClearPanelCallbacks() => SunfleckException.Check(Native.SetPanelCallbacks(Handle, null));

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe Result MeasurePanel(nint context, Size available, Size* children, uint count, Size* desired)
    {
        try
        {
            var (measure, _) = ((PanelMeasure, PanelArrange))GCHandle.FromIntPtr(context).Target!;
            *desired = measure(available, new ReadOnlySpan<Size>(children, checked((int)count)));
            return Result.Ok;
        }
        catch { return Result.InvalidState; }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static unsafe Result ArrangePanel(nint context, Size available, Size* children, uint count, LayoutRect* slots)
    {
        try
        {
            var (_, arrange) = ((PanelMeasure, PanelArrange))GCHandle.FromIntPtr(context).Target!;
            arrange(available, new ReadOnlySpan<Size>(children, checked((int)count)), new Span<LayoutRect>(slots, checked((int)count)));
            return Result.Ok;
        }
        catch { return Result.InvalidState; }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void ReleasePanel(nint context) => GCHandle.FromIntPtr(context).Free();
}

internal static unsafe partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct PanelCallbacks
    {
        internal uint StructSize, Version;
        internal delegate* unmanaged[Cdecl]<nint, Size, Size*, uint, Size*, Result> Measure;
        internal delegate* unmanaged[Cdecl]<nint, Size, Size*, uint, LayoutRect*, Result> Arrange;
        internal nint Context;
        internal delegate* unmanaged[Cdecl]<nint, void> Release;
    }

    [DllImport(Library, EntryPoint = "ui_panel_set_callbacks", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetPanelCallbacks(nint node, PanelCallbacks* callbacks);
}
