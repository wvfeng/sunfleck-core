using System.IO.Pipes;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Sunfleck;

/// <summary>Small local debug service used by editor integrations.</summary>
internal sealed class DebugServer : IDisposable
{
    private readonly App app;
    internal string PipeName { get; }
    private readonly NamedPipeServerStream server;
    private readonly string session = $"pipe-{Environment.ProcessId}-{Guid.NewGuid():N}";
    private readonly CancellationTokenSource stopping = new();
    private readonly Task serverTask;

    internal DebugServer(App app, string pipeName)
    {
        this.app = app;
        PipeName = pipeName;
        try
        {
            // Keep ownership between clients and report name conflicts at startup.
            server = new NamedPipeServerStream(pipeName, PipeDirection.InOut, 1,
                PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly | PipeOptions.FirstPipeInstance);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            stopping.Dispose();
            throw new InvalidOperationException($"Cannot open debug Pipe '{pipeName}'. It may already be in use; choose a different SUNFLECK_DEBUG_PIPE for each application.", error);
        }
        serverTask = Task.Run(RunAsync);
    }

    private async Task RunAsync()
    {
        try
        {
            while (!stopping.IsCancellationRequested)
            {
                await server.WaitForConnectionAsync(stopping.Token);
                try { await HandleClientAsync(server, stopping.Token); }
                catch (IOException) { /* A disconnected editor must not stop the listener. */ }
                catch (OperationCanceledException) when (!stopping.IsCancellationRequested) { }
                finally
                {
                    // A client that closed its handle leaves a broken connection;
                    // IsConnected is then false, but Disconnect is still required.
                    if (!stopping.IsCancellationRequested) { server.Disconnect(); }
                }
            }
        }
        catch (OperationCanceledException) when (stopping.IsCancellationRequested)
        {
        }
        catch (IOException) when (stopping.IsCancellationRequested)
        {
        }
        catch (ObjectDisposedException) when (stopping.IsCancellationRequested)
        {
        }
    }

    private async Task HandleClientAsync(Stream stream, CancellationToken cancellationToken)
    {
        using var reader = new StreamReader(stream, Encoding.UTF8, false, 4096, leaveOpen: true);
        await using var writer = new StreamWriter(stream, new UTF8Encoding(false), 4096, leaveOpen: true) { AutoFlush = true };
        while (!cancellationToken.IsCancellationRequested)
        {
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeout.CancelAfter(TimeSpan.FromSeconds(2));
            var line = await reader.ReadLineAsync(timeout.Token);
            if (line is null) { return; }
            JsonObject response;
            try
            {
                var request = JsonNode.Parse(line)?.AsObject() ?? throw new ArgumentException("Invalid debug request.");
                response = await HandleRequestAsync(request, timeout.Token);
            }
            catch (Exception error) when (error is ArgumentException or InvalidOperationException or IOException or JsonException or NotSupportedException or SunfleckException)
            {
                response = new JsonObject { ["error"] = error.Message };
            }
            await writer.WriteLineAsync(response.ToJsonString().AsMemory(), timeout.Token);
        }
    }

    private async Task<JsonObject> HandleRequestAsync(JsonObject request, CancellationToken cancellationToken)
    {
        string method = request["method"]?.GetValue<string>() ?? throw new ArgumentException("Debug method is required.");
        return method switch
        {
            "hello" => new JsonObject
            {
                ["protocol"] = 1,
                ["session"] = session,
                ["methods"] = new JsonArray("snapshot", "request")
            },
            "snapshot" => new JsonObject { ["result"] = JsonNode.Parse(await SnapshotAsync(cancellationToken)) },
            "request" => new JsonObject { ["result"] = await RequestAsync(request, cancellationToken) },
            _ => throw new NotSupportedException($"Unsupported debug method: {method}")
        };
    }

    private async Task<string> SnapshotAsync(CancellationToken cancellationToken)
    {
        string? snapshot = null;
        await app.InvokeAsync(() => snapshot = app.GetDebugSnapshot(), cancellationToken);
        var result = JsonNode.Parse(snapshot ?? throw new InvalidOperationException("Debug snapshot is unavailable."))!.AsObject();
        result["debugSession"] = session;
        result["processId"] = Environment.ProcessId;
        result["debugProtocol"] = new JsonObject
        {
            ["version"] = 1,
            ["methods"] = new JsonArray("snapshot", "pick", "cancelPick", "highlight")
        };
        return result.ToJsonString();
    }

    private async Task<JsonObject> RequestAsync(JsonObject request, CancellationToken cancellationToken)
    {
        string method = request["request"]?.GetValue<string>() ?? request["command"]?.GetValue<string>() ??
            throw new ArgumentException("Debug request method is required.");
        if (method is not ("pick" or "cancelPick" or "highlight"))
        {
            throw new NotSupportedException($"Unsupported debug request: {method}");
        }
        Result result = Result.Ok;
        ulong objectId = 0;
        if (method == "highlight" && !ulong.TryParse(request["object"]?.GetValue<string>(),
            System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture, out objectId))
        {
            throw new ArgumentException("Invalid highlight object identity.");
        }
        await app.InvokeAsync(() =>
        {
            try {
                if (method == "highlight") { result = Native.DebugHighlight(app.Handle, objectId, 1000); }
                else { app.SetDebugPicking(method == "pick"); }
            }
            catch (SunfleckException error) { result = error.Result; }
        }, cancellationToken);
        return new JsonObject { ["session"] = session, ["result"] = (int)result, ["method"] = method };
    }

    public void Dispose()
    {
        stopping.Cancel();
        // Release the name immediately, including a listener awaiting a client.
        server.Dispose();
    }
}

public sealed partial class App
{
    private DebugServer? debugServer;

    /// <summary>Override or disable the local editor service automatically configured by SUNFLECK_DEBUG_PIPE.</summary>
    public void EnableDebugServer(string? pipeName)
    {
        _ = Handle;
        if (!string.IsNullOrWhiteSpace(pipeName))
        {
            if (pipeName.Any(char.IsWhiteSpace) || pipeName.Contains('\\') || pipeName.Contains('/'))
            {
                throw new ArgumentException("The debug pipe name must be a local name without spaces or path separators.", nameof(pipeName));
            }
            if (debugServer?.PipeName == pipeName) { return; }
            var next = new DebugServer(this, pipeName);
            debugServer?.Dispose();
            debugServer = next;
        }
        else
        {
            debugServer?.Dispose();
            debugServer = null;
        }
    }
}
