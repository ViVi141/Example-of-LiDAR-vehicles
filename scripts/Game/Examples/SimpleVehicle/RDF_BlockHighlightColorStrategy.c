// filepath: scripts/Game/Examples/SimpleVehicle/RDF_BlockHighlightColorStrategy.c
// 块障碍物检测高亮颜色策略 / Block obstacle detection highlight color strategy
// 当检测到块状障碍时改变点云颜色以强调警告
// Changes point cloud color when block obstacles are detected to emphasize warning

class RDF_BlockHighlightColorStrategy : RDF_LidarColorStrategy
{
    protected bool m_HasBlock = false;
    protected int m_HighlightColor = 0xFFFF0000;  // 红色 / Red
    protected int m_NormalColor = 0xFF00FF00;      // 绿色 / Green

    void RDF_BlockHighlightColorStrategy()
    {
    }

    void SetHighlight(bool hasBlock)
    {
        m_HasBlock = hasBlock;
    }

    bool GetHighlight()
    {
        return m_HasBlock;
    }

    override int BuildPointColor(float dist, bool hit, float lastRange, RDF_LidarVisualSettings settings)
    {
        if (!hit)
            return 0xFFFF6600;  // 橙色，未命中 / Orange for miss

        // 如果检测到块，返回红色；否则根据距离返回颜色
        // If block detected return red; otherwise use distance gradient
        if (m_HasBlock)
            return m_HighlightColor;

        // 正常情况下，根据距离渐变：近绿→中黄→远红
        // Normal case: distance gradient green→yellow→red
        float t;
        if (lastRange > 0.001)
            t = dist / lastRange;
        else
            t = 0.5;
        t = Math.Clamp(t, 0.0, 1.0);

        if (t < 0.5)
        {
            // 近距离：绿→黄 / Near distance: green → yellow
            float localT = t * 2.0;  // 0 to 1
            int r = Math.Round(localT * 255.0);
            int g = 255;
            int b = 0;
            return 0xFF000000 | (b << 16) | (g << 8) | r;
        }
        else
        {
            // 远距离：黄→红 / Far distance: yellow → red
            float localT = (t - 0.5) * 2.0;  // 0 to 1
            int r = 255;
            int g = Math.Round((1.0 - localT) * 255.0);
            int b = 0;
            return 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

    override int BuildRayColorAtT(float t, bool hit, RDF_LidarVisualSettings settings)
    {
        if (!hit)
            return 0x44FF6600;  // 半透明橙色，未命中 / Semi-transparent orange for miss

        if (m_HasBlock)
            return 0x88FF0000;  // 半透明红色 / Semi-transparent red when block detected

        // 正常时射线颜色
        // Normal ray color based on t parameter
        int r = Math.Round(t * 255.0);
        int g = Math.Round((1.0 - t) * 255.0);
        int b = 0;
        int alpha = 0x88;

        return (alpha << 24) | (b << 16) | (g << 8) | r;
    }
}
