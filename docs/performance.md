# 可复现性能基线

本机基线通过安装后的 SDK 构建 `tests/performance`，与消费应用使用相同公开接口，不依赖框架内部头文件。

```powershell
cmake -S tests/performance -B out/performance -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=<SDK安装目录>"
cmake --build out/performance
python tools/sdk/benchmark.py out/performance/sunfleck_baseline.exe out/performance/baseline.json
```

固定为 Windows x64 Release、Headless CPU、800×600，分别构建 100 与 1000 个绑定文本节点。每个场景启动 5 个独立进程，每进程预热 5 次、测量 50 次。输出保留原始样本、二进制 SHA-256、OS/CPU 和统计值；不同硬件／构建／场景不能直接合并。

| 指标 | 计时边界 |
| --- | --- |
| startupMs | 进程内 App 创建、模型／页面加载至首次软件渲染完成 |
| commandNotifyFrameMs | 公共命令激活、属性通知至 Poll 完成软件帧；不是物理键盘／鼠标到屏幕延迟 |
| refreshFrameMs | 同结构 XML 事务刷新、绑定与软件帧完成；不含磁盘读取或 IDE 监听 |
| idlePollMs | 无修改时 Poll(0) 调度开销 |
| privateLoadedBytes / privateAfterBytes | Windows 进程 private bytes，分别在首次帧后和全部循环后采样；不是 GPU 内存 |
| processWallMs | 整个进程（含所有循环）墙钟时间，用于复现实验总成本，不作为启动耗时 |

大量节点超出可见视口，因此该场景同时包含绑定／树遍历成本和可见区域绘制，不代表 1000 个可见控件。OS 文件缓存不主动清空；首次加载值不是严格冷启动。报告 P95 使用最近秩法，启动与内存仅 5 个样本，尾部判断需增加样本量。

Web 测量使用独立宿主测试，范围见 [Web 回归](web.md#回归与证据)。当前基线没有 GPU 完成时间、物理输入呈现、跨设备或全控件性能承诺；后续优化应在同一环境和场景进行前后对照。

## 比较与验收

性能门禁在交付阶段执行。比较前后源码时，使用同机、同配置、同字体和同负载，记录源码与二进制哈希及原始样本。中位数或 P95 回退超过 15% 时重复测量并分析原因。

2026-11-21 的 Windows x64 Release 验收以同机重建的 `f9e57cd` 安装 SDK 为基线。六组交替顺序的配对测量中，有两组的空闲 Poll P95 单次超限。汇总每种构建、每种规模各 30 个独立进程后，28 个中位数和 P95 均在 15% 门限内。最大回退为 100 节点的空闲 Poll P95，从 4.8 微秒升至 5.2 微秒（+8.3%）。原始样本、各轮差异和最终统计位于 `out/gate-20261121/` 的 `*-perf-*.json`、`performance-gate*.json` 及 `performance-pooled.json`。

历史版本的单批数字不能证明当前版本无回退。有效基线保存在对应候选的输出目录；旧日志或二进制已缺失时重新采集，不继续引用不可复现的通过结论。
