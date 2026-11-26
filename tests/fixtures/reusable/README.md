# Fleck 可复用控件回归测试

同一个 `UserCard.fleck` 通过 `primary`、`secondary` 两个标签创建独立实例，分别绑定 Alice 和 Bob。`Rating` 的 `Value` 默认双向绑定到 `Score`。测试切换到 `CompactRatingTemplate` 后检查控件行为和绑定回写。

在 Visual Studio x64 开发者 PowerShell 中，从仓库根目录执行：

```powershell
cmake -S . -B out/devtools -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUI_BUILD_CSHARP=ON
cmake --build out/devtools --target reusable_c reusable_cpp reusable_binary sunfleck_native
ctest --test-dir out/devtools -R '^reusable_' --output-on-failure

dotnet build tests/fixtures/reusable/csharp/Reusable.csproj
$env:PATH=(Resolve-Path out/devtools).Path+';'+$env:PATH
dotnet tests/fixtures/reusable/csharp/bin/Debug/net10.0/Reusable.dll --test
```

`controls.h` 是 C/C++ 的唯一类型声明来源：运行时注册和 `reusable_type_exporter` 共用同一组属性、事件、模板部件和基类声明。CMake 在构建时生成 `.fleckcomponents.json` 作为 `fleckc` 与语言服务的交换格式，应用源码不再手工维护这份 JSON。直接加载 `page.flkb` 的程序必须先调用同一注册函数。资源中包含组件定义，无需从当前目录读取 XML。生成 C 和二进制源码映射分别由 `reusable_c`、`reusable_binary_map` 测试验证。

每个内建 Fleck 标签都有对应的 `Sunfleck.Elements` 类型；例如 `TextBox.TextProperty` 和 `Slider.ValueProperty` 提供属性类型、默认绑定方向和 F12 定义位置。没有自定义属性的 `UserCard` 仍有明确类型：C# 在 [UserCard.cs](library/UserCard.cs) 中声明 `Example.UserCard : UserControl` 空类，C/C++ 在 [controls.h](controls.h) 中声明。如果省略 C# 空类，源生成器会依据 Fleck 的 `Class` 生成同名类型。

C# 应用引用 `library/Reusable.Controls.csproj`，该库把公开属性、事件、模板部件与 Fleck 资源导出给源生成器。应用的 `RegisterFleckComponents()` 自动注册被引用库，运行时不执行反射式类型扫描或业务 getter。示例把页面 XML 作为程序集内嵌资源；原始文件无需随发布目录分发。

模板中的 `PART_Value` 是必需的 Slider。自定义事件 `Changed` 由代码订阅；页面业务动作仍使用命令绑定。`UserControl.FindNode` / `ui_component_find_node` 只查询本实例的名称作用域，页面查找不会穿透组件。

组件语法与使用范围见 [组件文档](../../../../docs/guides/fleck-components.md#可复用组件)，测试和安装式消费入口见 [验证与复现](../../../docs/validation.md)。
