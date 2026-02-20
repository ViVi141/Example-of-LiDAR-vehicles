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

// ---- Demo 配置（游戏用车载 LiDAR：4096 射线、30m、10 Hz、60°×30° 前向视场） ----
// ---- Demo config (vehicle LiDAR: 4096 rays, 30m, 10 Hz, 60°×30° forward FOV) ----
// 设为 true 时：在地面（玩家）也运行，用于测试框架是否正常
// When true: also run on foot (player), for testing framework
static bool s_TestOnFoot = false;

static int s_SessionCounter = 0;

// 射线数量，4096 条高密度扫描（约 40,960 pts/s @ 10 Hz）/ Ray count, 4096 high-density scan (~40,960 pts/s @ 10 Hz)
static int s_RayCount = 4096;

// 探测距离（米）/ Range (m)
static float s_Range = 210.0;

// 扫描间隔（秒），10 Hz / Scan interval (s), 10 Hz
static float s_UpdateInterval = 0.1;

// 矩形视场：120° 水平（前向±60°），25.4° 垂直（±12.7°）/ Rect FOV: 120° horiz (forward ±60°), 25.4° vert (±12.7°)
static float s_RectFOVHorizDeg = 120.0;
static float s_RectFOVVertDeg = 25.4;
static int s_RectCols = 64;
static int s_RectRows = 64;  // 增加行数以匹配4096射线（64x64）

// 扫描但不可视化 3D 点云/射线，仅用数据驱动 HUD / Scan without 3D visualization, data-driven HUD only
static bool s_ScanWithoutVisualization = true;

// 可选：对 HUD 输出进行实体去重（每个实体仅显示一次）
// Optional: dedupe entities for HUD (show each IEntity only once)
static bool s_DedupeEntitiesForHUD = true;

// 可选：开启/关闭扫描调试输出（控制台）
// Optional: enable/disable verbose scan debug output
static bool s_DebugScanOutput = false; // 已禁用默认的扫描调试日志

// 使用基于物理体积的“无体积实体”检测（优先于字符串启发式）
// Use volume-based detection for volueless entities (preferred over string heuristics)
static bool s_UseVolumeCheckForVolueless = true; // 推荐开启以提高可靠性
static float s_VolumeThreshold_m3 = 0.001;    // 体积阈值（立方米）；低于此值视为“无体积”

// 是否为被过滤实体显示单独的 3D 命中点/射线（调试用）
// Show hit points/rays for filtered samples (debug visualization)
static bool s_ShowFilteredVisuals = false; // 默认关闭（改为可视化被保留的物体）
static int s_FilteredVisualColor = 0xFF00FFFF; // 过滤项可视化颜色（ARGB）

// 是否为被保留（非过滤）实体显示单独的 3D 命中点/射线
// Show hit points/rays for kept (displayed) samples
static bool s_ShowKeptVisuals = false; // 关闭 3D 射线/命中点，仅保留 HUD 显示
static int s_KeptVisualColor = 0xFF00FF00; // 保留项可视化颜色（ARGB，默认绿色）

// 是否使用批量三角网格渲染（推荐：高射线计数/大点云场景）
// Use batched mesh rendering (recommended for high ray-count / large point-cloud scenes)
static bool s_UseBatchedMesh = true;

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
static ref RDF_LidarVisualizer s_RDFVL_HUDVisualizer; // 用于在关闭全局可视化时，仅渲染 HUD 显示点
static ref RDF_AdaptiveSampleStrategy s_RDFVL_Strategy;
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
static string s_RDFVL_ModeLabelBase = ""; // HUD base mode label (e.g. "Vehicle 4096")

// HUD 雷达比例尺（显示距离，独立于扫描距离）/ HUD radar display range (independent of scan range)
static float s_HUDDisplayRange = 0.0;      // 0 = 待初始化（进车时同步 s_Range）/ 0 = uninit, synced from s_Range on enter
static float s_HUDZoomFactor = 1.5;        // 每次缩放倍率 / Zoom factor per key press
static float s_HUDZoomMin = 10.0;          // 最小显示距离（米）/ Min display range (m)
static float s_HUDZoomMax = 2000.0;        // 最大显示距离（米）/ Max display range (m)
static bool s_HUDZoomInPending = false;    // Q 键事件标志 / Q key event flag
static bool s_HUDZoomOutPending = false;   // E 键事件标志 / E key event flag
static bool s_HUDZoomListenersRegistered = false; // 是否已注册监听器 / Listeners registered flag

// Q 键回调：缩小 HUD 显示范围 / Q key callback: zoom in HUD range
void RDF_VehicleLidarBootstrap_OnZoomIn(float value, EActionTrigger reason)
{
    s_HUDZoomInPending = true;
}

// E 键回调：放大 HUD 显示范围 / E key callback: zoom out HUD range
void RDF_VehicleLidarBootstrap_OnZoomOut(float value, EActionTrigger reason)
{
    s_HUDZoomOutPending = true;
}

// 调试：打印所有可用 Context 和 Action 名称（仅首次调用）
// Debug: print all available context and action names (first call only)
static bool s_RDF_DebugInputDumped = false;
void RDF_VehicleLidarBootstrap_DumpInputContexts()
{
    if (s_RDF_DebugInputDumped)
        return;
    s_RDF_DebugInputDumped = true;
    InputManager im = GetGame().GetInputManager();
    if (!im)
    {
        Print("RDF: InputManager is null!");
        return;
    }
    // 通过 CreateUserBinding 获取 InputBinding 以枚举 Context
    // Use CreateUserBinding to get InputBinding for context enumeration
    InputBinding ib = im.CreateUserBinding();
    if (!ib)
    {
        Print("RDF: InputBinding is null!");
        return;
    }
    array<string> contexts = new array<string>();
    ib.GetContexts(contexts);
    Print("RDF: === Available Input Contexts (" + contexts.Count().ToString() + ") ===");
    for (int i = 0; i < contexts.Count(); i++)
        Print("RDF: Context[" + i.ToString() + "] = " + contexts.Get(i));

    // 打印当前 InputManager 中所有 Action 名称（遍历索引）
    // Print all action names in InputManager
    int actionCount = im.GetActionCount();
    Print("RDF: === Available Actions (" + actionCount.ToString() + ") ===");
    for (int j = 0; j < actionCount; j++)
        Print("RDF: Action[" + j.ToString() + "] = " + im.GetActionName(j));
}

// 注册 Q/E 按键监听器 / Register Q/E key listeners
void RDF_VehicleLidarBootstrap_RegisterZoomListeners()
{
    if (s_HUDZoomListenersRegistered)
        return;
    InputManager im = GetGame().GetInputManager();
    if (!im)
        return;

    // 调试：首次注册时打印所有 Context 和 Action，用于确认正确名称
    // Debug: dump all contexts and actions on first registration to identify correct names
    RDF_VehicleLidarBootstrap_DumpInputContexts();

    im.AddActionListener("CharacterLeanLeft",  EActionTrigger.DOWN, RDF_VehicleLidarBootstrap_OnZoomIn);
    im.AddActionListener("CharacterLeanRight", EActionTrigger.DOWN, RDF_VehicleLidarBootstrap_OnZoomOut);
    s_HUDZoomListenersRegistered = true;
    Print("RDF: HUD zoom listeners registered (Q=zoom in, E=zoom out)");
}

// 注销 Q/E 按键监听器 / Unregister Q/E key listeners
void RDF_VehicleLidarBootstrap_UnregisterZoomListeners()
{
    if (!s_HUDZoomListenersRegistered)
        return;
    InputManager im = GetGame().GetInputManager();
    if (!im)
        return;
    im.RemoveActionListener("CharacterLeanLeft",  EActionTrigger.DOWN, RDF_VehicleLidarBootstrap_OnZoomIn);
    im.RemoveActionListener("CharacterLeanRight", EActionTrigger.DOWN, RDF_VehicleLidarBootstrap_OnZoomOut);
    s_HUDZoomListenersRegistered = false;
    Print("RDF: HUD zoom listeners unregistered");
}

// 运行时配置 API：允许在控制台或脚本中切换 bootstrap 行为
void RDF_VehicleLidarBootstrap_SetUseBatchedMesh(bool use)
{
    s_UseBatchedMesh = use;
    Print("RDF: Vehicle bootstrap SetUseBatchedMesh = " + use.ToString());
}

bool RDF_VehicleLidarBootstrap_GetUseBatchedMesh()
{
    return s_UseBatchedMesh;
}

// 运行时开关：是否启用基于体积的无体积检测
void RDF_VehicleLidarBootstrap_SetUseVolumeCheck(bool use)
{
    s_UseVolumeCheckForVolueless = use;
    Print("RDF: Vehicle bootstrap SetUseVolumeCheck = " + use.ToString());
}

bool RDF_VehicleLidarBootstrap_GetUseVolumeCheck()
{
    return s_UseVolumeCheckForVolueless;
}

// 运行时接口：体积阈值（立方米）
void RDF_VehicleLidarBootstrap_SetVolumeThreshold(float m3)
{
    s_VolumeThreshold_m3 = m3;
    Print("RDF: Vehicle bootstrap SetVolumeThreshold_m3 = " + m3.ToString());
}

float RDF_VehicleLidarBootstrap_GetVolumeThreshold()
{
    return s_VolumeThreshold_m3;
}

// 运行时开关：是否显示被过滤样本的 3D 可视化（点/射线）
void RDF_VehicleLidarBootstrap_SetShowFilteredVisuals(bool show)
{
    s_ShowFilteredVisuals = show;
    Print("RDF: Vehicle bootstrap SetShowFilteredVisuals = " + show.ToString());
}

bool RDF_VehicleLidarBootstrap_GetShowFilteredVisuals()
{
    return s_ShowFilteredVisuals;
}

// 运行时：设置被过滤项可视化颜色（ARGB）
void RDF_VehicleLidarBootstrap_SetFilteredVisualColor(int argb)
{
    s_FilteredVisualColor = argb;
    Print("RDF: Vehicle bootstrap SetFilteredVisualColor = 0x" + argb.ToString(16));
}

int RDF_VehicleLidarBootstrap_GetFilteredVisualColor()
{
    return s_FilteredVisualColor;
}

// 运行时开关/颜色：保留项可视化
void RDF_VehicleLidarBootstrap_SetShowKeptVisuals(bool show)
{
    s_ShowKeptVisuals = show;
    Print("RDF: Vehicle bootstrap SetShowKeptVisuals = " + show.ToString());

    if (!show)
    {
        // 清除 HUD visualizer 的残留 shapes 并清空 HUD
        array<ref RDF_LidarSample> _empty = new array<ref RDF_LidarSample>();
        RDF_LidarHUD.FeedSamples(_empty);
        if (s_RDFVL_HUDVisualizer)
        {
            if (s_RDFVL_Subject)
                s_RDFVL_HUDVisualizer.RenderWithSamples(s_RDFVL_Subject, _empty);
            else
                s_RDFVL_HUDVisualizer.RenderWithSamples(null, _empty);
        }
    }
}

bool RDF_VehicleLidarBootstrap_GetShowKeptVisuals()
{
    return s_ShowKeptVisuals;
}

void RDF_VehicleLidarBootstrap_SetKeptVisualColor(int argb)
{
    s_KeptVisualColor = argb;
    Print("RDF: Vehicle bootstrap SetKeptVisualColor = 0x" + argb.ToString(16));
}

int RDF_VehicleLidarBootstrap_GetKeptVisualColor()
{
    return s_KeptVisualColor;
}

// 运行时工具：立即清除 HUD 与 3D 可视化残留（可在调试或HUD未刷新时调用）
void RDF_VehicleLidarBootstrap_ClearVisuals()
{
    array<ref RDF_LidarSample> _empty = new array<ref RDF_LidarSample>();
    RDF_LidarHUD.FeedSamples(_empty);

    if (s_RDFVL_Visualizer)
        s_RDFVL_Visualizer.RenderWithSamples(s_RDFVL_Subject, _empty);
    if (s_RDFVL_HUDVisualizer)
        s_RDFVL_HUDVisualizer.RenderWithSamples(s_RDFVL_Subject, _empty);

    Print("RDF: Cleared HUD and visualizer shapes");
} 

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
            s_RDFVL_Strategy = new RDF_AdaptiveSampleStrategy();
            s_RDFVL_Scanner.SetSampleStrategy(s_RDFVL_Strategy);
            if (!s_ScanWithoutVisualization)
            {
                s_RDFVL_Visualizer = new RDF_LidarVisualizer();
                RDF_LidarVisualSettings vs = s_RDFVL_Visualizer.GetSettings();
                vs.m_DrawPoints = false;  // 关闭点云绘制，使用 HUD 显示
                vs.m_DrawRays = false;    // 关闭射线绘制，使用 HUD 显示
                vs.m_RenderWorld = true;
                vs.m_UseDistanceGradient = false;
                // 根据 bootstrap 配置决定是否启用批量三角网格渲染（减少 Shape.CreateTris 调用）
                vs.m_UseBatchedMesh = s_UseBatchedMesh;
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
            
            // 自动启动 HUD（无需控制台命令）/ Auto-start HUD (no console commands needed)
            s_HUDDisplayRange = s_Range; // 初始化 HUD 显示范围与扫描范围一致 / Init HUD range = scan range
            s_HUDZoomInPending = false;
            s_HUDZoomOutPending = false;
            RDF_VehicleLidarBootstrap_RegisterZoomListeners();
            RDF_LidarHUD.Show();
            RDF_LidarHUD.SetDisplayRange(s_HUDDisplayRange);
            s_RDFVL_ModeLabelBase = "Vehicle " + s_RayCount.ToString();
            RDF_LidarHUD.SetMode(s_RDFVL_ModeLabelBase);
            Print("RDF: LiDAR HUD auto-started with " + s_RayCount.ToString() + " rays");
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
            
            // 隐藏 HUD / Hide HUD
            RDF_LidarHUD.Hide();
            Print("RDF: LiDAR HUD hidden (vehicle exited)");

            // 清理 3D 可视化与 HUD（确保 Shape/Widgets 被移除）
            array<ref RDF_LidarSample> _empty = new array<ref RDF_LidarSample>();
            RDF_LidarHUD.FeedSamples(_empty); // 清空 HUD 内容
            if (s_RDFVL_Visualizer && s_RDFVL_Subject)
                s_RDFVL_Visualizer.RenderWithSamples(s_RDFVL_Subject, _empty);
            else if (s_RDFVL_Visualizer)
                s_RDFVL_Visualizer.RenderWithSamples(null, _empty);

            if (s_RDFVL_HUDVisualizer && s_RDFVL_Subject)
                s_RDFVL_HUDVisualizer.RenderWithSamples(s_RDFVL_Subject, _empty);
            else if (s_RDFVL_HUDVisualizer)
                s_RDFVL_HUDVisualizer.RenderWithSamples(null, _empty);

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
            // 清理 HUD visualizer（若存在）
            s_RDFVL_HUDVisualizer = null;
            s_RDFVL_ScanAccum = 0.0;
            s_RDFVL_FrameIndex = 0;
            s_RDFVL_LastSubjectTime = -1.0;
            s_HUDDisplayRange = 0.0;
            s_HUDZoomInPending = false;
            s_HUDZoomOutPending = false;
            RDF_VehicleLidarBootstrap_UnregisterZoomListeners();
        }

        if (s_RDFVL_Active && s_RDFVL_Scanner && s_RDFVL_Subject)
        {
            if (!s_RDFVL_Subject || !s_RDFVL_Scanner)
                return;

            // Q/E 缩放 HUD 雷达比例尺（由 AddActionListener 回调设置标志位，此处读取并处理）
            // Q/E zoom HUD radar scale (flag set by AddActionListener callback, processed here)
            //
            // 每帧强制激活 CharacterLeanLeft/Right 所在的 Context，确保其在载具内也能触发
            // Force-activate the lean context every frame so listeners fire while in vehicle
            InputManager _im = GetGame().GetInputManager();
            if (_im)
                _im.ActivateContext("CharacterMovementContext", 1);
            if (s_HUDZoomInPending || s_HUDZoomOutPending)
            {
                float _cur;
                if (s_HUDDisplayRange > 0.0)
                    _cur = s_HUDDisplayRange;
                else
                    _cur = s_Range;
                float _next;
                if (s_HUDZoomInPending)
                    _next = Math.Clamp(_cur / s_HUDZoomFactor, s_HUDZoomMin, s_HUDZoomMax);
                else
                    _next = Math.Clamp(_cur * s_HUDZoomFactor, s_HUDZoomMin, s_HUDZoomMax);
                s_HUDDisplayRange = _next;
                RDF_LidarHUD.SetDisplayRange(s_HUDDisplayRange);
                s_HUDZoomInPending = false;
                s_HUDZoomOutPending = false;
                Print("RDF: HUD display range -> " + s_HUDDisplayRange.ToString() + " m");
            }

            // Update adaptive strategy with current speed (m/s)
            if (s_RDFVL_Strategy)
            {
                vector subjectVel = vector.Zero;
                SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(s_RDFVL_Subject.FindComponent(SCR_CharacterControllerComponent));
                if (charCtrl)
                {
                    subjectVel = charCtrl.GetVelocity();
                }
                else
                {
                    World world = GetGame().GetWorld();
                    if (world)
                    {
                        float currentTime = world.GetWorldTime();
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
                }
                vector horizVel = subjectVel;
                horizVel[1] = 0.0;
                float speedMps = horizVel.Length();
                s_RDFVL_Strategy.SetSpeedMps(speedMps);
            }
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
            
            // 每次扫描完成后自动更新 HUD：过滤地形并按实体去重 / Auto-update HUD: filter terrain and dedupe entities
            if (didScan && samples && samples.Count() > 0)
            {
                array<ref RDF_LidarSample> filteredSamples = new array<ref RDF_LidarSample>();
                array<ref RDF_LidarSample> filteredOutSamples = new array<ref RDF_LidarSample>(); // 被过滤的样本（单独可视化）

                for (int i = 0; i < samples.Count(); i++)
                {
                    RDF_LidarSample s = samples.Get(i);
                    if (!s || !s.m_Hit || !s.m_Entity)
                        continue;

                    // 地形实体不支持 GetVolumeFromColliders，直接过滤 / Terrain entities crash GetVolumeFromColliders, skip early
                    string typeName = s.m_Entity.Type().ToString();
                    if (typeName.Contains("Terrain") || typeName.Contains("VonTerrain") || typeName.Contains("TerrainEntity"))
                    {
                        filteredOutSamples.Insert(s);
                        continue;
                    }

                    // 体积过滤 / Filter by volume
                    float vol = MeshObjectVolumeCalculator.GetVolumeFromColliders(s.m_Entity, EPhysicsLayerDefs.FireGeometry);
                    if (vol <= s_VolumeThreshold_m3)
                    {
                        filteredOutSamples.Insert(s);
                        continue;
                    }

                    filteredSamples.Insert(s);
                }

                if (s_DebugScanOutput)
                    Print(string.Format("RDF: Scan summary — total=%1 kept=%2", samples.Count().ToString(), filteredSamples.Count().ToString()));

                array<ref RDF_LidarSample> hudSamples = filteredSamples;

                // 单独渲染被过滤的样本（命中点 + 射线） — 仅当启用可视化并允许时
                if (s_ShowFilteredVisuals && filteredOutSamples.Count() > 0)
                {
                    if (s_RDFVL_Visualizer)
                    {
                        RDF_LidarVisualSettings vs = s_RDFVL_Visualizer.GetSettings();
                        bool oldDrawPoints = vs.m_DrawPoints;
                        bool oldDrawRays = vs.m_DrawRays;

                        // 临时开启点/射线显示并设置专用颜色策略
                        vs.m_DrawPoints = true;
                        vs.m_DrawRays = true;
                        s_RDFVL_Visualizer.SetColorStrategy(new RDF_ThreeColorStrategy(s_FilteredVisualColor, s_FilteredVisualColor, s_FilteredVisualColor));

                        s_RDFVL_Visualizer.RenderWithSamples(s_RDFVL_Subject, filteredOutSamples);

                        // 恢复默认策略与设置（bootstrap 的默认颜色）
                        s_RDFVL_Visualizer.SetColorStrategy(new RDF_ThreeColorStrategy(0xFF00FF00, 0xFFFFFF00, 0xFFFF0000));
                        vs.m_DrawPoints = oldDrawPoints;
                        vs.m_DrawRays = oldDrawRays;
                    }
                }

                // 单独渲染被保留的样本（命中点 + 射线） — 用户要求：对被保留物体进行可视化
                if (s_ShowKeptVisuals && filteredSamples.Count() > 0)
                {
                    // 优先使用全局 visualizer；若全局被禁用（s_RDFVL_Visualizer == null），使用 HUD 专用 visualizer
                    RDF_LidarVisualizer useViz = s_RDFVL_Visualizer;
                    bool usingHudViz = false;
                    if (!useViz)
                    {
                        if (!s_RDFVL_HUDVisualizer)
                            s_RDFVL_HUDVisualizer = new RDF_LidarVisualizer();
                        useViz = s_RDFVL_HUDVisualizer;
                        usingHudViz = true;
                    }

                    if (useViz)
                    {
                        RDF_LidarVisualSettings vs2 = useViz.GetSettings();
                        bool oldDrawPoints2 = vs2.m_DrawPoints;
                        bool oldDrawRays2 = vs2.m_DrawRays;
                        bool oldRenderWorld = vs2.m_RenderWorld;

                        vs2.m_DrawPoints = true;
                        vs2.m_DrawRays = true;
                        // 在世界空间渲染 HUD 中显示的点/射线（以便在场景中可见）
                        vs2.m_RenderWorld = true;
                        vs2.m_UseDistanceGradient = false;
                        vs2.m_UseBatchedMesh = s_UseBatchedMesh;

                        useViz.SetColorStrategy(new RDF_ThreeColorStrategy(s_KeptVisualColor, s_KeptVisualColor, s_KeptVisualColor));

                        Print("RDF: Render HUD samples count=" + hudSamples.Count().ToString() + " usingHUDViz=" + usingHudViz.ToString());
                        useViz.RenderWithSamples(s_RDFVL_Subject, hudSamples);

                        // 恢复设置
                        useViz.SetColorStrategy(new RDF_ThreeColorStrategy(0xFF00FF00, 0xFFFFFF00, 0xFFFF0000));
                        vs2.m_DrawPoints = oldDrawPoints2;
                        vs2.m_DrawRays = oldDrawRays2;
                        vs2.m_RenderWorld = oldRenderWorld;

                        // 如果使用的是临时 HUD visualizer 并且主 visualizer 被禁用，则保留 hud visualizer（继续重用）
                        // 若你希望在不显示时销毁它，可以在 SetShowKeptVisuals(false) 时清理。
                    }
                }

                if (s_RDFVL_ModeLabelBase == "")
                    s_RDFVL_ModeLabelBase = "Vehicle " + s_RayCount.ToString();

                RDF_LidarHUD.SetMode(s_RDFVL_ModeLabelBase);

                // 将（已去重或完整的）HUD 用样本推送给 HUD，保证 HUD 与 3D 可视化一致
                RDF_LidarHUD.FeedSamples(hudSamples); // 始终推送（空数组将清空 HUD）

                // 若 HUD 本次没有样本，强制重建 HUD（Hide→Show→Feed）以避免 HUD 保留上一帧条目的实现差异
                if (hudSamples.Count() == 0)
                {
                    RDF_LidarHUD.Hide();
                    RDF_LidarHUD.Show();
                    RDF_LidarHUD.SetDisplayRange(s_Range);
                    RDF_LidarHUD.SetMode(s_RDFVL_ModeLabelBase);
                    RDF_LidarHUD.FeedSamples(hudSamples); // 再次确保 HUD 为空
                }

                // 若本次扫描没有任何命中（didScan 且 samples 为空或全部被过滤），确保清理 3D visualizer 的残留
                // 这避免 HUD 在从有命中切换到“无命中”时仍保留上一帧信息的情况
                if (didScan && (!samples || samples.Count() == 0 || hudSamples.Count() == 0))
                {
                    array<ref RDF_LidarSample> _empty = new array<ref RDF_LidarSample>();
                    // 强制清空 visualizers（主/ HUD 专用）
                    if (s_RDFVL_Visualizer)
                        s_RDFVL_Visualizer.RenderWithSamples(s_RDFVL_Subject, _empty);
                    if (s_RDFVL_HUDVisualizer)
                        s_RDFVL_HUDVisualizer.RenderWithSamples(s_RDFVL_Subject, _empty);

                    // 清除缓存的上次样本，防止帧间重绘
                    s_RDFVL_LastSamples = null;
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
