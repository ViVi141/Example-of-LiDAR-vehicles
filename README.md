# Example of LiDAR vehicles

Arma Reforger 车载 LiDAR 示例 addon，基于 [Radar Development Framework](https://github.com/ViVi141/Radar-Development-Framework) 实现上车自动扫描与点云导出。

Arma Reforger vehicle LiDAR example addon. Uses [Radar Development Framework](https://github.com/ViVi141/Radar-Development-Framework) for automatic in-vehicle scanning and point cloud export.

---

## 依赖 / Dependencies

- Radar Development Framework（RDF）
- Arma Reforger / Workbench

---

## 项目结构 / Project Structure

```
scripts/Game/Examples/SimpleVehicle/   # 脚本 / Scripts
  RDF_VehicleLidarBootstrap.c         # 上车自动 LiDAR / Auto LiDAR on vehicle
  RDF_RectangularFOVSampleStrategy.c  # 矩形视场采样 / Rectangular FOV sampling
  SCR_SpawnAndBindVehicleDemo.c
  SCR_SimpleVehicleDemo.c
  RDF_LidarFrameworkTest.c
  RDF_StatusPrinter.c
tools/lidar_viewer/                   # Python 点云查看器 / Python point cloud viewer
  lidar_viewer.py
  requirements.txt
Examples/SimpleVehicle/               # 示例与文档 / Examples and docs
  README.md
  SpawnBind/README.md
```

---

## 功能 / Features

- **上车自动 LiDAR**：玩家进入载具后自动开始扫描，下车停用
  **Auto LiDAR on vehicle**: Scanning starts when player enters vehicle, stops when exiting.
- **矩形视场**：120°×19° 前向 FOV，减少射向天空的射线
  **Rectangular FOV**: 120°×19° forward FOV, fewer rays toward sky.
- **可配置**：射线数、探测距离、扫描频率、CSV 输出、可视化开关
  **Configurable**: Ray count, range, scan rate, CSV output, visualization toggle.
- **CSV 导出**：扩展格式，支持缓冲写入，可配合 `tools/lidar_viewer` 离线查看
  **CSV export**: Extended format with buffered writes, works with `tools/lidar_viewer` for offline viewing.

---

## 默认配置 / Default Configuration

| 参数 / Parameter | 值 / Value |
|------|-----|
| 射线数 / Ray count | 1024（64×16） |
| 探测距离 / Range | 30 m |
| 扫描频率 / Scan rate | 10 Hz |
| 视场角 / FOV | 120°×19° |
| 角分辨率 / Angular resolution | 约 1.875°×1.1875° |

---

## 使用 / Usage

1. 将 addon 加入 Workbench 工程并构建
   Add addon to Workbench project and build.
2. 启动游戏，进入载具
   Launch game and enter a vehicle.
3. LiDAR 自动启用（可修改 `RDF_VehicleLidarBootstrap.c` 顶部配置）
   LiDAR enables automatically (edit top of `RDF_VehicleLidarBootstrap.c` to change config).

**游戏模式 / Game mode**: 使用 `modded SCR_BaseGameMode` 的 EOnFrame 驱动，支持所有基于 SCR_BaseGameMode 的场景（工作台、Conflict、Campaign 等）。Uses modded SCR_BaseGameMode EOnFrame; supports all SCR_BaseGameMode-based scenarios.

CSV 输出路径 / CSV output path: `$profile:LiDAR/lidar_live_N.csv`

---

## 点云查看器 / Point Cloud Viewer

```bash
cd tools/lidar_viewer
pip install -r requirements.txt
python lidar_viewer.py "path/to/lidar_live_1.csv"
```

详见 `tools/lidar_viewer/README.md`。
See `tools/lidar_viewer/README.md` for details.

---

## 文档 / Documentation

- [API.md](API.md) — API 摘要 / API reference
- [DEVELOPMENT.md](DEVELOPMENT.md) — 开发指南 / Developer guide
- [Examples/SimpleVehicle/SpawnBind/README.md](Examples/SimpleVehicle/SpawnBind/README.md) — 车载 Bootstrap 配置说明 / Vehicle Bootstrap config
