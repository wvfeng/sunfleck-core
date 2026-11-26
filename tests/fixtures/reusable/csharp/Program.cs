using System.ComponentModel;
using System.Reflection;
using Sunfleck;
using Sunfleck.Generated;
using Example;

bool test = args.Contains("--test");
using var app = new App(Renderer.Software, headless: test);
app.RegisterFleckComponents();
var model = new MainModel();
using var properties = app.CreatePropertyModel(model);
using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Main.fleck")!;
using var reader = new StreamReader(stream);
app.LoadFleck(reader.ReadToEnd(), properties);
// Embedded XML has no file identity. Attach build-time metadata for source navigation.
app.SetFleckSourceUri(MainFleckSourceUri());
var rating = (Rating)app.FindComponent("rating");
rating.ApplyTemplate("urn:example:controls", "CompactRatingTemplate");
rating.Value = 70;
if (model.Score != 70 || rating.FindNode("display").Text != "70") { throw new Exception("Component binding failed."); }
if (((UserCard)app.FindComponent("primary")).FindNode("name").Text != "Alice" || ((UserCard)app.FindComponent("secondary")).FindNode("name").Text != "Bob") { throw new Exception("Component scopes are not independent."); }
app.OpenWindow();
if (test) { app.RequestClose(); } else { app.Run(); }

static string MainFleckSourceUri([System.Runtime.CompilerServices.CallerFilePath] string sourceFile = "") =>
    new Uri(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(sourceFile)!, "..", "main.fleck"))).AbsoluteUri;

public sealed class User(string name) : INotifyPropertyChanged
{
    public string Name { get; set; } = name;
    public event PropertyChangedEventHandler? PropertyChanged;
    public void Rename(string name) { Name = name; PropertyChanged?.Invoke(this, new(nameof(Name))); }
}
public sealed class MainModel : INotifyPropertyChanged
{
    public User PrimaryUser { get; } = new("Alice");
    public User SecondaryUser { get; } = new("Bob");
    private int score = 40;
    public int Score { get => score; set { score = value; PropertyChanged?.Invoke(this, new(nameof(Score))); } }
    public event PropertyChangedEventHandler? PropertyChanged;
}
