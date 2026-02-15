class SCR_SimpleVehicleDemo : SCR_BaseGameMode
{
    private bool m_Started = false;

    // 自适应采样策略：根据命中率动态调整射线数量
    private float m_LastHitRate = 0.0;
    private float m_AdaptInterval = 2.0;
    private float m_Elapsed = 0.0;
    private int m_RayCount = 256;
    private float m_ScanRange = 50.0;
    private ref RDF_AdaptiveSampleStrategy m_AdaptiveStrategy;
    private ref RDF_LidarScanner m_Scanner;
    private ref RDF_LidarVisualizer m_Visualizer;
    private ref RDF_BlockHighlightColorStrategy m_ColorStrategy;
    private IEntity m_Subject;
    private vector m_LastSubjectPos = vector.Zero;
    private float m_LastSubjectTime = -1.0;
    private float m_BlockDetectRange = 25.0;
    private int m_BlockBinThreshold = 8;

    void OnUpdate(float dt)
    {
        if (!m_Started)
        {
            m_Started = true;

            m_Subject = RDF_LidarSubjectResolver.ResolveLocalSubject(true);
            m_AdaptiveStrategy = new RDF_AdaptiveSampleStrategy();

            m_Scanner = new RDF_LidarScanner();
            RDF_LidarSettings s = m_Scanner.GetSettings();
            s.m_RayCount = m_RayCount;
            s.m_Range = m_ScanRange;
            s.m_UpdateInterval = 0.1;
            m_Scanner.SetSampleStrategy(m_AdaptiveStrategy);

            m_Visualizer = new RDF_LidarVisualizer();
            RDF_LidarVisualSettings vs = m_Visualizer.GetSettings();
            vs.m_DrawPoints = true;
            vs.m_DrawRays = true;
            vs.m_ShowHitsOnly = false;
            vs.m_RenderWorld = true;
            vs.m_UseDistanceGradient = false;
            vs.m_UseBatchedMesh = true;
            m_ColorStrategy = new RDF_BlockHighlightColorStrategy();
            m_Visualizer.SetColorStrategy(m_ColorStrategy);

            Print("RDF: SimpleVehicle demo started with adaptive strategy (manual visualizer).");
        }

        if (!m_Subject)
            m_Subject = RDF_LidarSubjectResolver.ResolveLocalSubject(true);

        // 每隔 m_AdaptInterval 秒自适应调整射线数量
        m_Elapsed += dt;
        if (m_Elapsed >= m_AdaptInterval)
        {
            m_Elapsed = 0.0;
            UpdateStrategySpeed();
            if (m_Visualizer)
            {
                array<ref RDF_LidarSample> samples = m_Visualizer.GetLastSamples();
                if (samples && samples.Count() > 0)
                {
                    bool hasBlock = DetectObstacleBlock(samples);
                    if (m_ColorStrategy)
                        m_ColorStrategy.SetHighlight(hasBlock);
                    int hitCount = RDF_LidarSampleUtils.GetHitCount(samples);
                    float hitRate = hitCount / (float)samples.Count();
                    m_LastHitRate = hitRate;
                    // 根据命中率调整射线数量（示例：命中率低则增加射线，高则减少）
                    if (hitRate < 0.3 && m_RayCount < 1024)
                        m_RayCount += 64;
                    else if (hitRate > 0.7 && m_RayCount > 128)
                        m_RayCount -= 64;
                    if (m_Scanner)
                        m_Scanner.GetSettings().m_RayCount = m_RayCount;
                    Print("Adaptive: hitRate=" + hitRate.ToString() + ", rayCount=" + m_RayCount.ToString());
                }
            }
        }

        if (m_Visualizer && m_Scanner && m_Subject)
            m_Visualizer.Render(m_Subject, m_Scanner);
    }

    void UpdateStrategySpeed()
    {
        if (!m_AdaptiveStrategy)
            return;

        IEntity subject = RDF_LidarSubjectResolver.ResolveLocalSubject(true);
        if (!subject)
            return;

        vector velocity = vector.Zero;
        SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(subject.FindComponent(SCR_CharacterControllerComponent));
        if (charCtrl)
        {
            velocity = charCtrl.GetVelocity();
        }
        else
        {
            World world = GetGame().GetWorld();
            if (!world)
                return;
            float currentTime = world.GetWorldTime();
            vector pos = subject.GetOrigin();
            if (m_LastSubjectTime >= 0.0)
            {
                float dtMs = currentTime - m_LastSubjectTime;
                if (dtMs > 10.0)
                {
                    float dtSec = dtMs / 1000.0;
                    velocity = (pos - m_LastSubjectPos) / dtSec;
                }
            }
            m_LastSubjectPos = pos;
            m_LastSubjectTime = currentTime;
        }

        vector horizVel = velocity;
        horizVel[1] = 0.0;
        float speedMps = horizVel.Length();
        m_AdaptiveStrategy.SetSpeedMps(speedMps);
    }

    bool DetectObstacleBlock(array<ref RDF_LidarSample> samples)
    {
        int azBins = 12;
        int elBins = 6;
        int totalBins = azBins * elBins;
        array<int> counts = new array<int>();
        counts.Resize(totalBins);

        for (int i = 0; i < samples.Count(); i++)
        {
            RDF_LidarSample s = samples.Get(i);
            if (!s || !s.m_Hit)
                continue;
            if (s.m_Distance > m_BlockDetectRange)
                continue;

            vector dir = s.m_Dir;
            float z = Math.Clamp(dir[2], -1.0, 1.0);
            float az = Math.Atan2(dir[1], dir[0]);
            float el = Math.Asin(z);

            int azIdx = Math.Floor(((az + Math.PI) / (2.0 * Math.PI)) * azBins);
            int elIdx = Math.Floor(((el + (Math.PI * 0.5)) / Math.PI) * elBins);
            azIdx = Math.Clamp(azIdx, 0, azBins - 1);
            elIdx = Math.Clamp(elIdx, 0, elBins - 1);

            int idx = elIdx * azBins + azIdx;
            counts[idx] = counts[idx] + 1;
            if (counts[idx] >= m_BlockBinThreshold)
                return true;
        }

        return false;
    }
}
