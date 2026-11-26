using Sunfleck;

using var app = new App();
var counter = new CounterModel(app);
using var model = app.CreatePropertyModel(counter);

app.RunFleckFile("main.fleck", model);
