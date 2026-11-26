# 安装式 SDK 与独立消费

框架和 SDK 的发布版本为 `1.0.0`，取自根 CMake 的 `PROJECT_VERSION`。同一版本同时发布 VS Code 和 Visual Studio 扩展。安装包括 C/C++ 头文件、原生库、可选 `fleckc`、CMake 导出以及启用 C# 时的 .NET 10 程序集和原生共享库。

SDK 1.0.0 使用 API v5 和 Fleck 资源 v16。升级时需一起重建应用、语言绑定和页面。旧资源和旧生成页面会被明确拒绝。具体步骤见 [SDK 迁移](sdk-migration.md)。

在配置好的本机工具链中：

```powershell
cmake -S . -B out/sdk-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUI_BUILD_CSHARP=ON -DUI_BUILD_EXAMPLES=OFF -DUI_BUILD_DEMOS=OFF -DBUILD_TESTING=OFF
cmake --build out/sdk-build --target sunfleck sunfleck_native fleckc sunfleck_sdk_managed
cmake --install out/sdk-build --prefix C:/SDK/Sunfleck
```

自动本机验收入口为 `pwsh -NoProfile -File tools/sdk/validate.ps1`，它使用独立输出、将 SDK 移到含空格的新前缀，再复制安装内的消费示例到临时目录，验证三个语言入口。不需要修改系统安装目录。验收产物见脚本输出的报告。

## macOS 安装与复验

准备 Apple Command Line Tools、CMake 和同架构的 .NET 10 SDK。启用 C# 时使用以下命令将 SDK 安装到仓库内的独立目录。只需要 C/C++ 时，可关闭 `UI_BUILD_CSHARP`，并构建 `sunfleck` 与 `fleckc`。

```sh
cmake -S . -B out/macos-sdk-build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DUI_BUILD_CSHARP=ON -DUI_BUILD_EXAMPLES=OFF -DUI_BUILD_DEMOS=OFF -DBUILD_TESTING=OFF
cmake --build out/macos-sdk-build --target sunfleck fleckc sunfleck_sdk_managed
cmake --install out/macos-sdk-build --prefix "$PWD/out/macos-sdk"
```

将安装包内的示例复制到新的独立目录。传入 SDK 的绝对路径，可在移动安装树后用同一方式复验。

```sh
sdk="$PWD/out/macos-sdk"
cp -R "$sdk/share/sunfleck/examples" out/macos-sdk-consumer
cmake -S out/macos-sdk-consumer -B out/macos-sdk-consumer-build \
  -DCMAKE_PREFIX_PATH="$sdk" -DCMAKE_BUILD_TYPE=Release
cmake --build out/macos-sdk-consumer-build
ctest --test-dir out/macos-sdk-consumer-build --output-on-failure
dotnet publish out/macos-sdk-consumer/Consumer.csproj -c Release \
  "-p:SunfleckSdkRoot=$sdk" -o out/macos-sdk-consumer-publish
dotnet out/macos-sdk-consumer-publish/Consumer.dll
```

C# 构建和发布会将 `libsunfleck_native.dylib` 复制到应用目录。使用 `Renderer.Metal` 的窗口需要在 macOS 图形会话的主线程打开。[原生 SDK 验收](validation.md#原生桌面与-sdk)

## C 与 C++

消费工程通过 `-DCMAKE_PREFIX_PATH=<SDK安装目录>` 找到 SDK：

```cmake
find_package(Sunfleck 1.0.0 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Sunfleck::sunfleck)
sunfleck_compile_page(page_bindings INPUT main.fleck OUTPUT bindings.hpp FORMAT cpp SYMBOL load_main)
add_dependencies(MyApp page_bindings)
target_include_directories(MyApp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
```

`Sunfleck::fleckc` 为安装的编译工具，生成头按 Fleck 声明自动适配字段、属性和命令。先包含模型定义，再包含生成头；`sunfleck::generated::load_main::load(app, model, xml)` 可加载原始 XML。完整 C/C++ 消费示例位于 SDK 的 `share/sunfleck/examples`。

## C#

消费项目导入 SDK 的 `share/sunfleck/Sunfleck.props`：

```xml
<Import Project="$(SunfleckSdkRoot)/share/sunfleck/Sunfleck.props" />
```

构建时传入 `-p:SunfleckSdkRoot=<安装目录>`。props 引用 `lib/net10.0/Sunfleck.dll`，并把对应平台的原生库复制到构建和发布输出。完整示例为安装包内的 `Consumer.csproj`；应用运行需要 .NET 10，Windows 原生库需要匹配架构的 MSVC 运行库。

## 配置与交付范围

Windows 安装式验收默认使用 x64 Release、MSVC 与动态 CRT，消费方应匹配 ABI、架构和构建配置。Linux 依赖通过 CMake 查找 Threads、X11/Xext 和 FreeType。独立 C++ Wasm 项目可以使用安装式 SDK，配置示例见 [文档站维护](../../docs/guides/documentation-site.md#准备-wasm-sdk)。.NET 的浏览器构建见 [Web 交付](web-delivery.md)。

安装布局支持整体移动；C++ 生成头中的应用源码位置属于消费工程自身的调试元数据。SDK 不包含 IDE VSIX、运行时字体或操作系统运行库，交付清单见 [分发与候选包](distribution.md)。

当前验证范围见 [验证与复现](validation.md)；安装与消费者须使用同一源码候选。

## 按语言分发与内建类型导航

完整安装树可用 `tools/sdk/package-language.py` 拆分为单语言 Windows SDK 和独立编辑器工具。输出目录必须不存在；该入口不覆盖应用代码。

```powershell
python tools/sdk/package-language.py --language c --sdk out/components-sdk --tools out/devtools/tools/development --output out/c-distribution
```

`--language` 支持 c、cpp、csharp。每份输出包括：

- `sdk`：本语言的公开声明、运行库及构建依赖。C 为 ui.h/elements.h 和原生库；C++ 为 .hpp、必需的 C ABI ui.h 和原生库；C# 为程序集、原生运行库、组件生成器、MSBuild 配置及完整的 C# 参考源码工程。
- `.tools/fleck`：共用 .NET 语言服务及依赖，与应用 SDK 分开。C/C++ 应用不链接这些托管程序集；编辑器仍需 .NET 10。
- `fleck.project.json`：language 和 sdk 路径。SDK 的 fleck.types.json 保存相对路径与类型/属性源码位置，整体移动后仍可定位。

VS Code 设置 `sunfleck.serverPath` 为 `${workspaceFolder}/.tools/fleck/Sunfleck.Development.dll`。C# 的模型导出工具也可使用该目录。C# 项目应通过 DefaultItemExcludes 排除 sdk/** 和 .tools/**，避免把接口定义当作应用源文件编译。

C# SDK 的 `types/Sunfleck.Navigation.csproj` 为独立的源码导航工程。将它与应用项目一起加入解决方案，并在 VS Code 的 `dotnet.defaultSolution` 中选择该解决方案，官方 C# 扩展即可继续解析父类、泛型属性描述和 partial 类型。应用仍通过 Sunfleck.props 引用 SDK 中的 Sunfleck.dll；不要给应用添加指向导航工程的 ProjectReference。完整安装树中的导航工程位于 `share/sunfleck/types`，按语言打包后位于 `sdk/types`。

例如应用的 `.slnx` 可以包含：

```xml
<Solution>
  <Project Path="MyApp.csproj" />
  <Folder Name="/SDK Reference Source/">
    <Project Path="sdk/types/Sunfleck.Navigation.csproj" />
  </Folder>
</Solution>
```

Fleck 的定义查询使用 SDK 元数据；打开 `.cs` 后继续按 F12 则由 C# 扩展解析，因此仅复制 Elements.cs 和 Components.cs 不够。FrameworkElement 为 partial 类，定义分别位于 Components.cs 和 FrameworkElement.cs；DataContextProperty 位于 Components.cs。多个定义时编辑器可能显示候选列表。已有窗口更新 SDK 和解决方案后执行 Developer: Reload Window。

内建声明与导航映射由 `tools/development/generate-element-types.py <schema.json>` 同时生成，覆盖普通节点、文档根及主题标签，数量以当前生成 schema 为准。公开声明保留属性类型、可绑定性及默认方向；不会把原本不可绑定的布局属性变成绑定目标。

```cpp
#include <sunfleck/elements.hpp>
sunfleck::elements::Slider slider(app);
slider.bind(sunfleck::elements::Slider::ValueProperty, model, "Score");
```

C 对应 `ui_Slider_create(app, &slider)` 与 `ui_element_bind(ui_Slider_node(&slider), &ui_Slider_ValueProperty, model, "Score", UI_BIND_DEFAULT)`。实例均借用 App 管理的原生节点，不独立销毁节点。

内建视觉控件继承 VisualElement 的公共属性；布局控件再继承 LayoutElement 的 Spacing/Padding。各语言 SDK 的映射包含 declaringType，继承属性定位到父类声明。C 的公共描述使用兼容宏别名，实例改用首成员嵌入父类，使用 ui_控件名_node 访问原生节点。

组件公共属性导航还需要随包的 componentAttributes 映射：C 的 component_attributes.h、C++ 的 components.hpp、C# 的 types/Components.cs。package-language.py 会同时复制并重定向这些相对路径。语言服务共用宿主视觉属性的类型、绑定能力和源码位置，DataContext/Template 则使用组件父类的专用描述。


组件独立消费检查使用 `python tests/sdk/check_components.py --sdk <安装目录> --configuration Release`；安装的是 Debug SDK 时选择 `--configuration Debug`（默认）。原生消费者必须与 SDK 的配置一致，避免 MSVC CRT 与迭代器调试级别不匹配。
