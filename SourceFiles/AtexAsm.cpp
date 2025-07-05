#include "pch.h"
#include "AtexAsm.h"
#include "AtexDecompress.h"

// C++ implementation of AtexSubCode3
void AtexSubCode3_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
                      SImageData* imageData, unsigned int blockCount, unsigned int blockSize)
{
    if (blockCount == 0) {
        return;
    }

    // Extract 4 bits from the bit stream
    uint32_t nextBits = imageData->nextBits;
    uint32_t currentBits = imageData->currentBits;
    uint32_t remainingBits = imageData->remainingBits;
    
    uint32_t extractedBits = (currentBits >> 28) & 0xF;
    currentBits = (currentBits << 4) | (nextBits >> 28);
    
    // Refill bit buffer if needed
    if (remainingBits < 4) {
        if (imageData->DataPos != imageData->EndPos) {
            uint32_t newData = *imageData->DataPos++;
            currentBits |= newData >> (remainingBits + 28);
            nextBits = newData << (4 - remainingBits);
            remainingBits += 28;
        } else {
            nextBits = 0;
            remainingBits = 0;
        }
    } else {
        nextBits <<= 4;
        remainingBits -= 4;
    }
    
    // Update image data
    imageData->currentBits = currentBits;
    imageData->nextBits = nextBits;
    imageData->remainingBits = remainingBits;
    
    // Build color palette
    uint32_t colorPattern = extractedBits;
    colorPattern = (colorPattern << 4) | extractedBits;
    colorPattern = (colorPattern << 8) | colorPattern;
    colorPattern = (colorPattern << 16) | colorPattern;
    
    // Color table structure
    uint32_t colorTable[8];
    colorTable[0] = 0;          // Index 0, word 0 (not used)
    colorTable[1] = 0;          // Index 0, word 1 (not used)
    colorTable[2] = 0;          // Index 1, word 0
    colorTable[3] = 0;          // Index 1, word 1
    colorTable[4] = colorPattern; // Index 2, word 0
    colorTable[5] = colorPattern; // Index 2, word 1
    colorTable[6] = 0;          // Index 3, word 0
    colorTable[7] = 0;          // Index 3, word 1
    
    unsigned int blockIndex = 0;
    
    // Main decompression loop
    while (blockIndex < blockCount) {
        // Decode huffman code
        currentBits = imageData->currentBits;
        unsigned int shiftIndex = currentBits >> 26;
        unsigned int bitsToRead = byte_79053D[shiftIndex * 2] + 1;
        unsigned int shiftAmount = byte_79053C[shiftIndex * 2];
        
        // Shift bits
        if (shiftAmount != 0) {
            currentBits = (currentBits << shiftAmount) | 
                         (imageData->nextBits >> (32 - shiftAmount));
        }
        
        // Refill bit buffer if needed
        remainingBits = imageData->remainingBits;
        if (shiftAmount > remainingBits) {
            if (imageData->DataPos != imageData->EndPos) {
                uint32_t newData = *imageData->DataPos++;
                currentBits |= newData >> (remainingBits + 32 - shiftAmount);
                imageData->nextBits = newData << (shiftAmount - remainingBits);
                imageData->remainingBits = remainingBits + 32 - shiftAmount;
            } else {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
        } else {
            imageData->remainingBits = remainingBits - shiftAmount;
            imageData->nextBits <<= shiftAmount;
        }
        
        imageData->currentBits = currentBits;
        
        // Extract first flag bit
        currentBits = imageData->currentBits;
        uint32_t flagBit1 = currentBits >> 31;
        imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);
        
        // Update bit buffer
        if (imageData->remainingBits > 0) {
            imageData->nextBits <<= 1;
            imageData->remainingBits--;
        } else {
            if (imageData->DataPos != imageData->EndPos) {
                uint32_t newData = *imageData->DataPos++;
                imageData->currentBits |= newData >> 31;
                imageData->nextBits = newData << 1;
                imageData->remainingBits = 31;
            } else {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
        }
        
        uint32_t colorIndex = flagBit1;
        
        // If first flag bit was set, read another bit
        if (flagBit1 != 0) {
            currentBits = imageData->currentBits;
            uint32_t flagBit2 = currentBits >> 31;
            imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);
            
            if (imageData->remainingBits > 0) {
                imageData->nextBits <<= 1;
                imageData->remainingBits--;
            } else {
                if (imageData->DataPos != imageData->EndPos) {
                    uint32_t newData = *imageData->DataPos++;
                    imageData->currentBits |= newData >> 31;
                    imageData->nextBits = newData << 1;
                    imageData->remainingBits = 31;
                } else {
                    imageData->nextBits = 0;
                    imageData->remainingBits = 0;
                }
            }
            
            colorIndex = flagBit1 + flagBit2;
        }
        
        // Process blocks
        while (bitsToRead > 0 && blockIndex < blockCount) {
            uint32_t bitMask = 1u << (blockIndex & 0x1F);
            uint32_t arrayIndex = blockIndex >> 5;
            
            // Check if this block needs processing
            if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
                if (colorIndex != 0) {
                    // Write color data to output
                    outBuffer[0] = colorTable[colorIndex * 2];
                    outBuffer[1] = colorTable[colorIndex * 2 + 1];
                    // Mark block as processed in dcmpBuffer1
                    dcmpBuffer1[arrayIndex] |= bitMask;
                }
                bitsToRead--;
            }
            
            blockIndex++;
            outBuffer += blockSize;
        }
        
        // Skip already processed blocks
        while (blockIndex < blockCount) {
            uint32_t bitMask = 1u << (blockIndex & 0x1F);
            uint32_t arrayIndex = blockIndex >> 5;
            
            if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
                break;
            }
            
            blockIndex++;
            outBuffer += blockSize;
        }
    }
}

// C++ implementation of AtexSubCode4
void AtexSubCode4_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
                      SImageData* imageData, unsigned int blockCount, unsigned int blockSize)
{
    if (blockCount == 0) {
        return;
    }

    // Extract 8 bits from the bit stream (vs 4 bits in SubCode3)
    uint32_t nextBits = imageData->nextBits;
    uint32_t currentBits = imageData->currentBits;
    uint32_t remainingBits = imageData->remainingBits;
    
    uint32_t extractedBits = (currentBits >> 24) & 0xFF;
    currentBits = (currentBits << 8) | (nextBits >> 24);
    
    // Refill bit buffer if needed
    if (remainingBits < 8) {
        if (imageData->DataPos != imageData->EndPos) {
            uint32_t newData = *imageData->DataPos++;
            currentBits |= newData >> (remainingBits + 24);
            nextBits = newData << (8 - remainingBits);
            remainingBits += 24;
        } else {
            nextBits = 0;
            remainingBits = 0;
        }
    } else {
        nextBits <<= 8;
        remainingBits -= 8;
    }
    
    // Update image data
    imageData->currentBits = currentBits;
    imageData->nextBits = nextBits;
    imageData->remainingBits = remainingBits;
    
    // Build color palette
    // The assembly builds a different pattern from the 8-bit value
    uint32_t colorPattern = (extractedBits << 8) | extractedBits;
    
    // Color table structure
    uint32_t colorTable[8];
    colorTable[0] = 0;           // Index 0, word 0 (not used)
    colorTable[1] = 0;           // Index 0, word 1 (not used)
    colorTable[2] = 0;           // Index 1, word 0
    colorTable[3] = 0;           // Index 1, word 1
    colorTable[4] = colorPattern; // Index 2, word 0
    colorTable[5] = 0;           // Index 2, word 1
    colorTable[6] = 0;           // Index 3, word 0
    colorTable[7] = 0;           // Index 3, word 1
    
    unsigned int blockIndex = 0;
    
    // Main decompression loop
    while (blockIndex < blockCount) {
        // Decode huffman code
        currentBits = imageData->currentBits;
        unsigned int shiftIndex = currentBits >> 26;
        unsigned int bitsToRead = byte_79053D[shiftIndex * 2] + 1;
        unsigned int shiftAmount = byte_79053C[shiftIndex * 2];
        
        // Shift bits
        if (shiftAmount != 0) {
            currentBits = (currentBits << shiftAmount) | 
                         (imageData->nextBits >> (32 - shiftAmount));
        }
        
        // Refill bit buffer if needed
        remainingBits = imageData->remainingBits;
        if (shiftAmount > remainingBits) {
            if (imageData->DataPos != imageData->EndPos) {
                uint32_t newData = *imageData->DataPos++;
                currentBits |= newData >> (remainingBits + 32 - shiftAmount);
                imageData->nextBits = newData << (shiftAmount - remainingBits);
                imageData->remainingBits = remainingBits + 32 - shiftAmount;
            } else {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
        } else {
            imageData->remainingBits = remainingBits - shiftAmount;
            imageData->nextBits <<= shiftAmount;
        }
        
        imageData->currentBits = currentBits;
        
        // Extract first flag bit
        currentBits = imageData->currentBits;
        uint32_t flagBit1 = currentBits >> 31;
        imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);
        
        // Update bit buffer
        if (imageData->remainingBits > 0) {
            imageData->nextBits <<= 1;
            imageData->remainingBits--;
        } else {
            if (imageData->DataPos != imageData->EndPos) {
                uint32_t newData = *imageData->DataPos++;
                imageData->currentBits |= newData >> 31;
                imageData->nextBits = newData << 1;
                imageData->remainingBits = 31;
            } else {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
        }
        
        uint32_t colorIndex = flagBit1;
        
        // If first flag bit was set, read another bit
        if (flagBit1 != 0) {
            currentBits = imageData->currentBits;
            uint32_t flagBit2 = currentBits >> 31;
            imageData->currentBits = (currentBits << 1) | (imageData->nextBits >> 31);
            
            if (imageData->remainingBits > 0) {
                imageData->nextBits <<= 1;
                imageData->remainingBits--;
            } else {
                if (imageData->DataPos != imageData->EndPos) {
                    uint32_t newData = *imageData->DataPos++;
                    imageData->currentBits |= newData >> 31;
                    imageData->nextBits = newData << 1;
                    imageData->remainingBits = 31;
                } else {
                    imageData->nextBits = 0;
                    imageData->remainingBits = 0;
                }
            }
            
            colorIndex = flagBit1 + flagBit2;
        }
        
        // Process blocks
        while (bitsToRead > 0 && blockIndex < blockCount) {
            uint32_t bitMask = 1u << (blockIndex & 0x1F);
            uint32_t arrayIndex = blockIndex >> 5;
            
            // Check if this block needs processing
            if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
                if (colorIndex != 0) {
                    // Write color data to output
                    outBuffer[0] = colorTable[colorIndex * 2];
                    outBuffer[1] = colorTable[colorIndex * 2 + 1];
                    // Mark block as processed in dcmpBuffer1
                    dcmpBuffer1[arrayIndex] |= bitMask;
                }
                bitsToRead--;
            }
            
            blockIndex++;
            outBuffer += blockSize;
        }
        
        // Skip already processed blocks
        while (blockIndex < blockCount) {
            uint32_t bitMask = 1u << (blockIndex & 0x1F);
            uint32_t arrayIndex = blockIndex >> 5;
            
            if ((dcmpBuffer2[arrayIndex] & bitMask) == 0) {
                break;
            }
            
            blockIndex++;
            outBuffer += blockSize;
        }
    }
}

// C++ implementation of AtexSubCode6
uint32_t AtexSubCode6_Cpp(uint32_t* output, uint32_t colorValue, uint32_t flag)
{
    // Extract color components (note: colorValue has 0xFF in high byte)
    uint32_t r = colorValue & 0xFF;
    uint32_t g = (colorValue >> 8) & 0xFF;
    uint32_t b = (colorValue >> 16) & 0xFF;

    // Calculate adjusted values following assembly exactly
    // Assembly: mov edx, eax; shr ecx, 5; sub edx, ecx
    uint32_t r_adj = r - (r >> 5);
    uint32_t g_adj = g - (g >> 6);
    uint32_t b_adj = b - (b >> 5);

    // Assembly: shr edx, 3; shr esi, 2; shr edi, 3
    r_adj >>= 3;
    g_adj >>= 2;
    b_adj >>= 3;

    // Calculate base values (stored at ebp-0x14, ebp-0x10, ebp-0xC)
    uint32_t r_base = r_adj;
    uint32_t g_base = g_adj;
    uint32_t b_base = b_adj;

    // Calculate Q values following assembly pattern
    // lea ecx, [ecx + edx * 8] means ecx = ecx + edx * 8
    uint32_t qr = (r_adj >> 2) + r_adj * 8;
    uint32_t qg = (g_adj >> 4) + g_adj * 4;
    uint32_t qb = (b_adj >> 2) + b_adj * 8;

    // Increment for next calculation
    r_adj++;
    g_adj++;
    b_adj++;

    uint32_t qr_next = (r_adj >> 2) + r_adj * 8;
    uint32_t qg_next = (g_adj >> 4) + g_adj * 4;
    uint32_t qb_next = (b_adj >> 2) + b_adj * 8;

    // Calculate differences
    uint32_t dr = qr_next - qr;
    uint32_t dg = qg_next - qg;
    uint32_t db = qb_next - qb;

    // Calculate ratios (values at ebp-0x2C, ebp-0x28, ebp-0x24)
    // Assembly: lea eax, [eax + eax * 2]; shl eax, 2 means eax = eax * 12
    uint32_t cr = 0, cg = 0, cb = 0;
    if (dr != 0) {
        cr = (r * 12 - qr * 12) / dr;
    }
    if (dg != 0) {
        cg = (g * 12 - qg * 12) / dg;
    }
    if (db != 0) {
        cb = (b * 12 - qb * 12) / db;
    }

    // Build palette table (at ebp-0x50)
    struct {
        uint32_t val1;
        uint32_t val2;
    } palette[3];

    // Process each color component
    uint32_t values[3] = { cr, cg, cb };
    uint32_t bases[3] = { r_base, g_base, b_base };

    for (int i = 0; i < 3; i++) {
        uint32_t val = values[i];
        uint32_t base = bases[i];

        if (val < 2) {
            palette[i].val1 = base;
            palette[i].val2 = base;
        }
        else if (val < 6) {
            palette[i].val1 = base;
            palette[i].val2 = base + 1;
        }
        else if (val < 10) {
            // When val is 6-9, assembly does: lea esi, [edx + 1]; mov[eax], esi; mov[eax + 4], edx
            // This means val1 = base+1, val2 = base
            palette[i].val1 = base + 1;
            palette[i].val2 = base;
        }
        else {
            palette[i].val1 = base + 1;
            palette[i].val2 = base + 1;
        }
    }

    // Build colors from palette values
    // The assembly builds these from the palette entries
    uint32_t color1_val1 = palette[0].val1 | (palette[1].val1 << 5) | (palette[2].val1 << 11);
    uint32_t color1_val2 = palette[0].val2 | (palette[1].val2 << 5) | (palette[2].val2 << 11);

    // These will be assigned based on the logic below
    uint32_t color1, color2;

    // Calculate average score
    uint32_t score = 0;
    uint32_t count = 0;

    for (int i = 0; i < 3; i++) {
        if (palette[i].val1 != palette[i].val2) {
            if (palette[i].val1 == bases[i]) {
                score += values[i];
            }
            else {
                score += (12 - values[i]);
            }
            count++;
        }
    }

    uint32_t avg = 0;
    if (count > 0) {
        avg = (score + count / 2) / count;
    }

    // Determine swap flag
    uint32_t swap = 0;
    if (flag != 0) {
        if ((avg == 5 || avg == 6) || count == 0) {
            swap = 1;
        }
    }

    // Initially assign colors
    color1 = color1_val1;
    color2 = color1_val2;

    // Handle special case when count == 0 and swap == 0
    if (count == 0 && swap == 0) {
        if (color2 != 0xFFFF) {
            avg = 0;
            color2++;
        }
        else {
            avg = 12;
            color1--;
        }
    }

    // Check if we need to swap colors
    // The assembly compares (color1 < color2) with swap
    // If they're different, it swaps
    if ((color1 < color2) != swap) {
        uint32_t temp = color1;
        color1 = color2;
        color2 = temp;
        avg = 12 - avg;
    }

    // Determine table index
    uint32_t table;
    if (swap != 0) {
        table = 2;
    }
    else if (avg < 2) {
        table = 0;
    }
    else if (avg < 6) {
        table = 2;
    }
    else if (avg < 10) {
        table = 3;
    }
    else {
        table = 1;
    }

#ifdef DEBUG_ATEXSUBCODE6
    printf("AtexSubCode6 Debug:\n");
    printf("  Input: R=%u, G=%u, B=%u\n", r, g, b);
    printf("  Base values: R=%u, G=%u, B=%u\n", r_base, g_base, b_base);
    printf("  Ratios: CR=%u, CG=%u, CB=%u\n", cr, cg, cb);
    printf("  Palette: R(%u,%u), G(%u,%u), B(%u,%u)\n",
        palette[0].val1, palette[0].val2,
        palette[1].val1, palette[1].val2,
        palette[2].val1, palette[2].val2);
    printf("  Before swap: color1=0x%04X, color2=0x%04X\n", color1, color2);
    printf("  Comparison: color2(%u) < color1(%u) = %d, swap=%u\n",
        color2, color1, (color2 < color1), swap);
    printf("  Final: avg=%u, table=%u\n", avg, table);
    printf("  After swap: color1=0x%04X, color2=0x%04X\n", color1, color2);
    printf("  Storing: (0x%04X << 16) | 0x%04X = 0x%08X\n", color1, color2, (color1 << 16) | color2);
#endif

    // Store output - AtexSubCode6 stores TWO values in memory:
    // The assembly always stores the smaller value in the high word
    // This appears to be the opposite of what we'd expect
    output[0] = (color2 << 16) | color1;

    // The assembly builds the second value in a complex way
    // Looking at the debug output: 0x753C76FD
    // This appears to be a pattern fill based on the table value
    // Let me build it the same way the assembly does:
    // lea ecx, ds:0[eax * 4]; or ecx, eax gives ecx = table * 5
    // Then it does: mov eax, ecx; shl eax, 4; or eax, ecx
    // mov ecx, eax; shl ecx, 8; or ecx, eax
    // mov eax, ecx; shl eax, 10h; or eax, ecx
    uint32_t pattern = table * 5;
    pattern = (pattern << 4) | pattern;
    pattern = (pattern << 8) | pattern;
    pattern = (pattern << 16) | pattern;
    output[1] = pattern;

    // Return value in EDI is built differently
    return avg | ((table * 5) << 16);
}

// C++ implementation of AtexSubCode5
void AtexSubCode5_Cpp(uint32_t* outBuffer, uint32_t* dcmpBuffer1, uint32_t* dcmpBuffer2,
    SImageData* imageData, unsigned int blockCount, unsigned int blockSize,
    unsigned int flag)
{
    if (blockCount == 0) {
        return;
    }

    // Extract 24 bits from stream
    // Assembly does:
    // mov ecx, [esi + 10h]  ; nextBits
    // mov eax, [esi + 0Ch]  ; currentBits
    // shr edi, 8            ; edi = nextBits >> 8
    // shl eax, 18h          ; eax = currentBits << 24
    // or edi, eax           ; edi = (currentBits << 24) | (nextBits >> 8)
    // mov[esi + 0Ch], edi   ; currentBits = edi
    // shr edx, 8            ; edx = old currentBits >> 8
    // cmp remainingBits, 18h (24)

    uint32_t currentBits = imageData->currentBits;
    uint32_t nextBits = imageData->nextBits;
    uint32_t remainingBits = imageData->remainingBits;

    // Extract color value (upper 24 bits of currentBits, shifted right by 8)
    uint32_t colorValue = (currentBits >> 8) | 0xFF000000;

    // Update bit stream - extract 24 bits
    uint32_t newCurrentBits = (currentBits << 24) | (nextBits >> 8);

    // Check if we need to refill
    if (remainingBits >= 24) {
        // We have enough bits
        nextBits <<= 24;
        remainingBits -= 24;
    }
    else {
        // Need to refill
        if (imageData->DataPos != imageData->EndPos) {
            uint32_t newData = *imageData->DataPos++;
            // Assembly does: lea ecx, [edi + 8] where edi is remainingBits
            uint32_t bitsToShift = remainingBits + 8;
            newCurrentBits |= newData >> bitsToShift;
            nextBits = newData << (24 - remainingBits);
            remainingBits = bitsToShift;
        }
        else {
            nextBits = 0;
            remainingBits = 0;
        }
    }

    imageData->currentBits = newCurrentBits;
    imageData->nextBits = nextBits;
    imageData->remainingBits = remainingBits;

    // Call AtexSubCode6
    // AtexSubCode6 stores TWO values in memory:
    // colorData[0] = color pair
    // colorData[1] = pattern fill
    uint32_t colorData[2];
    uint32_t ediValue = AtexSubCode6_Cpp(colorData, colorValue, flag);

    // Main processing loop
    for (unsigned int blockIdx = 0; blockIdx < blockCount; ) {
        // Decode huffman value
        uint32_t huffIdx = (imageData->currentBits >> 26) & 0x3F;
        uint32_t huffBits = byte_79053C[huffIdx * 2];
        uint32_t huffValue = byte_79053D[huffIdx * 2] + 1;

        if (huffBits < 32) {
            // Shift bits by huffBits
            if (huffBits != 0) {
                imageData->currentBits = (imageData->currentBits << huffBits) |
                    (imageData->nextBits >> (32 - huffBits));
            }

            if (huffBits <= imageData->remainingBits) {
                imageData->nextBits <<= huffBits;
                imageData->remainingBits -= huffBits;
            }
            else {
                if (imageData->DataPos != imageData->EndPos) {
                    uint32_t newData = *imageData->DataPos++;
                    uint32_t bitsNeeded = huffBits - imageData->remainingBits;
                    imageData->currentBits |= newData >> (32 - bitsNeeded);
                    imageData->nextBits = newData << bitsNeeded;
                    imageData->remainingBits = 32 - bitsNeeded;
                }
                else {
                    imageData->nextBits = 0;
                    imageData->remainingBits = 0;
                }
            }
        }

        // Extract 1 bit
        uint32_t bit = (imageData->currentBits >> 31) & 1;
        imageData->currentBits = (imageData->currentBits << 1) | (imageData->nextBits >> 31);

        if (imageData->remainingBits >= 1) {
            imageData->nextBits <<= 1;
            imageData->remainingBits--;
        }
        else {
            if (imageData->DataPos != imageData->EndPos) {
                uint32_t newData = *imageData->DataPos++;
                imageData->currentBits |= newData >> 31;
                imageData->nextBits = newData << 1;
                imageData->remainingBits = 31;
            }
            else {
                imageData->nextBits = 0;
                imageData->remainingBits = 0;
            }
        }

        // Process huffValue blocks
        uint32_t remaining = huffValue;
        while (remaining > 0 && blockIdx < blockCount) {
            uint32_t blockOffset = blockIdx & 0x1F;
            uint32_t blockWord = blockIdx >> 5;
            uint32_t blockMask = 1 << blockOffset;

            if (!(dcmpBuffer2[blockWord] & blockMask)) {
                if (bit) {
                    // Store color data - both values from AtexSubCode6
                    outBuffer[0] = colorData[0];
                    outBuffer[1] = colorData[1];
                    dcmpBuffer2[blockWord] |= blockMask;
                }
                remaining--;
            }

            outBuffer += blockSize;
            blockIdx++;
        }

        // Skip already processed blocks
        while (blockIdx < blockCount) {
            uint32_t blockOffset = blockIdx & 0x1F;
            uint32_t blockWord = blockIdx >> 5;
            uint32_t blockMask = 1 << blockOffset;

            if (!(dcmpBuffer2[blockWord] & blockMask)) {
                break;
            }

            outBuffer += blockSize;
            blockIdx++;
        }
    }
}

void __declspec(naked) AtexSubCode7()
{
    __asm {

        push    ebp
        mov ebp, esp

        sub esp, 14h
        mov eax, edx
        mov[ebp + -0x8], ecx
        xor edx, edx
        mov[ebp + -0x14], eax
        test    eax, eax
        mov[ebp + -0x10], ecx
        mov[ebp + -0xC], edx
        jbe loc_610392
        push    ebx
        push    esi
        push    edi

        loc_6101D1 :
        mov eax, edx
            mov esi, edx
            and eax, 3Fh
            mov edi, 1
            mov ecx, eax
            mov ebx, 1
            and ecx, 1Fh
            shr esi, 6
            shl edi, cl
            mov ecx, esi
            and ecx, 1Fh
            shl ebx, cl
            and edi, 0C0000003h
            and ebx, 0C0000003h
            test    edi, edi
            jnz short loc_61020D
            test    ebx, ebx
            jz  loc_610377
            jmp short loc_610210
            ; -------------------------------------------------------------------------- -

            loc_61020D:
        xor eax, 3

            loc_610210 :
            test    ebx, ebx
            mov[ebp + -0x4], eax
            jz  short loc_61021A
            xor esi, 3

            loc_61021A :
            mov ecx, [ebp + -0x4]
            mov eax, 1
            and ecx, 1Fh
            shl eax, cl
            test    eax, 0C0000003h
            jz  short loc_610242
            ; -------------------------------------------------------------------------- -

            loc_610242:
        mov ecx, esi
            mov edx, 1
            and ecx, 1Fh
            shl edx, cl
            test    edx, 0C0000003h
            jz  short loc_61026A
            ; -------------------------------------------------------------------------- -

            loc_61026A:
        mov edx, [ebp + -0x4]
            mov eax, [ebp + -0x10]
            shl esi, 6
            add esi, edx
            shl esi, 4
            add esi, eax
            test    edi, edi
            mov edx, [esi + 8]
            mov eax, [esi]
            mov ecx, [esi + 4]
            mov[ebp + -0x4], edx
            mov edx, [esi + 0Ch]
            jz  loc_610323
            mov ecx, eax
            mov esi, eax
            shr ecx, 8
            and ecx, 0F000F0h
            and esi, 0F000F00h
            or ecx, esi
            mov esi, eax
            and esi, 0FFFF000Fh
            and eax, 0F000F0h
            shl esi, 8
            or esi, eax
            shr ecx, 4
            shl esi, 4
            or ecx, esi
            mov eax, ecx
            shr ecx, 8
            mov esi, eax
            and ecx, 0F000F0h
            and esi, 0F000F00h
            mov edi, eax
            or ecx, esi
            mov esi, eax
            and esi, 0FFFF000Fh
            and edi, 0F000F0h
            shl esi, 8
            or esi, edi
            mov edi, edx
            shr ecx, 4
            shl esi, 4
            or ecx, esi
            mov esi, edx
            and esi, 0FF030303h
            and edi, 0C0C0C0Ch
            shl esi, 4
            or esi, edi
            mov edi, edx
            shr edi, 4
            and edi, 0C0C0C0Ch
            and edx, 30303030h
            or edi, edx
            shl esi, 2
            shr edi, 2
            or esi, edi
            mov edx, esi

            loc_610323 :
        test    ebx, ebx
            jz  short loc_610363
            mov esi, eax
            mov eax, ecx
            shr eax, 10h
            shl ecx, 10h
            or eax, ecx
            mov ecx, esi
            shr ecx, 10h
            shl esi, 10h
            or ecx, esi
            mov esi, edx
            mov edi, edx
            and esi, 0FF0000h
            shr edi, 10h
            or esi, edi
            mov edi, edx
            shl edi, 10h
            and edx, 0FF00h
            or edi, edx
            shr esi, 8
            shl edi, 8
            or esi, edi
            mov edx, esi

            loc_610363 :
        mov esi, [ebp + -0x8]
            mov[esi], eax
            mov eax, [ebp + -0x4]
            mov[esi + 0Ch], edx
            mov edx, [ebp + -0xC]
            mov[esi + 4], ecx
            mov[esi + 8], eax

            loc_610377 :
        mov ecx, [ebp + -0x8]
            mov eax, [ebp + -0x14]
            inc edx
            add ecx, 10h
            cmp edx, eax
            mov[ebp + -0xC], edx
            mov[ebp + -0x8], ecx
            jb  loc_6101D1
            pop edi
            pop esi
            pop ebx

            loc_610392 :
        mov esp, ebp
            pop ebp

            retn
    }
}

void AtexSubCode7_Asm(unsigned int a, unsigned int b)
{
    __asm {
        mov ecx, a
        mov edx, b
        call AtexSubCode7
    }
}
