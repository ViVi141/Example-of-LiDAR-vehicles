// 前向扫描策略：100% 前向集中扫描（60°×30° FOV）
class RDF_AdaptiveSampleStrategy : RDF_LidarSampleStrategy
{
    protected float m_FrontHorizFov = 60.0;
    protected float m_FrontVertFov = 30.0;
    protected float m_SideHorizFov = 0.0;
    protected float m_SideVertFov = 0.0;
    protected float m_RearHorizFov = 0.0;
    protected float m_RearVertFov = 0.0;
    protected float m_PitchBiasDeg = 4.0;
    protected float m_MinPitchDeg = -3.0;

    protected float m_FrontRatio = 1.00;
    protected float m_SideRatio = 0.00;
    protected float m_RearRatio = 0.00;
    protected int m_MinSideRays = 24;
    protected int m_MinRearRays = 16;

    protected float m_SpeedMps = 0.0;

    void RDF_AdaptiveSampleStrategy()
    {
    }

    void SetSpeedMps(float speedMps)
    {
        m_SpeedMps = Math.Max(0.0, speedMps);
        // 前向扫描模式：无论速度如何，全部集中在前方
        // Forward-only mode: always 100% front focus regardless of speed
        m_FrontRatio = 1.00;
        m_SideRatio = 0.00;
        m_RearRatio = 0.00;
    }

    override vector BuildDirection(int index, int count)
    {
        int safeCount = Math.Max(1, count);
        int frontCount = Math.Max(1, Math.Round(safeCount * m_FrontRatio));
        int sideCount = Math.Max(0, Math.Round(safeCount * m_SideRatio));
        int rearCount = Math.Max(0, safeCount - frontCount - sideCount);

        if (safeCount >= (m_MinSideRays + m_MinRearRays + 8))
        {
            sideCount = Math.Max(sideCount, m_MinSideRays);
            rearCount = Math.Max(rearCount, m_MinRearRays);
            frontCount = Math.Max(1, safeCount - sideCount - rearCount);
        }

        if (index < frontCount)
            return BuildRectDir(index, frontCount, m_FrontHorizFov, m_FrontVertFov, 0.0);

        int sideIndex = index - frontCount;
        if (sideIndex < sideCount)
        {
            int leftCount = Math.Max(1, sideCount / 2);
            int rightCount = Math.Max(1, sideCount - leftCount);
            if (sideIndex < leftCount)
                return BuildRectDir(sideIndex, leftCount, m_SideHorizFov, m_SideVertFov, -90.0);
            return BuildRectDir(sideIndex - leftCount, rightCount, m_SideHorizFov, m_SideVertFov, 90.0);
        }

        int rearIndex = index - frontCount - sideCount;
        return BuildRectDir(rearIndex, Math.Max(1, rearCount), m_RearHorizFov, m_RearVertFov, 180.0);
    }

    protected vector BuildRectDir(int index, int count, float horizFovDeg, float vertFovDeg, float yawCenterDeg)
    {
        int cols = Math.Max(1, Math.Round(Math.Sqrt(count * 2.0)));
        int rows = Math.Max(1, Math.Round(count / (float)cols));
        int total = cols * rows;
        int idx = index % total;
        int col = idx % cols;
        int row = idx / cols;

        float u = (col + 0.5) / (float)cols;
        float v = (row + 0.5) / (float)rows;

        float phi = (u - 0.5) * horizFovDeg + yawCenterDeg;
        float theta = (v - 0.5) * vertFovDeg + m_PitchBiasDeg;
        if (theta < m_MinPitchDeg)
            theta = m_MinPitchDeg;

        float phiRad = phi * Math.DEG2RAD;
        float thetaRad = theta * Math.DEG2RAD;

        float cz = Math.Cos(thetaRad);
        float x = Math.Sin(phiRad) * cz;
        float y = Math.Sin(thetaRad);
        float z = Math.Cos(phiRad) * cz;

        return Vector(x, y, z);
    }
}