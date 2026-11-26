using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed partial class App
{
    private static readonly AsyncLocal<CancellationToken> commandCancellation = new();

    /// <summary>On desktop, runs an asynchronous delegate on a worker. In the
    /// browser, uses the .NET event loop without blocking or creating a worker.
    /// Use InvokeAsync or Post for UI/model changes. Repeated
    /// execution of the same command is ignored until its Task completes.</summary>
    public Command CreateAsyncCommand(Func<CancellationToken, Task> execute)
    {
        ArgumentNullException.ThrowIfNull(execute);
        _ = Handle;
        if (OperatingSystem.IsBrowser()) { return CreateBrowserAsyncCommand(execute); }
        var state = new AsyncCommandState(this, execute);
        return RegisterAsyncCommand(in state.Options, state);
    }

    private sealed class AsyncCommandState : IDisposable
    {
        private readonly object gate = new();
        private CancellationTokenSource? active;
        private Exception? error;
        internal readonly Native.AsyncCommandOptions Options;

        internal AsyncCommandState(App app, Func<CancellationToken, Task> execute)
        {
            Options = new Native.AsyncCommandOptions
            {
                Size = (uint)Marshal.SizeOf<Native.AsyncCommandOptions>(), Version = 2,
                Execute = (_, nativeToken) =>
                {
                    CancellationTokenSource cancellation;
                    lock (gate)
                    {
                        active?.Dispose();
                        active = cancellation = CancellationTokenSource.CreateLinkedTokenSource(app.shutdown.Token);
                        error = null;
                    }
                    var previous = commandCancellation.Value;
                    commandCancellation.Value = cancellation.Token;
                    try
                    {
                        if (Native.CancellationRequested(nativeToken) != 0) { cancellation.Cancel(); }
                        cancellation.Token.ThrowIfCancellationRequested();
                        (execute(cancellation.Token) ?? throw new InvalidOperationException("Async command returned null."))
                            .GetAwaiter().GetResult();
                        return cancellation.IsCancellationRequested ? Result.Cancelled : Result.Ok;
                    }
                    catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
                    {
                        return Result.Cancelled;
                    }
                    catch (Exception failure)
                    {
                        error = failure;
                        return Result.InvalidState;
                    }
                    finally { commandCancellation.Value = previous; }
                },
                Cancel = _ =>
                {
                    app.EnterBindingCallback();
                    try { lock (gate) { active?.Cancel(); } }
                    catch (Exception failure) { app.CaptureBindingError(failure); }
                    finally { app.ExitBindingCallback(); }
                },
                Completed = (_, result) =>
                {
                    if (error is not null) { app.CaptureBindingError(error); }
                    else if (result is not (Result.Ok or Result.Cancelled)) { app.CaptureBindingError(new SunfleckException(result)); }
                    Dispose();
                }
            };
        }
        public void Dispose()
        {
            lock (gate) { active?.Dispose(); active = null; }
        }
    }
}
