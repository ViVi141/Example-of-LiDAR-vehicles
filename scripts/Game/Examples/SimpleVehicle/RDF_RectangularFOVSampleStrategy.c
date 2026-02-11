// 矩形视场采样策略：120° x 25.4° FOV，1200 x 128 分辨率（对应 1,536,000 pts/s @ 10 Hz）
// Rectangular FOV sample strategy: 120° x 25.4° FOV, 1200 x 128 resolution (~1,536,000 pts/s @ 10 Hz)
class RDF_RectangularFOVSampleStrategy : RDF_LidarSampleStrategy
{
    protected float m_HorizHalfDeg;
    protected float m_VertHalfDeg;
    protected int m_Cols;
    protected int m_Rows;

    void RDF_RectangularFOVSampleStrategy(float horizFOVDeg = 120.0, float vertFOVDeg = 25.4, int cols = 1200, int rows = 128)
    {
        m_HorizHalfDeg = Math.Clamp(horizFOVDeg * 0.5, 0.1, 180.0);
        m_VertHalfDeg = Math.Clamp(vertFOVDeg * 0.5, 0.1, 90.0);
        m_Cols = Math.Max(1, cols);
        m_Rows = Math.Max(1, rows);
    }

    override vector BuildDirection(int index, int count)
    {
        int total = m_Cols * m_Rows;
        int idx = index % total;
        int col = idx % m_Cols;
        int row = idx / m_Cols;

        float u = (col + 0.5) / (float)m_Cols;
        float v = (row + 0.5) / (float)m_Rows;

        float phi = (u - 0.5) * 2.0 * m_HorizHalfDeg * Math.DEG2RAD;
        float theta = (v - 0.5) * 2.0 * m_VertHalfDeg * Math.DEG2RAD;

        float cz = Math.Cos(theta);
        float x = Math.Sin(phi) * cz;
        float y = Math.Sin(theta);
        float z = Math.Cos(phi) * cz;

        return Vector(x, y, z);
    }
}
