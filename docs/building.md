# 构建与测试

以下命令均从仓库根目录执行。工程要求 C11/C++20 工具链和 CMake 3.25+。`debug` 与 `release` 预设使用 Ninja，分别输出到 `out/debug` 和 `out/release`。未显式指定的选项会沿用 CMake 缓存。需要恢复默认值时使用独立构建目录或 `cmake --fresh --preset debug`。

## Windows

准备 Visual Studio 的 C++ 桌面开发组件和带 `icu.h` / `icu.lib` 的 Windows SDK（当前验证 10.0.26100.0），在 x64 开发者终端执行。DirectWrite 文字后端与系统 ICU 需要 Windows 10 1903 或更新版本；最低版本实机验证待补，详见 [字体](../../docs/guides/fonts.md)。

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -LE interactive
```

将 `debug` 替换为 `release` 可构建优化版本。D3D11 默认启用，构建需要 Windows SDK 中的 `fxc`。如遇 PDB 错误 C1902，可重新配置：

```powershell
cmake --fresh --preset debug -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
```

若遇 `STL1001`，检查编译器和标准库头文件是否来自同一套 Visual Studio，再从对应开发者终端重新配置。

MSVC/Ninja 使用 `build/Sunfleck.MsvcDependencies.cmake` 按原始字节探测 `/showIncludes` 前缀，避免中文编译器输出被 CMake 按错误代码页解码，导致头文件改动未触发重编译。更新旧构建目录后重新运行配置及所需目标构建。可用 `ninja -C out/devtools -t deps CMakeFiles/sunfleck.dir/src/app.cpp.obj` 核对依赖列表应包含 `src/ui_tree.h` 等项目头文件；`#deps 0` 不能作为正常增量构建使用。旧测试对象也需检查，例如 `CMakeFiles/headless_png.dir/tests/headless_png.cpp.obj` 和 `CMakeFiles/dock_window_test.dir/tests/dock_window.cpp.obj`。修复前缀不会自动重编所有旧对象；发现依赖缺失时，配置后执行一次 `cmake --build out/devtools --clean-first`，再确认依赖和测试结果，避免新核心与旧测试对象的结构布局不一致。日常运行与改动对应的定向检查，完整回归按需要单独执行。

## 本机滚动与外部输入复验

桌面、4096 节点及真实计数器的夹具、交互步骤和审计入口集中在 [桌面与滚动复验](../../devtools/tests/development/README.md#桌面与滚动复验)。

## Linux X11

默认启用 X11。以 Ubuntu 24.04 的包名为例：

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkgconf \
  libx11-dev libxext-dev libfreetype-dev fonts-dejavu-core fonts-noto-cjk \
  libharfbuzz-dev libfribidi-dev libatk1.0-dev libatk-bridge2.0-dev \
  xvfb xauth libxtst6
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

FreeType 由 CMake 自动探测；需要文本绘制和编辑时应安装其开发包。Xvfb、xauth 和 libXtst 用于 X11 自动化测试，不是普通应用的全部运行依赖。桌面应用运行需要 X11 或 XWayland 会话；X11 使用会话启动时的屏幕 DPI。

HarfBuzz 由 CMake 自动探测。启用后使用塑形结果的字形索引、advance 和偏移，再由 FreeType 栅格化。检测到 FriBidi 时解析混排方向。缺少 HarfBuzz 开发包时保留 FreeType 字符路径。

X11 构建默认探测 ATK 和 AT-SPI bridge。具备开发包时，桌面窗口向会话的 AT-SPI 总线导出基础语义。运行独立验收客户端还需要 Python GObject 的 Atspi typelib（Ubuntu 上的 `python3-gi` 和 `gir1.2-atspi-2.0`）。

启用 GLES3 前额外安装 `libegl-dev libgles-dev libegl-mesa0`，再配置 `-DUI_ENABLE_GLES3=ON`。Mesa 软件驱动验收可在测试命令前设置 `LIBGL_ALWAYS_SOFTWARE=1`。仅需 Headless 时使用 `-DUI_PLATFORM_X11=OFF -DUI_ENABLE_GLES3=OFF`。

## macOS

安装 Apple Command Line Tools（`xcode-select --install`）和 CMake 3.25+。通过 `xcrun --sdk macosx --show-sdk-path` 确认 SDK 可用。AppKit、CoreText 和 Metal 来自系统 SDK，以下命令使用系统自带的 Make。

```sh
cmake -S . -B out/macos -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build out/macos --parallel
ctest --test-dir out/macos -LE interactive --output-on-failure
```

在已登录的 macOS 桌面会话运行窗口测试和预览。

```sh
ctest --test-dir out/macos -L interactive --output-on-failure
./out/macos/fleck_preview --renderer=metal tests/fixtures/welcome.fleck
```

`metal_pixels` 比较 Metal 纹理读回和 CPU 栅格结果。无可用 Metal 设备时跳过此用例。`macos_window` 检查窗口开关、尺寸变化、键盘编辑和渲染模式。用 `-DUI_ENABLE_METAL=OFF` 可构建软件渲染版。

在 Apple 芯片虚拟机完成构建后，可将 `out/macos/macos_rendering_test` 单独复制到同架构 Mac 的临时目录。先核对产物的 SHA-256，再运行像素测试和桌面窗口测试。桌面测试使用已登录的图形会话，两项测试结束后可删除临时目录。

```sh
./macos_rendering_test --pixels
./macos_rendering_test
```

构建 C# 绑定需将同架构的 .NET 10 SDK 加入 `PATH`，并启用 `-DUI_BUILD_CSHARP=ON`。使用已安装的 SDK 时可参照独立消费者步骤。[安装式 SDK](sdk.md#macos-安装与复验)

## 构建选项

| 选项 | 默认值 | 作用 |
|---|---|---|
| `UI_BUILD_EXAMPLES` | ON | 构建 C/C++ 计数器、展厅和独立 Dock 工程 |
| `UI_BUILD_DEMOS` | ON | 启用测试时构建旧计数器回归夹具；独立 Demos 已移出仓库 |
| `BUILD_TESTING` | ON | 注册 CTest 用例 |
| `UI_BUILD_CSHARP` | OFF | 构建原生共享库、.NET 绑定、托管示例与测试；需要 .NET 10 SDK |
| `UI_ENABLE_SKIA_D3D12` | OFF | Windows Auto 接入带补丁 Skia 原生 D3D12；[构建与验收](skia-runtime.md) |
| `UI_SKIA_ROOT` | 空 | 固定 Skia 源码目录，启用 Skia 时必填 |
| `UI_SKIA_LIBRARY_DIR` | `<UI_SKIA_ROOT>/out/sunfleck-d3d12` | 已重新构建的 Release `/MD` 静态库目录 |
| `UI_ENABLE_D3D11` | Windows 上 ON | 构建原生 D3D11 后端 |
| `UI_ENABLE_METAL` | macOS 上 ON | 构建原生 Metal 后端 |
| `UI_ENABLE_GLES3` | OFF | 构建 EGL/GLES3 后端，需要对应开发头文件 |
| `UI_PLATFORM_X11` | Linux 上 ON | 构建 X11 桌面后端；关闭后可运行 Headless |
| `UI_ENABLE_ATSPI` | X11 上 ON | 在检测到 ATK 和 AT-SPI bridge 开发包时构建无障碍桥接 |

例如，同时启用 C# 和 GLES3：

```sh
cmake --preset debug -DUI_BUILD_CSHARP=ON -DUI_ENABLE_GLES3=ON
cmake --build --preset debug
```

Windows 的 ANGLE 头文件和运行库配置见 [渲染与部署](../../docs/guides/rendering.md)，托管产物见 [C# 接入](../../docs/guides/csharp.md)。

仓库示例见 [示例入口](../../samples/README.md)。仓库外的三语言项目使用各自的 SDK 和启动脚本。[独立 Demos](../../docs/guides/windows-quickstart.md#独立-demos)

## 开发工具构建

开发工具使用独立 Debug 目录；Windows 在 VS x64 开发者终端运行，需要 .NET 10 SDK。下列配置关闭 D3D11 和原生示例／演示，启用测试、共享服务及托管项目：

```sh
cmake -S . -B out/devtools -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DUI_BUILD_CSHARP=ON -DUI_BUILD_EXAMPLES=OFF -DUI_BUILD_DEMOS=OFF -DUI_ENABLE_D3D11=OFF
cmake --build out/devtools
ctest --test-dir out/devtools -LE interactive --output-on-failure
```

共享服务输出到 `out/devtools/tools/development`。C/C++ 元数据由测试目标生成，C# 元数据由托管 Fleck 示例生成；它们是隔离 VS Code 宿主脚本的前置输入。服务 CLI、两端扩展打包／配置及应用接口见 [开发工具接入](../../docs/guides/devtools.md)。此配置的非交互检查不验证 GPU 或实际桌面；包与运行库哈希必须对应本次构建。

## 测试与输出

先构建再运行测试；测试数量取决于平台、后端、.NET 选项和已发现的测试依赖。查看当前配置注册的用例：

```sh
ctest --preset debug -N
```

Windows 桌面测试标记为 `interactive`，需要已解锁、无遮挡的交互桌面。无桌面时用 `-LE interactive`；具备桌面时用 `-L interactive` 单独验收。Linux 的 X11 用例在发现 `xvfb-run` 时注册，CTest 为每个用例启动独立的 Xvfb。

运行时模型自动同步的本机定向检查复用 `csharp_api`、`inspector_client_proxy` 和 `language_server_stdio`：前者通过真实原生快照核对模型名/实例、补全/跳转/悬停、命令模式、无 Binding 页面及 getter 调用计数；代理检查使用真实 stdio 服务验证源文件隔离、配置重读、属性/进程替换、冲突/错误恢复、多订阅释放、读取中释放及显式/静态回退。三加载路径的模型关联另由 `development_metadata` 检查。复现入口：

```sh
cmake --build out/devtools --target sunfleck_development csharp_tests development_test
ctest --test-dir out/devtools -R '^(development_metadata|csharp_api|inspector_client_proxy|language_server_stdio)$' --output-on-failure
```

这些用例无需安装 IDE 扩展。源码或适配器检查不能替代最新安装包的实际宿主联动；其他 IDE 版本和 Linux 环境按计划后续集中验收。

IDE、Pipe、模型／生命周期、滚动和 PDB 诊断的专项入口集中在 [开发工具测试说明](../../devtools/tests/development/README.md)。按本次改动选择测试，不需要日常反复运行全矩阵。当前候选范围和记录规则见 [验证与复现](validation.md)。

`showcase_theme_png` 生成五套主题明暗模式各一张展厅组合图、展厅 Dock 页及树控件图，共 30 张 PNG。输出在构建目录，以 `showcase-theme-*`、`showcase-theme-dock-*`、`showcase-theme-tree-*` 命名；无需交互桌面。CTest 日志默认位于构建目录的 `Testing/Temporary/LastTest.log`，也可用 `--output-log` 指定保留位置。

```sh
ctest --preset debug -R '^showcase_theme_png$'
```

CI 的 Windows 作业运行非交互测试，Linux 作业启用 GLES3 并使用 Mesa 软件驱动，均未启用 C#。桌面、物理 GPU 和多屏结果见 [验证与复现](validation.md)，自动化配置见 [ci.yml](../.github/workflows/ci.yml)。

## Linux 容器

[Dockerfile](../Dockerfile) 提供 CPU/X11 测试依赖；[Dockerfile.m3](../Dockerfile.m3) 在其基础上增加 Mesa EGL/GLES3。二者均未安装 .NET SDK。

```sh
docker build -t sunfleck-m2 .
docker build -f Dockerfile.m3 -t sunfleck-m3 .
docker run --rm -v "$PWD:/checkout:ro" sunfleck-m3 sh -ec '
  cp -r /checkout/CMakeLists.txt /checkout/CMakePresets.json /checkout/build /checkout/src /checkout/include /checkout/examples /checkout/tests /checkout/tools .
  for preset in debug release; do
    cmake --preset "$preset" -DUI_ENABLE_GLES3=ON
    cmake --build --preset "$preset"
    LIBGL_ALWAYS_SOFTWARE=1 ctest --preset "$preset"
  done
'
```

源码以只读方式挂载，构建在容器的 `/src` 内执行，退出后不保留产物。若只测试 CPU，将镜像换为 `sunfleck-m2`，并去掉 GLES3 配置选项。

## 在新环境复验

迁移工作区时保留 Git 历史、未提交改动和新增文件。换机前后用 `git rev-parse HEAD` 和 `git status --short --untracked-files=all` 核对，按目标平台重新配置构建目录。已有 Windows SDK 的对照证据和包路径见 [M8 Windows 交付验收](validation.md#m8-windows-交付验收)。

Linux 先按 [X11 依赖](#linux-x11)配置，使用 `ctest --preset debug -N` 保存用例清单，再运行全套测试。涉及 IME、焦点、缩放和辅助技术时使用真实桌面。浏览器分别准备 C++ 与 .NET Wasm 工具链和有分发许可的测试字体。[Web 接入](web.md)和 [人工复核](validation.md#人工复核)

## Fleck 组件构建

组件资源由 `sunfleck_compile_page` 收集。使用 `sunfleck_add_component_metadata(MyTypes EXPORTER my_type_exporter OUTPUT "${CMAKE_BINARY_DIR}/.fleck/types/controls.fleckcomponents.json")` 从代码声明生成元数据，再声明 `sunfleck_component_library(MyControls RESOURCES controls/UserCard.fleck TYPES "${CMAKE_BINARY_DIR}/.fleck/types/controls.fleckcomponents.json")`，页面目标通过 `COMPONENT_LIBRARIES MyControls` 引入；跨项目库会沿依赖递归收集。MSBuild 使用 `Sunfleck.Components.targets` 和 C# 生成器扫描 `AdditionalFiles`，生成每个程序集的注册入口及元数据。

安装 SDK 的 C# 项目在导入 `Sunfleck.props` 前设置 `SunfleckEnableComponents=true`，即可使用包内的生成器和元数据导出器。安装后可在 Visual Studio 开发者终端运行 `python tests/sdk/check_components.py --sdk out/components-sdk`；测试在 `out` 中创建独立消费项目，覆盖 C/C++、C# 项目引用、子组件增量重建、无 XML 发布运行和生成器错误诊断。
