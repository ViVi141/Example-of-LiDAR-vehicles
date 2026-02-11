// 状态打印：每 m_Interval 秒打印一次 RDF 状态。使用 EOnFrame 驱动，不依赖 GetCallqueue（部分环境无此 API）。
// Status printer: prints RDF status every m_Interval seconds. EOnFrame-driven, no GetCallqueue dependency.
class RDF_StatusPrinter
{
    protected float m_Interval = 2.0;
    protected float m_AccumulatedTime = 0.0;

    void RDF_StatusPrinter()
    {
    }

    // 由 modded SCR_PlayerController 每帧调用，累积时间到间隔后执行一次打印。
    // Called by modded SCR_PlayerController each frame; prints when interval elapsed.
    void Tick(float timeSlice)
    {
        m_AccumulatedTime = m_AccumulatedTime + timeSlice;
        if (m_AccumulatedTime >= m_Interval)
        {
            StatusTick();
            m_AccumulatedTime = 0.0;
        }
    }

    void StatusTick()
    {
        bool demoEnabled = false;
        bool running = false;
        bool renderWorld = true;
        int rayCount = -1;
        string cfgName = "none";

        demoEnabled = RDF_LidarAutoRunner.IsDemoEnabled();
        running = RDF_LidarAutoRunner.IsRunning();
        renderWorld = RDF_LidarAutoRunner.GetDemoRenderWorld();

        RDF_LidarDemoConfig cfg = RDF_LidarAutoRunner.GetDemoConfig();
        if (cfg)
        {
            rayCount = cfg.m_RayCount;
            cfgName = "preset";
        }

        IEntity subj = RDF_LidarSubjectResolver.ResolveLocalSubject(true);
        string subjName;
        if (subj)
            subjName = subj.GetName();
        else
            subjName = "NULL";

    }
}

ref RDF_StatusPrinter g_RDF_StatusPrinter = new RDF_StatusPrinter();

modded class SCR_PlayerController
{
    override void EOnFrame(IEntity owner, float timeSlice)
    {
        super.EOnFrame(owner, timeSlice);
        if (!GetGame())
            return;
        if (GetGame().GetPlayerController() != this)
            return;
        if (g_RDF_StatusPrinter)
            g_RDF_StatusPrinter.Tick(timeSlice);
    }
}
