# SDK 迁移

当前契约为 **SDK 1.0.0 / API v5 / Fleck 资源 v16**。版本常量见 [公共头文件](../include/sunfleck/ui.h)。

## 重建顺序

1. 使用同一源码候选重建并安装原生库、头文件、C# 程序集、fleckc、组件生成器和开发工具。
2. 原生消费者使用 `find_package(Sunfleck 1.0.0 CONFIG REQUIRED)`。CMake 按 `SameMinorVersion` 匹配版本，升级时需要更新现有项目的 `find_package` 版本要求。
3. 重新导出组件元数据并编译页面，更新 `.flkb`、生成 C/C++ 页面和内嵌组件资源。只重新链接旧生成页面不够。
4. 重新编译应用并成套部署托管与原生产物。独立 Demos 的私有 SDK 和开发工具也需更新。
5. 从安装后的 SDK 验证三语言消费，记录实际源码、库和资源身份；不要沿用旧候选的通过结论。

## 版本入口

| 入口 | 当前契约 | 不匹配时 |
|---|---|---|
| C App 创建 | `ui_app_options.api_version=UI_API_VERSION`，当前为 5 | 返回 `UI_ERROR_VERSION_MISMATCH`，输出 App 为空 |
| 运行时查询 | `ui_api_version()` | C# 创建 App 前检查；旧库缺少入口也报告 VersionMismatch |
| 页面/组件资源 | `FLECK` + 字节 `0x10`，即 v16；生成 C/C++ 内嵌同一资源 | 版本字节不匹配时返回 `UI_ERROR_VERSION_MISMATCH`，要求重编译 |
| 手写 Builder | 添加节点前调用 `ui_fleck_require_version`，固定为编写该 Builder 时使用的 ABI 版本 | 缺失握手或版本错误在读取载荷前拒绝 |
| 组件类型描述 | `UI_COMPONENT_TYPE_VERSION` 当前为 2，独立于 App ABI | 读取属性数组前拒绝旧版描述 |

Fleck XML 的 `xmlns="urn:sunfleck:v1"` 是标记语法命名空间，不随 App ABI 改名；主题 `FLKTH` 有独立资源版本。生成器将资源版本固化在产物中，重新编译旧生成源码不能绕过检查。

## 尺寸与轨道

API v5 引入二维 Grid。省略行列定义时，所有子项默认放入同一单元格。旧单行页面需补上 `ColumnDefinitions` 和 `Grid.Column`，或改用 Flex。[二维 Grid](../../docs/guides/layout-properties.md#二维-grid)

API v3 删除 `UI_PROPERTY_PREFERRED_WIDTH`、生成类型中的同名描述、`ui_fleck_node_spec.preferred_width` 和 `ui_node_set_track` / C# `SetTrack`，后续属性编号发生变化。固定宽度使用 Width；Flex 的增长基础使用 MinWidth + Weight；DockHost 高度使用 Height。完整例外与替代写法见 [PreferredWidth 迁移](../../docs/guides/layout-properties.md#preferredwidth-迁移)。

Width/Height/Min/Max 的原生有效值从整数改为 `UI_VALUE_LENGTH`，C# 属性改为 `Length`。整数或 double 模型源仍可绑定；读取必须按 Length 处理。C# 数字隐式转换为 Pixels，Auto/Infinity 使用对应静态值。旧 C 整数 Height 便利入口的范围与读取限制见 [布局属性](../../docs/guides/layout-properties.md#绑定与代码)。

Margin 从字符串改为 `UI_VALUE_THICKNESS`；C++ 使用 `ui_thickness`，C# 使用 `Thickness`。Fleck 保留逗号分隔写法并支持小数，模型和 TemplateBinding 的属性类型需同步迁移。

## 间距与内边距

API v4 将 `ui_node_set_layout` 的两个参数及 `ui_fleck_node_spec.spacing/padding` 改为 double；旧二进制必须重编译。Spacing/Padding 的属性值改为 `UI_VALUE_DOUBLE`，C++/C# 生成描述使用 double。整数模型仍可绑定，Weight 保持整数。Fleck 数值范围仍为 0..64，Spacing 可写 `Theme`；API 中仅 -1 表示主题默认值。Padding 保持四边相同的单值，不是 Thickness。资源 v15 用 binary64 保存这两个值。

## 属性与上下文

- 既有 setter 与绑定共用属性层。清除局部值显露较低层；动画覆盖期间基础值继续更新。相同有效值不会重置光标/选区，无其他脏内容时 Poll(0) 可返回 Timeout。
- 普通节点和组件使用同一 DataContext 继承规则：显式空值截断继承，清除局部值恢复继承。模型销毁会移除其提供的绑定值，不保留文本快照；需要快照时应先显式解绑或赋值。
- 自定义属性的 Inherits 由框架维护；不同注册类型的同名属性不互相继承。原枚举仍表示内建宿主属性，注册标识优先解析组件自定义同名属性，并拒绝写入只读属性。

值优先级、生命周期、注册标识和代码示例见 [属性系统](../../docs/guides/property-system.md)。
## Windows 交互

Windows 原生窗口向 UI Automation 提供控件名称、角色、焦点、屏幕坐标及适用的交互模式。Fleck 的 `Id` 对应 AutomationId。文本框通过 Value 模式编辑，密码框拒绝读取明文。内建控件主题响应 Windows 对比度主题的启用、颜色变更及恢复。

按钮和图标按钮可用 `DragText` 提供文本，编辑控件可用 `AllowDrop="true"` 接受文本并发布编辑结果。以下页面可以直接操作。

```xml
<Window xmlns="urn:sunfleck:v1" Title="Drag and drop">
  <StackPanel>
    <Button Content="拖动地址" DragText="上海"/>
    <TextBox Id="address" AllowDrop="true"/>
  </StackPanel>
</Window>
```

原生应用在指针事件中调用 `ui_node_start_drag`，用 `ui_node_set_drop_handler` 注册接收的格式。C++ 对应 `Node::startDrag` 和 `setDropHandler`，C# 对应 `Node.StartDrag` 和 `SetDropHandler`。Windows OLE 支持复制 UTF-8 文本、多个文件路径，以及仅供同一应用使用的 64 位对象标识。C 回调中的路径和文本在回调期间有效。取消或未成功投放时 C 返回 `UI_CANCELLED`，C++ 和 C# 返回 false。使用拖放的宿主应在 STA 线程创建 Windows 原生窗口。
