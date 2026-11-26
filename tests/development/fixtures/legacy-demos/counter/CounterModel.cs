using System.ComponentModel;
using Sunfleck;

internal sealed class CounterModel : INotifyPropertyChanged, IDisposable
{
    public event PropertyChangedEventHandler? PropertyChanged;
    public string Caption => "同步立即完成；异步等待 100 毫秒，期间界面仍可操作。";
    public int SyncCount { get; set; }
    public int AsyncCount { get; set; }
    public Command Increment { get; set; } = null!;
    [CommandModes(CommandExecutionModes.Async)]
    public Command IncrementAsync { get; set; } = null!;

    public CounterModel(App app)
    {
        Increment = app.CreateCommand(_ =>
        {
            Thread.Sleep(100);
            SyncCount++;

            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SyncCount)));
        });

        IncrementAsync = app.CreateAsyncCommand(async cancellationToken =>
        {
            await Task.Delay(100, cancellationToken);
            await app.InvokeAsync(() =>
            {
                AsyncCount++;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(AsyncCount)));
            });
        });

    }

    public void Dispose()
    {
        Increment?.Dispose();
        IncrementAsync?.Dispose();
    }
}
