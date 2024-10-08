#pragma once

struct SImageDescriptor
{
    int xres, yres;
    unsigned char* Data;
    int a;
    int b;
    unsigned char* image;
    int imageformat;
    int c;
};

struct SImageData
{
    unsigned int *DataPos, *EndPos, remainingBits, currentBits, nextBits, xres, yres;
};



constexpr unsigned int ImageFormats[] = {0x0B2, 0x12,  0x0B2, 0x72,  0x12,  0x12, 0x12,  0x100,
                               0x1A4, 0x1A4, 0x1A4, 0x104, 0x0A2, 0x78, 0x400, 0x71,
                               0x0B1, 0x0B1, 0x0B1, 0x0B1, 0x0A1, 0x11, 0x201};

constexpr unsigned char byte_79053C[] = {
    0x6, 0x10, 0x6, 0x0F, 0x6, 0x0E, 0x6, 0x0D, 0x6, 0x0C, 0x6, 0x0B, 0x6, 0x0A, 0x6, 0x9,
    0x6, 0x8,  0x6, 0x7,  0x6, 0x6,  0x6, 0x5,  0x6, 0x4,  0x6, 0x3,  0x6, 0x2,  0x6, 0x1,
    0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11,
    0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11,
    0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,
    0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,
    0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,
    0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0};

constexpr unsigned char byte_79053D[] = {
    0x10, 0x6, 0x0F, 0x6, 0x0E, 0x6, 0x0D, 0x6, 0x0C, 0x6, 0x0B, 0x6, 0x0A, 0x6, 0x9,  0x6,
    0x8,  0x6, 0x7,  0x6, 0x6,  0x6, 0x5,  0x6, 0x4,  0x6, 0x3,  0x6, 0x2,  0x6, 0x1,  0x2,
    0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2,
    0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x2, 0x11, 0x1,
    0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1,
    0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1,
    0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1,
    0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0,  0x1, 0x0};

int DecompressAtex(int a, int b, int imageformat, int d, int e, int f, int g);
void AtexDecompress(unsigned int* input, unsigned int unknown, unsigned int imageformat,
                    const SImageDescriptor& ImageDescriptor, unsigned int* output);
