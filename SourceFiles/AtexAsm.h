#pragma once

struct SImageData;
void AtexSubCode1_Asm(uint32_t* array1, uint32_t* array2, unsigned int count);
void AtexSubCode2_Asm(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode3_Asm(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode4_Asm(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode5_Asm(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f, unsigned int g);
void AtexSubCode7_Asm(unsigned int a, unsigned int b);
