#include "pch.h"
#include "AtexDecompress.h"
#include "AtexAsm.h"

struct SImageData;

void AtexSubCode1_Cpp(uint32_t* array1, uint32_t* array2, unsigned int count);
void AtexSubCode2_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);
void AtexSubCode3_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2, SImageData* imageData, unsigned int blockCount, unsigned int blockSize);

int ImgFmt(unsigned int Format)
{
    if (Format >= 0x17)
    {
        printf("ERROR: bad image format (%d)!", Format);
        exit(0);
    }
    return ImageFormats[Format];
}

void AtexDecompress(unsigned int* InputBuffer, unsigned int BufferSize, unsigned int ImageFormat, const SImageDescriptor& ImageDescriptor, unsigned int* OutBuffer)
{
    unsigned int HeaderSize = 12;

    SImageData ImageData;

    int AlphaDataSize2 = ((ImageFormat && 21) - 1) & 2;

    int ColorDataSize = ImgFmt(ImageFormat);
    int AlphaDataSize = ColorDataSize;

    AlphaDataSize &= 640;
    if (AlphaDataSize)
    {
        AlphaDataSize = 2;
    }

    ColorDataSize &= 528;
    if (ColorDataSize)
    {
        ColorDataSize = 2;
    }

    int BlockSize = ColorDataSize + AlphaDataSize2 + AlphaDataSize;

    int BlockCount = ImageDescriptor.xres * ImageDescriptor.yres / 16;

    if (! BlockCount)
    {
        printf("BlockCount zero\n");
        return;
    }

    unsigned int* DcmpBuffer1 = new unsigned int[BlockCount];
    unsigned int* DcmpBuffer2 = DcmpBuffer1 + BlockCount / 2;
    memset(DcmpBuffer1, 0, BlockCount * 4);

    ImageData.xres = ImageDescriptor.xres;
    ImageData.yres = ImageDescriptor.yres;

    unsigned int DataSize = InputBuffer[HeaderSize >> 2];

    if (HeaderSize + 8 >= BufferSize)
    {
        printf("Error 567h\n");
    }
    if (DataSize <= 8)
    {
        printf("Error 569h\n");
    }
    if (DataSize + HeaderSize > BufferSize)
    {
        printf("Error 56Ah\n");
    }

    int CompressionCode = InputBuffer[(HeaderSize + 4) >> 2];

    ImageData.DataPos = InputBuffer + ((HeaderSize + 8) >> 2);

    if (CompressionCode)
    {
        ImageData.currentBits = ImageData.remainingBits = ImageData.nextBits = 0;
        ImageData.EndPos = ImageData.DataPos + ((DataSize - 8) >> 2);

        if (ImageData.DataPos != ImageData.EndPos)
        {
            ImageData.currentBits = ImageData.DataPos[0];
            ImageData.DataPos++;
        }

        if (CompressionCode & 0x10 && ImageData.xres == 256 && ImageData.yres == 256 &&
            (ImageFormat == 0x11 || ImageFormat == 0x10))
        {
            AtexSubCode1_Cpp(DcmpBuffer1, DcmpBuffer2, BlockCount);
        }
        if (CompressionCode & 1 && ColorDataSize && ! AlphaDataSize && ! AlphaDataSize2)
        {
            AtexSubCode2_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 2 && ImageFormat >= 0x10 && ImageFormat <= 0x11)
        {
            AtexSubCode3_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 4 && ImageFormat >= 0x12 && ImageFormat <= 0x15)
        {
            AtexSubCode4_Cpp(OutBuffer, DcmpBuffer1, DcmpBuffer2, &ImageData, BlockCount, BlockSize);
        }
        if (CompressionCode & 8 && ColorDataSize)
        {
            AtexSubCode5_Cpp(OutBuffer + AlphaDataSize2 + AlphaDataSize,
                          DcmpBuffer1, DcmpBuffer2, &ImageData,
                          BlockCount, BlockSize, ImageFormat == 0xf);
        }
        ImageData.DataPos--;
    }

    [[maybe_unused]] unsigned int* DataEnd = InputBuffer + ((HeaderSize + DataSize) >> 2);

    if ((AlphaDataSize || AlphaDataSize2) && BlockCount)
    {
        unsigned int* BufferVar = OutBuffer;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer1[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                BufferVar[1] = ImageData.DataPos[1];
                ImageData.DataPos += 2;
            }
            BufferVar += BlockSize;
        }
    }

    if (ColorDataSize && BlockCount)
    {
        unsigned int* BufferVar = OutBuffer + AlphaDataSize2 + AlphaDataSize;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer2[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                ImageData.DataPos++;
            }
            BufferVar += BlockSize;
        }

        BufferVar = OutBuffer + AlphaDataSize2 + AlphaDataSize + 1;

        for (int x = 0; x < BlockCount; x++)
        {
            if (! (DcmpBuffer2[x >> 5] & 1 << x))
            {
                BufferVar[0] = ImageData.DataPos[0];
                ImageData.DataPos++;
            }
            BufferVar += BlockSize;
        }
    }

    if (CompressionCode & 0x10 && ImageData.xres == 256 && ImageData.yres == 256 &&
        (ImageFormat == 0x10 || ImageFormat == 0x11))
    {
        AtexSubCode7_Cpp(OutBuffer, BlockCount);
    }

    delete[] (unsigned char*)DcmpBuffer1;
}

void AtexSubCode1_Cpp(uint32_t* array1, uint32_t* array2, unsigned int count)
{
    for (auto i = 0u; i < count; ++i) {
        const uint32_t mask = 1 << (i & 0x1F);
        if ((mask & 0xC0000003) != 0 || ((1 << ((i >> 6) & 0x1F)) & 0xC0000003) != 0) {
            const uint32_t array_index = i >> 5; // 4 * (i >> 5), but uint32_t* points to 4 bytes
            array1[array_index] |= mask;
            array2[array_index] |= mask;
        }
    }
}

void AtexSubCode2_Cpp(uint32_t *outBuffer, uint32_t *dcmpBuffer1, uint32_t *dcmpBuffer2, SImageData *imageData, uint32_t blockCount, uint32_t blockSize)
{
    if (blockCount == 0) {
        return;
    }

    uint32_t block_index = 0;
    int extractedBits;
    uint32_t* dataPosition;
    uint32_t currentWord;

    while (block_index < blockCount)
    {
        const auto shiftLeft = imageData->currentBits >> 26;
        const auto shiftValue = byte_79053C[2 * shiftLeft];
        int remainingBits = byte_79053D[2 * shiftLeft] + 1;

        if (shiftValue) {
            imageData->currentBits = (imageData->currentBits << shiftValue) | (imageData->nextBits >> (32 - shiftValue));
        }

        uint32_t bitCount = imageData->remainingBits;

        if (shiftValue > bitCount)
        {
            dataPosition = imageData->DataPos;

            if (dataPosition == imageData->EndPos)
            {
                extractedBits = 0;
                imageData->nextBits = 0;
            }
            else
            {
                currentWord = *dataPosition;
                imageData->DataPos = dataPosition + 1;
                imageData->nextBits = currentWord;
                imageData->currentBits |= currentWord >> (bitCount - shiftValue + 32);
                imageData->nextBits = currentWord << (shiftValue - bitCount);
                extractedBits = bitCount - shiftValue + 32;
            }
        }
        else
        {
            extractedBits = bitCount - shiftValue;
            imageData->nextBits <<= shiftValue;
        }

        const uint64_t combinedValue = (static_cast<uint64_t>(imageData->currentBits) << 32) | imageData->nextBits;
        imageData->remainingBits = extractedBits;
        imageData->currentBits = static_cast<uint32_t>(combinedValue >> 31);
        int bitMask = static_cast<int>(combinedValue >> 63);

        if (imageData->remainingBits)
        {
            imageData->nextBits = static_cast<uint32_t>(combinedValue << 1);
            imageData->remainingBits -= 1;
        }
        else
        {
            dataPosition = imageData->DataPos;

            if (dataPosition == imageData->EndPos)
            {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
            else
            {
                const auto curPos = *dataPosition;
                imageData->DataPos = dataPosition + 1;
                currentWord = curPos;
                imageData->currentBits |= currentWord >> 31;
                imageData->remainingBits = 31;
                imageData->nextBits = 2 * currentWord;
            }
        }

        while (remainingBits > 0 && block_index < blockCount)
        {
            const int bitPosition = 1 << (block_index & 0x1F);
            const int bufferIndex = block_index >> 5;

            if ((bitPosition & dcmpBuffer2[bufferIndex]) == 0)
            {
                if (bitMask)
                {
                    outBuffer[0] = static_cast<uint32_t>(-2);
                    outBuffer[1] = static_cast<uint32_t>(-1);
                    dcmpBuffer2[bufferIndex] |= bitPosition;
                    dcmpBuffer1[bufferIndex] |= bitPosition;
                    bitMask = static_cast<int>(combinedValue >> 63);
                }
                remainingBits -= 1;
            }

            block_index++;
            outBuffer += blockSize;
        }

        while (block_index < blockCount && (1 << (block_index & 0x1F) & dcmpBuffer2[block_index >> 5]) != 0)
        {
            block_index++;
            outBuffer += blockSize;
        }
    }
}


