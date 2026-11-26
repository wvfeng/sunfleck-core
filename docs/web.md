# Web 接入：当前桌面浏览器环境

本页维护 Web 的接入契约、构建和测试方法。双宿主、DOM 文本编辑、页面内 Dock、通用滚动和基础语义已接入。默认优先使用 WebGL2 命令批处理，无法创建 WebGL2 上下文时回退 Canvas 2D。浏览器宿主负责主视图和浮动视图的调度。Windows Edge 和 Chrome 有完整回归记录，macOS Safari 27.2 有定向验收记录。[Windows 浏览器验收](validation.md#m9-linuxweb-平台专项) · [Safari 验收](validation.md#macos-safari-交互)

逐项接口支持情况见 [Web API 清单](../../docs/guides/web-api-support.md)。生成独立 C++/.NET 应用、字体报告、资源预算和冻结候选包见 [Web 应用交付](web-delivery.md)。浏览器版本、自动化结果及真实系统输入法、读屏和拖放的复核记录见 [验证与复现](validation.md#m9-linuxweb-平台专项)。

## 实现边界

共享 C++ 核心继续负责 Fleck、布局、绑定、控件行为和绘制命令生成。`web/host.js` 负责浏览器事件、Canvas、尺寸/DPR、帧调度和关闭；它同时供独立 C++ Wasm 和 .NET Wasm 示例使用。

宿主可以提供 `backend.fileDrop({ files, viewId, x, y })` 回调，接收拖到画布或活动 DOM 编辑代理的浏览器 `File` 对象，坐标以设备像素表示。宿主也可以提供 `backend.fileDropTarget` 元素作为独立投放区，投放区事件的 `viewId` 为 `0`，`x` 和 `y` 为 `null`。示例在画布上方的虚线框中显示文件名和字节数。纯文本拖到启用的编辑框时，即使编辑框尚未聚焦，宿主也会聚焦该编辑框，并在投放位置插入文字。

```text
浏览器事件 / requestAnimationFrame
                 │
             web/host.js
                 │
       ┌─────────┴───────────┐
       │                     │
 Emscripten JS 导出     .NET JSExport
       │                     │
 web/main.cpp         C# App / HostedSurface
       │                     │ P/Invoke
       └─────────┬───────────┘
                 │
 C ABI：ui_host_* / ui_app_poll(0)
                 │
        共享布局、绑定、矩形绘制命令
                 │
     web/renderer.js → WebGL2 批量合成
      或 CPU raster → Canvas ImageData
```

两个入口是两种独立构建和运行方式。.NET 入口将核心静态链接进自己的原生 Wasm 运行环境，不需要再加载独立 C++ 示例的 Wasm。

已加入的接口：

- `ui_host_resize/pointer/key_event/focus/scroll/context_menu`：设备像素输入、滚动、尺寸/DPI、捕获取消和基础键盘导航；要求选中的 Headless 视图已打开。
- `ui_host_select_view/pixels/snapshot/semantic_action/capabilities`：选择主视图或 Dock 视图、读取像素和基础语义、执行可访问性操作及查询能力。宿主调度整个 App，逐视图输入和绘制，不创建浏览器弹窗。
- `ui_host_set_font`：为当前线程的 FreeType 后端复制并注册字体内存，在创建 App 前调用。其他文字后端返回 `UNSUPPORTED`。
- `ui_command_create_external`：主线程开始操作，异步完成后交回完成令牌，下一次 `Poll` 统一处理完成。
- C# `HostedSurface` 以及浏览器专用的 `CreateAsyncCommand` 执行器。
- `ui_host_text_read/update`：带会话、版本、UTF-16 选区和设备像素几何的文本编辑协议。
- `ui_host_text_preview/hit_test/scroll`：独立组合输入草稿、核心字符命中和编辑区域滚动；与提交接口使用相同会话及版本校验。

C# 当前浏览器验证范围包括 `App`、`TextSource`、`FleckDataContext`、同步/异步 Command、`InvokeAsync`、`HostedSurface`、节点 Action、属性模型 getter/setter、Dock 事件和自定义皮肤回调。原生反向回调使用顶层类型上的 `UnmanagedCallersOnly` 静态入口及 GCHandle 上下文；普通闭包由上下文保活。裁剪发布使用 `CreatePropertyModel<T>` 的保留标注或显式 `Add<T>` 注册。此范围不等于全部桌面 C# API 对等支持：独立原生窗口和桌面专用诊断接口仍受浏览器宿主限制。

`app.cpp` 目前仍保存主要窗口和输入状态。`web/host.js` 分离浏览器平台和调度职责；`src/input/hosted_text.*` 提取编辑会话、版本和 UTF-8/UTF-16/平台 wchar_t 转换；`src/input/hosted_window.h` 管理浮窗正常尺寸、最小化高度、拖动起点及 DPI 缩放。全面提取共享状态、输入分发与平台实现仍未完成。

## Web 检查器

Wasm SDK 提供对象树、属性快照、高亮和拾取。Core 使用实际绘制顺序、裁剪和变换命中画布元素。共享宿主处理浏览器事件和本机 HTTP 通信，支持主画布和浮动 Dock 视图。

Emscripten 项目通过 `find_package(Sunfleck CONFIG REQUIRED)` 加载 SDK 后，对应用目标调用 `sunfleck_enable_web_inspector(目标 Web输出目录)`。创建或替换应用后使用 `<sunfleck/web_inspector.h>` 中的 `sunfleck_web_debug_bind(app)` 注册实例，销毁前绑定空指针。使用 `attachHost` 时将 Emscripten 模块作为 `wasmModule` 传给宿主。使用 Core 中 `sunfleck_add_web_app` 的示例已启用这个构建入口。

.NET Wasm 宿主可通过 Debug 配置的 JS 导出调用 `App.GetDebugSnapshot`、`App.SetDebugHighlight`、`App.SetDebugPicking` 和 `HostedSurface.DebugPointer`，并通过 `backend.inspector` 交给共享宿主。Core 自带的 .NET Wasm 示例包含对应接法。C++ 和 .NET 示例的构建脚本均可用 `-Configuration Debug` 启用检查器。

用 SDK 的 `share/sunfleck/web/preview.py` 启动预览，`--directory` 指向 Web 输出目录。可用 `--port` 指定端口，用 `--open-inspector` 打开调试页面。浏览器页面须为 `http://127.0.0.1`，并带 `?sunfleckInspector=1`。VS Code 与 Visual Studio 沿用现有快照连接，Visual Studio 工程可通过 `sunfleck-inspector.json` 配置地址。Debug 构建包含桥接导出，Release 构建不启用。

## DOM 编辑会话

`web/text-input.js` 在获得焦点的 TextBox/SearchBox/TextArea/PasswordBox 上保留 `opacity: 0` 的 DOM input/textarea。它仍参与布局并能获得焦点，负责浏览器输入服务；不使用 `display:none`、`visibility:hidden` 或屏幕外定位。文字、选区、光标和组合输入下划线统一由 Web 的 FreeType/Canvas 路径绘制，因此焦点切换不再切换文字光栅化引擎。

宿主通过 `FontFace` 加载核心使用的打包字体，为透明编辑器提供接近核心的输入法定位与浏览器编辑度量；编辑器的文本区域、行高及滚动位置跟随核心。C++ 从预加载虚拟 FS 读取字体字节，通过 Blob 交给 FontFace，已取消独立字体文件及第二次下载；.NET 从 `./font.ttf` 读取同一份字体字节并交给两个渲染路径。

.NET 示例只用一次 `fetch(..., { cache: 'no-cache' })` 获取字体，并把同一个字节数组交给 FontFace 和原生字体注册；请求要求 HTTP 缓存重验证。已打开的运行时仍需刷新才能加载新资源。中文回归使用“让世界更美好”，既检查文本往返，也检查 Canvas 字形不等同于六个缺字方框；仅验证字符串正确不足以证明字体覆盖正常。

- DOM 负责键盘编辑、原生复制粘贴、撤销/重做和 IME；不通过 keydown 拼接字符，也不主动读取系统剪贴板。
- `ui_host_text_preview` 把 composition 草稿、选区与合成范围传给独立的显示状态。Canvas 实时显示草稿，但不改节点的已提交文本、不触发模型绑定，也不递增已提交 revision。composition 结束或失焦时使用 `ui_host_text_update` 提交；取消则恢复已提交内容并清除显示草稿。
- 草稿和提交都携带 session/revision。外部模型更新、控件停用、页面替换或会话变化后拒绝旧事件；浏览器会替换失效的编辑元素，避免旧输入法会话覆盖新值。
- 原生 FreeType 绘制任意范围选区、合成下划线及光标。`ui_host_text_hit_test` 使用绘制时相同的字符位置处理点击、拖选和双击选词；`ui_host_text_scroll` 驱动长文本滚动，选择/输入时自动显示光标，DOM 同步核心滚动位置。
- 选区和合成范围使用 UTF-16 code unit 索引，桥接到 Windows UTF-16 或 Wasm UTF-32 的 wchar_t 索引。拒绝代理对内部边界；宿主规范化密码框可能报告的代理对内部光标。
- 原有 TwoWay/PropertyChanged 和 LostFocus 绑定继续使用核心提交路径；草稿与绑定值分离。几何查询不消耗待显示帧。
- 关闭移除 DOM 编辑器和监听器；旧 DOM 事件不能回调已销毁 App。

当前单次提交上限为 1 MiB UTF-8，DOM 长度上限为 262144 UTF-16 单元。基础 Tab/Shift+Tab 可在核心控件之间迁移，但完整无障碍焦点顺序和移动软键盘行为仍待完善。

### 输入故障排查

若首次点击编辑框后偶发无法输入，在重现前用 `?inputDiagnostics=1` 开启诊断，发生后调用 `window.sunfleckInputDiagnostics.report()`。打开 DevTools 可能改变焦点，应先开启记录。报告包含文档可见性和焦点、DOM 编辑器的边界与选区，以及最近 200 条事件。诊断记录事件类型、状态和文字长度，不保存输入文本。首次点击和 CDP 合成输入已有自动化覆盖，桌面焦点切换后的真实 IME 状态仍需人工复核。[人工复核](validation.md#人工复核)

## 异步与生命周期

C++ 示例通过 `emscripten_async_call` 演示非阻塞延迟完成，工作区通过 fetch 和完成令牌演示真实网络 I/O。C# 使用 .NET 自身的 `Task.Delay`、`HttpClient`、`async/await` 和现有 `InvokeAsync`；浏览器路径不调用 `Task.Wait`、`GetResult` 或启动原生 worker。桌面原有 worker v2 命令保留。网络验证覆盖成功、HTTP 503、超时、取消及关闭时中止请求。

`begin` 接收一个拥有的 `ui_completion*` 引用，必须最终调用一次 `ui_completion_release`。`ui_completion_complete` 只能在所属 UI 线程调用，完成通知延迟到 `Poll`，重复完成返回错误。命令/App 退役后，令牌可以继续存活，但完成返回 `CANCELLED`，不会访问已释放 App。

取消是协作请求，不能强制停止用户异步代码。回调必须遵守 C ABI 不抛异常的契约。应用自行创建的计时器、网络请求和订阅仍需自己清理。C# 适配器把业务异常转交 App，并在 `finally` 中释放令牌。

宿主使用 `requestAnimationFrame`，后台标签页用计时器尝试推进；浏览器可能节流或冻结计时器，因此不保证后台完成时限。关闭停止宿主调度、移除事件并销毁 App。`pagehide.persisted` 进入历史缓存时仅暂停调度，`pageshow.persisted` 恢复；普通离开仍销毁。浏览器构建拒绝 `Run()`、非零超时的 `Poll` 和独立原生窗口创建，业务应使用 `Poll(0)` 与浏览器异步 I/O。

## Windows 构建

需要 CMake 3.25+、Ninja、PowerShell，以及所选宿主对应的 Wasm 工具链。脚本当前在 Windows 验证。

两种 Wasm 宿主通过 Emscripten ports 链接 FreeType 与 HarfBuzz，并由 CMake 获取带固定 SHA-256 的 SheenBidi 3.0.0。首次构建需下载这些依赖或准备对应缓存。发布产物包含 `SheenBidi-LICENSE.txt`。

字体由调用者提供 TTF/OTF 或 TTC（首个 face），打包发布时需有分发许可。仓库不复制系统字体。中文 Canvas 显示需要所选字体包含中文字形；DOM 的系统字体回退不等于 Canvas 已支持字体回退，缺失的 emoji 仍可能显示方框。下面的 `C:/fonts/application.ttf` 为占位路径。

### 独立 C++

安装并激活官方 Emscripten SDK；最初使用 4.0.12 验证，当前编辑版本使用官方 3.1.56 与 .NET 工具链保持一致：

```powershell
./tools/web/build.ps1 -Target Cpp -EmsdkRoot C:/tools/emsdk -Font C:/fonts/application.ttf
python -m http.server 8765 --bind 127.0.0.1 --directory out/web-cpp/web
```

访问 `http://127.0.0.1:8765/`。必须使用 HTTP 服务，不能直接打开 `index.html`。产物包含 JS、Wasm 和字体数据包。

切换工具链时使用 `-BuildDirectory out/web-cpp-text` 等新目录，再将 HTTP 服务指向对应的 `web` 子目录。

### .NET

示例目标为 .NET 10，安装对应 SDK 的 `wasm-tools` 工作负载。编译核心和链接 .NET 宿主必须使用兼容的 Emscripten ABI，不要混用独立 C++ 产物。

```powershell
dotnet workload install wasm-tools
# 使用独立 SDK 目录，安装并激活与本机 .NET 10.0.14 配套版本一致的官方工具链。
C:/tools/emsdk-dotnet/emsdk.bat install 3.1.56
C:/tools/emsdk-dotnet/emsdk.bat activate 3.1.56
./tools/web/build.ps1 -Target Dotnet -Dotnet dotnet -EmsdkRoot C:/tools/emsdk-dotnet -Font C:/fonts/application.ttf
python -m http.server 8766 --bind 127.0.0.1 --directory web/dotnet/bin/Release/net10.0/wwwroot
```

构建脚本通过 MSBuild 查询工作负载工具路径，以相同工具编译 C++ 核心，再通过 `NativeFileReference` 链接。可通过 `-EmsdkRoot` 显式选择激活的官方 SDK；版本必须匹配所用 .NET 运行时的要求，不能简单取最新版。

Web 托管构建显式设置 `Platform=AnyCPU` / `PlatformTarget=AnyCPU`，避免 VS 开发者终端的 `Platform=x64` 环境变量影响 browser-wasm。原生核心仍由 Emscripten 编译，并非将 Wasm 改为 x64。`-DotnetProject` 可以构建独立应用，`-CppProject` 对应独立 CMake 工程，具体用法见应用交付文档。

本机使用 SDK 10.0.403 / 工作负载 10.0.14。其自带的 Emscripten 3.1.56 / LLVM 19 组合在编译 FreeType 后出现 `__wasm_setjmp`、`__wasm_setjmp_test` 未定义。显式指定官方 Emscripten 3.1.56 后，原生链接及真实浏览器测试均通过；上面的命令使用这条已验证路径。此问题是原生工具链一致性问题，不是 .NET Task 异步限制。其他 SDK/工作负载版本需重新核对工具链，不应盲目固定为 3.1.56。

访问 `http://127.0.0.1:8766/`。静态资源由 .NET 10 的 Content/wwwroot 规则打包；支持 `-Publish` 生成正式发布产物，并通过泛型属性模型保留标注处理裁剪；AOT 仍是未启用的可选发布模式。更新工具链版本时应使用新的构建目录，避免复用旧版本原生对象。

## 回归与证据

原生测试新增 `hosted_runtime`，覆盖实际 down/up、取消手势、尺寸边界、单次在途命令、重复完成、错误线程、协作取消和 App 销毁后的迟到完成。

新增 `hosted_text` 验证补充平面字符、UTF-16 边界、非法编码、单行换行拒绝、选区方向、模型更新后的旧版本拒绝、LostFocus 只提交一次、页面退役、控件禁用、回调重入拒绝，以及查询后仍有待显示帧。

新增 `hosted_views` 验证浮动视图、中文往返与重新停靠，以及最小化/最大化、尺寸恢复、缩放取消和 DPI 状态转换。

浏览器集成测试使用 Playwright 和已安装的 Edge：

```powershell
npm --prefix tests/web ci
node tests/web/smoke.cjs http://127.0.0.1:8765/ out/web-evidence/cpp
node tests/web/smoke.cjs http://127.0.0.1:8766/ out/web-evidence/dotnet
node tests/web/text-input.cjs http://127.0.0.1:8765/ out/web-evidence/text-cpp
node tests/web/text-input.cjs http://127.0.0.1:8766/ out/web-evidence/text-dotnet
```

测试默认使用已安装的 Edge；上述命令假设对应预览服务已启动。测试以真实鼠标操作与 Canvas 图像比较验证：同步更新、异步重复点击只执行一次、等待时同步按钮仍可用、键盘激活、窗口缩放、双倍 DPI，以及异步未完成时关闭不产生异常。截图位于指定证据目录，浏览器版本和结果输出到终端；截图是本地可清理构建产物。`BROWSER_CHANNEL` 可选择浏览器通道，已验证的浏览器版本见 [验证与复现](validation.md#m9-linuxweb-平台专项)。

文本集成测试覆盖真实输入事件、原生复制粘贴快捷键、跨 emoji 选区替换、反向选择、缩放后选择保持、textarea、password、Tab 迁移及合成期间关闭。同时检查打包字体已经加载、DOM 代理透明，并在排除边框与光标的采样区域内逐像素比较单行、多行文字的聚焦/失焦结果，保存对应截图。设置 `SUNFLECK_TEST_DPR=2` 可在双倍 DPI 下执行同一套文本测试。IME 通过 Chromium CDP 的 `Input.imeSetComposition`/`Input.insertText` 驱动浏览器合成事件，验证草稿可见但不发布到模型、取消及最终提交；不等于人工验收 Windows 中文输入法候选窗、Safari/iOS 或 Android 输入法。文本测试另保存 `result.json`。

`ui_host_text_state` 返回渲染器实际文本区域、行高、光标及滚动位置；原生库、C++/C# 序列化、语言绑定和浏览器资源须成套重建。Hosted 接口尚未形成独立的稳定版本兼容承诺，不能混用旧版 Wasm 与新版 JS。

## 当前环境的完整回归入口

```powershell
$env:SUNFLECK_BROWSER_TEST_MODULES = "$PWD/out/web-test" # 已安装测试依赖的本地目录；npm ci 后可省略
./tools/web/verify.ps1 -Font C:/fonts/application.ttf -EmsdkRoot C:/tools/emsdk-dotnet -Dotnet dotnet
```

该脚本构建 C++、发布 .NET，生成 SHA-256 资源清单及 gzip 文件，并使用仅绑定 127.0.0.1 的服务执行双宿主回归。默认矩阵共 22 组，覆盖 smoke、DPR 1/2 文本、外部拖放、DPR 1/1.25/2 工作区、动态窗口状态、渲染、部署和性能测试。可用 `SUNFLECK_TEST_HOSTS` 和 `SUNFLECK_TEST_CASES` 筛选宿主和场景。默认端口为 8768/8769。

`-Complete` 强制运行双宿主所有场景并在结束后恢复原筛选变量；`-EvidenceDirectory` 指定独立证据目录，各场景保存 result.json、日志和截图，总结果保存在 suite-result.json。`-SkipBuild -CppDirectory ... -DotnetDirectory ...` 可检查冻结站点或独立应用。发布验收优先用 `candidate.ps1`，它从冻结站点执行完整回归并校验源码、资源和证据身份。

发布目录为 `web/dotnet/bin/Release/net10.0/publish/wwwroot`。`tools/web/serve.py` 提供显式 Wasm MIME、gzip 和 `no-cache` 重验证，供本地部署验证使用，不是生产服务器。生产部署应原子替换同一版本的整套文件；目前共享 JS/字体没有内容哈希文件名，不应配置永久缓存。`asset-manifest.json` 可核对资源字节，字体仍须由项目方提供有分发许可的文件。独立 C++ 的 DOM FontFace 直接复用预加载字体字节，已取消第二次字体下载。

访问 `?scenario=workspace` 可运行共享工作区示例，包含 SearchBox、列表、树、滚动容器、Dock 浮动、真实 HTTP 请求及取消。浮动视图可以拖动标题、从右下角调整尺寸、关闭，并在重新停靠后保留文字。主视图不再绘制无法控制浏览器窗口的原生标题按钮。

浮窗的最小化/最大化恢复尺寸与拖动起点分别保存，随 DPI 变化换算；最大化视图随主视图尺寸变化。Escape、失焦或 DPI 变化取消正在进行的移动/缩放，恢复手势开始前的位置和尺寸。DPI 变化时先取消手势并同步 DOM 几何，再计算新像素尺寸，避免把取消前的尺寸写回核心。最大化期间不移动或缩放浮窗，最小化期间不进行边角缩放。

`tests/web/window-state.cjs` 在同一页面切换 DPR 1/2/1.25，检查最小化/最大化/恢复、主视图缩放、Escape 取消移动/缩放、缩放中变更 DPI、最小化期间变更 DPI，以及浮窗中文合成后重新停靠。可通过 `SUNFLECK_TEST_CASES=window-state` 单独运行；中文合成采用 CDP，不替代操作系统候选窗的人工验收。

CI 的同一验证入口可在已配置的 Windows 自托管 runner 上手动触发。[Web workflow](../.github/workflows/web.yml)

## 支持范围与限制

- Windows 上的 Edge 和 Chrome 已完成自动化和桌面交互验收。macOS Safari 27.2 已定向验证原生拼音、VoiceOver、外部拖放、页面内 Dock、历史返回和 WebGL2 上下文恢复。Linux 浏览器、Firefox、移动设备软键盘以及 .NET AOT 尚未验收。[Safari 交互结果](validation.md#macos-safari-交互)
- 页面内 Dock 支持浮动视图。独立浏览器窗口、跨窗口 Dock 和画布外部纯文本接收尚未实现。
- DOM/ARIA 已覆盖基础角色、名称、状态、边界和动作。动态通知和更多控件的读屏行为仍需验证，真实的后台标签恢复和浏览器节流也需复验。
- 字体和混排的待测范围见 [M9 平台专项](validation.md#m9-linuxweb-平台专项)。手动动画时钟、大数据虚拟化及模型校验的待测范围见 [跨平台验收](validation.md#m0m8-linuxweb-跨平台验收)。

## WebGL2 渲染后端

共享 [renderer.js](../web/renderer.js) 同时服务 C++、.NET 和页面内 Dock 视图。URL 参数 `renderer=auto`（默认）优先创建 WebGL2，创建失败时使用 Canvas 2D；`renderer=webgl2` 强制 WebGL2，不可用时明确报错；`renderer=canvas2d` 强制 CPU 基线。工作区可使用 `?scenario=workspace&renderer=webgl2`。成功创建上下文后发生的着色器/绘制错误会报告到状态区域，不会静默当作正常帧。

核心通过 `ui_host_set_render_mode(app, 1)` 保留命令并跳过整帧 CPU rasterize；`ui_host_draw_commands` 读取当前视图的 v1 字节流。每个矩形占 20 字节：四个 little-endian int32 边界，随后 RGBA8 非预乘颜色；右/下边界不包含，按原始顺序 source-over 混合。核心已完成控件裁剪，WebGL 继续执行视口裁剪。默认软件模式不改变桌面行为。命令模式下 `ui_host_pixels` / `ui_dock_pixels` 可按需生成 CPU 像素，用于回退；旧 `ui_app_pixels` 不触发延迟栅格化。

后端使用实例化矩形，每批最多 65,536 条命令，批次保留绘制顺序；不按颜色排序，不逐矩形提交 draw call。采用独立 RGB/alpha 混合，禁用深度、抗锯齿和 dithering，保留核心生成的字形/圆角覆盖率。文字栅格化、材质采样和几何展开仍在 CPU；尚未引入字形图集、纹理图片命令或 WebGPU。此接入不意味着所有业务都更快，尤其长文本可能生成大量矩形。

每个 Canvas 有独立上下文和可复用 GPU 缓冲。为支持只画脏帧及截图，启用 `preserveDrawingBuffer`。宿主复制最新命令以避开 Wasm 内存增长和跨帧失效：上下文丢失期间继续处理输入及核心状态，恢复时重建资源并重绘最新命令，即使核心没有新帧；未恢复期间该视图可能空白，不自动替换正在输入的 DOM Canvas。关闭释放 GPU 资源和恢复监听器。`.NET` 路径仍存在托管到 JS 的字节复制，不声称零复制。性能脚本中的动作到语义发布计时在 draw 提交后结束，不等待 GPU 完成，不等同于实际屏幕呈现延迟。

`tests/web/renderer.cjs` 检查 CPU/WebGL2 首屏差异、批次边界、视口裁剪、重叠半透明混合、闲置帧恢复、丢失期间更新、显式 CPU 模式、auto 回退和关闭资源释放。普通文本测试通过独立 Canvas 读取画面，兼容两种渲染后端。画布 `data-renderer`、`data-renderer-state`、`data-rectangles` 和 `data-draw-calls` 可供诊断；自动测试以 WebGL2 实际创建成功为条件，浏览器内部是否使用物理 GPU 由驱动/浏览器决定。
