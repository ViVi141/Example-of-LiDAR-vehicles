# LiDAR Framework — Developer Guide

Repository: https://github.com/ViVi141/Radar-Development-Framework  
Contact: 747384120@qq.com  
License: Apache-2.0

---

## 目标 / Goals

- 框架默认**静默**，只有在明确启用时才会运行。
  Framework is **silent** by default; runs only when explicitly enabled.
- 将扫描核心、可视化与演示隔离，便于替换与测试。
  Isolate scan core, visualization, and demo for easier replacement and testing.
- 对外暴露稳定的扩展点以便第三方复用与扩展。
  Expose stable extension points for third-party reuse and extension.

---

## 模块布局 / Module Layout

```
scripts/Game/RDF/Lidar/
  Core/
    RDF_LidarSettings.c        // 参数与校验 / Params and validation
    RDF_LidarTypes.c           // 数据结构（RDF_LidarSample 等）/ Data types
    RDF_LidarScanner.c         // 扫描与射线构建 / Scan and ray building
    RDF_LidarSampleStrategy.c  // 采样策略接口与默认均匀策略
    RDF_HemisphereSampleStrategy.c
    RDF_ConicalSampleStrategy.c
    RDF_StratifiedSampleStrategy.c
    RDF_ScanlineSampleStrategy.c
    RDF_SweepSampleStrategy.c  // 雷达式扇区旋转扫描 / Sweep sector scan
  Visual/
    RDF_LidarVisualSettings.c  // 可视化参数（含 m_RenderWorld 仅点云开关）
    RDF_LidarVisualizer.c      // 渲染与数据获取
    RDF_LidarColorStrategy.c   // 颜色策略接口与默认实现
    RDF_IndexColorStrategy.c
  Network/
    RDF_LidarNetworkAPI.c      // 网络同步 API（基类）
    RDF_LidarNetworkComponent.c // Rpl 实现
    RDF_LidarNetworkUtils.c    // 网络辅助工具（自动绑定）
    RDF_LidarNetworkScanner.c  // 网络扫描适配器

  // === 网络模块（分片 / 压缩 / 异步） ===
  // - RequestScan() 无参：组件的 owner 作为扫描主体。
  // - 序列化：服务器端使用 SamplesToCSV()，客户端 ParseCSVToSamples()。
  // - 分片：若载荷较大，服务器分片广播；客户端支持乱序组装。
  // - 压缩（可选）：RLE 压缩，前缀 RLE:。
  // - 异步扫描：ScanAsync() / ScanWithAutoRunnerAPIAsync()，超时回退本地扫描。
  // - 配置以原子字段形式传输并由服务器验证。

  Util/
    RDF_LidarSubjectResolver.c // 解析扫描主体（玩家/载具）
    RDF_LidarExport.c          // CSV 导出
    RDF_LidarSampleUtils.c     // 统计与过滤
    RDF_LidarScanCompleteHandler.c // 扫描完成回调基类
  Demo/
    RDF_LidarAutoBootstrap.c   // 统一 bootstrap（默认关闭）
    RDF_LidarAutoRunner.c      // 演示唯一入口与统一开关
    RDF_LidarDemoConfig.c      // 配置与预设工厂 Create*()
    RDF_LidarDemo_Cycler.c     // 策略轮换（仅调用 API）
    RDF_LidarDemoStatsHandler.c // 内置统计回调
```

---

## 数据流 / Data Flow

1. `RDF_LidarScanner` 根据 `RDF_LidarSettings` 生成 `RDF_LidarSample` 列表。
   `RDF_LidarScanner` generates `RDF_LidarSample` list from `RDF_LidarSettings`.
2. `RDF_LidarVisualizer` 使用 `RDF_LidarVisualSettings` 绘制点云与射线。
   `RDF_LidarVisualizer` draws point cloud and rays using `RDF_LidarVisualSettings`.
3. 演示工具（`RDF_LidarAutoRunner`）可选地驱动定期扫描循环。
   Demo (`RDF_LidarAutoRunner`) optionally drives periodic scan loop.

---

## 扩展点（示例）/ Extension Points (Examples)

- **采样策略 / Sample strategy**: 实现 `RDF_LidarSampleStrategy::BuildDirection()`，并通过 `RDF_LidarScanner.SetSampleStrategy()` 注入。Implement BuildDirection(), inject via SetSampleStrategy().
  - 内置：RDF_UniformSampleStrategy、RDF_HemisphereSampleStrategy、RDF_ConicalSampleStrategy、RDF_StratifiedSampleStrategy、RDF_ScanlineSampleStrategy.
- **可视化 / Visualization**: 实现或扩展 `RDF_LidarColorStrategy`；样本级 hook：
  - `BuildPointColorFromSample(ref RDF_LidarSample sample, float lastRange, RDF_LidarVisualSettings settings)`
  - `BuildPointSizeFromSample(ref RDF_LidarSample sample, float lastRange, RDF_LidarVisualSettings settings)`
- **输出 / Output**: 通过 `RDF_LidarVisualizer.GetLastSamples()` 获取数据并在外部导出（CSV/JSON）。Get data via GetLastSamples(), export externally (CSV/JSON).

`scripts/tests/lidar_sample_checks.c` 提供基础自检函数 / provides basic self-check for sample directions.

---

## Example 项目脚本 / Example Project Scripts (Example of LiDAR vehicles)

```
scripts/Game/Examples/SimpleVehicle/
  RDF_VehicleLidarBootstrap.c    // 上车自动 LiDAR，可配置射线/视场/CSV
  RDF_RectangularFOVSampleStrategy.c  // 矩形视场 120°×19°
  RDF_StatusPrinter.c            // 状态打印（可选）
  SCR_SpawnAndBindVehicleDemo.c
  SCR_SimpleVehicleDemo.c
  RDF_LidarFrameworkTest.c
```

- `RDF_RectangularFOVSampleStrategy`：矩形 FOV（水平×垂直），适于车载前向扫描。Rectangular FOV (H×V), suited for vehicle forward scan.
- `RDF_VehicleLidarBootstrap`：CSV 缓冲写入、扫描节流、仅扫描不渲染。CSV buffered write, scan throttling, scan-only mode.

---

## Demo 与隔离（统一 API + 统一开关）/ Demo and Isolation (Unified API + Switch)

- **统一开关 / Unified switch**: `RDF_LidarAutoRunner.SetDemoEnabled(true/false)` 为演示的唯一起停入口。
- **统一配置入口 / Unified config**: 所有演示通过 `RDF_LidarDemoConfig` 预设或自定义 config + `SetDemoConfig()` / `StartWithConfig()` 完成。
- 预设启动示例 / Preset examples:
  - `RDF_LidarAutoRunner.StartWithConfig(RDF_LidarDemoConfig.CreateHemisphere(256));`
  - `RDF_LidarAutoRunner.StartWithConfig(RDF_LidarDemoConfig.CreateConical(25.0, 256));`
- 自定义配置 / Custom config: `RDF_LidarDemoConfig cfg = new RDF_LidarDemoConfig();` 设置各字段后 `SetDemoConfig(cfg); SetDemoEnabled(true);`。若 demo 已开启，再次调用 `SetDemoConfig(cfg)` 会立即应用新配置。
- **统一 Bootstrap**: `SCR_BaseGameMode.SetBootstrapEnabled(true)` 在游戏启动时开演示；`SetBootstrapAutoCycle(true)` 可改为开局自动轮换策略。默认 bootstrap 为 **关闭**。
- **RDF_LidarDemoCycler**：仅调用 SetDemoConfig + SetDemoEnabled，用于策略轮换。
- **仅点云模式 / Point cloud only**: `SetDemoRenderWorld(false)` 时，在相机前绘制黑色四边形遮挡场景；点云在 NOZBUFFER 上层绘制。

---

## 性能建议 / Performance Tips

- 将 `m_RayCount` 限制在合理范围内（默认 512），并在运行时降低以减小开销。Keep m_RayCount reasonable (default 512), reduce at runtime to lower cost.
- 使用较大的 `m_UpdateInterval` 来降低频率开销，最小值 ≥ 0.01s。Use larger m_UpdateInterval to reduce frequency; min ≥ 0.01s.
- 减少 `m_RaySegments` 和点绘制数量以控制渲染负载。Reduce m_RaySegments and point count to control render load.

---

## 开发与贡献指南 / Development and Contribution

- **代码风格 / Code style**: 遵循项目现有约定与注释风格，文件放在相应模块目录下。Follow project conventions; place files in the corresponding module.
- **提交 PR / PR**: 在 PR 描述中说明变更动机、兼容性影响与性能影响（如有）。Describe motivation, compatibility, and performance impact in PR.
- **测试 / Testing**: 尽量提供自检脚本或简单场景验证。Provide self-check scripts or simple scene validation.
- **CI 建议 / CI**: 添加 GitHub Actions 工作流以自动运行基础验证（lint、构建检查、文档链接检查）。Add GitHub Actions for lint, build, and doc link checks.
