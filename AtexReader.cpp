
#include "pch.h"
#include "AtexAsm.h"
#include <stdio.h>

#pragma pack(1)

union RGBA
{
    unsigned char c[4];
    struct
    {
        unsigned char r, g, b, a;
    };
    unsigned int dw;
};

union DXT1Color
{
    struct
    {
        unsigned r1 : 5, g1 : 6, b1 : 5, r2 : 5, g2 : 6, b2 : 5;
    };
    struct
    {
        unsigned short c1, c2;
    };
};

struct DXT5Alpha
{
    unsigned char a0, a1;
    __int64 table;
};

RGBA* ProcessDXT1(unsigned char* data, int xr, int yr)
{
    DXT1Color* coltable = new DXT1Color[xr * yr / 16];
    unsigned int* blocktable = new unsigned int[xr * yr / 16];

    unsigned int* d = (unsigned int*)data;

    for (int x = 0; x < xr * yr / 16; x++)
    {
        coltable[x] = *(DXT1Color*)&d[x * 2];
        blocktable[x] = d[x * 2 + 1];
    }

    RGBA* image = new RGBA[xr * yr];
    memset(image, 0, xr * yr * 4);

    int p = 0;
    for (int y = 0; y < yr / 4; y++)
        for (int x = 0; x < xr / 4; x++)
        {
            RGBA ctbl[4];
            memset(ctbl, 255, 16);
            DXT1Color c = coltable[p];
            ctbl[0].r = c.r1 << 3;
            ctbl[0].g = c.g1 << 2;
            ctbl[0].b = c.b1 << 3;
            ctbl[1].r = c.r2 << 3;
            ctbl[1].g = c.g2 << 2;
            ctbl[1].b = c.b2 << 3;

            if (c.c1 > c.c2)
            {
                ctbl[2].r = (int)((ctbl[0].r * 2 + ctbl[1].r) / 3.);
                ctbl[2].g = (int)((ctbl[0].g * 2 + ctbl[1].g) / 3.);
                ctbl[2].b = (int)((ctbl[0].b * 2 + ctbl[1].b) / 3.);
                ctbl[3].r = (int)((ctbl[0].r + ctbl[1].r * 2) / 3.);
                ctbl[3].g = (int)((ctbl[0].g + ctbl[1].g * 2) / 3.);
                ctbl[3].b = (int)((ctbl[0].b + ctbl[1].b * 2) / 3.);
            }
            else
            {
                ctbl[2].r = (int)((ctbl[0].r + ctbl[1].r) / 2.);
                ctbl[2].g = (int)((ctbl[0].g + ctbl[1].g) / 2.);
                ctbl[2].b = (int)((ctbl[0].b + ctbl[1].b) / 2.);
                ctbl[3].r = 0;
                ctbl[3].g = 0;
                ctbl[3].b = 0;
                ctbl[3].a = 0;
            }

            unsigned int t = blocktable[p];

            for (int b = 0; b < 4; b++)
                for (int a = 0; a < 4; a++)
                {
                    image[x * 4 + a + (y * 4 + b) * xr] = ctbl[t & 3];
                    t = t >> 2;
                }

            p++;
        }

    delete[] coltable;
    delete[] blocktable;
    return image;
}

RGBA* ProcessDXT3(unsigned char* data, int xr, int yr)
{
    DXT1Color* coltable = new DXT1Color[xr * yr / 16];
    __int64* alphatable = new __int64[xr * yr / 16];
    unsigned int* blocktable = new unsigned int[xr * yr / 16];

    unsigned int* d = (unsigned int*)data;

    for (int x = 0; x < xr * yr / 16; x++)
    {
        alphatable[x] = ((__int64*)d)[x * 2];
        coltable[x] = *(DXT1Color*)&d[x * 4 + 2];
        blocktable[x] = d[x * 4 + 3];
    }

    RGBA* image = new RGBA[xr * yr];
    memset(image, 0, xr * yr * 4);

    int p = 0;
    for (int y = 0; y < yr / 4; y++)
        for (int x = 0; x < xr / 4; x++)
        {
            RGBA ctbl[4];
            memset(ctbl, 255, 16);
            DXT1Color c = coltable[p];
            ctbl[0].r = c.r1 << 3;
            ctbl[0].g = c.g1 << 2;
            ctbl[0].b = c.b1 << 3;
            ctbl[1].r = c.r2 << 3;
            ctbl[1].g = c.g2 << 2;
            ctbl[1].b = c.b2 << 3;

            ctbl[2].r = (int)((ctbl[0].r * 2 + ctbl[1].r) / 3.);
            ctbl[2].g = (int)((ctbl[0].g * 2 + ctbl[1].g) / 3.);
            ctbl[2].b = (int)((ctbl[0].b * 2 + ctbl[1].b) / 3.);
            ctbl[3].r = (int)((ctbl[0].r + ctbl[1].r * 2) / 3.);
            ctbl[3].g = (int)((ctbl[0].g + ctbl[1].g * 2) / 3.);
            ctbl[3].b = (int)((ctbl[0].b + ctbl[1].b * 2) / 3.);

            unsigned int t = blocktable[p];
            __int64 k = alphatable[p];

            for (int b = 0; b < 4; b++)
                for (int a = 0; a < 4; a++)
                {
                    image[x * 4 + a + (y * 4 + b) * xr] = ctbl[t & 3];
                    t = t >> 2;
                    image[x * 4 + a + (y * 4 + b) * xr].a = (unsigned char)((k & 15) << 4);
                    k = k >> 4;
                }

            p++;
        }

    delete[] coltable;
    delete[] blocktable;
    delete[] alphatable;
    return image;
}

RGBA* ProcessDXT5(unsigned char* data, int xr, int yr)
{
    DXT1Color* coltable = new DXT1Color[xr * yr / 16];
    DXT5Alpha* alphatable = new DXT5Alpha[xr * yr / 16];
    unsigned int* blocktable = new unsigned int[xr * yr / 16];

    unsigned int* d = (unsigned int*)data;

    for (int x = 0; x < xr * yr / 16; x++)
    {
        alphatable[x] = *(DXT5Alpha*)&(((__int64*)d)[x * 2]);
        coltable[x] = *(DXT1Color*)&d[x * 4 + 2];
        blocktable[x] = d[x * 4 + 3];
    }

    RGBA* image = new RGBA[xr * yr];
    memset(image, 0, xr * yr * 4);

    int p = 0;
    for (int y = 0; y < yr / 4; y++)
        for (int x = 0; x < xr / 4; x++)
        {
            RGBA ctbl[4];
            memset(ctbl, 255, 16);
            DXT1Color c = coltable[p];
            ctbl[0].r = c.r1 << 3;
            ctbl[0].g = c.g1 << 2;
            ctbl[0].b = c.b1 << 3;
            ctbl[1].r = c.r2 << 3;
            ctbl[1].g = c.g2 << 2;
            ctbl[1].b = c.b2 << 3;

            ctbl[2].r = (int)((ctbl[0].r * 2 + ctbl[1].r) / 3.);
            ctbl[2].g = (int)((ctbl[0].g * 2 + ctbl[1].g) / 3.);
            ctbl[2].b = (int)((ctbl[0].b * 2 + ctbl[1].b) / 3.);
            ctbl[3].r = (int)((ctbl[0].r + ctbl[1].r * 2) / 3.);
            ctbl[3].g = (int)((ctbl[0].g + ctbl[1].g * 2) / 3.);
            ctbl[3].b = (int)((ctbl[0].b + ctbl[1].b * 2) / 3.);

            unsigned char atbl[8];
            DXT5Alpha l = alphatable[p];

            atbl[0] = l.a0;
            atbl[1] = l.a1;

            if (l.a0 > l.a1)
            {
                for (int z = 0; z < 6; z++)
                    atbl[z + 2] = ((6 - z) * l.a0 + (z + 1) * l.a1) / 7;
            }
            else
            {
                for (int z = 0; z < 4; z++)
                    atbl[z + 2] = ((4 - z) * l.a0 + (z + 1) * l.a1) / 5;
                atbl[6] = 0;
                atbl[7] = 255;
            }

            unsigned int t = blocktable[p];
            __int64 k = alphatable[p].table;

            for (int b = 0; b < 4; b++)
                for (int a = 0; a < 4; a++)
                {
                    image[x * 4 + a + (y * 4 + b) * xr] = ctbl[t & 3];
                    t = t >> 2;
                    image[x * 4 + a + (y * 4 + b) * xr].a = atbl[k & 7];
                    k = k >> 3;
                }

            p++;
        }

    delete[] coltable;
    delete[] blocktable;
    delete[] alphatable;
    return image;
}
