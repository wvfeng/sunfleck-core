using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Sunfleck;

public sealed partial class App
{
    // Keep exports on the top-level App type: the Wasm callback table uses
    // namespace/type/method keys, which must resolve without a nested type name.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserAsyncBegin(nint raw, nint completion) => BrowserAsyncCommandState.Begin(raw, completion);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserAsyncCancel(nint raw) => BrowserAsyncCommandState.Cancel(raw);
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void BrowserAsyncCompleted(nint raw, Result result) => BrowserAsyncCommandState.Completed(raw, result);

    private Command CreateBrowserAsyncCommand(Func<CancellationToken, Task> execute)
    {
        var state = new BrowserAsyncCommandState(this, execute);
        try
        {
            asyncCommands.EnsureCapacity(asyncCommands.Count + 1);
            SunfleckException.Check(Native.CreateExternalCommand(Handle, in state.Options, out var command));
            asyncCommands.Add(command, state);
            return new Command(this, command, asynchronous: true);
        }
        catch { state.Dispose(); throw; }
    }

    // The native callback returns before the Task finishes. Its owned completion
    // token survives native App destruction and is always released in finally.
    private sealed class BrowserAsyncCommandState : IDisposable
    {
        private readonly App app;
        private readonly Func<CancellationToken, Task> execute;
        private GCHandle lifetime;
        private CancellationTokenSource? active;
        private Exception? failure;
        private bool disposed;
        internal readonly Native.ExternalCommandOptions Options;

        internal unsafe BrowserAsyncCommandState(App app, Func<CancellationToken, Task> execute)
        {
            this.app = app;
            this.execute = execute;
            lifetime = GCHandle.Alloc(this);
            Options = new Native.ExternalCommandOptions
            {
                Size = (uint)Marshal.SizeOf<Native.ExternalCommandOptions>(), Version = 1,
                Begin = (nint)(delegate* unmanaged[Cdecl]<nint, nint, void>)&BrowserAsyncBegin,
                Cancel = (nint)(delegate* unmanaged[Cdecl]<nint, void>)&BrowserAsyncCancel,
                Completed = (nint)(delegate* unmanaged[Cdecl]<nint, Result, void>)&BrowserAsyncCompleted,
                Context = GCHandle.ToIntPtr(lifetime)
            };
        }

        private static BrowserAsyncCommandState Get(nint raw)
            => (BrowserAsyncCommandState)GCHandle.FromIntPtr(raw).Target!;
        internal static void Begin(nint raw, nint completion)
        {
            var state = Get(raw);
            // RunAsync catches every business failure; it is never async void.
            _ = state.RunAsync(completion);
        }
        internal static void Cancel(nint raw)
        {
            var state = Get(raw);
            state.app.EnterBindingCallback();
            try { state.active?.Cancel(); }
            catch (Exception error) { state.app.CaptureBindingError(error); }
            finally { state.app.ExitBindingCallback(); }
        }
        internal static void Completed(nint raw, Result result)
        {
            var state = Get(raw);
            if (state.failure is not null) { state.app.CaptureBindingError(state.failure); }
            else if (result is not (Result.Ok or Result.Cancelled)) { state.app.CaptureBindingError(new SunfleckException(result)); }
        }

        private async Task RunAsync(nint completion)
        {
            Result result = Result.Ok;
            CancellationTokenSource? cancellation = null;
            var previous = commandCancellation.Value;
            failure = null;
            try
            {
                active = cancellation = CancellationTokenSource.CreateLinkedTokenSource(app.shutdown.Token);
                commandCancellation.Value = cancellation.Token;
                cancellation.Token.ThrowIfCancellationRequested();
                Task task;
                app.EnterBindingCallback();
                try { task = execute(cancellation.Token) ?? throw new InvalidOperationException("Async command returned null."); }
                finally { app.ExitBindingCallback(); }
                await task;
                if (cancellation.IsCancellationRequested) { result = Result.Cancelled; }
            }
            catch (OperationCanceledException) when (cancellation?.IsCancellationRequested == true) { result = Result.Cancelled; }
            catch (Exception error) { failure = error; result = Result.InvalidState; }
            finally
            {
                commandCancellation.Value = previous;
                try
                {
                    if (!disposed && !app.IsDisposed)
                    {
                        // Browser Tasks resume through the runtime's UI context.
                        // Keep the native thread check as a guard against host mistakes.
                        var status = Native.CompleteExternal(completion, result);
                        if (status is not (Result.Ok or Result.Cancelled)) { app.CaptureBindingError(new SunfleckException(status)); }
                    }
                }
                finally
                {
                    Native.ReleaseExternal(completion);
                    active = null;
                    cancellation?.Dispose();
                }
            }
        }

        public void Dispose()
        {
            if (disposed) { return; }
            disposed = true;
            try { active?.Cancel(); }
            finally { if (lifetime.IsAllocated) { lifetime.Free(); } }
        }
    }
}

internal static partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct ExternalCommandOptions
    {
        internal uint Size, Version;
        internal nint Begin, Cancel, Completed;
        internal nint Context;
    }
    [DllImport(Library, EntryPoint = "ui_command_create_external", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CreateExternalCommand(nint app, in ExternalCommandOptions options, out nint command);
    [DllImport(Library, EntryPoint = "ui_completion_complete", CallingConvention = CallingConvention.Cdecl)]
    internal static extern Result CompleteExternal(nint completion, Result result);
    [DllImport(Library, EntryPoint = "ui_completion_release", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void ReleaseExternal(nint completion);
}
