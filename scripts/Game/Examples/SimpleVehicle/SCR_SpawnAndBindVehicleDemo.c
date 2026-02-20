class SCR_SpawnAndBindVehicleDemo : SCR_BaseGameMode
{
    private bool m_Active = false;
    private IEntity m_Subject;
    private ref RDF_LidarVisualizer m_Visualizer;
    private ref RDF_LidarScanner m_Scanner;

    void OnUpdate(float dt)
    {
        // 每帧检查玩家是否在载具（通过解析 preferVehicle=true/false 区分）
        // Check each frame if player is in vehicle (preferVehicle=true/false)
        IEntity vehicle = RDF_LidarSubjectResolver.ResolveLocalSubject(true);
        IEntity playerSubject = RDF_LidarSubjectResolver.ResolveLocalSubject(false);

        bool isInVehicle = false;
        if (vehicle && playerSubject && vehicle != playerSubject)
            isInVehicle = true;

        if (isInVehicle && !m_Active)
        {
            m_Active = true;
            m_Subject = vehicle;

            // 创建 scanner / visualizer 并设置参数（4096 条射线，半径 50，前向60°×30°扫描）
            // Create scanner/visualizer and set params (4096 rays, radius 50, forward 60°×30°)
            m_Scanner = new RDF_LidarScanner();
            RDF_LidarSettings s = m_Scanner.GetSettings();
            s.m_RayCount = 4096;
            s.m_Range = 50.0;
            s.m_UpdateInterval = 0.5; // 每 0.5s 一次扫描（示例）/ Scan every 0.5s (example)

            // 使用矩形前向FOV采样策略（60°×30°）/ Use rectangular forward FOV (60°×30°)
            m_Scanner.SetSampleStrategy(new RDF_RectangularFOVSampleStrategy(60.0, 30.0, 64, 64));

            m_Visualizer = new RDF_LidarVisualizer();
            RDF_LidarVisualSettings vs = m_Visualizer.GetSettings();
            vs.m_DrawPoints = false;  // 关闭点云绘制，使用 HUD 显示
            vs.m_DrawRays = false;    // 关闭射线绘制，使用 HUD 显示
            vs.m_RenderWorld = true; // 世界空间渲染 / World-space render
            vs.m_UseDistanceGradient = false; // 使用自定义三端颜色策略 / Use custom 3-color strategy

            // 三端颜色：近=绿, 中=黄, 远=红 / Three colors: near=green, mid=yellow, far=red
            m_Visualizer.SetColorStrategy(new RDF_ThreeColorStrategy(0xFF00FF00, 0xFFFFFF00, 0xFFFF0000));

            Print("RDF: Spawn & Bind demo started and bound to vehicle: " + m_Subject.GetName());
        }
        else if (!vehicle && m_Active)
        {
            // 玩家离开载具，停止可视化 / Player left vehicle; stop visualization
            m_Active = false;
            m_Subject = null;
            m_Scanner = null;
            m_Visualizer = null;
            Print("RDF: Player left vehicle; LiDAR visualization stopped.");
        }

        // 若处于活跃状态则渲染 / Render when active
        if (m_Active && m_Visualizer && m_Scanner && m_Subject)
        {
            m_Visualizer.Render(m_Subject, m_Scanner);
        }
    }
}