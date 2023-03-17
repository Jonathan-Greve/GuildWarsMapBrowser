#pragma once
class CheckerboardTexture
{
public:
    CheckerboardTexture(int width, int height)
        : m_width(width)
        , m_height(height)
        , m_data(width * height * 4)
    {
        generateTexture();
    }

    const std::vector<uint8_t>& getData() const { return m_data; }

private:
    void generateTexture()
    {
        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                int index = (y * m_width + x) * 4;
                if ((x % 2 == 0) == (y % 2 == 0))
                {
                    m_data[index] = m_data[index + 1] = m_data[index + 2] = 255; // White
                }
                else
                {
                    m_data[index] = m_data[index + 1] = m_data[index + 2] = 100; // Black
                }
                m_data[index + 3] = 255; // Alpha
            }
        }
    }

    int m_width;
    int m_height;
    std::vector<uint8_t> m_data;
};
