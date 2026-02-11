class SCR_SimpleVehicleDemo : SCR_BaseGameMode
{
    private bool m_Started = false;

    void OnUpdate(float dt)
    {
        if (!m_Started)
        {
            m_Started = true;

            // 创建默认配置并启动 demo / Create default config and start demo
            RDF_LidarDemoConfig cfg = RDF_LidarDemoConfig.CreateDefault(256);
            cfg.m_RenderWorld = true;
            cfg.m_Verbose = true;

            // 应用并启动演示 / Apply and start demo
            RDF_LidarAutoRunner.StartWithConfig(cfg);
            RDF_LidarAutoRunner.SetDemoEnabled(true);

            Print("RDF: SimpleVehicle demo started.");
        }
    }
}