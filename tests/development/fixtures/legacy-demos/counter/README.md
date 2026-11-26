> 此项目已移入历史调试回归夹具。新项目请使用 [独立 Demos](../../../../../../docs/guides/windows-quickstart.md#独立-demos)。

# 最小 C#＋Fleck 计数器

对应的独立 C++ 工程见 [counter-cpp](../counter-cpp/README.md)。

日常只需看三个文件：

| 文件 | 职责 |
| --- | --- |
| [Program.cs](Program.cs) | 创建 App、注册模型并运行页面 |
| [CounterModel.cs](CounterModel.cs) | 在构造函数创建同步／异步命令，用 `INotifyPropertyChanged` 通知绑定 |
| [main.fleck](main.fleck) | 声明页面、文本和两个按钮的绑定 |

在仓库根目录执行，打开仅包含本案例的 VS Code 窗口后按 **F5**：

```powershell
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter/start.ps1
```

依赖、格式化、对象树、保存页面更新和 C# Watch 的完整操作见 [Windows 快速开始](../../../../../../docs/guides/windows-quickstart.md)。F5 启动“简单计数器”，框架自动建立 Named Pipe 调试服务，应用代码无需调试接入语句。F5 配置与 Run／Watch 脚本传入 `SUNFLECK_DEBUG_PIPE`；未设置时，普通应用不会启动调试服务。

- **同步 +1**：在 UI 线程执行。目前示例包含 100 毫秒同步等待，然后递增并通知属性。
- **异步 +1**：后台等待 100 毫秒，通过 UI 投递递增原模型并通知属性。同一个异步命令不重复并发执行。

异步命令在模型构造函数中使用以下形式：

```csharp
IncrementAsync = app.CreateAsyncCommand(async cancellationToken =>
{
    await Task.Delay(100, cancellationToken);
    await app.InvokeAsync(() =>
    {
        AsyncCount++;
        PropertyChanged?.Invoke(this,
            new PropertyChangedEventArgs(nameof(AsyncCount)));
    });
});
```

不创建模型快照，也不需要 Copy／Apply。`await` 后不自动回到 UI 线程；访问 UI 或修改绑定模型时使用 `InvokeAsync`，它继承当前命令的取消信号。关闭应用会请求取消，实际 I/O 应传入令牌或主动检查取消。跨语言契约见 [异步命令](../../../../../../docs/guides/bindings.md#async-命令与中途通知)。

`App.RunFleckFile` 负责文件监听和事件循环；保存 `main.fleck` 更新同一窗口，无效页面保留上一次成功界面。相对路径基于工作目录，工作区和启动脚本已配置为本案例目录。
