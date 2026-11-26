# 验证与复现

这里记录各平台已执行的验收、证据路径和复现命令。测试依赖与标签见 [构建与测试](building.md)。

## Windows 验收

M1～M8 Windows 阶段已验收。阶段记录对应各自的源码和二进制候选。

| 阶段 | 覆盖与证据 |
|---|---|
| M1/M2 属性与文字 | 属性优先级、Fleck、三档 DPI 的文字和输入、三语言 SDK 消费及 VS Code 导航。见 `out/m12-candidate/` |
| M3 布局与内容 | Grid、Panel、内容控件、DataGrid 布局、属性和三语言入口。定向复验两种后端各 22/22。见 `out/m3-grid/` |
| M4 变换与绘图 | Skia 17/17、D3D11 12/12。RTX 2080 Ti 硬件与 WARP 相对 CPU 的组合场景最大通道差均为 1，D3D12 调试层错误为 0。旋转编辑框覆盖拖选、替换及 IME 几何。见 `out/m3-grid/` 和 [表面对照](../tests/skia_d3d12_surface.cpp) |
| M5 动画 | 手动时钟、视觉状态、属性动画和三语言动画中点。物理机候选定向复验 6/6，GPU Dock 连续通过三次。见 `out/m5-sdk/result.json` 和 Windows 后续复验目录 |
| M6 样式与路由 | 样式、资源、模板、键盘和指针路由。同一候选跨环境累计 101/101，物理机 GPU 检查 14/14。见 `out/m6-acceptance/` |
| M7 数据控件 | 十万项列表和十万行 × 二百列 DataGrid 滚动到末端，视口实现行数小于 32、每行实现列数小于 16。覆盖排序、选择、编辑校验和模板刷新失败恢复。跨环境累计 101/101。见 `out/m7-acceptance/` |

### M8 Windows 交付验收

- 最终 SDK 已验证迁移到含空格的安装目录后的 C、C++、C# 消费，检查了 PE 依赖和无 ANGLE 导入。独立 Demos 三语言测试与 VSIX 六组均通过。最终消费报告为 `out/m8-final-sdk/result.json`，交付包为 `out/m8-final-delivery/sdk.zip`。
- 交付候选 102 项 Windows CTest 累计通过。物理机 RTX 2080 Ti 的 GPU 检查通过 14/14，六个程序的哈希与验收包一致。原始证据位于 `out/m8-acceptance/`。
- 2026-11-21 的候选复验通过 16/16 项定向检查和 7/7 项人工复核，覆盖讲述人、普通与对比度主题的键盘焦点、100%／150%／200% DPI，以及 OLE 文本、文件和应用对象拖放。外部 UI Automation 验证控件操作与密码保护。[UI Automation 客户端](../tests/windows_accessibility.ps1)和 [桌面复核脚本](../tests/windows-interaction-review.ps1)
- 安装 SDK 的 100／1000 节点性能对照在同机交替测量三轮，每轮 28 个统计量均在 15% 门限内。原始结果位于 `out/m8-acceptance/performance`。[性能比较](performance.md#比较与验收)

### Windows 后续复验

2026-11-21 的复验基于 `f9e57cd` 及当时的 M9 工作区源码，在 Windows build 26300 x64 Release、RTX 2080 Ti（驱动 `32.0.16.1088`）上串行执行。M3、M4、M5、M8 定向结果和身份清单位于 `out/stage-acceptance-20261121-135849/`，其中 `candidate/interaction-result.json` 对应人工复核。

Gallery 示例第二行仅含 Unicode 非字符 U+FFFF，触发 Headless CPU 墨迹断言，在原始 `f9e57cd` 同机重建后也可复现。改用有效文本后保留原断言，最终候选全量通过 102/102，报告为 `out/gate-20261121/windows-clean-recheck.xml`。D3D11 早期曾间歇返回 `0x80004005`，原因仍未确定，对照记录位于 `out/d3d11-diagnosis/`.

## M9 Linux/Web 基线

- UbuntuDesktop，Ubuntu 26.04.1 LTS，GCC 15.2.0，FreeType 2.14.2，HarfBuzz 12.3.2，.NET SDK 10.0.403。使用 `debug` 预设并启用 `UI_ENABLE_GLES3` 和 `UI_BUILD_CSHARP`，启用可选 HarfBuzz 后完整 CTest 通过 87/87。X11 用例由 Xvfb 启动，Mesa 使用 `llvmpipe`。原始提交 `f9e57cd` 的 X11 构建失败。仅修正遮蔽 X11 `Display*` 的局部变量后，原始基线为 76/87。
- GNOME XWayland 首轮在默认 Mesa `SVGA3D` 与显式 `llvmpipe` 下各通过 4/8，原始结果见 `out/stage-acceptance-20261121-135849/` 的 `stage-xwayland-svga3d.log` 和 `stage-xwayland-llvmpipe.log`。修复测试窗口定位、Fleck 刷新等待和浮动 Dock 指针捕获后，两组各通过 8/8。通过报告位于 `out/gate-20261121/` 的 `linux-desktop-verified.log` 和 `linux-verified.xml`。
- Windows 上使用 Emscripten 3.1.56 和 .NET 10.0.403 / wasm-tools 10.0.14 重建两个 Wasm 宿主。Edge 157.0.4341.60 和 Chrome 158.0.8127.36 的 C++、.NET 基线回归各通过 10/10，每组首次点击输入复跑 6 轮。修复 DOM 焦点后，每个浏览器又通过 14 项回归和 6 项定向复测，Web 工作区讲述人经人工确认。系统输入法候选窗见后续专项。证据位于 `out/gate-20261121/` 的 `web-regression-*` 和 `web-focus-*`。[Web 回归](web.md#回归与证据)

## M9 Linux/Web 平台专项

2026-11-22 的专项候选基于 `f9e57cd` 及当时的工作区改动。结果保存在 `out/m9-special-20261121/`。

| 范围 | 结果 | 证据 |
|---|---|---|
| UbuntuDesktop 全量回归 | CTest 88/88。覆盖 HarfBuzz 字形、FriBidi 混排双光标位置与 X11 外部拖放 | `linux-final-ctest.xml` |
| UbuntuDesktop 外部拖放 | 真实 GNOME XWayland 会话通过 Xdnd 文本、多个本地文件接收和拒绝远程 URI | `linux-atspi-orca-client.log` |
| UbuntuDesktop 无障碍 | 独立 AT-SPI 客户端验证窗口发现和活动状态、控件角色、文本、密码保护、聚焦、复选框激活及退出清理 | `linux-atspi-orca-client.log`、[客户端](../tests/x11_accessibility.py) |
| UbuntuDesktop 中文输入 | GNOME 会话输入 `nihao`，目视确认 IBus 候选窗位于编辑光标下方，空格提交后通过 AT-SPI 读回「你好」 | `linux-ime-candidate-final.log` |
| UbuntuDesktop Orca | 语音日志确认 `Apply` 按钮、`Enabled` 复选框及勾选状态、`editor` 的角色与预置文本。覆盖窗口激活及按行取文本 | `linux-orca-speech.log` |
| Edge、Chrome | 各 22/22，覆盖 C++ 与 .NET 两种宿主、双向文字命中、CDP 合成输入、画布和编辑框的外部文件接收，以及编辑框外部文本接收 | `m9-final-{msedge,chrome}/suite-result.json`、[拖放测试](../tests/web/external-drop.cjs) |
| Web 桌面交互 | 用户确认系统拼音候选窗定位正常，搜索框和文档编辑区均可提交中文。Windows 资源管理器文件拖入在 Edge、Chrome 的 C++ 和 .NET 工作区均通过 | `web-explorer-drop-manual.png` |
| Windows 定向复验 | `hosted_runtime`、`hosted_text`、`windows_accessibility` 通过 3/3 | `out/gate-20261121/m9-windows-targeted.xml` |
| Web 投放区定向回归 | 新增显式投放区后，Edge 158.0.4370.33、Chrome 158.0.8127.36 各 10/10，覆盖两种宿主的拖放、工作区与部署 | `drop-zone-{msedge,chrome}/suite-result.json` |

文字专项已覆盖连字及希伯来语与拉丁语混排。仍需扩大跨脚本塑形、字体回退、语言断行及混排选区和光标几何测试。交互能力边界见 [渲染与部署](../../docs/guides/rendering.md#平台边界)和 [Web 接入](web.md#支持范围与限制)。

## macOS 验收

2026-11-25 在 macOS 27.2.1 ARM64 上验证。原生构建使用基于 `76f1093` 的工作区源码和 Apple Clang 21。Safari 定向检查使用已有 C++ 和 .NET Wasm 产物，并用更新后的共享 JavaScript 重验历史返回和纯文本拖放。

### 原生桌面与 SDK

| 环境 | 已验证 | 证据 |
|---|---|---|
| SunfleckMacVM，Apple Paravirtual GPU | Release CTest 60/60。覆盖 Metal 像素读回、AppKit 窗口开关、尺寸变化和键盘编辑。关闭 Metal 的构建通过软件窗口和 Auto 回退检查 | [Metal 与窗口用例](../tests/macos_rendering.mm) |
| SunfleckMacVM，安装式 SDK | 将 SDK 移到含空格的路径后，独立 C 和 C++ 工程通过。使用 .NET SDK 10.0.404 构建和发布独立 C# 工程，输出均能加载 ARM64 `libsunfleck_native.dylib`。C# 严格 Metal 窗口通过启动和关闭检查 | `out/macos-metal/sdk-vm.log`、[复验命令](sdk.md#macos-安装与复验) |
| 实体 M2 Mac mini，Mac14,3 | 校验并运行虚拟机编译的 ARM64 产物。通过 Metal 像素读回、AppKit 窗口检查、360 帧连续重绘、窗口缩放、最小化与恢复。测试在临时目录运行，完成后已清理 | `out/macos-metal/physical-macmini.log` |

实机显示缩放为 1×。Retina、跨显示器缩放和整机睡眠唤醒待验收。原生 AppKit 的功能范围见 [渲染与部署](../../docs/guides/rendering.md#平台边界)。

### macOS Safari 交互

SunfleckMacVM 使用 Safari 27.2，设备像素比为 1。证据位于 `out/safari-vm/evidence/`。

| 范围 | 已验证 | 证据文件 |
|---|---|---|
| C++ 和 .NET Wasm、文档站 | 页面检查 10/10，WebDriver 交互 5/5。覆盖 WebGL2、Canvas 2D、键鼠输入和文档示例。两个宿主的自动模式均选用 WebGL2 | `in-page.json`、`webdriver.json` |
| 原生拼音 | 人工确认候选窗显示在编辑框附近。两个宿主均收到系统合成事件，并通过数字键选词提交「你好」。全选后开始合成，再按 Esc 会清空原文，Safari 的普通 HTML 输入框也复现了该行为 | `interaction/interaction-cpp-ime.jsonl`、`interaction/interaction-dotnet-ime.jsonl`、`interaction/interaction-safari-control-clean.jsonl` |
| 文件和纯文本拖放 | Finder 文件拖入 C++ 示例的专用接收区通过，两个宿主均接收拖到画布上的文件。修复后，从原生 macOS 应用拖入纯文本，两个宿主均能向尚未聚焦的编辑框插入文字 | `interaction/interaction-cpp-file.jsonl`、`interaction/interaction-{cpp,dotnet}-native-text.jsonl`、[拖放回归](../tests/web/external-drop.cjs) |
| 页面内 Dock | 两个宿主均通过浮动面板移动、缩放、重新停靠和关闭 | `interaction/dock.json` |
| VoiceOver | 人工确认可进入网页内容，读出 `Increment` 按钮和编辑框的名称。Safari WebDriver 在两个宿主中均确认这两个控件的名称与角色 | `interaction/safari-text-drop.json` 的 `accessibility` 字段记录浏览器检查 |
| 历史返回与 WebGL2 恢复 | 两个宿主在页面缓存返回后保留编辑内容，在模拟 WebGL2 上下文丢失并恢复后重绘最新内容 | `lifecycle.json` |

Safari 后台标签页返回在 WebDriver 中未能稳定复验。Safari Retina、iOS Safari、VoiceOver 对动态状态和更多控件的朗读待验收。

## M0～M8 Linux/Web 跨平台验收

本节记录 M0～M8 在 Linux/Web 的自动化验收结果。阶段能力通过共享核心测试、Linux CTest、宿主测试和浏览器套件映射验证。Windows 专属系统能力保留在对应 Windows 候选记录中。

| 阶段 | Linux | Web | 验收范围 |
|---|---|---|---|
| M0 | 通过 | 通过 | 工具链、版本、构建、资源清单、部署服务和宿主能力检查 |
| M1 | 通过 | 通过 | 属性系统、绑定来源、布局缓存、尺寸和宿主状态 |
| M2 | 通过 | 通过 | FreeType/HarfBuzz、Run/Span、文字编辑、选区和光标几何。字形塑形与混排补充记录见 M9 |
| M3 | 通过 | 通过 | Grid、Panel、内容控件、树、滚动、Dock 视图、DPR 和窗口状态 |
| M4 | 通过 | 通过 | 变换、裁剪、画刷、图像、遮罩、效果、WebGL2 和 Canvas2D 回退 |
| M5 | 通过 | 部分通过 | Linux 覆盖可控时钟、视觉状态和属性动画。Web 验证了宿主生命周期、按下状态的 Storyboard 和松开后恢复基础值，待直接覆盖手动时钟 API |
| M6 | 通过 | 通过 | 资源、样式、模板、事件路由、键盘激活和 ARIA 语义 |
| M7 | 通过 | 部分通过 | Linux 覆盖绑定、集合、TreeView、ListView、DataGrid 虚拟化和编辑校验。Web 覆盖工作区基础交互和 20 行 DataGrid 单元格编辑，待补大数据虚拟化与模型校验 |
| M8 | 部分通过 | 部分通过 | Linux 覆盖焦点、窗口生命周期、XWayland Dock、AT-SPI 客户端及外部文本和文件接收。Web 覆盖工作区讲述人、编辑框外部文本、页面内 Dock、生命周期、部署和资源管理器文件拖入 |

## 人工复核

Windows 使用 `tests/windows-interaction-review.ps1 -Directory <candidate目录>`。报告和覆盖范围见 [M8 Windows 交付验收](#m8-windows-交付验收)。

Linux 在真实 GNOME XWayland 会话中操作 Dock、IBus、Orca 和外部拖入。Web 在 Windows 的 Edge 和 Chrome 上操作系统输入法、讲述人和资源管理器。浮窗、滚动、动态 DPR 组合及桌面焦点切换仍需进一步复验。已通过的范围见 [M9 平台专项](#m9-linuxweb-平台专项)，输入故障取证见 [Web 输入排查](web.md#输入故障排查)。

## 选择测试

普通变更运行受影响分组；基础结构收口或阶段候选再执行完整回归。具体构建选项见 [构建与测试](building.md)。

```powershell
ctest --test-dir out/dev -R '^(visual_layout|element_height|property_system|property_bindings|fleck_components|element_types_c|element_types_cpp|csharp_api)$' --output-on-failure -j 1
out/dev/fleckc.exe --schema out/property-schema.json
python tests/development/audit-element-properties.py out/property-schema.json out/dev/sunfleck_native.dll out/property-schema-audit.md
python tests/development/audit-runtime-properties.py out/property-schema.json out/dev/sunfleck_native.dll out/property-runtime-audit.md
```

安装式消费见 [SDK](sdk.md)，组件消费入口为 [check_components.py](../tests/sdk/check_components.py)。IDE、Pipe、IME、监听重启和 PDB 诊断入口统一在 [开发工具测试说明](../../devtools/tests/development/README.md)；Web 实际宿主验证见 [Web 接入](web.md#回归与证据)。

### 候选验收要点

- 比较 Fleck XML、生成的 C/C++、编译资源和 C# 行为。新增公开语法同步检查 schema、补全、诊断和源码定位。
- 验证非法属性、资源或模板错误、刷新失败时保留原有效页面，并检查销毁后的订阅、动画及回调清理。
- 布局与输入用几何断言检查，GPU 与 CPU 在固定字体和配置下作像素对照。Windows 桌面覆盖三档 DPI、焦点、多窗口及系统 IME。
- 动画使用可控时钟，数据控件使用十万项场景检查容器数量、状态保留和回收。
- 移动安装后的 SDK 并重建代表性消费者，检查文档链接。性能使用同机同配置的配对比较。[性能基线](performance.md)

## 记录与清理规则

结果必须同时说明源码/二进制身份、环境、命令、结果及未覆盖范围。只保留当前候选、必要基线和未解决问题的最小复现证据；新结果取代旧结果后可删除冗余构建日志、临时 schema 和失败产物。原始失败记录若仍被当前汇总引用，应与复验记录一起保留。

生成审计表、截图、JUnit 和性能样本输出到 `out/`，不复制到维护文档中。`out/` 是可清理的本机产物，使用代码路径引用；仓库文档链接测试源码与使用说明。清理不代表重新验收，旧版本结果不能套用到新 ABI、SDK 或平台。
