# 分发与候选包

Sunfleck 自有源码采用 [MIT](../LICENSE)。MIT 不替代外部依赖、操作系统字体及工具链的许可。

## GitHub Releases

推送 `v*` 标签会触发 SDK 构建。所有平台通过后，工作流将以下文件上传到同一标签的 GitHub Release 的 Assets 区域。[发布工作流](../.github/workflows/release-sdk.yml)

| 平台 | 下载文件 |
| --- | --- |
| Windows x64 | `Sunfleck-SDK-windows-x64.zip` |
| Linux x64 | `Sunfleck-SDK-linux-x64.tar.gz` |
| macOS Apple Silicon | `Sunfleck-SDK-macos-arm64.tar.gz` |
| WebAssembly | `Sunfleck-SDK-web-wasm32.tar.gz` |

桌面包包含 C、C++ 和 C# SDK。Linux 包在 Ubuntu 24.04 上构建。Web 包包含用于 Emscripten 3.1.56 的 C/C++ 静态库、CMake 配置和浏览器宿主资源。编译 Fleck 页面时，从同一版本的桌面 SDK 获取 `fleckc`。[安装式 SDK](sdk.md) · [Wasm SDK 用法](../../docs/guides/documentation-site.md#准备-wasm-sdk)

编辑器扩展由 [Sunfleck DevTools](https://github.com/wvfeng/sunfleck-devtools) 单独发布。

| 编辑器 | 下载文件 |
| --- | --- |
| VS Code · Windows x64 | `Sunfleck-VSCode-win32-x64.vsix` |
| VS Code · Linux x64 | `Sunfleck-VSCode-linux-x64.vsix` |
| VS Code · macOS Apple Silicon | `Sunfleck-VSCode-darwin-arm64.vsix` |
| Visual Studio · Windows x64 | `Sunfleck-Visual-Studio-win-x64.vsix` |

扩展包附带对应平台的 Fleck 语言服务，需要 .NET 10 运行时。Visual Studio 扩展适用于 17.14 及以上的 x64 版本，具体范围以 VSIX 清单为准。[开发工具接入](../../docs/guides/devtools.md)

## 依赖与许可清单

| 组件 | 用途与是否打包 | 许可／交付处理 |
| --- | --- | --- |
| Sunfleck 原生库、头文件、工具、C# 绑定 | SDK 内包含 | MIT，附 LICENSE |
| Windows 系统 DLL（user32、gdi32、imm32、dwmapi、advapi32、D3D11 等） | 系统提供，不复制 | Microsoft Windows 许可；按实际 PE 导入表记录 |
| MSVC / Universal CRT | 当前 Windows Release 原生库运行依赖，不随 SDK 复制 | 使用 Microsoft 官方 Visual C++ Redistributable；依赖扫描记录实际 DLL，不能以 SDK 的 MIT 覆盖 |
| .NET 10 runtime | C# 消费应用运行依赖，不随 SDK 复制 | .NET Runtime 的 MIT 及第三方 notices，应用自行选择 framework-dependent 或合规自包含发布 |
| FreeType、X11/Xext、EGL/GLES / ANGLE | Linux／可选后端／Web 所需；当前 Windows 默认 SDK 不包含 | 对应项目许可；启用或随包分发时补充版本、许可文本和组件记录 |
| Emscripten、.NET Browser runtime、ICU 等 | Web 构建／产物依赖，独立于原生 SDK | 参照实际工具链及 Web 包内容生成交付清单，不能沿用原生 SDK 的 SBOM 作为完整 Web SBOM |
| Windows 中文字体 | 本机 Web 验证使用，不放进原生 SDK | 系统字体不默认获得再分发授权；公开 Web 交付前选用有许可字体及明确字符集 |
| VS Code 扩展依赖 | 独立 VSIX，不放进原生 SDK | 维护 npm 锁文件和各依赖许可证；扩展打包另行审计 |

## 本地候选身份

`tools/sdk/package.py` 对安装树生成逐文件 SHA-256 清单、SPDX 2.3 SBOM、依赖扫描记录及 ZIP，同时归档本次源码快照和验证证据。候选名称必须唯一，拒绝覆盖。源码身份由实际文件内容计算，包括未提交源码，Git HEAD 仅作背景。ZIP 是 SDK 归档，不是 Web 站点启动器。

产物验证检查清单遗漏、篡改、ZIP 解压后的哈希与路径安全。SBOM 标明当前原生 SDK 的范围和外部运行依赖；未知版本或许可用 `NOASSERTION`，不编造推断。本机候选通过不能代表未知机器／工具链兼容性，公开发布前应按目标系统验证运行依赖。

框架基线入口与测量边界见 [性能基线](performance.md)，SDK 构建和消费见 [安装式 SDK](sdk.md)。

## 本机候选验收入口

```powershell
python tests/sdk/test_package.py
python tools/sdk/package.py out/sdk-candidates/local-001 --evidence out/sdk-validation --performance out/performance/baseline.json
python tools/sdk/package.py out/sdk-candidates/local-001 --verify
```

第二条命令创建新候选，已存在时拒绝覆盖；重跑应选择新目录。test_package.py 检查内容篡改、清单遗漏／额外成员、重复成员与越界路径。生成候选包含 `sdk.zip`、`source.zip`、`candidate.json`、`validation.json`、`consumers.xml`、`dependencies.json`、`sbom.spdx.json` 及 `performance.json`。本机验证没有对外发布，也没有加入 Windows 字体、MSVC 或 .NET 运行时二进制。
