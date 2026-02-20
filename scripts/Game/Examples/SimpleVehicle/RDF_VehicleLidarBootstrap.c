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
static float s_Range = 30.0;

// 扫描间隔（秒），10 Hz / Scan interval (s), 10 Hz
static float s_UpdateInterval = 0.1;

// 矩形视场：60° 水平（前向±30°），30° 垂直（±15°）/ Rect FOV: 60° horiz (forward ±30°), 30° vert (±15°)
static float s_RectFOVHorizDeg = 60.0;
static float s_RectFOVVertDeg = 30.0;
static int s_RectCols = 64;
static int s_RectRows = 64;  // 增加行数以匹配4096射线（64x64）

// 扫描但不可视化 3D 点云/射线，仅用数据驱动 HUD / Scan without 3D visualization, data-driven HUD only
static bool s_ScanWithoutVisualization = true;

// 可选：对 HUD 输出进行实体去重（每个实体仅显示一次）
// Optional: dedupe entities for HUD (show each IEntity only once)
static bool s_DedupeEntitiesForHUD = true;

// 可选：开启/关闭扫描调试输出（控制台）
// Optional: enable/disable verbose scan debug output
static bool s_DebugScanOutput = true;

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

// 试图从实体的 EntityPrefabData 中读取更可读的 prefab 名称（若存在），
// 返回 name（首选 prefab 名）和 tree（预制体层级路径，若需要）。
bool RDF_GetQueryTargetInfo(IEntity ent, out string name, out string tree)
{
    name = "";
    tree = "";

    if (!ent)
        return false;

    EntityPrefabData prefabData = ent.GetPrefabData();
    if (!prefabData)
        return false;

    name = prefabData.GetPrefabName();
    tree = "";

    BaseContainer cont = prefabData.GetPrefab();
    while (cont)
    {
        string contName = cont.GetName();
        if (!contName.IsEmpty())
        {
            if (!tree.IsEmpty())
                tree = tree + "\n";
            tree = tree + contName;

            if (name.IsEmpty())
                name = contName;
        }

        cont = cont.GetAncestor();
    }

    return (name != "" || !tree.IsEmpty());
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
            RDF_LidarHUD.Show();
            RDF_LidarHUD.SetDisplayRange(s_Range);
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

                // 用于实体去重与统计 / containers for dedupe & stats
                array<IEntity> seenEntities = new array<IEntity>();
                array<float> seenDistances = new array<float>();
                array<string> seenTypeNames = new array<string>();

                for (int i = 0; i < samples.Count(); i++)
                {
                    RDF_LidarSample s = samples.Get(i);
                    if (!s || !s.m_Hit || !s.m_Entity)
                        continue;

                    // 获取实体类型/预制名/实例名/距离（prefab > instance name > type 作为回退）
                    typename entityType = s.m_Entity.Type();
                    string typeName = entityType.ToString();
                    string entityName = s.m_Entity.GetName();
                    float dist = s.m_Distance;

// 本地友好名（prefab/实例名/类型回退）
string readableName = "";
string prefabName = "";
string prefabTree = "";
if (RDF_GetQueryTargetInfo(s.m_Entity, prefabName, prefabTree) && prefabName != "")
    readableName = prefabName;
else if (entityName && entityName != "")
    readableName = entityName;
else
    readableName = typeName;

                    // 跳过地形类实体
                    bool isTerrain = typeName.Contains("Terrain") || typeName.Contains("Ground");
                    if (isTerrain)
                        continue;

                    // 保留所有非地形样本（不对射线本身去重）；仅在用于显示/标题时对实体去重
                    // 将样本加入 filteredSamples（供 HUD/CSV/可视化使用）
                    filteredSamples.Insert(s);

                    // 若能从 prefab 读取更友好的名称则优先写入样本字段，供 HUD/CSV 使用（对每个样本都写入）
                    string prefabNameOut = "";
                    string prefabTreeOut = "";
                    if (RDF_GetQueryTargetInfo(s.m_Entity, prefabNameOut, prefabTreeOut) && prefabNameOut != "")
                        s.m_ColliderName = prefabNameOut;
                    else if (readableName && readableName != "")
                        s.m_ColliderName = readableName;

                    // 显示去重（仅用于构建显示清单，不影响 samples 本身）
                    int idx = seenEntities.Find(s.m_Entity);
                    if (s_DedupeEntitiesForHUD)
                    {
                        if (idx == -1)
                        {
                            // 首次遇到该实体：记录以供后续在 HUD/日志 中显示
                            seenEntities.Insert(s.m_Entity);
                            seenDistances.Insert(dist);
                            seenTypeNames.Insert(typeName);
                        }
                        else
                        {
                            // 如果当前射线更近，可以更新记录的距离（可选）
                            if (dist < seenDistances.Get(idx))
                                seenDistances.Set(idx, dist);
                        }
                    }
                }

                // 打印摘要与去重实体清单（若启用调试输出）
                int total = samples.Count();
                int displayed;
                if (s_DedupeEntitiesForHUD)
                    displayed = seenEntities.Count();
                else
                    displayed = filteredSamples.Count();
                int filtered = total - displayed;
                Print(string.Format("RDF: Scan summary — total=%1 displayed=%2 filtered=%3", total.ToString(), displayed.ToString(), filtered.ToString()));

                if (s_DebugScanOutput)
                {
                    if (displayed > 0)
                    {
                        Print("RDF: Displayed entities:");
                        int listCount;
                        if (s_DedupeEntitiesForHUD)
                            listCount = seenEntities.Count();
                        else
                            listCount = filteredSamples.Count();

                        for (int j = 0; j < listCount; j++)
                        {
                            if (s_DedupeEntitiesForHUD)
                            {
                                string tn = seenTypeNames.Get(j);
                                IEntity e = seenEntities.Get(j);
                                string prefabOut = "";
                                string treeOut = "";                                string displayLabel = "";                                if (RDF_GetQueryTargetInfo(e, prefabOut, treeOut) && prefabOut != "")
                                    displayLabel = prefabOut;
                                else
                                {
                                    string en = e.GetName();
                                    if (en && en != "")
                                        displayLabel = en;
                                    else
                                        displayLabel = tn;
                                }
                                float d = seenDistances.Get(j);
                                Print(string.Format("RDF:  - %1 (%2) at %3m", displayLabel, tn, d));
                            }
                            else
                            {
                                RDF_LidarSample samp = filteredSamples.Get(j);
                                if (samp && samp.m_Entity)
                                {
                                    string tn = samp.m_Entity.Type().ToString();
                                    IEntity e = samp.m_Entity;
                                    string prefab = e.GetName();
                                    float d = samp.m_Distance;
                                    string displayLabel = prefab;
                                    if (!displayLabel || displayLabel == "")
                                        displayLabel = tn;
                                    Print(string.Format("RDF:  - %1 (%2) at %3m", displayLabel, tn, d));
                                }
                            }
                        }
                    }
                    else
                    {
                        Print("RDF: No displayable entities this scan (all filtered).");
                    }
                }

                // 更新 HUD 标题以包含前几个已去重实体的 prefab/名称（便于在 HUD 上快速识别目标）
                string names = "";
                int maxShown = 4;
                int listCountForMode;
                if (s_DedupeEntitiesForHUD)
                    listCountForMode = seenEntities.Count();
                else
                    listCountForMode = filteredSamples.Count();
                for (int k = 0; k < Math.Min(maxShown, listCountForMode); k++)
                {
                    string label = "";
                    if (s_DedupeEntitiesForHUD)
                    {
                        IEntity e = seenEntities.Get(k);
                        string pn = "";
                        string tr = "";
                        if (RDF_GetQueryTargetInfo(e, pn, tr) && pn != "")
                            label = pn;
                        else
                        {
                            string en = e.GetName();
                            if (en && en != "")
                                label = en;
                            else
                                label = seenTypeNames.Get(k);
                        }
                    }
                    else
                    {
                        RDF_LidarSample samp = filteredSamples.Get(k);
                        if (samp && samp.m_Entity)
                        {
                            string pn = "";
                            string tr = "";
                            if (RDF_GetQueryTargetInfo(samp.m_Entity, pn, tr) && pn != "")
                                label = pn;
                            else
                            {
                                string en = samp.m_Entity.GetName();
                                if (en && en != "")
                                    label = en;
                                else
                                    label = samp.m_Entity.Type().ToString();
                            }
                        }
                    }

                    if (label != "")
                    {
                        if (names == "")
                            names = label;
                        else
                            names = names + ", " + label;
                    }
                }

                if (s_RDFVL_ModeLabelBase == "")
                    s_RDFVL_ModeLabelBase = "Vehicle " + s_RayCount.ToString();

                if (names != "")
                    RDF_LidarHUD.SetMode(s_RDFVL_ModeLabelBase + " | " + names);
                else
                    RDF_LidarHUD.SetMode(s_RDFVL_ModeLabelBase);

                // 将去重后的样本推送给 HUD（HUD 仍然期望 samples 列表）
                if (filteredSamples.Count() > 0)
                    RDF_LidarHUD.FeedSamples(filteredSamples);
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
