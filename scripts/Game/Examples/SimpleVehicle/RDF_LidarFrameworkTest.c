// 框架自检：控制台输入 RDF_RunFrameworkTest() 可直接测试 RDF 扫描是否正常。
// Framework self-check: run RDF_RunFrameworkTest() in console to test RDF scan.
// 用法：进入游戏后按 ` 打开控制台，输入 RDF_RunFrameworkTest() 回车。
// Usage: Press ` in-game to open console, type RDF_RunFrameworkTest() Enter.

void RDF_RunFrameworkTest()
{
    Print("RDF: === Framework Test Start ===");
    PlayerController pc = GetGame().GetPlayerController();
    if (!pc)
    {
        Print("RDF: [FAIL] GetPlayerController null");
        return;
    }
    IEntity subject = pc.GetControlledEntity();
    if (!subject)
    {
        Print("RDF: [FAIL] GetControlledEntity null");
        return;
    }
    World world = subject.GetWorld();
    if (!world)
    {
        world = GetGame().GetWorld();
        Print("RDF: subject.GetWorld() was null, using GetGame().GetWorld(): " + (world != null));
    }
    if (!world)
    {
        Print("RDF: [FAIL] No world");
        return;
    }
    Print("RDF: subject=" + subject.GetName());

    RDF_LidarScanner scanner = new RDF_LidarScanner();
    RDF_LidarSettings s = scanner.GetSettings();
    s.m_RayCount = 64;
    s.m_Range = 50.0;
    array<ref RDF_LidarSample> samples = new array<ref RDF_LidarSample>();
    scanner.Scan(subject, samples);

    Print("RDF: Scan done, samples.Count=" + samples.Count().ToString());
    if (samples.Count() > 0)
    {
        RDF_LidarSample first = samples.Get(0);
        Print("RDF: First sample: hit=" + first.m_Hit.ToString() + " dist=" + first.m_Distance.ToString());
        FileIO.MakeDirectory("$profile:LiDAR");
        FileHandle f = FileIO.OpenFile("$profile:LiDAR/test_diagnostic.csv", FileMode.WRITE);
        if (f)
        {
            f.WriteLine(RDF_LidarExport.GetCSVHeader());
            for (int i = 0; i < samples.Count(); i++)
            {
                RDF_LidarSample samp = samples.Get(i);
                if (samp)
                    f.WriteLine(RDF_LidarExport.SampleToCSVRow(samp));
            }
            f.Close();
            Print("RDF: Diagnostic CSV saved to $profile:LiDAR/test_diagnostic.csv");
        }
    }
    Print("RDF: === Framework Test End ===");
}
