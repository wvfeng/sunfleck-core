using System.Runtime.InteropServices;

namespace Sunfleck;

[Flags]
public enum DropKind { Text = 1, Files = 2, Object = 4 }

public sealed record DropData(DropKind Kind, string? Text = null, IReadOnlyList<string>? Files = null, ulong ObjectId = 0);

internal static partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct DropPayload
    {
        internal DropKind Kind;
        internal nint Text, Paths;
        internal uint Count;
        internal nint Owner;
        internal ulong Object;
    }
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int DropCallback(nint node, in DropPayload data, nint context);
    [DllImport(Library, EntryPoint="ui_node_set_drop_handler", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetDropHandler(nint node, DropKind kinds, DropCallback? callback, nint context);
    [DllImport(Library, EntryPoint="ui_node_start_drag", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result StartDrag(nint node, in DropPayload data);
}

public sealed partial class App
{
    private readonly Dictionary<nint,(ulong Id,Native.DropCallback Callback)> dropHandlers = [];
    internal void SetDropHandler(Node node, DropKind kinds, Func<Node, DropData, bool>? handler)
    {
        var handle = node.Handle;
        Native.DropCallback? callback = handler is null ? null : (nint sender, in Native.DropPayload data, nint _) => {
            ++callbackDepth;
            try {
                var paths = new string[data.Count];
                for (int i = 0; i < paths.Length; ++i)
                {
                    paths[i] = Marshal.PtrToStringUTF8(Marshal.ReadIntPtr(data.Paths, i * IntPtr.Size))!;
                }

                return handler(new Node(this,sender), new DropData(data.Kind, Marshal.PtrToStringUTF8(data.Text), paths, data.Object)) ? 1 : 0;
            } catch(Exception error) { CaptureBindingError(error); return 0; }
            finally { --callbackDepth; }
        };
        dropHandlers.EnsureCapacity(dropHandlers.Count + 1);
        SunfleckException.Check(Native.SetDropHandler(handle,kinds,callback,0));
        if (callback is null)
        {
            dropHandlers.Remove(handle);
        }
        else
        {
            dropHandlers[handle] = (node.DebugId,callback);
        }
    }
}

public sealed partial class Node
{
    public void SetDropHandler(DropKind kinds, Func<Node, DropData, bool>? handler) => App.SetDropHandler(this,kinds,handler);

    public unsafe bool StartDrag(DropData data)
    {
        ArgumentNullException.ThrowIfNull(data);
        var paths = new nint[data.Files?.Count ?? 0];
        var payload = new Native.DropPayload { Kind=data.Kind, Owner=App.Handle, Object=data.ObjectId };
        try {
            if (data.Text is not null)
            {
                payload.Text=Marshal.StringToCoTaskMemUTF8(data.Text);
            }

            for (int i=0; i<paths.Length; ++i)
            {
                paths[i]=Marshal.StringToCoTaskMemUTF8(data.Files![i]);
            }

            fixed (nint* first=paths) {
                payload.Paths=(nint)first;
                payload.Count=(uint)paths.Length;
                var result=Native.StartDrag(Handle,in payload);
                if(result==Result.Cancelled)
                {
                    return false;
                }

                SunfleckException.Check(result);
                return true;
            }
        } finally {
            Marshal.FreeCoTaskMem(payload.Text);
            foreach(var path in paths)
            {
                Marshal.FreeCoTaskMem(path);
            }
        }
    }
}
