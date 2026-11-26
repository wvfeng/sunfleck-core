# Web 独立应用与候选交付

候选脚本使用本地 Windows 与指定浏览器执行自动验收。真实系统输入法及读屏的独立复核见 [平台专项](validation.md#m9-linuxweb-平台专项)。静态包的发布和字体授权由项目方处理。

## 使用已有的静态站点包

`cpp.zip` 和 `dotnet.zip` 必须先完整解压，再通过 HTTP(S) 访问；不能双击 index.html 使用 file:// 打开。页面要加载 ES modules、Wasm、字体和其他资源，浏览器的本地文件来源限制会阻止相关请求。这两个包没有可执行程序，也没有双击启动服务器的入口。

本机候选目录已包含对应的解压站点。使用已安装的 Python，在框架根目录打开两个终端分别运行（端口可自行调整）：

```powershell
# 终端 1：C++
python tools/web/serve.py out/web-candidates/20261117-candidate-002/cpp --port 8787
```

```powershell
# 终端 2：.NET
python tools/web/serve.py out/web-candidates/20261117-candidate-002/dotnet --port 8788
```

随后在浏览器打开 `http://127.0.0.1:8787/` 或 `http://127.0.0.1:8788/`；工作区演示在同一地址后加 `?scenario=workspace`。保持服务终端运行，结束时按 Ctrl+C。使用其他解压位置时，将命令中的站点目录换成那个目录即可；服务根目录应直接包含 index.html。只浏览这些产物不需要安装 .NET SDK 或 Emscripten。

如果只搬运 ZIP 到另一台机器，需要另行提供 HTTP 静态服务器；也可将仓库的 `tools/web/serve.py` 一并带走，用 Python 启动。该脚本设置 Wasm/JS MIME 和 gzip 响应头，包内 `.gz` 文件由服务器选择，用户不应手动打开或重命名它们。这里只说明使用方法，不代表新增机器已进入验证范围。

## 创建可编辑应用

在框架根目录执行，目标目录必须不存在：

```powershell
python tools/web/new-app.py out/my-cpp-app --language cpp
python tools/web/new-app.py out/my-dotnet-app --language dotnet
```

生成应用拥有自己的 `main.cpp` / `Program.cs`、`main.js`、`index.html`、`workspace.fleck` 和 `message.txt`；复制的是可运行的计数器/工作区起点，可继续删改业务代码。`host.js` 和 `text-input.js` 继续引用框架维护的版本。源码引用模式不要求先安装全局 SDK，也不复制系统字体。已有目录会报错，不覆盖业务代码。

C++ 生成工程使用 `add_subdirectory` 消费共享核心，并调用 [sunfleck_add_web_app](../web/SunfleckWeb.cmake)；可修改 CMake 的 `SUNFLECK_ROOT` 指向另一个兼容源码副本。.NET 工程引用框架 C# 项目和共享宿主资源，将核心静态库链接进自己的运行时；移动框架后需要更新 csproj 的引用路径。普通应用可以更改自己的 JSExport/原生导出，但须同时更新 main.js；示例导出名不是额外稳定 SDK ABI。

```powershell
$env:EM_CACHE = "$PWD/out/web-dotnet-cache-emsdk" # 当前机器已验证的兼容缓存
./tools/web/build.ps1 -Target Cpp -CppProject "$PWD/out/my-cpp-app" `
  -BuildDirectory "$PWD/out/my-cpp-build" -EmsdkRoot "$PWD/out/toolchains/emsdk" `
  -Font C:/fonts/application.ttf
./tools/web/build.ps1 -Target Dotnet -DotnetProject "$PWD/out/my-dotnet-app/Application.csproj" `
  -Dotnet "$PWD/out/toolchains/dotnet-local/dotnet.exe" -EmsdkRoot "$PWD/out/toolchains/emsdk" `
  -Font C:/fonts/application.ttf -Publish
```

产物分别位于 `out/my-cpp-build/web` 和 `out/my-dotnet-app/bin/Release/net10.0/publish/wwwroot`。HTTP 服务必须指向整套资源根目录；生产服务器需要正确 Wasm MIME、gzip 内容编码、`Vary: Accept-Encoding`、当前共享资源的缓存重验证和版本原子替换。本地 `serve.py` 只用于验证。

`.NET -Publish` 先写入全新暂存目录，成功后替换项目的 `publish` 目录，避免保留旧版本的哈希 Wasm；替换失败会恢复旧目录。上一次产物保存在同级 `.publish-previous-*`，属于可清理的构建输出。候选检查还要求恰好一个当前原生 Wasm；压缩同时覆盖 ICU `.dat` 文件，完整包体预算包含未在首屏加载的资源。

保留样例 UI 时，可对独立应用运行原有用例；换成业务 UI 后必须使用相应业务断言，不能继续宣称样例用例覆盖新业务：

```powershell
$env:SUNFLECK_TEST_CASES = 'smoke,workspace'
./tools/web/verify.ps1 -SkipBuild -Font C:/fonts/application.ttf `
  -EmsdkRoot "$PWD/out/toolchains/emsdk" -Dotnet "$PWD/out/toolchains/dotnet-local/dotnet.exe" `
  -CppDirectory "$PWD/out/my-cpp-build/web" `
  -DotnetDirectory "$PWD/out/my-dotnet-app/bin/Release/net10.0/publish/wwwroot" `
  -EvidenceDirectory "$PWD/out/my-app-evidence"
```

## 字体与资源预算

```powershell
python tools/web/font-report.py C:/fonts/application.ttf --output out/font-report.json
# 应用有自己的必需字符集时：
python tools/web/font-report.py C:/fonts/application.ttf --text application-characters.txt --output out/font-report.json
```

工具读取 TTF/OTF 或 TTC face 0 的 Unicode cmap、名称/许可元数据和 OS/2 嵌入标志，记录原始/gzip 字节和 SHA-256。默认必需字符集为 ASCII 与中文回归基线；缺字使命令失败。实际输入和 Canvas 字形仍由浏览器文本测试验证。元数据/嵌入标志不构成分发授权，检查通过也不等于覆盖任意 Unicode。

当前保留完整字体，避免按样例文字裁剪后破坏用户任意中文输入；C++ 已复用预加载字体字节，.NET 也只请求一次字体。没有选择有许可的替代字体和产品字符集前，不自动改变字体或删除字形。

候选工具将“单宿主全部静态资源 gzip 后不超过 20 MiB、字体 gzip 不超过 14 MiB”作为当前样例的包体回归上限；这不是生产加载 SLA，也不是新的网络环境测试。首屏耗时、输入延迟由同环境 performance 场景记录，不以开发机一次采样设置硬性耗时门槛。

## 固定本地候选

在 VS x64 开发者 PowerShell 中运行；使用已有 Debug、D3D11 OFF、C# OFF 的 `out/web-native` 配置。`OutputDirectory` 必须是新目录，旧候选不会被覆盖。

```powershell
$env:EM_CACHE = "$PWD/out/web-dotnet-cache-emsdk"
$env:SUNFLECK_BROWSER_TEST_MODULES = "$PWD/out/web-test"
./tools/web/candidate.ps1 -Font C:/fonts/application.ttf `
  -EmsdkRoot "$PWD/out/toolchains/emsdk" -Dotnet "$PWD/out/toolchains/dotnet-local/dotnet.exe" `
  -OutputDirectory "$PWD/out/web-candidates/candidate-001"
```

脚本执行：

1. 构建双宿主，比较构建前后源码指纹；修改源码会终止候选生成。
2. 创建源码清单/快照和两套冻结静态站点，记录 Git HEAD 加包含未提交文件的内容指纹。快照范围含核心、绑定、示例、测试和构建工具；文档与 IDE 包不在源码身份内。
3. 记录工具链、字体覆盖及包体；执行原生 25 项、桌面 C# API 回归，并恢复原生 C# OFF 配置。
4. 清除测试筛选后对冻结站点运行双宿主浏览器回归。各场景保存 `result.json`、日志和截图，并汇总到 `suite-result.json`。
5. 核对源码未变、快照内容、完整测试集合、全部静态资源及 gzip 内容、字体身份和包体上限。只有全部通过才写入 `automated-validation-passed`，生成两个 zip 和 SHA256SUMS.json。

目前浏览器默认矩阵已加入双宿主外部拖放，共 22 组，而候选封包校验仍要求原有 20 组，会拒绝新报告。失败目录保留用于诊断。`candidate.json` 明确保留 `manualAcceptance: skipped-by-user` 和仅本地验证的分发状态；自动通过不能冒充人工体验或跨环境验收。

## 交付和许可边界

候选包含源码快照、应用静态资源、资源清单、环境身份、测试证据和完整哈希。ZIP 是本地可搬运产物；对外分发前需要项目方落实以下许可，不由工具推断：

| 组成 | 当前处理 |
|---|---|
| Sunfleck 源码及绑定 | 2026-11-17 用户选择 MIT，见 [LICENSE](../LICENSE)；旧冻结候选保持原身份，新候选纳入许可文件 |
| 验证字体 | Windows 系统字体仅用于本机验证；名称/嵌入标志报告不是再分发许可 |
| FreeType | 来自当前 Emscripten port；保留所用版本对应许可和版权通知，按适用许可条款分发 |
| .NET、Emscripten 运行库及其原生依赖 | 依所选运行时/工具链的 LICENSE、ThirdPartyNotices 和依赖清单处理；工具链版本已记录，但版本记录不是完整法律审核或全依赖 SBOM |

框架许可已选择 MIT；Web 全依赖 SBOM 和公开发布仍需按实际产物准备。此处已完成可复用的生成/构建/验证与打包机制；有许可的发布字体和框架分发条款需由项目方确定。


## 核心构建回归

M1 的属性/布局变更可用 `pwsh -File tests/web/check-core.ps1 -EmsdkRoot out/toolchains/emsdk` 检查现有 Wasm 核心。脚本重建共享核心，在 Node 中运行与 Windows 共用的数值字面量测试及宿主生命周期测试；禁用 Wasm 文件系统，未启用 pthreads，因此跨线程拒绝用例保留给 native/pthreads 构建。脚本会对每个编译和执行步骤检查退出码。这一结果不表示浏览器渲染、IME、读屏或 Linux 已通过验收。
