# 简单车辆示例（Simple Vehicle Demo）

展示如何使用本仓库的 LiDAR 框架快速在场景中启动一个最小示例，并通过内置 `RDF_LidarAutoRunner` 启动 demo。

Shows how to launch a minimal LiDAR example in the scene using the built-in `RDF_LidarAutoRunner`.

---

## 包含文件 / Included Files

- 运行脚本位于：`scripts/Game/Examples/SimpleVehicle/`（引擎编译路径），请在该路径查阅可运行脚本，例如 `SCR_SimpleVehicleDemo.c`。
  Scripts are in `scripts/Game/Examples/SimpleVehicle/`; see e.g. `SCR_SimpleVehicleDemo.c`.
- `README.md` — 本文档 / This document.

**快速提示 / Quick tip**: 若希望示例随项目一起被其他人轻松运行，请确保 `scripts/Game/Examples/SimpleVehicle/` 已包含在 `addon.gproj` 的编译输入中。  
Ensure `scripts/Game/Examples/SimpleVehicle/` is included in `addon.gproj` for others to run the example.

**车载 Bootstrap / Vehicle Bootstrap**: `RDF_VehicleLidarBootstrap.c` 在玩家上车时自动启用 LiDAR（1024 射线、30 m、10 Hz、120°×19° 矩形视场），下车停用。配置与说明见 `SpawnBind/README.md`。  
Enables LiDAR when player enters vehicle (1024 rays, 30 m, 10 Hz, 120°×19° rect FOV); disables when exiting. See `SpawnBind/README.md` for config.

---

## 要求 / Requirements

- 依赖本仓库中的 LiDAR 框架（已包含在此仓库中）。Depends on LiDAR framework (included in this repo).
- 在 Arma Reforger 开发环境中加载此 addon。Load this addon in Arma Reforger dev environment.

---

## 安装 / 使用步骤 / Installation & Usage

1. 将 `Examples/SimpleVehicle` 文件夹保留在 addon 目录（如果你已在仓库内则无需移动）。Keep `Examples/SimpleVehicle` in addon directory.
2. 确保 `Scripts/SCR_SimpleVehicleDemo.c` 在你的资源构建流程中被包含（一般放在 addon 的脚本目录会自动被收集）。Ensure the script is included in your build (usually auto-collected from addon scripts).
3. 重建资源数据库（在 Workbench 或通过命令行的构建步骤）。Rebuild resource database (Workbench or CLI).
4. 启动游戏或进入 Play 模式：Launch game or Play mode:
   - 脚本会在游戏运行后自动启动 demo（在 `SCR_BaseGameMode` 的生命周期内，脚本只会运行一次）。Demo starts automatically after game launch.
   - 或者在控制台手动执行（测试）：Or run manually in console:

```c
RDF_LidarAutoRunner.SetDemoConfig(RDF_LidarDemoConfig.CreateDefault(256));
RDF_LidarAutoRunner.SetDemoEnabled(true);
```

---

## 验证 / 测试 / Verification & Testing

- 启用后应能在玩家视角看到点云/射线（或在控制台看到 `RDF: SimpleVehicle demo started.` 日志）。You should see point cloud/rays in view, or console log `RDF: SimpleVehicle demo started.`
- 可修改射线数、间隔等配置来观察性能变化。Modify ray count, interval, etc. to observe performance.

---

## 说明 / Notes

- 该示例旨在最小化对项目结构的侵入，演示如何通过 API 启动演示配置。This example minimizes project intrusion; demonstrates API-based demo startup.
- 绑定到实际载具、导出 CSV 等见 `SpawnBind/README.md`。For vehicle binding and CSV export, see `SpawnBind/README.md`.
