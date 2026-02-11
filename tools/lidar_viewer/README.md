# LiDAR CSV 点云查看器 / LiDAR CSV Point Cloud Viewer

从 Arma Reforger 游戏导出的 LiDAR CSV 文件中反推并绘制 3D 点云。

Renders 3D point cloud from LiDAR CSV exported by Arma Reforger.

---

## 安装 / Installation

```bash
pip install -r requirements.txt
```

---

## 使用 / Usage

```bash
# 启动后通过界面选择 CSV 文件 / Launch and select CSV via UI
python lidar_viewer.py

# 或直接指定文件 / Or specify file directly
python lidar_viewer.py "path/to/lidar_live_1.csv"
```

---

## 功能 / Features

- **打开 CSV**：加载游戏导出的 `lidar_live_N.csv` / **Open CSV**: Load game-exported `lidar_live_N.csv`
- **导出 PNG**：将当前帧 3D 图导出为图片 / **Export PNG**: Export current frame 3D view as image
- **帧切换**：上一帧 / 下一帧 / 滑块 / **Frame navigation**: Prev/Next frame, slider
- **播放**：自动按帧播放，可调速度（ms/帧）/ **Playback**: Auto-play frames, adjustable speed (ms/frame)
- **着色**：`hit`（命中绿/未命中红）或 `distance`（距离色条）/ **Color**: `hit` (green/red) or `distance` (distance gradient)
- **过滤**：`all` / `hit` / `miss` 仅显示对应点 / **Filter**: `all` / `hit` / `miss` for point types
- **原点**：勾选显示射线起点（黄色三角）/ **Origin**: Toggle ray origin display (yellow triangle)
- **快捷键**：←→ 帧 | Space 播放 | Home 重置视角 / **Shortcuts**: ←→ frame | Space play | Home reset view

---

## CSV 格式 / CSV Format

扩展格式（`RDF_LidarExport.GetExtendedCSVHeader()`）/ Extended format:

```
index,hit,time,originX,originY,originZ,dirX,dirY,dirZ,elevation,azimuth,maxRange,distance,hitPosX,hitPosY,hitPosZ,targetName,subjectVelX,subjectVelY,subjectVelZ,subjectYaw,subjectPitch,scanId,frameIndex,entityClass
```

- 每行一条射线，`hitPosX/Y/Z` 为 3D 点坐标，`frameIndex` 用于分组帧
  One row per ray; `hitPosX/Y/Z` are 3D point coords; `frameIndex` groups frames.
- 游戏内由 `RDF_VehicleLidarBootstrap` 写入 `$profile:LiDAR/lidar_live_N.csv`（缓冲写入）
  Game writes via `RDF_VehicleLidarBootstrap` to `$profile:LiDAR/lidar_live_N.csv` (buffered).
