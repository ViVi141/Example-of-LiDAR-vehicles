// Vehicle LiDAR bootstrap: 直接使用 Visualizer.Render()，不依赖 RDF_LidarAutoRunner / GetCallqueue。
// Vehicle LiDAR bootstrap: uses Visualizer.Render() directly, no dependency on RDF_LidarAutoRunner/GetCallqueue.
// 上车后自动 CSV 导出到磁盘，下车或游戏结束时停止。
// CSV export on vehicle enter; stops on exit or game end.
// 若载具检测失败，回退到玩家主体（测试框架是否正常）。Fallback to player if vehicle detection fails.
// 循环检测：每帧 EOnFrame 检查依赖，遇 null 则跳过本帧，等待下一帧重试，避免崩溃。
// Loop check: EOnFrame per frame; skip on null, retry next frame to avoid crash.
//
// 使用 modded SCR_BaseGameMode 的 EOnFrame 驱动，支持所有基于 SCR_BaseGameMode 的游戏模式（工作台、Conflict、Campaign 等）。
// Uses modded SCR_BaseGameMode EOnFrame; supports all SCR_BaseGameMode-based game modes (Workbench, Conflict, Campaign, etc.).

// ---- Demo 配置（游戏用车载 LiDAR：1024 射线、30m、10 Hz、120°×19° 矩形视场） ----
// ---- Demo config (vehicle LiDAR: 1024 rays, 30m, 10 Hz, 120°×19° rect FOV) ----
// 设为 true 时：在地面（玩家）也运行，用于测试框架是否正常
// When true: also run on foot (player), for testing framework
static bool s_TestOnFoot = false;

static int s_SessionCounter = 0;

// 射线数量（64x16=1024，约 10,240 pts/s @ 10 Hz）/ Ray count (64x16=1024, ~10,240 pts/s @ 10 Hz)
static int s_RayCount = 1024;

// 探测距离（米）/ Range (m)
static float s_Range = 30.0;

// 扫描间隔（秒），10 Hz / Scan interval (s), 10 Hz
static float s_UpdateInterval = 0.1;

// 矩形视场：120° 水平，垂直约 ±9.5°（30m 处高度 ±5m）/ Rect FOV: 120° horiz, ±9.5° vert
static float s_RectFOVHorizDeg = 120.0;
static float s_RectFOVVertDeg = 19.0;
static int s_RectCols = 64;
static int s_RectRows = 16;

// 扫描但不可视化 / Scan without visualization
static bool s_ScanWithoutVisualization = false;

// 是否输出 CSV 到磁盘 / Output CSV to disk
static bool s_OutputCSV = false;

// CSV 缓冲写入间隔（秒），期间累积在内存，减少 IO / CSV flush interval (s), buffer in memory to reduce IO
static float s_CSVFlushInterval = 1.0;

// 检测目标：0=仅玩家, 1=仅载具, 2=两者 / Target: 0=player only, 1=vehicle only, 2=both
static int s_TargetMode = 1;

// 静态状态（GameMode 为单例，用静态存储）
// Static state (GameMode is singleton)
static bool s_RDFVL_Active = false;
static IEntity s_RDFVL_Subject;
static ref RDF_LidarVisualizer s_RDFVL_Visualizer;
static ref RDF_LidarScanner s_RDFVL_Scanner;
static string s_RDFVL_ExportPath = "";
static float s_RDFVL_ScanAccum = 0.0;
static float s_RDFVL_FlushAccum = 0.0;
static ref array<string> s_RDFVL_CSVBuffer;
static int s_RDFVL_FrameIndex = 0;
static int s_RDFVL_ScanId = 0;
static vector s_RDFVL_LastSubjectPos = vector.Zero;
static float s_RDFVL_LastSubjectTime = -1.0;
static bool s_RDFVL_InitPending = false;
static ref array<ref RDF_LidarSample> s_RDFVL_ScanOnlySamples;
static ref array<ref RDF_LidarSample> s_RDFVL_LastSamples;

modded class SCR_BaseGameMode
{
    override void EOnFrame(IEntity owner, float timeSlice)
    {
        super.EOnFrame(owner, timeSlice);
        if (!GetGame())
            return;
        if (g_RDF_StatusPrinter)
            g_RDF_StatusPrinter.Tick(timeSlice);
        PlayerController ctrl = GetGame().GetPlayerController();
        if (!ctrl)
            return;
        IEntity playerSubject = ctrl.GetControlledEntity();
        if (!playerSubject)
            return;
        IEntity vehicle = null;
        ChimeraCharacter character = ChimeraCharacter.Cast(playerSubject);
        if (character)
            vehicle = CompartmentAccessComponent.GetVehicleIn(character);
        bool isInVehicle = (vehicle != null && playerSubject != null && vehicle != playerSubject);
        IEntity subject = null;
        bool shouldRun = false;
        if (s_TargetMode == 0)
        {
            subject = playerSubject;
            shouldRun = true;
        }
        else if (s_TargetMode == 1)
        {
            subject = vehicle;
            shouldRun = isInVehicle;
        }
        else
        {
            subject = vehicle;
            if (!subject)
                subject = playerSubject;
            shouldRun = isInVehicle || s_TestOnFoot;
        }
        if (!subject)
            return;
        if (shouldRun && !s_RDFVL_Active)
        {
            if (!s_RDFVL_InitPending)
            {
                s_RDFVL_InitPending = true;
                return;
            }
            s_RDFVL_InitPending = false;
            s_RDFVL_Active = true;
            s_RDFVL_Subject = subject;
            RDF_LidarSettings settings = new RDF_LidarSettings();
            settings.m_RayCount = s_RayCount;
            settings.m_Range = s_Range;
            settings.m_UpdateInterval = s_UpdateInterval;
            s_RDFVL_Scanner = new RDF_LidarScanner(settings);
            s_RDFVL_Scanner.SetSampleStrategy(new RDF_RectangularFOVSampleStrategy(s_RectFOVHorizDeg, s_RectFOVVertDeg, s_RectCols, s_RectRows));
            if (!s_ScanWithoutVisualization)
            {
                s_RDFVL_Visualizer = new RDF_LidarVisualizer();
                RDF_LidarVisualSettings vs = s_RDFVL_Visualizer.GetSettings();
                vs.m_DrawPoints = true;
                vs.m_DrawRays = true;
                vs.m_RenderWorld = true;
                vs.m_UseDistanceGradient = false;
                s_RDFVL_Visualizer.SetColorStrategy(new RDF_ThreeColorStrategy(0xFF00FF00, 0xFFFFFF00, 0xFFFF0000));
            }
            else
            {
                s_RDFVL_Visualizer = null;
                s_RDFVL_ScanOnlySamples = new array<ref RDF_LidarSample>();
            }
            s_RDFVL_ExportPath = "";
            if (s_OutputCSV)
            {
                FileIO.MakeDirectory("$profile:LiDAR");
                s_SessionCounter = s_SessionCounter + 1;
                s_RDFVL_ScanId = s_SessionCounter;
                s_RDFVL_FrameIndex = 0;
                s_RDFVL_ExportPath = "$profile:LiDAR/lidar_live_" + s_SessionCounter.ToString() + ".csv";
                FileHandle fh = FileIO.OpenFile(s_RDFVL_ExportPath, FileMode.WRITE);
                if (fh)
                {
                    string header = RDF_LidarExport.GetExtendedCSVHeader();
                    if (header)
                        fh.WriteLine(header);
                    fh.Close();
                }
                else
                {
                    s_RDFVL_ExportPath = "";
                    Print("RDF: CSV file open failed.");
                }
            }
            s_RDFVL_ScanAccum = s_UpdateInterval;
            s_RDFVL_FlushAccum = 0.0;
            if (s_OutputCSV)
                s_RDFVL_CSVBuffer = new array<string>();
            else
                s_RDFVL_CSVBuffer = null;
        }
        else if (shouldRun && s_RDFVL_Active)
        {
            if (s_RDFVL_Subject != subject)
                s_RDFVL_LastSubjectTime = -1.0;
            s_RDFVL_Subject = subject;
        }
        else if (!shouldRun && s_RDFVL_Active)
        {
            s_RDFVL_InitPending = false;
            s_RDFVL_Active = false;
            if (s_RDFVL_CSVBuffer && s_RDFVL_CSVBuffer.Count() > 0 && s_RDFVL_ExportPath != "")
            {
                FileHandle f = FileIO.OpenFile(s_RDFVL_ExportPath, FileMode.APPEND);
                if (f)
                {
                    for (int i = 0; i < s_RDFVL_CSVBuffer.Count(); i++)
                        f.WriteLine(s_RDFVL_CSVBuffer.Get(i));
                    f.Close();
                }
            }
            s_RDFVL_ExportPath = "";
            s_RDFVL_CSVBuffer = null;
            s_RDFVL_ScanOnlySamples = null;
            s_RDFVL_LastSamples = null;
            s_RDFVL_FlushAccum = 0.0;
            s_RDFVL_Subject = null;
            s_RDFVL_Scanner = null;
            s_RDFVL_Visualizer = null;
            s_RDFVL_ScanAccum = 0.0;
            s_RDFVL_FrameIndex = 0;
            s_RDFVL_LastSubjectTime = -1.0;
        }

        if (s_RDFVL_Active && s_RDFVL_Scanner && s_RDFVL_Subject)
        {
            if (!s_RDFVL_Subject || !s_RDFVL_Scanner)
                return;
            s_RDFVL_ScanAccum = s_RDFVL_ScanAccum + timeSlice;
            s_RDFVL_FlushAccum = s_RDFVL_FlushAccum + timeSlice;
            ref array<ref RDF_LidarSample> samples = null;
            bool didScan = false;
            if (s_RDFVL_ScanAccum >= s_UpdateInterval)
            {
                s_RDFVL_ScanAccum = 0.0;
                if (s_ScanWithoutVisualization && s_RDFVL_ScanOnlySamples)
                {
                    s_RDFVL_ScanOnlySamples.Clear();
                    s_RDFVL_Scanner.Scan(s_RDFVL_Subject, s_RDFVL_ScanOnlySamples);
                    samples = s_RDFVL_ScanOnlySamples;
                    didScan = true;
                }
                else if (s_RDFVL_Visualizer)
                {
                    s_RDFVL_Visualizer.Render(s_RDFVL_Subject, s_RDFVL_Scanner);
                    samples = s_RDFVL_Visualizer.GetLastSamples();
                    didScan = true;
                }
            }
            else if (s_RDFVL_Visualizer && s_RDFVL_LastSamples && s_RDFVL_LastSamples.Count() > 0)
            {
                // 扫描间隔内每帧重绘，避免 ShapeFlags.ONCE 导致的闪烁
                // Redraw every frame between scans to avoid flicker from ShapeFlags.ONCE
                s_RDFVL_Visualizer.RenderWithSamples(s_RDFVL_Subject, s_RDFVL_LastSamples);
                samples = s_RDFVL_LastSamples;
            }
            if (didScan && samples && s_RDFVL_Visualizer)
            {
                s_RDFVL_LastSamples = new array<ref RDF_LidarSample>();
                for (int i = 0; i < samples.Count(); i++)
                {
                    RDF_LidarSample s = samples.Get(i);
                    if (s)
                        s_RDFVL_LastSamples.Insert(s);
                }
            }
            if (!samples)
                return;
            if (didScan && s_RDFVL_ExportPath != "" && samples && samples.Count() > 0 && s_RDFVL_CSVBuffer)
            {
                if (!GetGame())
                    return;
                World w = GetGame().GetWorld();
                float currentTime = 0.0;
                if (w)
                    currentTime = w.GetWorldTime();
                float maxRange = s_RDFVL_Scanner.GetSettings().m_Range;
                vector subjectVel = vector.Zero;
                SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(s_RDFVL_Subject.FindComponent(SCR_CharacterControllerComponent));
                if (charCtrl)
                    subjectVel = charCtrl.GetVelocity();
                else
                {
                    vector pos = s_RDFVL_Subject.GetOrigin();
                    if (s_RDFVL_LastSubjectTime >= 0.0)
                    {
                        float dtMs = currentTime - s_RDFVL_LastSubjectTime;
                        if (dtMs > 10.0)
                        {
                            float dtSec = dtMs / 1000.0;
                            subjectVel = (pos - s_RDFVL_LastSubjectPos) / dtSec;
                        }
                    }
                    s_RDFVL_LastSubjectPos = pos;
                    s_RDFVL_LastSubjectTime = currentTime;
                }
                float subjectYaw = 0.0;
                float subjectPitch = 0.0;
                vector worldMat[4];
                s_RDFVL_Subject.GetWorldTransform(worldMat);
                vector fwd = worldMat[0];
                float horiz = Math.Sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
                subjectYaw = Math.Atan2(fwd[1], fwd[0]);
                subjectPitch = Math.Atan2(-fwd[2], horiz);
                s_RDFVL_FrameIndex = s_RDFVL_FrameIndex + 1;
                for (int i = 0; i < samples.Count(); i++)
                {
                    RDF_LidarSample s = samples.Get(i);
                    if (s)
                    {
                        string row = RDF_LidarExport.SampleToExtendedCSVRow(s, currentTime, maxRange, subjectVel, subjectYaw, subjectPitch, s_RDFVL_ScanId, s_RDFVL_FrameIndex);
                        if (row != "")
                            s_RDFVL_CSVBuffer.Insert(row);
                    }
                }
                if (s_RDFVL_FlushAccum >= s_CSVFlushInterval)
                {
                    s_RDFVL_FlushAccum = 0.0;
                    FileHandle f = FileIO.OpenFile(s_RDFVL_ExportPath, FileMode.APPEND);
                    if (f)
                    {
                        for (int j = 0; j < s_RDFVL_CSVBuffer.Count(); j++)
                            f.WriteLine(s_RDFVL_CSVBuffer.Get(j));
                        f.Close();
                        s_RDFVL_CSVBuffer.Clear();
                    }
                }
            }
        }
    }
}
