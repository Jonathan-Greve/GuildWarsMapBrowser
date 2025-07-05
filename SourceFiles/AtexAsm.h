#pragma once

struct SImageData;
// C++ implementations (defined in AtexDecompress.cpp and AtexAsm.cpp)
void AtexSubCode1_Cpp(uint32_t* array1, uint32_t* array2, unsigned int count);
void AtexSubCode2_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode3_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode4_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);

// Assembly implementations (still needed for 64-bit conversion)
void AtexSubCode5_Asm(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f, unsigned int g);
void AtexSubCode7_Asm(unsigned int a, unsigned int b);
