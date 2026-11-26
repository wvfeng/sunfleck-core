using Sunfleck;

using var app = new App(Renderer.Software, headless: true);
app.LoadFleck("<Window xmlns='urn:sunfleck:v1'><TextBox Id='draft'/></Window>");
var previous = app.FindFleckNode("draft");
previous.Text = "Application-owned draft";
app.OpenWindow();
string? captured = null;
int restored = 0;
app.SetFleckRefreshStateCallback((phase, result) =>
{
    if (phase == FleckRefreshPhase.Capture)
    {
        captured = app.FindFleckNode("draft").Text;
    }
    else
    {
        if (result == Result.Ok)
        {
            // A new node has the same Fleck ID. Resolve it after successful commit.
            app.FindFleckNode("draft").Text = captured!;
            restored++;
        }
        captured = null;
    }
    return Result.Ok;
});
app.QueueFleckRefresh("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='draft'/></Window>");
app.Poll();
if (app.FindFleckNode("draft").Text != "Application-owned draft" || restored != 1)
{
    return 1;
}
app.SetFleckRefreshStateCallback(null);
Console.WriteLine("C# restored application state onto the replacement node");
return 0;
