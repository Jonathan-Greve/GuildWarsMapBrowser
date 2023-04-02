#pragma once

union RGBA
{
    unsigned char c[4];
    struct
    {
        unsigned char r, g, b, a;
    };
    unsigned int dw;
};

struct DatTexture
{
    int width;
    int height;
    std::vector<RGBA> rgba_data;
};

DatTexture ProcessImageFile(unsigned char* img, int size);
