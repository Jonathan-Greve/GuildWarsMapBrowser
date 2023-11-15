#pragma once
class CheckerboardTexture
{
public:
    // Define a struct to hold RGB color values
    struct CheckerboardColor {
        uint8_t r, g, b;
    };

    static const CheckerboardColor presetColors[20];

    enum ColorChoice : uint8_t {
        Red, Green, Blue, Yellow, Cyan,
        Magenta, Silver, Gray, Maroon, Olive,
        DarkGreen, Purple, Teal, Navy, Orange,
        Pink, Brown, White, Black, DimGray
    };

    CheckerboardTexture(int width, int height, int tile_size, ColorChoice color1 = ColorChoice::White, ColorChoice color2 = ColorChoice::Silver)
        : m_width(width)
        , m_height(height)
        , m_tile_size(tile_size)
        , m_data(width* height * 4)
    {
        m_color1 = presetColors[static_cast<size_t>(color1)];
        m_color2 = presetColors[static_cast<size_t>(color2)];
        generateTexture();
    }

    const std::vector<uint8_t>& getData() const { return m_data; }

    static ColorChoice GetColorForIndex(int index) {
        int numColors = 20; // Total number of available colors
        return static_cast<CheckerboardTexture::ColorChoice>(index % numColors);
    }

private:
    void generateTexture()
    {
        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                bool is_color1 = ((x / m_tile_size) % 2) ^ ((y / m_tile_size) % 2);
                CheckerboardColor& color = is_color1 ? m_color1 : m_color2;

                int index = (y * m_width + x) * 4;
                m_data[index] = color.r; // R
                m_data[index + 1] = color.g; // G
                m_data[index + 2] = color.b; // B
                m_data[index + 3] = 255; // A
            }
        }
    }

private:
    int m_width;
    int m_height;
    int m_tile_size;
    CheckerboardColor m_color1, m_color2;
    std::vector<uint8_t> m_data;
};