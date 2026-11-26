using System.Runtime.InteropServices;

namespace Sunfleck;

public enum Easing { Linear, EaseIn, EaseOut, EaseInOut }
public enum AnimationAction { Pause, Resume, Seek, Stop }
public sealed record KeyFrame(double Position, object Value, Easing Easing = Easing.Linear);
public sealed record Timeline(IReadOnlyList<KeyFrame> Frames, uint DurationMs, uint DelayMs = 0,
    uint RepeatCount = 1, bool AutoReverse = false, bool HoldEnd = false);

internal static unsafe partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct Frame { internal double Position; internal PropertyValue Value; internal Easing Easing; }
    [StructLayout(LayoutKind.Sequential)]
    internal struct TimelineOptions {
        internal Frame* Frames;
        internal uint Count, Duration, Delay, Repeat, Reverse, Hold;
    }
    [DllImport(Library, EntryPoint = "ui_node_start_animation", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result StartAnimation(nint node, PropertyTarget property, in TimelineOptions timeline);
    [DllImport(Library, EntryPoint = "ui_node_begin_storyboard", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result BeginStoryboard(nint node);
    [DllImport(Library, EntryPoint = "ui_node_register_visual_state", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result RegisterVisualState(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string group,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name, [MarshalAs(UnmanagedType.LPUTF8Str)] string? storyboard);
    [DllImport(Library, EntryPoint = "ui_node_go_to_visual_state", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result GoToVisualState(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string group,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Library, EntryPoint = "ui_app_advance_animation_clock", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result AdvanceAnimationClock(nint app,uint elapsedMs);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void AnimationCompletion(nint node, PropertyTarget property, nint context);
    [DllImport(Library, EntryPoint = "ui_app_set_animation_completed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetAnimationCompleted(nint app, AnimationCompletion? callback, nint context);
    [DllImport(Library, EntryPoint = "ui_app_set_reduced_motion", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result SetReducedMotion(nint app, uint enabled);
    [DllImport(Library, EntryPoint = "ui_node_control_animation", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result ControlAnimation(nint node, PropertyTarget property, AnimationAction action, uint position);
}

public sealed partial class App
{
    public void AdvanceAnimationClock(uint elapsedMs) => SunfleckException.Check(Native.AdvanceAnimationClock(Handle,elapsedMs));
    public void SetReducedMotion(bool enabled) => SunfleckException.Check(Native.SetReducedMotion(Handle,enabled ? 1u : 0u));
    private Native.AnimationCompletion? animationCompletion;
    public void SetAnimationCompleted(Action<Node,PropertyTarget>? callback) {
        Native.AnimationCompletion? handler = callback is null ? null : (node,property,_) => {
            ++callbackDepth;
            try { callback(new Node(this,node),property); }
            catch (Exception error) { CaptureBindingError(error); }
            finally { --callbackDepth; }
        };
        SunfleckException.Check(Native.SetAnimationCompleted(Handle,handler,0));
        animationCompletion = handler;
    }
}

public sealed partial class Node
{
    public void BeginStoryboard() => SunfleckException.Check(Native.BeginStoryboard(Handle));
    public void RegisterVisualState(string group, string name, string? storyboard = null) =>
        SunfleckException.Check(Native.RegisterVisualState(Handle,group,name,storyboard));
    public void GoToVisualState(string group, string name) =>
        SunfleckException.Check(Native.GoToVisualState(Handle,group,name));
    public unsafe void StartAnimation(PropertyTarget property, Timeline timeline) {
        ArgumentNullException.ThrowIfNull(timeline);
        var frames = new Native.Frame[timeline.Frames.Count];
        var values = new ComponentValues?[frames.Length];
        try {
            for (int i = 0; i < frames.Length; ++i) {
                var frame = timeline.Frames[i];
                ArgumentNullException.ThrowIfNull(frame.Value);
                var value = new ComponentValues(frame.Value, frame.Value.GetType());
                values[i] = value;
                frames[i] = new() { Position = frame.Position, Value = value.Value, Easing = frame.Easing };
            }
            fixed (Native.Frame* first = frames) {
                var options = new Native.TimelineOptions { Frames = first, Count = (uint)frames.Length,
                    Duration = timeline.DurationMs, Delay = timeline.DelayMs, Repeat = timeline.RepeatCount,
                    Reverse = timeline.AutoReverse ? 1u : 0u, Hold = timeline.HoldEnd ? 1u : 0u };
                SunfleckException.Check(Native.StartAnimation(Handle, property, in options));
            }
        } finally {
            foreach (var value in values) { value?.Dispose(); }
        }
    }
    public void ControlAnimation(PropertyTarget property, AnimationAction action, uint positionMs = 0)
        => SunfleckException.Check(Native.ControlAnimation(Handle, property, action, positionMs));
}
