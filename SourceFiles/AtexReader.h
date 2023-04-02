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

std::vector<RGBA> ProcessImageFile(unsigned char* img, int size);
