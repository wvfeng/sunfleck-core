> 此项目已移入历史调试回归夹具。新项目请使用 [独立 Demos](../../../../../../docs/guides/windows-quickstart.md#独立-demos)。

# 独立 C++＋Fleck 计数器

对应 [C# 计数器](../counter/README.md) 的 C++20 工程，包含同步／异步计数、页面保存刷新、模型补全与源码定位、Named Pipe 检查器和 Windows F5 调试。

| 文件 | 职责 |
| --- | --- |
| [main.cpp](main.cpp) | 创建 App、模型和页面，驱动事件循环 |
| [CounterModel.hpp](CounterModel.hpp) | 普通字段、属性 getter、同步／异步业务方法和变更通知 |
| [main.fleck](main.fleck) | 与 C# 版对应的页面和绑定 |
| [FleckFile.hpp](FleckFile.hpp) | 加载 XML、登记源码位置、检测文件变化并排队刷新 |
| [CMakeLists.txt](CMakeLists.txt) | 从 Fleck 自动生成 C++ 绑定，独立构建应用、模型元数据和定向测试 |

## Windows 启动

依赖 Visual Studio 的“使用 C++ 的桌面开发”、CMake 3.25+、Ninja、PowerShell 7。VS Code F5 需要 Microsoft C/C++ 扩展（`ms-vscode.cpptools`）；Fleck 编辑与检查器由仓库内 Sunfleck 扩展提供，语言服务需要 .NET SDK。直接构建和运行应用不依赖 .NET。

从仓库根目录运行：

```powershell
# 自动导入 MSVC 环境、构建并打开仅含本项目的 VS Code 窗口
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter-cpp/start.ps1

# 只构建／直接运行／运行定向测试
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter-cpp/start.ps1 -Action Build
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter-cpp/start.ps1 -Action Run
pwsh -NoProfile -File tests/development/fixtures/legacy-demos/counter-cpp/start.ps1 -Action Test
```

打开后按 **F5** 启动“C++ 简单计数器”。构建任务生成 `bin/Debug/CounterCpp.exe` 与 `CounterCpp.fleckmodel.json`；在 `CounterModel::Increment` 或 `IncrementAsync` 设置断点。该项目使用 `sunfleck-counter-cpp` Pipe，与 C# 计数器的 `sunfleck-counter` 分开。断点暂停时检查器保留最近快照，继续后恢复更新。

使用 `CounterCpp.code-workspace` 打开项目时，Sunfleck 扩展 0.2.2 已修复文件夹 Pipe 配置读取，启动后会自动连接对象树。升级扩展后，对已有窗口执行一次 `Developer: Reload Window`，再按 F5。实际自动连接、源码导航及重新连接验收见[开发工具测试说明](../../../../../../devtools/tests/development/README.md)。

## 页面与业务行为

`main.fleck` 是绑定清单：`Binding Caption` 自动适配同名字段，`SyncCount`／`AsyncCount` 自动适配同名 getter，`Increment`／`IncrementAsync` 自动创建命令，执行方式取自 `CommandMode`。CMake 调用 `fleckc --format cpp` 生成 `bin/Debug/counter_bindings.hpp`，应用通过 `sunfleck::generated::counter_cpp::bind(app, counter)` 一次创建并持有绑定。业务模型无需逐项注册、创建命令句柄或编写 C 回调；异常和命令析构由框架处理。

已有绑定的页面布局、文本和格式修改仍可保存刷新；新增绑定名称或修改命令模式时，重新构建会生成对应适配代码。属性值变化通过 `PropertyChangedEvent` 通知，普通字段赋值本身不会自动产生通知。

- 同步按钮在 UI 线程等待 100 毫秒后递增，与当前 C# 版行为一致。
- 异步按钮在后台等待 100 毫秒，再通过 `app.invokeAsync(..., token).get()` 在 UI 线程修改原模型和通知属性；重复点击同一运行中命令不会并发执行。
- 保存 `main.fleck` 后，应用在事件循环中检测变化并请求事务刷新；解析／绑定错误保留原页面，修复保存后继续刷新。异步执行或 IME 组合期间等待，刷新保留模型计数。
- 页面刷新沿用当前框架行为，保留已有窗口标题；修改标题后重启应用生效。
- 修改 C++ 代码后重新构建并重启调试；页面保存刷新不需要重建应用。关闭应用时取消并回收异步命令。

普通运行仅在配置 `SUNFLECK_DEBUG_PIPE` 时启用原生调试服务；启动脚本和 F5 已配置。运行时加载原始 XML 并直接登记路径，构建阶段仅生成绑定头，无需生成 C 页面或 sidecar。`--export` 使用同一份生成绑定导出元数据，不加载页面、创建窗口或读取业务 getter；模型元数据定位到 Fleck 绑定声明。

## 独立 CMake 构建

在已配置编译器的终端中，可以只打开本目录：

```powershell
cd tests/development/fixtures/legacy-demos/counter-cpp
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./bin/Debug/CounterCpp.exe main.fleck
```

默认从 `../..` 引入 Sunfleck 源码，只构建本应用需要的库，不启用框架示例、C# 绑定或其他 demo。项目复制到仓库外时通过 `-DSUNFLECK_SOURCE_DIR=<Sunfleck源码绝对路径>` 指定依赖；VS Code 中的语言服务路径也需相应调整。仓库根构建启用 `UI_BUILD_DEMOS` 时同样包含 `CounterCpp`，测试名为 `counter_cpp_demo`。

输出均在忽略的 `bin/` 下。独立预设使用软件渲染和 Debug 符号；[tests.cpp](tests.cpp) 覆盖真实绑定调用、同步／异步、重复点击、文件刷新、错误回滚、状态保留及运行中关闭。

启动脚本优先使用所选 Visual Studio 自带的 Windows 原生 CMake／Ninja／CTest，并打印实际 CMake 路径。本机 PATH 中的 Strawberry/MinGW CMake 在 VS Code 任务终端处理中文 `/showIncludes` 前缀时会抛出 `basic_filebuf::_M_convert_to_external conversion error`。脚本同时在配置与构建期间统一控制台输出编码为 UTF-8，结束后恢复，确保依赖探测和实际编译使用相同编码；不修改系统或 IDE 显示语言。

2026-11-16 本机验证：独立构建和仓库根构建的 `counter_cpp_demo` 均通过；实际窗口通过 Named Pipe 连接，保存按钮文本后原进程、原会话更新通过。补齐 Microsoft C/C++ 1.35.6 并修复 CMake 工具选择及控制台编码后，真实 VS Code 原有任务从干净输出构建、`cppvsdbg` 启动、`main.cpp` 第 19 行源码断点、继续后 Pipe 连接、停止及再次启动新会话全部通过。证据位于 `out/windows-dev/counter-f5-host/debug-result.json`；同目录 `header-dependencies.log` 确认入口文件的 5 个头文件依赖正常，`final-ctest.log` 记录最终定向测试通过。
