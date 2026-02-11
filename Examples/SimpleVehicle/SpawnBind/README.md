# 生成并绑定到载具的示例（Spawn & Bind Vehicle Demo）

展示如何将 LiDAR 绑定到“实际载具”（或玩家所处载具），并在运行时对该实体执行扫描与可视化。

Shows how to bind LiDAR to an actual vehicle (or the player's vehicle) and perform scan + visualization at runtime.

---

## 行为说明 / Behavior

- 启动后脚本会尝试通过 `RDF_LidarSubjectResolver.ResolveLocalSubject(true)` 查找本地玩家所处的载具并将 LiDAR 绑定到该实体。
  On start, script finds local player's vehicle via `ResolveLocalSubject(true)` and binds LiDAR to it.
- 若未找到载具，脚本会退回绑定到玩家本体（仍可观察点云/射线）。
  If no vehicle, falls back to player (point cloud/rays still visible).
- 脚本会在 `OnUpdate` 中每帧调用 `RDF_LidarVisualizer.Render(subject, scanner)` 来实时渲染点云。
  OnUpdate calls `RDF_LidarVisualizer.Render(subject, scanner)` each frame for real-time point cloud.

**自动 Bootstrap（已编译并随 addon 加载）/ Auto Bootstrap (compiled with addon)**:

- `RDF_VehicleLidarBootstrap.c` 为 modded SCR_BaseGameMode，在 EOnFrame 中按 `s_UpdateInterval` 检测玩家是否在载具内，上车启用 LiDAR，下车自动停用。不依赖 GetCallqueue，使用定时循环自动检测玩家状态。
  Modded SCR_BaseGameMode; checks vehicle state in EOnFrame at s_UpdateInterval; enables LiDAR on enter, disables on exit.
- **默认配置 / Default config**: 1024 射线、30 m 探测、10 Hz、120°×19° 矩形视场（64×16 分辨率）、RDF_RectangularFOVSampleStrategy。
- **可配置项**（在 `RDF_VehicleLidarBootstrap.c` 顶部修改）/ **Config** (edit top of file):
  - `s_RayCount`：射线数量 / Ray count
  - `s_Range`：探测距离（米）/ Range (m)
  - `s_UpdateInterval`：扫描间隔（秒），10 Hz = 0.1 / Scan interval (s)
  - `s_RectFOVHorizDeg` / `s_RectFOVVertDeg`：矩形视场角（度）/ Rect FOV (deg)
  - `s_RectCols` / `s_RectRows`：分辨率（需满足 cols×rows = s_RayCount）/ Resolution
  - `s_ScanWithoutVisualization`：为 true 时仅扫描，不绘制点云/射线 / Scan-only mode
  - `s_OutputCSV`：是否输出 CSV 到磁盘 / CSV output
  - `s_CSVFlushInterval`：CSV 缓冲写入间隔（秒）/ CSV flush interval (s)
  - `s_TargetMode`：0=仅玩家, 1=仅载具, 2=两者 / 0=player only, 1=vehicle only, 2=both

**运行脚本位置 / Script location**: `scripts/Game/Examples/SimpleVehicle/`。请确保该路径已包含在 `addon.gproj` 的编译输入中。Ensure path is in `addon.gproj` for compilation.

**注意 / Note**: 本 addon 在脚本加载时自动启用车载 bootstrap。可通过控制台或修改配置调整。Addon auto-enables vehicle bootstrap on load; adjust via console or config.

**游戏模式 / Game mode**: 使用 `modded SCR_BaseGameMode`，支持所有基于 SCR_BaseGameMode 的场景。Uses modded SCR_BaseGameMode; supports all SCR_BaseGameMode-based scenarios.

---

## 快速测试 / Quick Test

1. 将 `Examples/SimpleVehicle/SpawnBind` 保留在 addon 中并重建资源数据库。Keep SpawnBind in addon, rebuild resource DB.
2. 启动游戏并确保本地玩家处于一个载具内（若没有载具，脚本会退回到玩家本体）。Launch game, ensure player is in vehicle (or script falls back to player).
3. 观察屏幕上的点云/射线或控制台输出。Observe point cloud/rays on screen or console output.

```c
// 若想手动绑定到载具或调整参数，可在控制台执行：Manual bind / adjust params in console:
RDF_LidarScanner scanner = new RDF_LidarScanner();
RDF_LidarSettings s = scanner.GetSettings();
s.m_RayCount = 512;
s.m_Range = 100.0;
RDF_LidarVisualizer viz = new RDF_LidarVisualizer();
viz.Render(RDF_LidarSubjectResolver.ResolveLocalSubject(true), scanner);
```

---

## 扩展建议 / Extension Ideas

- 在脚本中集成预制体创建接口，可在地图上自动生成载具并绑定 LiDAR。Integrate prefab spawn; auto-spawn vehicles on map with LiDAR.
- 将绑定逻辑改为组件形式，附加到载具预制体上。Refactor binding as component, attach to vehicle prefab.
