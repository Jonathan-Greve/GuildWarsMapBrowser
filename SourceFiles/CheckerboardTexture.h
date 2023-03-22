#pragma once
class CheckerboardTexture
{
public:
    CheckerboardTexture(int width, int height, int tile_size)
        : m_width(width)
        , m_height(height)
        , m_tile_size(tile_size)
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
                bool is_black_tile = ((x / m_tile_size) % 2) ^ ((y / m_tile_size) % 2);
                uint8_t color = is_black_tile ? 190 : 255;

                int index = (y * m_width + x) * 4;
                m_data[index] = color; // R
                m_data[index + 1] = color; // G
                m_data[index + 2] = color; // B
                m_data[index + 3] = 255; // A
            }
        }
    }

    int m_width;
    int m_height;
    int m_tile_size;
    std::vector<uint8_t> m_data;
};
