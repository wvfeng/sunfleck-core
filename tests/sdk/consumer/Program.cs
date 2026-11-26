using Sunfleck;
using var app = new App(Renderer.Software, headless: true);
using var rows = app.CreatePropertyModel();
var labels = new System.Collections.ObjectModel.ObservableCollection<string> { "first", "second" };
rows.ObserveTextCollection(labels);
using var grouped = app.CreatePropertyModel();
grouped.SetCollectionView(rows);
grouped.SetGroupByLabel(true);
if (grouped.GroupCount != 2 || grouped.GetGroup(0).Key != "first") throw new Exception("Installed group projection failed");
var second = rows.GetItem(1);
labels.Move(1, 0);
if (rows.ItemCount != 2 || rows.GetItem(0) != second) throw new Exception("Installed collection binding failed");
app.LoadFleck("<Window xmlns='urn:sunfleck:v1'><TextBlock Id='hello' Text='Installed C# SDK'/></Window>");
app.OpenWindow();
app.Poll(0);
if (app.FindFleckNode("hello").Text != "Installed C# SDK") throw new Exception("Installed binding failed");
var border = new Sunfleck.Elements.Border(app) { Background = new Color(0xff123456), Visibility = Visibility.Hidden, IsHitTestVisible = false, BorderThickness = new Thickness(1,2,3,4), CornerRadius = new CornerRadius(4,5,6,7) };
border.TabIndex = 7; border.IsTabStop = false;
if (border.TabIndex != 7 || border.IsTabStop) throw new Exception("Installed tab navigation properties failed");
if (border.Visibility != Visibility.Hidden || border.IsHitTestVisible || border.Background.Argb != 0xff123456 || border.BorderThickness.Bottom != 4 || border.CornerRadius.BottomRight != 6) throw new Exception("Installed Border API failed");
var font = new Sunfleck.Elements.TextBlock(app) { FontFamily = "Consolas", FontSize = 23.5, FontWeight = FontWeight.Bold, FontStyle = FontStyle.Italic, FontStretch = FontStretch.Condensed };
if (font.FontFamily != "Consolas" || font.FontSize != 23.5 || font.FontWeight != FontWeight.Bold || font.FontStyle != FontStyle.Italic || font.FontStretch != FontStretch.Condensed) throw new Exception("Installed font API failed");
font.Foreground = new Color(0x80224466);
font.TextAlignment = TextAlignment.Center;
font.TextWrapping = TextWrapping.NoWrap;
font.TextTrimming = TextTrimming.WordEllipsis;
font.LineHeight = 32;
font.TextDecorations = TextDecorations.Underline | TextDecorations.Strikethrough;
font.FlowDirection = FlowDirection.RightToLeft;
if (font.Foreground.Argb != 0x80224466 || font.LineHeight != 32 || font.FlowDirection != FlowDirection.RightToLeft || font.TextDecorations != (TextDecorations)3) throw new Exception("Installed text appearance API failed");
var node = app.FindFleckNode("hello");
node.SetResourceReference(PropertyTarget.Opacity,"Fade");
node.SetResource("Fade", .75);
if ((double)node.GetProperty(PropertyTarget.Opacity)! != .75) throw new Exception("Installed dynamic resource failed");
node.ClearResource("Fade");
node.RegisterVisualState("Input","Normal");
node.GoToVisualState("Input","Normal");
node.StartAnimation(PropertyTarget.Opacity, new Timeline([new KeyFrame(0, 0.0), new KeyFrame(1, 1.0)], 200));
node.ControlAnimation(PropertyTarget.Opacity, AnimationAction.Seek, 100);
if ((double)node.GetProperty(PropertyTarget.Opacity)! != 0.5) throw new Exception("Installed timeline interpolation failed");
node.ControlAnimation(PropertyTarget.Opacity, AnimationAction.Stop);
node.Width = 120.5;
var registeredWidth = node.ResolveProperty("Width");
if (node.DescribeProperty(registeredWidth).Type != PropertyValueType.Length || (Length)node.GetProperty(registeredWidth)! != new Length(120.5))
    throw new Exception("Installed registered property failed");
if (node.GetPropertySource(PropertyTarget.Width) != PropertyValueSource.Local) throw new Exception("Property source failed");
node.ClearProperty(PropertyTarget.Width);
if ((Length)node.GetProperty(PropertyTarget.Width)! != Length.Auto) throw new Exception("Property clear failed");
if (BuiltInPropertyDescriptor.Describe(NodeKind.TextBlock, PropertyTarget.Width).Type != PropertyValueType.Length) throw new Exception("Property descriptor failed");
if (PropertyValues.Parse<Point>("3.5,-2") != new Point(3.5, -2)) throw new Exception("Typed literal failed");
node.SetPropertyLayer(PropertyTarget.Width, ValuePrecedence.Style, 100);
node.SetPropertyLayer(PropertyTarget.Width, ValuePrecedence.Animation, 150);
if (node.Width != 150 || (Length)node.GetBaseProperty(PropertyTarget.Width)! != new Length(100)) throw new Exception("Layer precedence failed");
node.ClearPropertyLayer(PropertyTarget.Width, ValuePrecedence.Animation);
if (node.Width != 100 || node.GetPropertySource(PropertyTarget.Width) != PropertyValueSource.Style) throw new Exception("Layer clear failed");
Console.WriteLine("Installed C# managed/native SDK passed.");
using var contextModel = app.CreatePropertyModel().Add("Caption", () => "Context from installed SDK");
node.DataContext = contextModel;
node.BindContext(PropertyTarget.Text, "Caption", PropertyBindingMode.OneWay);
if (node.Text != "Context from installed SDK" || node.DataContext != contextModel) throw new Exception("DataContext binding failed");
node.DataContext = null;
if (node.Text != "" || node.DataContextSource != PropertyValueSource.Local) throw new Exception("Explicit null context failed");
node.ClearDataContext();
using var contextSource = app.CreatePropertyModel().Add("Selected", () => contextModel);
var parent = app.CreateNode(NodeKind.StackPanel);
var typed = new Sunfleck.Elements.TextBlock(app);
parent.Append(typed.Node);
parent.DataContext = contextSource;
typed.Node.BindContext(PropertyTarget.DataContext, "Selected");
typed.Node.BindContext(typed.Node.ResolveProperty("Text"), "Caption");
if (typed.Text != "Context from installed SDK" || typed.DataContext != contextModel ||
    typed.GetValueSource(Sunfleck.Elements.TextBlock.DataContextProperty) != PropertyValueSource.Binding) throw new Exception("Installed typed DataContext failed");
var panel = new Sunfleck.Elements.StackPanel(parent) { Spacing = 7.25, Padding = 4.5 };
if (panel.Spacing != 7.25 || panel.Padding != 4.5) throw new Exception("Installed metric properties failed");
parent.SetLayout(1.25, 2.75);
if (panel.Spacing != 1.25 || panel.Padding != 2.75) throw new Exception("Installed metric ABI failed");

app.RegisterComponent<InheritedScope>("urn:installed", "Scope", [new("Level", typeof(int), 7,
    Effects: PropertyEffects.Bindable | PropertyEffects.Inherits)], template: "ScopeTemplate");
app.RegisterFleckResources([new("scope.fleck", "<ControlTemplate xmlns='urn:sunfleck:v1' xmlns:c='urn:installed' Class='ScopeTemplate' Namespace='urn:installed' TargetType='c:Scope'><StackPanel Id='children'/></ControlTemplate>")]);
var inheritedParent = app.CreateComponent("urn:installed", "Scope");
var inheritedChild = app.CreateComponent("urn:installed", "Scope");
inheritedParent.Set("Level", 12);
inheritedParent.FindNode("children").Append(inheritedChild.Node);
if (inheritedChild is not InheritedScope || inheritedChild.Get<int>("Level") != 12 ||
    inheritedChild.GetValueSource("Level") != PropertyValueSource.Inherited) throw new Exception("Installed component inheritance failed");
inheritedChild.SetLayer("Level", ValuePrecedence.Style, 20);
inheritedParent.Set("Level", 13);
inheritedChild.ClearLayer("Level", ValuePrecedence.Style);
if (inheritedChild.Get<int>("Level") != 13) throw new Exception("Installed inherited fallback failed");
var shape = app.CreateNode(NodeKind.Path);
shape.SetPathData("M0,0 L10,0 L0,10 Z");
shape.SetBrush(BrushTarget.Background, new Brush(BrushKind.Linear) {
    Stops = [new(0,new Color(0xff005577)),new(1,new Color(0xff886633))] });
shape.SetEffect(new Effect(EffectKind.Blur, 2, default, default));
var drawingNode = new Sunfleck.Elements.Path(shape) { LayoutTransform = new Matrix(2,0,0,2,0,0) };
if (drawingNode.LayoutTransform.M11 != 2) throw new Exception("Installed drawing property failed");
using (var navigation = new App(Renderer.Software,headless:true)) {
    navigation.LoadFleck("<Window xmlns='urn:sunfleck:v1'><StackPanel Id='scope' IsFocusScope='true'><Button Id='target' Content='Press'/></StackPanel></Window>");
    navigation.OpenWindow(); navigation.Poll(0);
    var scope=navigation.FindFleckNode("scope");
    var target=navigation.FindFleckNode("target");
    target.Focus();
    scope.SetInputBinding(FunctionKey.F5, KeyModifiers.None, "Refresh");
    scope.SetInputBinding(FunctionKey.F5, KeyModifiers.None, null);
    if (scope.LogicalFocus?.DebugId != target.DebugId) throw new Exception("Installed focus scope failed");
    scope.SetInputBinding(FunctionKey.F6, KeyModifiers.None, EventPhase.Preview, "Refresh");
    scope.SetInputBinding(FunctionKey.F6, KeyModifiers.None, EventPhase.Preview, null);
    scope.Focus();
    scope.SetPointerHandler(PointerAction.Down,EventPhase.Preview,(sender,source,x,y) => true);
    scope.SetPointerHandler(PointerAction.Down,EventPhase.Preview,null);
}

using (var ranges = new App(Renderer.Software,headless:true)) {
    const string face = "<ControlTemplate xmlns='urn:sunfleck:v1' Class='RangeFace' Namespace='urn:sdk' TargetType='Slider'><TextBlock Id='readout' Text='{TemplateBinding Value}'/></ControlTemplate>";
    ranges.RegisterFleckResources([new("range.fleck",face)]);
    int score = 27;
    using var model = ranges.CreatePropertyModel().Add("Score", () => score, value => score = value);
    ranges.LoadFleck("<Window xmlns='urn:sunfleck:v1' xmlns:s='urn:sdk'><Slider Id='range' Template='s:RangeFace' Value='{Binding Score, Mode=TwoWay}'/></Window>", model);
    var range = (Control)ranges.FindComponent("range");
    if (range.FindNode("readout").Text != "27") throw new Exception("Installed slider template lost the initial model value");
    score = 79;
    model.NotifyPropertyChanged("Score");
    range.ApplyTemplate("urn:sdk","RangeFace");
    if (range.FindNode("readout").Text != "79") throw new Exception("Installed slider template lost the updated model value");
}

using (var validation = new App(Renderer.Software,headless:true)) {
    int score = 25;
    using var model = validation.CreatePropertyModel().Add("Score", () => score);
    validation.LoadFleck("""
        <Window xmlns="urn:sunfleck:v1"><ProgressBar Id="progress" Value="{Binding Score}">
          <ProgressBar.VisualStateGroups><VisualStateGroup Name="ValidationStates">
            <VisualState Name="InvalidUnfocused"><Storyboard><Timeline Property="Opacity" DurationMs="100" HoldEnd="true">
              <KeyFrame Position="0" Value="0.4"/><KeyFrame Position="1" Value="0.4"/>
            </Timeline></Storyboard></VisualState>
          </VisualStateGroup></ProgressBar.VisualStateGroups>
        </ProgressBar></Window>
        """, model);
    validation.OpenWindow();
    validation.Poll(0);
    var progress = validation.FindFleckNode("progress");
    score = -1;
    try {
        model.NotifyPropertyChanged("Score");
        throw new Exception("Invalid progress value was accepted");
    } catch (SunfleckException) { }
    validation.Poll(0);
    if ((double)progress.GetProperty(PropertyTarget.Opacity)! != 0.4 || progress.Value != 25)
        throw new Exception("Rejected model value did not preserve progress and show the error state");
    score = 55;
    model.NotifyPropertyChanged("Score");
    validation.Poll(0);
    if ((double)progress.GetProperty(PropertyTarget.Opacity)! != 1 || progress.Value != 55)
        throw new Exception("Corrected model value did not restore the valid appearance");
}

public sealed class InheritedScope : Control { }
