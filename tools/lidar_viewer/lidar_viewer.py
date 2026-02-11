# -*- coding: utf-8 -*-
"""
LiDAR CSV 点云查看器 - 从游戏导出的 CSV 反推并绘制 3D 点云
LiDAR CSV point cloud viewer - Renders 3D point cloud from game-exported CSV
支持帧切换、播放、按 hit/距离着色、快捷键、导出等
Supports frame nav, playback, hit/distance coloring, shortcuts, export
"""

import csv
import os
import sys
from collections import defaultdict

import matplotlib
matplotlib.use('TkAgg')
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import tkinter as tk
from tkinter import filedialog, ttk, messagebox

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False


def load_lidar_csv(filepath):
    """加载 LiDAR CSV，按 frameIndex 分组返回点云数据
    Load LiDAR CSV, return point cloud grouped by frameIndex"""
    frames = defaultdict(list)
    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                frame_idx = int(row.get('frameIndex', 0))
                x = float(row.get('hitPosX', 0))
                y = float(row.get('hitPosY', 0))
                z = float(row.get('hitPosZ', 0))
                hit = int(row.get('hit', 0))
                distance = float(row.get('distance', 0))
                max_range = float(row.get('maxRange', 50))
                origin_x = float(row.get('originX', 0))
                origin_y = float(row.get('originY', 0))
                origin_z = float(row.get('originZ', 0))
                frames[frame_idx].append({
                    'x': x, 'y': y, 'z': z,
                    'hit': hit, 'distance': distance, 'max_range': max_range,
                    'origin': (origin_x, origin_y, origin_z),
                })
            except (ValueError, KeyError):
                continue
    return dict(sorted(frames.items()))


class LidarViewerApp:
    """LiDAR 点云查看器主窗口 / LiDAR point cloud viewer main window"""

    def __init__(self):
        self.root = tk.Tk()
        self.root.title('LiDAR 点云查看器 / LiDAR Point Cloud Viewer')
        self.root.geometry('960x720')
        self.root.configure(bg='#1e1e1e')
        self.root.minsize(640, 480)

        self.frames_data = {}
        self.frame_indices = []
        self.current_frame = 0
        self.filepath = None
        self._play_running = False
        self._colorbar = None
        self._play_delay_ms = 80
        self._show_origin = tk.BooleanVar(value=True)
        self._filter_mode = tk.StringVar(value='all')

        self._build_ui()
        self._bind_keys()

    def _build_ui(self):
        """构建工具栏、滑块、3D 画布、状态栏 / Build toolbar, slider, 3D canvas, status bar"""
        top_frame = ttk.Frame(self.root, padding=5)
        top_frame.pack(fill=tk.X)
        ttk.Button(top_frame, text='打开 CSV / Open', command=self._open_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text='导出 PNG / Export', command=self._export_png).pack(side=tk.LEFT, padx=2)
        ttk.Separator(top_frame, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Button(top_frame, text='◀', command=self._prev_frame, width=3).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text='▶', command=self._next_frame, width=3).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text='▶▶', command=self._toggle_play, width=3).pack(side=tk.LEFT, padx=2)
        ttk.Label(top_frame, text='速度(ms) / Speed:').pack(side=tk.LEFT, padx=(8, 2))
        self.speed_var = tk.StringVar(value='80')
        ttk.Spinbox(top_frame, textvariable=self.speed_var, from_=20, to=500, increment=20, width=5).pack(side=tk.LEFT, padx=2)
        ttk.Separator(top_frame, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Label(top_frame, text='着色 / Color:').pack(side=tk.LEFT, padx=(0, 2))
        self.color_var = tk.StringVar(value='hit')
        self.color_combo = ttk.Combobox(top_frame, textvariable=self.color_var, values=['hit', 'distance'], width=8, state='readonly')
        self.color_combo.pack(side=tk.LEFT, padx=2)
        self.color_combo.bind('<<ComboboxSelected>>', lambda e: self._redraw())
        ttk.Label(top_frame, text='过滤 / Filter:').pack(side=tk.LEFT, padx=(8, 2))
        self.filter_combo = ttk.Combobox(top_frame, textvariable=self._filter_mode, values=['all', 'hit', 'miss'], width=6, state='readonly')
        self.filter_combo.pack(side=tk.LEFT, padx=2)
        self.filter_combo.bind('<<ComboboxSelected>>', lambda e: self._redraw())
        ttk.Checkbutton(top_frame, text='原点 / Origin', variable=self._show_origin, command=self._redraw).pack(side=tk.LEFT, padx=8)
        self.frame_label = ttk.Label(top_frame, text='帧 Frame: 0/0 | 点 Pts: 0')
        self.frame_label.pack(side=tk.LEFT, padx=10)

        self.slider = ttk.Scale(self.root, from_=0, to=0, orient=tk.HORIZONTAL, command=self._on_slider)
        self.slider.pack(fill=tk.X, padx=10, pady=2)

        fig_frame = ttk.Frame(self.root, padding=5)
        fig_frame.pack(fill=tk.BOTH, expand=True)
        self.fig = Figure(figsize=(9, 6), dpi=100, facecolor='#2d2d2d')
        self.ax = self.fig.add_subplot(111, projection='3d', facecolor='#2d2d2d')
        self.canvas = FigureCanvasTkAgg(self.fig, master=fig_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        toolbar = NavigationToolbar2Tk(self.canvas, fig_frame)
        toolbar.update()

        status_frame = ttk.Frame(self.root, padding=2)
        status_frame.pack(fill=tk.X)
        self.status_label = ttk.Label(status_frame, text='就绪 | Ready | 快捷键 Shortcuts: ←→ 帧 Frame  Space 播放 Play  Home 重置视角 Reset view', font=('', 9))
        self.status_label.pack(side=tk.LEFT)

    def _bind_keys(self):
        """绑定快捷键：←→ 帧、Space 播放、Home 重置 / Bind shortcuts: ←→ frame, Space play, Home reset"""
        self.root.bind('<Left>', lambda e: self._prev_frame())
        self.root.bind('<Right>', lambda e: self._next_frame())
        self.root.bind('<space>', lambda e: self._toggle_play())
        self.root.bind('<Home>', lambda e: self._reset_view())
        self.root.focus_set()

    def _reset_view(self):
        if not self.frame_indices:
            return
        self.ax.set_xlim(auto=True)
        self.ax.set_ylim(auto=True)
        self.ax.set_zlim(auto=True)
        self.canvas.draw()

    def _open_file(self):
        """打开文件对话框选择 CSV / Open file dialog to select CSV"""
        path = filedialog.askopenfilename(
            filetypes=[('CSV files', '*.csv'), ('All files', '*.*')],
            initialdir=os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..')),
        )
        if path:
            self._load_file(path)

    def _load_file(self, path):
        """加载 CSV 并按 frameIndex 分组 / Load CSV and group by frameIndex"""
        try:
            self.status_label.config(text='加载中... / Loading...')
            self.root.update()
            self.frames_data = load_lidar_csv(path)
            self.filepath = path
            self.frame_indices = sorted(self.frames_data.keys())
            if not self.frame_indices:
                messagebox.showerror('错误 / Error', '未找到有效帧数据 / No valid frame data')
                self.status_label.config(text='加载失败 / Load failed')
                return
            self.current_frame = 0
            self.slider.config(from_=0, to=max(0, len(self.frame_indices) - 1))
            self.slider.set(0)
            total_points = sum(len(self.frames_data[fi]) for fi in self.frame_indices)
            self.status_label.config(text=f'已加载 Loaded: {os.path.basename(path)} | {len(self.frame_indices)} 帧 frames | {total_points} 点 pts')
            self._update_label()
            self._redraw()
        except Exception as e:
            messagebox.showerror('错误 / Error', str(e))
            self.status_label.config(text='加载失败 / Load failed')

    def _export_png(self):
        """导出当前帧 3D 图为 PNG / Export current frame 3D view as PNG"""
        if not self.frame_indices:
            messagebox.showwarning('提示 / Notice', '请先加载 CSV 文件 / Please load CSV first')
            return
        path = filedialog.asksaveasfilename(
            defaultextension='.png',
            filetypes=[('PNG image', '*.png'), ('All files', '*.*')],
            initialfile=f'lidar_frame_{self.current_frame + 1}.png',
        )
        if path:
            try:
                self.fig.savefig(path, dpi=150, facecolor='#2d2d2d', edgecolor='none')
                self.status_label.config(text=f'已保存 Saved: {path}')
            except Exception as e:
                messagebox.showerror('错误 / Error', str(e))

    def _on_slider(self, val):
        idx = int(float(val))
        if 0 <= idx < len(self.frame_indices):
            self.current_frame = idx
            self._update_label()
            self._redraw()

    def _prev_frame(self):
        if self.current_frame > 0:
            self.current_frame -= 1
            self.slider.set(self.current_frame)
            self._update_label()
            self._redraw()

    def _next_frame(self):
        if self.current_frame < len(self.frame_indices) - 1:
            self.current_frame += 1
            self.slider.set(self.current_frame)
            self._update_label()
            self._redraw()

    def _toggle_play(self):
        self._play_running = not self._play_running
        try:
            self._play_delay_ms = max(20, int(self.speed_var.get()))
        except ValueError:
            self._play_delay_ms = 80
        if self._play_running:
            self._play_loop()

    def _play_loop(self):
        if not self._play_running or not self.frame_indices:
            return
        self._next_frame()
        if self.current_frame >= len(self.frame_indices) - 1:
            self._play_running = False
            return
        self.root.after(self._play_delay_ms, self._play_loop)

    def _update_label(self):
        if not self.frame_indices:
            return
        total = len(self.frame_indices)
        frame_idx = self.frame_indices[self.current_frame]
        pt_count = len(self.frames_data[frame_idx])
        self.frame_label.config(text=f'帧 Frame: {self.current_frame + 1}/{total} | 点 Pts: {pt_count}')

    def _filter_points(self, points):
        """按过滤模式筛选点：all / hit / miss / Filter points by mode"""
        fm = self._filter_mode.get() if hasattr(self, '_filter_mode') else 'all'
        if fm == 'hit':
            return [p for p in points if p['hit'] == 1]
        if fm == 'miss':
            return [p for p in points if p['hit'] == 0]
        return points

    def _redraw(self):
        """重绘当前帧点云（按着色、过滤、原点） / Redraw current frame point cloud"""
        if not self.frame_indices:
            return
        frame_idx = self.frame_indices[self.current_frame]
        points = self.frames_data[frame_idx]
        points = self._filter_points(points)
        if not points:
            self.ax.clear()
            self.ax.set_facecolor('#2d2d2d')
            self.ax.set_xlabel('X')
            self.ax.set_ylabel('Y')
            self.ax.set_zlabel('Z')
            self.canvas.draw()
            return

        if HAS_NUMPY:
            xs = np.array([p['x'] for p in points])
            ys = np.array([p['y'] for p in points])
            zs = np.array([p['z'] for p in points])
        else:
            xs = [p['x'] for p in points]
            ys = [p['y'] for p in points]
            zs = [p['z'] for p in points]

        if self._colorbar:
            try:
                self._colorbar.remove()
            except (KeyError, AttributeError, TypeError):
                pass
            self._colorbar = None
        self.ax.clear()
        self.ax.set_facecolor('#2d2d2d')

        color_mode = self.color_var.get() if hasattr(self, 'color_var') else 'hit'
        if color_mode == 'hit':
            hit_pts = [p for p in points if p['hit'] == 1]
            miss_pts = [p for p in points if p['hit'] == 0]
            if hit_pts:
                hx = [p['x'] for p in hit_pts]
                hy = [p['y'] for p in hit_pts]
                hz = [p['z'] for p in hit_pts]
                self.ax.scatter(hx, hy, hz, c='#00ff00', s=3, alpha=0.85, label='命中 / Hit')
            if miss_pts:
                mx = [p['x'] for p in miss_pts]
                my = [p['y'] for p in miss_pts]
                mz = [p['z'] for p in miss_pts]
                self.ax.scatter(mx, my, mz, c='#ff4444', s=2, alpha=0.6, label='未命中 / Miss')
        else:
            distances = [p['distance'] for p in points]
            sc = self.ax.scatter(xs, ys, zs, c=distances, cmap='viridis', s=3, alpha=0.85)
            self._colorbar = self.fig.colorbar(sc, ax=self.ax, shrink=0.6)
            self._colorbar.ax.tick_params(colors='white')

        if self._show_origin.get():
            origin = points[0]['origin']
            self.ax.scatter([origin[0]], [origin[1]], [origin[2]], c='#ffff00', s=80, marker='^', label='主体 / Origin')

        self.ax.set_xlabel('X')
        self.ax.set_ylabel('Y')
        self.ax.set_zlabel('Z')
        self.ax.tick_params(colors='white')
        self.ax.xaxis.pane.fill = False
        self.ax.yaxis.pane.fill = False
        self.ax.zaxis.pane.fill = False
        self.ax.set_box_aspect([1, 1, 1])
        self.canvas.draw()

    def run(self):
        if len(sys.argv) > 1 and os.path.isfile(sys.argv[1]):
            self._load_file(sys.argv[1])
        self.root.mainloop()


if __name__ == '__main__':
    app = LidarViewerApp()
    app.run()
