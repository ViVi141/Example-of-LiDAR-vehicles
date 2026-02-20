# LiDAR HUD 配置说明

## ✨ 核心特性

- ✅ **完全自动化** - 无需任何控制台命令
- ✅ **4096 条射线** - 高密度扫描（~40,960 pts/s @ 10 Hz）
- ✅ **仅 HUD 显示** - 不绘制 3D 点云/射线，性能更优
- ✅ **智能切换** - 进入车辆自动显示，离开自动隐藏

---

## 当前配置

项目已更新为以下配置：
- **射线数量**: 4096（64×64 分辨率）
- **探测距离**: 30 米
- **扫描频率**: 10 Hz
- **视场角**: **60°×30°（前向扫描）** 🎯  
- **扫描模式**: 仅前向（±30° 水平，±15° 垂直）
- **3D 可视化**: 已关闭
- **显示方式**: HUD（屏幕左下角）

---

## HUD 自动启动机制

### 进入车辆时 ⬆️
系统自动执行：
1. ✅ 启动 LiDAR 扫描（4096 条射线）
2. ✅ 显示 HUD 面板
3. ✅ 设置显示范围（30 米）
4. ✅ 开始实时更新点云数据
5. ✅ 控制台输出：`RDF: LiDAR HUD auto-started with 4096 rays`

### 离开车辆时 ⬇️
系统自动执行：
1. ✅ 隐藏 HUD 面板
2. ✅ 停止扫描
3. ✅ 控制台输出：`RDF: LiDAR HUD hidden (vehicle exited)`

**无需任何手动操作！**

---

## 🚀 快速开始

1. **构建项目** - 在 Workbench 中构建 addon
2. **启动游戏** - 进入任意游戏模式
3. **进入车辆** - 上车后 HUD 自动显示
4. **查看点云** - 左下角蓝色面板显示实时扫描数据

就这么简单！🎉

---

## HUD 功能说明

RDF_LidarHUD 提供以下功能：

- **PPI 俯视图**: 210×210 像素圆盘，显示俯视点云
- **距离环**: 50% 和 100% 量程参考环
- **命中统计**: 显示命中数和距离范围
- **颜色编码**: 绿色=近距离，黄色=中距离，红色=远距离
- **罗盘标记**: F(前) / B(后) / L(左) / R(右)
- **自动更新**: 每次扫描完成后自动刷新（10 Hz）
- **地形过滤**: 自动过滤掉地形点，仅显示车辆、建筑等实体目标 🆕
- **实体去重**: 每个 `IEntity` 仅显示一次（若多条射线命中同一实体则去重），减少 HUD 冗余显示 🆕  
- **显示优化**: 调试输出与 HUD 优先显示 `prefab`（如 `Garage_E_02`）；若 prefab 不可用则回退到实例名或类型名。HUD 会尝试从样本的 `m_ColliderName` 字段读取并显示该可读名称（若场景中在实体属性里设置了实例名，也会被展示） 🆕

## 技术实现

HUD 集成在 `RDF_VehicleLidarBootstrap.c` 中：
- 启动逻辑：进入车辆时调用 `RDF_LidarHUD.Show()` 和初始化设置
- 停止逻辑：离开车辆时调用 `RDF_LidarHUD.Hide()`
- 数据更新：每次扫描完成后调用 `RDF_LidarHUD.FeedSamples(samples)`
- **地形过滤**：使用 `entity.Type()` 方法识别实体类型，过滤掉包含 "Terrain" 或 "Ground" 的类型，只显示实体目标（车辆、建筑等） 🆕
- **调试输出**：每次扫描打印前 3 个实体的类型和距离信息，便于了解周围目标

无需手动操作，一切都是自动的！

## 高级用法（可选）

如果你想在其他场景中手动使用 HUD（不依赖车辆 Bootstrap），可以参考以下代码：

```c
// 手动显示 HUD
RDF_LidarHUD.Show();
RDF_LidarHUD.SetDisplayRange(30.0);
RDF_LidarHUD.SetMode("Custom Mode");

// 手动扫描并更新 HUD
RDF_LidarScanner scanner = new RDF_LidarScanner();
scanner.GetSettings().m_Range = 30.0;
scanner.GetSettings().m_RayCount = 4096;

array<ref RDF_LidarSample> samples = new array<ref RDF_LidarSample>();
IEntity subject = RDF_LidarSubjectResolver.ResolveLocalSubject();
scanner.Scan(subject, samples);
RDF_LidarHUD.FeedSamples(samples);

// 手动隐藏 HUD
RDF_LidarHUD.Hide();
```

## 配置详情

### RDF_VehicleLidarBootstrap.c
- `s_RayCount = 4096`: 射线数量
- `s_RectRows = 64`: 行数（64×64 = 4096）
- `s_ScanWithoutVisualization = true`: 关闭 3D 可视化
- `vs.m_DrawPoints = false`: 关闭点云绘制（仅当 visualization 启用时）
- `vs.m_DrawRays = false`: 关闭射线绘制（仅当 visualization 启用时）

### SCR_SimpleVehicleDemo.c
- `m_RayCount = 4096`: 初始射线数
- 自适应范围：2048 - 8192

### SCR_SpawnAndBindVehicleDemo.c
- `s.m_RayCount = 4096`: 射线数量

## 性能注意事项

4096 条射线会生成约 **40,960 pts/s @ 10 Hz** 的数据量。建议：

1. **CPU 密集**: 确保系统性能足够
2. **CSV 导出**: 如果启用 CSV 导出（`s_OutputCSV = true`），文件会很大
3. **HUD 更新**: HUD 有 0.5 秒节流，减少更新频率
4. **扫描频率**: 可以降低 `s_UpdateInterval`（例如改为 0.2，降至 5 Hz）

## 验证配置

运行游戏并进入载具后，检查控制台输出：
```
RDF: LiDAR HUD auto-started with 4096 rays
```

如果看到此消息，说明 HUD 已自动启动成功。同时你应该能在屏幕左下角看到蓝色 HUD 面板。

离开载具时会显示：
```
RDF: LiDAR HUD hidden (vehicle exited)
```

## 故障排除

1. **HUD 未显示**: 
   - 确保已进入载具（非步行）
   - 检查控制台是否有 "LiDAR HUD auto-started" 消息
   - 确保 RDF 框架已正确加载
   
2. **性能下降**: 
   - 考虑降低射线数（在 `RDF_VehicleLidarBootstrap.c` 中修改 `s_RayCount`）
   - 或降低扫描频率（增大 `s_UpdateInterval`，例如 0.2 = 5 Hz）
   
3. **HUD 类未找到**: 
   - 检查 addon.gproj 依赖配置
   - 确保 RDF 框架版本兼容

## 更多信息

参考：
- [API.md](API.md) - 完整 API 文档
- [DEVELOPMENT.md](DEVELOPMENT.md) - 开发指南
- RDF 框架文档
