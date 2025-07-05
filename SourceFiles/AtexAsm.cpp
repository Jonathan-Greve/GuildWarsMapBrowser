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

void AtexSubCode6();
void __declspec(naked) AtexSubCode5()
{
    __asm {
        push    ebp
        mov ebp, esp

        sub esp, 10h
        push    ebx
        push    esi
        mov esi, [ebp + 0x0c]
        mov[ebp + -0x4], ecx
        push    edi
        mov ecx, [esi + 10h]
        mov eax, [esi + 0Ch]
        mov edi, ecx
        mov edx, eax
        shr edi, 8
        shl eax, 18h
        or edi, eax
        mov[esi + 0Ch], edi
        mov edi, [esi + 8]
        shr edx, 8
        cmp edi, 18h
        jb  short loc_60FF1D
        shl ecx, 18h
        add edi, 0FFFFFFE8h
        mov[esi + 10h], ecx
        mov[esi + 8], edi
        jmp short loc_60FF5E
        ; -------------------------------------------------------------------------- -

        loc_60FF1D:
        mov eax, [esi]
            mov ecx, [esi + 4]
            cmp eax, ecx
            jz  short loc_60FF56
            mov ecx, [eax]
            add eax, 4
            mov[esi + 10h], ecx
            mov[esi], eax
            mov eax, [esi + 10h]
            lea ecx, [edi + 8]
            mov ebx, eax
            mov[ebp + 0x0c], ecx
            shr ebx, cl
            mov ecx, [esi + 0Ch]
            or ecx, ebx
            mov[esi + 0Ch], ecx
            mov ecx, 18h
            sub ecx, edi
            shl eax, cl
            mov[esi + 10h], eax
            mov eax, [ebp + 0x0c]
            jmp short loc_60FF5B
            ; -------------------------------------------------------------------------- -

            loc_60FF56:
        xor eax, eax
            mov[esi + 10h], eax

            loc_60FF5B :
        mov[esi + 8], eax

            loc_60FF5E :
        mov ecx, [ebp + 0x18]
            or edx, 0FF000000h
            push    ecx
            lea ecx, [ebp + -0x10]
            call    AtexSubCode6
            mov eax, [ebp + 0x10]
            xor edi, edi
            test    eax, eax
            jz  loc_610133
            ; -------------------------------------------------------------------------- -

            loc_60FF7D:
        mov eax, [esi + 0Ch]
            xor ecx, ecx
            shr eax, 1Ah
            xor ebx, ebx
            mov cl, ds : byte_79053D[eax * 2]
            mov bl, ds : byte_79053C[eax * 2]
            inc ecx
            cmp ebx, 20h
            mov[ebp + 0x0c], ecx
            jb  short loc_60FFAF
            ; -------------------------------------------------------------------------- -

            loc_60FFAF:
        test    ebx, ebx
            jz  short loc_60FFCB
            mov edx, [esi + 10h]
            mov eax, [esi + 0Ch]
            mov ecx, 20h
            sub ecx, ebx
            shr edx, cl
            mov ecx, ebx
            shl eax, cl
            or edx, eax
            mov[esi + 0Ch], edx

            loc_60FFCB :
        mov eax, [esi + 8]
            cmp ebx, eax
            mov[ebp + 0x18], eax
            ja  short loc_60FFE3
            mov edx, [esi + 10h]
            mov ecx, ebx
            shl edx, cl
            sub eax, ebx
            mov[esi + 10h], edx
            jmp short loc_61001E
            ; -------------------------------------------------------------------------- -

            loc_60FFE3:
        mov ecx, [esi]
            mov edx, [esi + 4]
            cmp ecx, edx
            jz  short loc_610019
            mov edx, [ecx]
            add ecx, 4
            sub eax, ebx
            mov[esi], ecx
            mov[esi + 10h], edx
            lea ecx, [eax + 20h]
            mov eax, edx
            shr edx, cl
            mov[ebp + -0x8], ecx
            mov ecx, [esi + 0Ch]
            or ecx, edx
            mov[esi + 0Ch], ecx
            mov ecx, ebx
            sub ecx, [ebp + 0x18]
            shl eax, cl
            mov[esi + 10h], eax
            mov eax, [ebp + -0x8]
            jmp short loc_61001E
            ; -------------------------------------------------------------------------- -

            loc_610019:
        xor eax, eax
            mov[esi + 10h], eax

            loc_61001E :
        mov ecx, [esi + 10h]
            mov[esi + 8], eax
            mov eax, [esi + 0Ch]
            mov edx, ecx
            mov ebx, eax
            add eax, eax
            shr edx, 1Fh
            or edx, eax
            mov[esi + 0Ch], edx
            mov edx, [esi + 8]
            shr ebx, 1Fh
            cmp edx, 1
            mov[ebp + 0x18], ebx
            jb  short loc_61004E
            add ecx, ecx
            dec edx
            mov[esi + 10h], ecx
            mov[esi + 8], edx
            jmp short loc_610095
            ; -------------------------------------------------------------------------- -

            loc_61004E:
        mov eax, [esi]
            mov ecx, [esi + 4]
            cmp eax, ecx
            jz  short loc_61008D
            mov ecx, [eax]
            add eax, 4
            mov[esi + 10h], ecx
            mov[esi], eax
            mov eax, [esi + 10h]
            lea ecx, [edx + 1Fh]
            mov ebx, eax
            mov[ebp + -0x8], ecx
            shr ebx, cl
            mov ecx, [esi + 0Ch]
            or ecx, ebx
            mov ebx, [ebp + 0x18]
            mov[esi + 0Ch], ecx
            mov ecx, 1
            sub ecx, edx
            mov edx, [ebp + -0x8]
            shl eax, cl
            mov[esi + 8], edx
            mov[esi + 10h], eax
            jmp short loc_610095
            ; -------------------------------------------------------------------------- -

            loc_61008D:
        xor eax, eax
            mov[esi + 10h], eax
            mov[esi + 8], eax

            loc_610095 :
        mov eax, [ebp + 0x0c]
            test    eax, eax
            jz  short loc_6100F8
            ; -------------------------------------------------------------------------- -

            loc_61009C:
        cmp edi, [ebp + 0x10]
            jz  loc_610133
            mov ecx, edi
            mov edx, 1
            and ecx, 1Fh
            mov eax, edi
            shl edx, cl
            mov ecx, [ebp + 0x08]
            shr eax, 5
            lea eax, [ecx + eax * 4]
            mov[ebp + -0x8], eax
            mov eax, [eax]
            test    edx, eax
            jnz short loc_6100E4
            test    ebx, ebx
            jz  short loc_6100E1
            mov ecx, [ebp + -0x4]
            mov ebx, [ebp + -0x10]
            or eax, edx
            mov edx, [ebp + -0x8]
            mov[ecx], ebx
            mov ebx, [ebp + -0xC]
            mov[ecx + 4], ebx
            mov ebx, [ebp + 0x18]
            mov[edx], eax

            loc_6100E1 :
        dec[ebp + 0x0c]

            loc_6100E4 :
            mov eax, [ebp + 0x14]
            mov ecx, [ebp + -0x4]
            inc edi
            lea edx, [ecx + eax * 4]
            mov eax, [ebp + 0x0c]
            test    eax, eax
            mov[ebp + -0x4], edx
            jnz short loc_61009C
            ; -------------------------------------------------------------------------- -

            loc_6100F8:
        mov eax, [ebp + 0x10]
            cmp edi, eax
            jz  short loc_610133
            ; -------------------------------------------------------------------------- -

            loc_6100FF:
        mov ebx, [ebp + 0x08]
            mov ecx, edi
            and ecx, 1Fh
            mov edx, 1
            shl edx, cl
            mov ecx, edi
            shr ecx, 5
            test[ebx + ecx * 4], edx
            jz  short loc_61012B
            mov edx, [ebp + -0x4]
            mov ecx, [ebp + 0x14]
            inc edi
            lea edx, [edx + ecx * 4]
            cmp edi, eax
            mov[ebp + -0x4], edx
            jz  short loc_610133
            jmp short loc_6100FF
            ; -------------------------------------------------------------------------- -

            loc_61012B:
        cmp edi, eax
            jnz loc_60FF7D
            ; -------------------------------------------------------------------------- -

            loc_610133:
        pop edi
            pop esi
            pop ebx

            mov esp, ebp
            pop ebp

            retn    14h
    }
}

void AtexSubCode5_Asm(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e,
                   unsigned int f, unsigned int g)
{
    __asm {
        mov ecx, a
        mov edx, b
        push g
        push f
        push e
        push d
        push c
        call AtexSubCode5
    }
}

void __declspec(naked) AtexSubCode6()
{
    __asm {
        push    ebp
        mov ebp, esp

        sub esp, 50h
        mov[ebp + -0x8], ecx
        mov[ebp + -0x4], edx
        xor ecx, ecx
        push    ebx
        mov cl, byte ptr[ebp + -0x4 + 2]
        mov eax, edx
        push    esi
        push    edi
        and eax, 0FFh
        xor ebx, ebx
        mov edi, ecx
        mov ecx, eax
        mov bl, dh
        mov edx, eax
        shr ecx, 5
        sub edx, ecx
        mov ecx, ebx
        shr ecx, 6
        mov esi, ebx
        mov[ebp + -0x3C], edi
        sub esi, ecx
        mov ecx, edi
        shr ecx, 5
        shr edx, 3
        sub edi, ecx
        mov ecx, edx
        shr ecx, 2
        shr esi, 2
        lea ecx, [ecx + edx * 8]
        mov[ebp + -0x14], edx
        mov[ebp + -0x20], ecx
        mov ecx, esi
        shr ecx, 4
        shr edi, 3
        lea ecx, [ecx + esi * 4]
        mov[ebp + -0x10], esi
        mov[ebp + -0x1C], ecx
        mov ecx, edi
        shr ecx, 2
        inc edx
        mov[ebp + -0xC], edi
        lea ecx, [ecx + edi * 8]
        lea eax, [eax + eax * 2]
        mov[ebp + -0x18], ecx
        mov ecx, edx
        shr ecx, 2
        inc esi
        lea ecx, [ecx + edx * 8]
        mov edx, esi
        shr edx, 4
        inc edi
        lea edx, [edx + esi * 4]
        mov esi, edi
        shr esi, 2
        shl eax, 2
        lea esi, [esi + edi * 8]
        mov edi, [ebp + -0x20]
        sub ecx, edi
        mov[ebp + -0x38], ecx
        mov ecx, [ebp + -0x1C]
        sub edx, ecx
        mov ecx, [ebp + -0x18]
        mov[ebp + -0x34], edx
        lea edx, [edi + edi * 2]
        shl edx, 2
        sub eax, edx
        xor edx, edx
        div[ebp + -0x38]
        sub esi, ecx
        mov[ebp + -0x2C], eax
        mov eax, [ebp + -0x1C]
        lea eax, [eax + eax * 2]
        shl eax, 2
        mov edx, eax
        lea eax, [ebx + ebx * 2]
        shl eax, 2
        sub eax, edx
        xor edx, edx
        div[ebp + -0x34]
        mov[ebp + -0x28], eax
        lea eax, [ecx + ecx * 2]
        shl eax, 2
        mov ecx, eax
        mov eax, [ebp + -0x3C]
        xor edx, edx
        xor edi, edi
        lea eax, [eax + eax * 2]
        shl eax, 2
        sub eax, ecx
        div esi
        xor ecx, ecx
        mov[ebp + -0x24], eax
        lea eax, [ebp + -0x50]

        loc_62A842:
        mov edx, [ebp + ecx + -0x2C]
            cmp edx, 2
            jnb short loc_62A851
            mov edx, [ebp + ecx + -0x14]
            jmp short loc_62A876
            ; -------------------------------------------------------------------------- -

            loc_62A851:
        cmp edx, 6
            jnb short loc_62A862
            mov edx, [ebp + ecx + -0x14]
            mov[eax], edx
            inc edx
            mov[eax + 4], edx
            jmp short loc_62A87B
            ; -------------------------------------------------------------------------- -

            loc_62A862:
        cmp edx, 0Ah
            mov edx, [ebp + ecx + -0x14]
            jnb short loc_62A875
            lea esi, [edx + 1]
            mov[eax], esi
            mov[eax + 4], edx
            jmp short loc_62A87B
            ; -------------------------------------------------------------------------- -

            loc_62A875:
        inc edx

            loc_62A876 :
        mov[eax + 4], edx
            mov[eax], edx

            loc_62A87B :
        add ecx, 4
            add eax, 8
            cmp ecx, 0Ch
            jb  short loc_62A842
            mov esi, [ebp + -0x40]
            mov ebx, [ebp + -0x48]
            mov ecx, [ebp + -0x50]
            mov eax, [ebp + -0x4C]
            shl esi, 6
            or esi, ebx
            mov ebx, [ebp + -0x44]
            shl esi, 5
            or esi, ecx
            mov ecx, [ebp + -0x3C]
            shl ecx, 6
            or ecx, ebx
            mov[ebp + -0x4], edi
            shl ecx, 5
            or ecx, eax
            lea ebx, [ebp + -0x50]
            xor eax, eax

            loc_62A8B4 :
        mov edx, [ebx]
            cmp edx, [ebx + 4]
            jz  short loc_62A8D9
            cmp edx, [ebp + eax + -0x14]
            jnz short loc_62A8C7
            mov edx, [ebp + eax + -0x2C]
            jmp short loc_62A8D0
            ; -------------------------------------------------------------------------- -

            loc_62A8C7:
        mov edx, 0Ch
            sub edx, [ebp + eax + -0x2C]

            loc_62A8D0 :
            add edi, edx
            mov edx, [ebp + -0x4]
            inc edx
            mov[ebp + -0x4], edx

            loc_62A8D9 :
        add eax, 4
            add ebx, 8
            cmp eax, 0Ch
            jb  short loc_62A8B4
            mov eax, [ebp + -0x4]
            test     eax, eax
            jz  short loc_62A8F6
            shr eax, 1
            add eax, edi
            xor edx, edx
            div[ebp + -0x4]
            mov edi, eax

            loc_62A8F6 :
        mov eax, [ebp + 0x8]
            test    eax, eax
            jz  short loc_62A915
            cmp edi, 5
            jz  short loc_62A90E
            cmp edi, 6
            jz  short loc_62A90E
            mov eax, [ebp + -0x4]
            test    eax, eax
            jnz short loc_62A915
            ; -------------------------------------------------------------------------- -

            loc_62A90E:
        mov edx, 1
            jmp short loc_62A917
            ; -------------------------------------------------------------------------- -

            loc_62A915:
        xor edx, edx

            loc_62A917 :
        mov eax, [ebp + -0x4]
            test    eax, eax
            jnz short loc_62A935
            test    edx, edx
            jnz short loc_62A935
            cmp ecx, 0FFFFh
            jz  short loc_62A92F
            xor edi, edi
            inc ecx
            jmp short loc_62A935
            ; -------------------------------------------------------------------------- -

            loc_62A92F:
        mov edi, 0Ch
            dec esi

            loc_62A935 :
        cmp ecx, esi
            sbb eax, eax
            inc eax
            cmp edx, eax
            jz  short loc_62A94D
            mov eax, esi
            mov esi, ecx
            mov ecx, eax
            mov eax, 0Ch
            sub eax, edi
            mov edi, eax

            loc_62A94D :
        test    edx, edx
            jz  short loc_62A958
            mov eax, 2
            jmp short loc_62A976
            ; -------------------------------------------------------------------------- -

            loc_62A958:
        cmp edi, 2
            jnb short loc_62A961
            xor eax, eax
            jmp short loc_62A976
            ; -------------------------------------------------------------------------- -

            loc_62A961:
        cmp edi, 6
            jnb short loc_62A96D
            mov eax, 2
            jmp short loc_62A976
            ; -------------------------------------------------------------------------- -

            loc_62A96D:
        cmp edi, 0Ah
            sbb eax, eax
            and eax, 2
            inc eax

            loc_62A976 :
        mov edx, [ebp + -0x8]
            pop edi
            shl ecx, 10h
            or ecx, esi
            pop esi
            mov[edx], ecx
            lea ecx, ds:0[eax * 4]
            or ecx, eax
            pop ebx
            mov eax, ecx
            shl eax, 4
            or eax, ecx
            mov ecx, eax
            shl ecx, 8
            or ecx, eax
            mov eax, ecx
            shl eax, 10h
            or eax, ecx
            mov[edx + 4], eax

            mov esp, ebp
            pop ebp

            retn    4
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
