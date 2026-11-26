# Windows Skia 原生 D3D12 运行时

Windows 使用 **Skia Ganesh → 原生 D3D12** 绘图，无需随应用分发 ANGLE 或 SkiaSharp。Windows 的字体、排版、文字簇、编辑命中和 IME 几何继续由 DirectWrite 提供。隔离探针中，SkParagraph 对家庭 emoji 的点命中曾返回字素内部索引，切换排版也会改变行高，因此沿用 DirectWrite 的排版和编辑契约。

## 当前接入范围

`UI_ENABLE_SKIA_D3D12=ON` 将 Windows `UI_AUTO` 接入 Skia D3D12。C、C++、C# 使用同一个原生实现，无需增加语言专属后端。此选项当前为显式启用，以便未准备 Skia 依赖的普通构建继续可用。

| 选择 | 行为 |
|---|---|
| 启用 Skia 的 `UI_AUTO` | 尝试硬件 D3D12；初始化或呈现失败后使用 CPU |
| 未启用 Skia 的 `UI_AUTO` | 保留既有自动选择行为 |
| `UI_D3D11` | 保留显式原生 D3D11 路径，需启用 `UI_ENABLE_D3D11` |
| `UI_SOFTWARE` / Headless | 保留既有 CPU 路径 |

正式运行不会自动选择 WARP。WARP 仅供测试构建显式注入。每个窗口独立持有 Skia 上下文、队列和双缓冲交换链。flip-model 绑定到不接收输入的专用子窗口，避免污染主窗口的 GDI 回退路径；GPU 失败时销毁呈现子窗口，保留主窗口、焦点和 IME 宿主。尺寸变化前等待在途工作，正常呈现只等待即将复用的缓冲。设备失效或等待超时返回失败，由应用现有路径释放 GPU 并回退 CPU。

CPU 提供参考像素。Skia 回放矩阵、裁剪、图层、矩形和 DirectWrite 字形覆盖命令。画刷、Shape、遮罩、效果及图像可以先在软件端生成纹理再上传。显式 D3D11 对简单帧使用矩形快路径，对复杂帧上传 CPU 合成结果。

### Fleck 绘图

通过属性元素设置复合对象。例如，在 `TextBox.RenderTransform` 中放入 `TransformGroup`，按声明顺序组合缩放、旋转等变换。`RenderTransformOrigin="0.5,0.5"` 表示绕控件中心变换。需要为变换后的控件预留布局空间时，使用 `LayoutTransform`。

`Border.Background`、形状的 `Fill` 和 `Stroke` 支持画刷。渐变使用子元素 `GradientStop` 声明偏移和颜色。子树 `Opacity` 在图层合成时应用一次。`Path.Data` 支持 M、L、H、V、Q、C、Z 及小写相对命令。[完整 Fleck 示例](../../docs/snippets/fleck/drawing.fleck)

## 构建

使用 x64 Visual Studio 开发者终端。固定候选为 `e26a2f19555c0cc7cfe5f457d6555bcbf22aaca1`，必须应用补丁后重新构建静态库。补丁修复 CPU 专用描述符堆的 GPU 句柄查询，以及新分配渲染和深度目标首次使用前的初始化，覆盖普通目标和 MSAA。准备步骤见 [探针说明](../tools/development/skia-spike/README.md#preparing-the-accepted-source-candidate)，补丁见 [d3d12-fixes.patch](../tools/development/skia-spike/d3d12-fixes.patch)。

```powershell
python tools/development/skia-spike/prepare-d3d12.py --skia .build-support/skia-evaluation/skia --apply
New-Item -ItemType Directory -Force .build-support/skia-evaluation/skia/out/sunfleck-d3d12 | Out-Null
Copy-Item tools/development/skia-spike/d3d12.gn .build-support/skia-evaluation/skia/out/sunfleck-d3d12/args.gn
# 在已有 GN/Ninja/Python 的开发环境中重新生成和串行构建：
.build-support/skia-evaluation/skia/bin/gn.exe gen .build-support/skia-evaluation/skia/out/sunfleck-d3d12 --root=.build-support/skia-evaluation/skia
ninja -C .build-support/skia-evaluation/skia/out/sunfleck-d3d12 -j 1 skia

cmake -S . -B out/skia-runtime -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUI_ENABLE_SKIA_D3D12=ON `
  -DUI_SKIA_ROOT="$PWD/.build-support/skia-evaluation/skia" `
  -DUI_BUILD_CSHARP=ON -DUI_BUILD_EXAMPLES=OFF -DUI_BUILD_DEMOS=OFF
cmake --build out/skia-runtime --target skia_d3d12_surface_test win32_smoke native_editing_test fleckc sunfleck_sdk_managed -j 1
ctest --test-dir out/skia-runtime -L skia --output-on-failure -j 1
cmake --install out/skia-runtime --prefix out/skia-sdk
```

`d3d12.gn` 中的 Visual Studio 路径和工具链版本须与实际环境一致；GN 的 Python 解释器也应指向真实安装，而非 WindowsApps 别名。必要时指定 `--script-executable=<python.exe>`，并确保 Ninja 执行期间 `python3` 可用。首次构建前需创建 `out/sunfleck-d3d12` 目录。

候选静态库使用 Release `/MD`；当前 Sunfleck Skia 构建也使用 `/MD`，包括选择 Debug 配置时；私有 Skia 头文件显式使用 `SK_RELEASE`，避免与 Release 静态库的调试布局不一致。安装消费者应使用匹配的 CRT 配置，现阶段推荐 Release。`UI_SKIA_LIBRARY_DIR` 可以覆盖库目录；源码校验不能证明外部库由该源码生成，必须重新构建匹配候选，不能把校验成功解释成旧二进制自动可信。

安装 SDK 包含四个私有静态库及许可证，CMake 包通过安装相对位置链接它们，不要求消费者持有 Skia 源码。C# 继续使用 Sunfleck 原生 DLL。公共头文件不暴露 Skia 类型。

生产式 SDK 验收使用关闭测试注入的独立构建，并移动安装前缀后验证三语言消费者：

```powershell
pwsh -NoProfile -File tools/sdk/validate.ps1 -SkiaRoot "$PWD/.build-support/skia-evaluation/skia"
```

该入口串行编译，记录安装文件哈希、候选源码身份及 PE 依赖，并检查没有 ANGLE 导入。它的消费者测试是 Headless API/安装验证，不能代替窗口呈现测试。

## 定向验证

`skia` 测试组使用测试专用 WARP 注入，覆盖双窗口、不同表面尺寸、重复呈现与调整尺寸、透明混合/裁剪的 CPU 图像对照、D3D12 调试层错误、销毁重建、实际窗口交互、初始化/呈现失败后的 CPU 回退，以及现有文字编辑/选区/双向光标/IME 几何回归。

在有物理 GPU 的环境将 `UI_SKIA_TEST_WARP=OFF` 后重新配置，再运行同一个 `ctest -L skia` 分组。该模式不设置 WARP，表面测试要求成功打开硬件 D3D12，窗口测试也断言 GPU 呈现；CPU 回退不能冒充硬件通过。初始化/呈现失败用例仍显式注入失败以验证回退。

表面对照覆盖矩形和组合场景，单通道最大容差为 3。调试层 error/corruption 必须为 0；不将已有采样器等 warning 计成 error。表面尺寸测试不能替代真实多屏 DPI 和物理 GPU 验收。测试构建环境变量以 `SUNFLECK_TEST_SKIA_D3D12_` 开头；发布构建使用 `BUILD_TESTING=OFF`，不编入这些注入入口。

桌面像素测试从最终合成画面取样，flip-model 交换链应在可交互桌面运行。2026-11-20 的带补丁隔离探针在 RTX 2080 Ti（驱动 `32.0.16.1088`）与 WARP 上通过，调试层错误为 0。结果位于 `out/skia-evaluation/d3d12-{hardware,warp}-fixed/result.json`。探针覆盖显式设备重建，真实驱动设备移除仍需单独复验。硬件适配器身份、组合场景差值及 Windows 候选记录见 [Windows 验收](validation.md#windows-验收)。
