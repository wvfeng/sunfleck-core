using System.Runtime.InteropServices;
using System.Runtime.InteropServices.JavaScript;
using System.Runtime.Versioning;
using System.Text.Json;
using System.Text.Json.Serialization;
using Sunfleck;

return;

[SupportedOSPlatform("browser")]
public static partial class BrowserApp
{
    private static App? app;
    private static HostedSurface? surface;
    private static TextSource? text;
    private static int count;

    [DllImport("sunfleck_native", EntryPoint = "ui_host_set_font", CallingConvention = CallingConvention.Cdecl)]
    private static extern unsafe Result SetFont(byte* data, nuint size);

    [JSExport]
    public static void Start(byte[] font, string page, string baseUrl)
    {
        if (app is not null) { throw new InvalidOperationException("Already started."); }
        unsafe
        {
            fixed (byte* data = font)
            {
                var result = SetFont(data, (nuint)font.Length);
                if (result != Result.Ok) { throw new SunfleckException(result); }
            }
        }
        app = new App(Renderer.Software, headless: true);
        try
        {
            count = 0;
            text = app.CreateTextSource("Count: 0");
            var draft = app.CreateTextSource("Edit me");
            var reset = app.CreateCommand(_ => draft.Set("Edit me"));
            var increment = app.CreateCommand(_ => Increment());
            var delayed = app.CreateAsyncCommand(async token =>
            {
                await Task.Delay(500, token);
                await app.InvokeAsync(Increment, token);
            });
            var networkStatus = app.CreateTextSource("Network: idle");
            var owner = app;
            var network = app.CreateAsyncCommand(async token => {
                networkStatus.Set("Network: loading");
                try {
                    using var client = new HttpClient { BaseAddress = new Uri(baseUrl), Timeout = TimeSpan.FromSeconds(2) };
                    await client.GetStringAsync("message.txt", token);
                    await owner.InvokeAsync(() => networkStatus.Set("Network: loaded"), token);
                } catch (OperationCanceledException) when (token.IsCancellationRequested) { }
                catch (OperationCanceledException) { await owner.InvokeAsync(() => networkStatus.Set("Network: timeout"), token); }
                catch (HttpRequestException) { await owner.InvokeAsync(() => networkStatus.Set("Network: failed"), token); }
            });
            var cancelNetwork = app.CreateCommand(_ => { network.Cancel(); networkStatus.Set("Network: cancelled"); });
            var context = new FleckDataContext().AddText("Counter", text).AddText("Draft", draft)
                .AddCommand("Increment", increment).AddCommand("Delayed", delayed).AddCommand("Reset", reset)
                .AddText("NetworkStatus", networkStatus).AddCommand("Network", network).AddCommand("CancelNetwork", cancelNetwork);
            app.LoadFleck(string.IsNullOrEmpty(page) ? """
                <Window xmlns="urn:sunfleck:v1" Title="Sunfleck .NET Web" Width="560" Height="560">
                  <StackPanel Spacing="12" Padding="16">
                    <TextBlock Text="Sunfleck / .NET WebAssembly"/>
                    <TextBlock Text="{Binding Counter}"/>
                    <Button Text="Increment" Command="{Binding Increment}"/>
                    <Button Text="Increment after 500 ms" Command="{Binding Delayed}" CommandMode="Async"/>
                    <TextBox Id="editor" Text="{Binding Draft, Mode=TwoWay}"/>
                    <TextArea Id="notes" Text="Multiline notes"/>
                    <PasswordBox Id="password"/>
                    <TextBlock Text="{Binding Draft}"/>
                    <Button Text="Reset text" Command="{Binding Reset}"/>
                  </StackPanel>
                </Window>
                """ : page, context);
            if (!string.IsNullOrEmpty(page)) {
                var dock = app.CreateDock(app.FindFleckNode("workspace"));
                app.RegisterDockSkin("web-fallback", (_, _) => false);
                app.ApplyTheme(BuiltinTheme.SunfleckLite, skin: "web-fallback");
                app.FindFleckNode("float").SetAction(_ => dock.Find("document").Float(new DockRectangle(150, 80, 400, 320)));
                app.FindFleckNode("resetDock").SetAction(_ => dock.Reset());
                var model = app.CreatePropertyModel(new SearchModel());
                model.Bind(app.FindFleckNode("search"), PropertyTarget.Text, "Search", PropertyBindingMode.TwoWay);
                dock.Event += (_, _) => { }; // Exercise static reverse P/Invoke events in the browser.
            }
            app.OpenWindow();
            surface = new HostedSurface(app);
        }
        catch { app.Dispose(); app = null; throw; }
    }

    private static void Increment() { text!.Set($"Count: {++count}"); }
    [JSExport] public static int Count() => count;
    [JSExport] public static void VerifyContracts() {
        var rejected = 0;
        try { app!.Run(); } catch (PlatformNotSupportedException) { ++rejected; }
        try { app!.Poll(1); } catch (SunfleckException error) when (error.Result == Result.Unsupported) { ++rejected; }
        try { app!.CreateWindow(); } catch (SunfleckException error) when (error.Result == Result.Unsupported) { ++rejected; }
        if (rejected != 3 || (int)HostedSurface.Capabilities != 63) { throw new InvalidOperationException("Hosted contracts failed."); }
        GC.Collect();
    }
    [JSExport] public static int SelectView(int id) { surface!.SelectView((uint)id); return 0; }
    [JSExport] public static string Snapshot() => surface!.Snapshot();
    [JSExport] public static int SemanticAction(string id, int action) { surface!.SemanticAction(ulong.Parse(id), action != 0); return 0; }
    [JSExport] public static int Tick() => (int)app!.Poll(0);
    [JSExport] public static int SetRenderMode(int mode) { surface!.SetCommandRendering(mode != 0); return 0; }
    [JSExport] public static byte[] DrawCommands() => surface!.CopyDrawCommands();
    [JSExport] public static byte[] Pixels() => surface!.CopyPixels();
    [JSExport] public static int Resize(int width, int height, int dpi) { surface!.Resize((uint)width, (uint)height, (uint)dpi); return 0; }
    [JSExport] public static int Pointer(int action, int x, int y) { surface!.Pointer((PointerAction)action, x, y); return 0; }
    [JSExport] public static int Key(int key, int shift) { surface!.Key((HostedKey)key, shift != 0); return 0; }
    [JSExport] public static int Focus(int focused) { surface!.Focus(focused != 0); return 0; }
    [JSExport] public static int Scroll(int x, int y, int dy) { surface!.Scroll(x, y, dy); return 0; }
    [JSExport] public static int ContextMenu(int x, int y) { surface!.ContextMenu(x, y); return 0; }
    [JSExport] public static string ReadText() => JsonSerializer.Serialize(surface!.ReadText(), WebJson.Default.HostedTextState);
    [JSExport] public static int UpdateText(int session, int revision, string value, int first, int last, int backward)
        => (int)surface!.UpdateText((uint)session, (uint)revision, value, (uint)first, (uint)last, backward != 0);
    [JSExport] public static void Stop() { app?.Dispose(); app = null; surface = null; text = null; }
    [JSExport] public static int PreviewText(int session, int revision, string json)
    {
        var draft = JsonSerializer.Deserialize(json, WebJson.Default.CompositionDraft)!;
        return (int)surface!.PreviewText((uint)session, (uint)revision, draft.Text, draft.First, draft.Last, draft.Backward, draft.CompositionFirst, draft.CompositionLast);
    }
    [JSExport] public static int HitText(int session, int revision, int x, int y)
    {
        var result = surface!.HitText((uint)session, (uint)revision, x, y, out var offset);
        return result == Result.Ok ? checked((int)offset) : -(int)result;
    }
    [JSExport] public static int ScrollText(int session, int revision, int dx, int dy)
        => (int)surface!.ScrollText((uint)session, (uint)revision, dx, dy);
}

[JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase)]
[JsonSerializable(typeof(HostedTextState))]
[JsonSerializable(typeof(CompositionDraft))]
internal partial class WebJson : JsonSerializerContext { }

internal sealed record CompositionDraft(string Text, uint First, uint Last, bool Backward, uint CompositionFirst, uint CompositionLast);
public sealed class SearchModel { public string Search { get; set; } = "Search here"; }
