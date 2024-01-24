#pragma once
class CheckerboardTexture
{
public:
    // Define a struct to hold RGB color values
    struct CheckerboardColor {
        uint8_t r, g, b;
    };

    enum ColorChoice;

    static const CheckerboardColor presetColors[220];

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
        int numColors = 220; // Total number of available colors
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
                m_data[index + 3] = 128; // A
            }
        }
    }

private:
    int m_width;
    int m_height;
    int m_tile_size;
    CheckerboardColor m_color1, m_color2;
    std::vector<uint8_t> m_data;

public:
    enum ColorChoice : uint8_t {
        Red,
        Green,
        Blue,
        Yellow,
        Cyan,
        Magenta,
        Silver,
        Gray,
        Maroon,
        Olive,
        DarkGreen,
        Purple,
        Teal,
        Navy,
        Orange,
        Pink,
        Brown,
        White,
        Black,
        DimGray,
        Color21,
        Color22,
        Color23,
        Color24,
        Color25,
        Color26,
        Color27,
        Color28,
        Color29,
        Color30,
        Color31,
        Color32,
        Color33,
        Color34,
        Color35,
        Color36,
        Color37,
        Color38,
        Color39,
        Color40,
        Color41,
        Color42,
        Color43,
        Color44,
        Color45,
        Color46,
        Color47,
        Color48,
        Color49,
        Color50,
        Color51,
        Color52,
        Color53,
        Color54,
        Color55,
        Color56,
        Color57,
        Color58,
        Color59,
        Color60,
        Color61,
        Color62,
        Color63,
        Color64,
        Color65,
        Color66,
        Color67,
        Color68,
        Color69,
        Color70,
        Color71,
        Color72,
        Color73,
        Color74,
        Color75,
        Color76,
        Color77,
        Color78,
        Color79,
        Color80,
        Color81,
        Color82,
        Color83,
        Color84,
        Color85,
        Color86,
        Color87,
        Color88,
        Color89,
        Color90,
        Color91,
        Color92,
        Color93,
        Color94,
        Color95,
        Color96,
        Color97,
        Color98,
        Color99,
        Color100,
        Color101,
        Color102,
        Color103,
        Color104,
        Color105,
        Color106,
        Color107,
        Color108,
        Color109,
        Color110,
        Color111,
        Color112,
        Color113,
        Color114,
        Color115,
        Color116,
        Color117,
        Color118,
        Color119,
        Color120,
        Color121,
        Color122,
        Color123,
        Color124,
        Color125,
        Color126,
        Color127,
        Color128,
        Color129,
        Color130,
        Color131,
        Color132,
        Color133,
        Color134,
        Color135,
        Color136,
        Color137,
        Color138,
        Color139,
        Color140,
        Color141,
        Color142,
        Color143,
        Color144,
        Color145,
        Color146,
        Color147,
        Color148,
        Color149,
        Color150,
        Color151,
        Color152,
        Color153,
        Color154,
        Color155,
        Color156,
        Color157,
        Color158,
        Color159,
        Color160,
        Color161,
        Color162,
        Color163,
        Color164,
        Color165,
        Color166,
        Color167,
        Color168,
        Color169,
        Color170,
        Color171,
        Color172,
        Color173,
        Color174,
        Color175,
        Color176,
        Color177,
        Color178,
        Color179,
        Color180,
        Color181,
        Color182,
        Color183,
        Color184,
        Color185,
        Color186,
        Color187,
        Color188,
        Color189,
        Color190,
        Color191,
        Color192,
        Color193,
        Color194,
        Color195,
        Color196,
        Color197,
        Color198,
        Color199,
        Color200,
        Color201,
        Color202,
        Color203,
        Color204,
        Color205,
        Color206,
        Color207,
        Color208,
        Color209,
        Color210,
        Color211,
        Color212,
        Color213,
        Color214,
        Color215,
        Color216,
        Color217,
        Color218,
        Color219,
        Color220
    };
};