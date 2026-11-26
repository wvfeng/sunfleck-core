using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed partial class App
{
    /// <summary>Executes an action on the UI thread and returns its completion.
    /// Cancellation before execution skips the action. Closing the app or
    /// cancelling its originating command also cancels pending callbacks.</summary>
    public Task InvokeAsync(Action action, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(action);
        var cancellation = CancellationTokenSource.CreateLinkedTokenSource(
            cancellationToken, shutdown.Token, commandCancellation.Value);
        var invocation = new UiInvocation(this, action, cancellation);
        lock (postingLock)
        {
            if (handle == 0 || disposing || cancellation.IsCancellationRequested)
            {
                invocation.Release();
                return invocation.Completion.Task;
            }
            if (IsUiThread)
            {
                invocation.Run();
                invocation.Release();
                return invocation.Completion.Task;
            }
            var allocated = GCHandle.Alloc(invocation);
            var result = PostInvocation(handle, GCHandle.ToIntPtr(allocated));
            if (result != Result.Ok)
            {
                allocated.Free();
                if (result != Result.Cancelled) { invocation.Completion.TrySetException(new SunfleckException(result)); }
                invocation.Release();
            }
        }
        return invocation.Completion.Task;
    }

    private static readonly Native.PostCallback invokeWork = raw => ((UiInvocation)GCHandle.FromIntPtr(raw).Target!).Run();
    private static readonly Native.PostCallback invokeRelease = raw =>
    {
        var allocated = GCHandle.FromIntPtr(raw);
        ((UiInvocation)allocated.Target!).Release();
        allocated.Free();
    };
    private sealed class UiInvocation
    {
        internal readonly TaskCompletionSource Completion = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly App app;
        private readonly Action action;
        private readonly CancellationTokenSource cancellation;
        private readonly CancellationTokenRegistration registration;
        private int started;

        internal UiInvocation(App app, Action action, CancellationTokenSource cancellation)
        {
            this.app = app;
            this.action = action;
            this.cancellation = cancellation;
            registration = cancellation.Token.Register(() =>
            {
                if (Interlocked.CompareExchange(ref started, 2, 0) == 0) { Completion.TrySetCanceled(cancellation.Token); }
            });
        }
        internal void Run()
        {
            if (Interlocked.CompareExchange(ref started, 1, 0) != 0) { return; }
            app.EnterBindingCallback();
            try { action(); Completion.TrySetResult(); }
            catch (Exception error) { Completion.TrySetException(error); }
            finally { app.ExitBindingCallback(); }
        }
        internal void Release()
        {
            if (Interlocked.CompareExchange(ref started, 2, 0) == 0) { Completion.TrySetCanceled(); }
            registration.Dispose();
            cancellation.Dispose();
        }
    }
}
