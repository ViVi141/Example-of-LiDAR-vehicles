// Vehicle LiDAR bootstrap: 直接使用 Visualizer.Render()，不依赖 RDF_LidarAutoRunner / GetCallqueue。
// Vehicle LiDAR bootstrap: uses Visualizer.Render() directly, no dependency on RDF_LidarAutoRunner/GetCallqueue.
// 上车后自动 CSV 导出到磁盘，下车或游戏结束时停止。
// CSV export on vehicle enter; stops on exit or game end.
// 若载具检测失败，回退到玩家主体（测试框架是否正常）。Fallback to player if vehicle detection fails.
// 循环检测：每帧 EOnFrame 检查依赖，遇 null 则跳过本帧，等待下一帧重试，避免崩溃。
// Loop check: EOnFrame per frame; skip on null, retry next frame to avoid crash.

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

modded class SCR_PlayerController
{
    protected bool m_Active = false;
    protected IEntity m_Subject;
    protected ref RDF_LidarVisualizer m_Visualizer;
    protected ref RDF_LidarScanner m_Scanner;
    protected FileHandle m_ExportFile;
    protected string m_ExportPath;
    protected float m_ScanAccum = 0.0;
    protected float m_FlushAccum = 0.0;
    protected ref array<string> m_CSVBuffer;
    protected int m_FrameIndex = 0;
    protected int m_ScanId = 0;
    protected vector m_LastSubjectPos = vector.Zero;
    protected float m_LastSubjectTime = -1.0;
    protected bool m_InitPending = false;
    protected ref array<ref RDF_LidarSample> m_ScanOnlySamples;

    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
    }

    override void EOnFrame(IEntity owner, float timeSlice)
    {
        super.EOnFrame(owner, timeSlice);
        if (!GetGame())
            return;
        if (!owner)
            return;
        PlayerController ctrl = GetGame().GetPlayerController();
        if (!ctrl || ctrl != this)
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
        if (shouldRun && !m_Active)
        {
            if (!m_InitPending)
            {
                m_InitPending = true;
                return;
            }
            m_InitPending = false;
            m_Active = true;
            m_Subject = subject;
            RDF_LidarSettings settings = new RDF_LidarSettings();
            settings.m_RayCount = s_RayCount;
            settings.m_Range = s_Range;
            settings.m_UpdateInterval = s_UpdateInterval;
            m_Scanner = new RDF_LidarScanner(settings);
            m_Scanner.SetSampleStrategy(new RDF_RectangularFOVSampleStrategy(s_RectFOVHorizDeg, s_RectFOVVertDeg, s_RectCols, s_RectRows));
            if (!s_ScanWithoutVisualization)
            {
                m_Visualizer = new RDF_LidarVisualizer();
                RDF_LidarVisualSettings vs = m_Visualizer.GetSettings();
                vs.m_DrawPoints = true;
                vs.m_DrawRays = true;
                vs.m_RenderWorld = true;
                vs.m_UseDistanceGradient = false;
                m_Visualizer.SetColorStrategy(new RDF_ThreeColorStrategy(0xFF00FF00, 0xFFFFFF00, 0xFFFF0000));
            }
            else
            {
                m_Visualizer = null;
                m_ScanOnlySamples = new array<ref RDF_LidarSample>();
            }
            m_ExportPath = "";
            if (s_OutputCSV)
            {
                FileIO.MakeDirectory("$profile:LiDAR");
                s_SessionCounter = s_SessionCounter + 1;
                m_ScanId = s_SessionCounter;
                m_FrameIndex = 0;
                m_ExportPath = "$profile:LiDAR/lidar_live_" + s_SessionCounter.ToString() + ".csv";
                FileHandle fh = FileIO.OpenFile(m_ExportPath, FileMode.WRITE);
                if (fh)
                {
                    string header = RDF_LidarExport.GetExtendedCSVHeader();
                    if (header)
                        fh.WriteLine(header);
                    fh.Close();
                }
                else
                {
                    m_ExportPath = "";
                    Print("RDF: CSV file open failed.");
                }
            }
            m_ExportFile = null;
            m_ScanAccum = s_UpdateInterval;
            m_FlushAccum = 0.0;
            if (s_OutputCSV)
                m_CSVBuffer = new array<string>();
            else
                m_CSVBuffer = null;
        }
        else if (shouldRun && m_Active)
        {
            if (m_Subject != subject)
                m_LastSubjectTime = -1.0;
            m_Subject = subject;
        }
        else if (!shouldRun && m_Active)
        {
            m_InitPending = false;
            m_Active = false;
            if (m_CSVBuffer && m_CSVBuffer.Count() > 0 && m_ExportPath != "")
            {
                FileHandle f = FileIO.OpenFile(m_ExportPath, FileMode.APPEND);
                if (f)
                {
                    for (int i = 0; i < m_CSVBuffer.Count(); i++)
                        f.WriteLine(m_CSVBuffer.Get(i));
                    f.Close();
                }
            }
            m_ExportPath = "";
            m_ExportFile = null;
            m_CSVBuffer = null;
            m_ScanOnlySamples = null;
            m_FlushAccum = 0.0;
            m_Subject = null;
            m_Scanner = null;
            m_Visualizer = null;
            m_ScanAccum = 0.0;
            m_FrameIndex = 0;
            m_LastSubjectTime = -1.0;
        }

        if (m_Active && m_Scanner && m_Subject)
        {
            if (!m_Subject || !m_Scanner)
                return;
            m_ScanAccum = m_ScanAccum + timeSlice;
            m_FlushAccum = m_FlushAccum + timeSlice;
            if (m_ScanAccum < s_UpdateInterval)
                return;
            m_ScanAccum = 0.0;
            ref array<ref RDF_LidarSample> samples = null;
            if (s_ScanWithoutVisualization && m_ScanOnlySamples)
            {
                m_ScanOnlySamples.Clear();
                m_Scanner.Scan(m_Subject, m_ScanOnlySamples);
                samples = m_ScanOnlySamples;
            }
            else if (m_Visualizer)
            {
                m_Visualizer.Render(m_Subject, m_Scanner);
                samples = m_Visualizer.GetLastSamples();
            }
            if (!samples)
                return;
            if (m_ExportPath != "" && samples && samples.Count() > 0 && m_CSVBuffer)
            {
                if (!GetGame())
                    return;
                World w = GetGame().GetWorld();
                float currentTime = 0.0;
                if (w)
                    currentTime = w.GetWorldTime();
                float maxRange = m_Scanner.GetSettings().m_Range;
                vector subjectVel = vector.Zero;
                SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(m_Subject.FindComponent(SCR_CharacterControllerComponent));
                if (charCtrl)
                    subjectVel = charCtrl.GetVelocity();
                else
                {
                    vector pos = m_Subject.GetOrigin();
                    if (m_LastSubjectTime >= 0.0)
                    {
                        float dtMs = currentTime - m_LastSubjectTime;
                        if (dtMs > 10.0)
                        {
                            float dtSec = dtMs / 1000.0;
                            subjectVel = (pos - m_LastSubjectPos) / dtSec;
                        }
                    }
                    m_LastSubjectPos = pos;
                    m_LastSubjectTime = currentTime;
                }
                float subjectYaw = 0.0;
                float subjectPitch = 0.0;
                vector worldMat[4];
                m_Subject.GetWorldTransform(worldMat);
                vector fwd = worldMat[0];
                float horiz = Math.Sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
                subjectYaw = Math.Atan2(fwd[1], fwd[0]);
                subjectPitch = Math.Atan2(-fwd[2], horiz);
                m_FrameIndex = m_FrameIndex + 1;
                for (int i = 0; i < samples.Count(); i++)
                {
                    RDF_LidarSample s = samples.Get(i);
                    if (s)
                    {
                        string row = RDF_LidarExport.SampleToExtendedCSVRow(s, currentTime, maxRange, subjectVel, subjectYaw, subjectPitch, m_ScanId, m_FrameIndex);
                        if (row != "")
                            m_CSVBuffer.Insert(row);
                    }
                }
                if (m_FlushAccum >= s_CSVFlushInterval)
                {
                    m_FlushAccum = 0.0;
                    FileHandle f = FileIO.OpenFile(m_ExportPath, FileMode.APPEND);
                    if (f)
                    {
                        for (int j = 0; j < m_CSVBuffer.Count(); j++)
                            f.WriteLine(m_CSVBuffer.Get(j));
                        f.Close();
                        m_CSVBuffer.Clear();
                    }
                }
            }
        }
    }
}
