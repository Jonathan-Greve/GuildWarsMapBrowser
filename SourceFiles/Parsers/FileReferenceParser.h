#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <optional>

namespace GW::Parsers {

// File reference chunk IDs (B-series for "other" format, F-series for "standard" format)
// Note: Some chunk IDs like FA6 are defined in BB9AnimationParser.h as animation chunks
#ifndef CHUNK_ID_BBB
constexpr uint32_t CHUNK_ID_BBB = 0x00000BBB;  // Texture filenames (other format)
#endif
#ifndef CHUNK_ID_BBC
constexpr uint32_t CHUNK_ID_BBC = 0x00000BBC;  // Additional filenames / file references (other format)
#endif
#ifndef CHUNK_ID_BBD
constexpr uint32_t CHUNK_ID_BBD = 0x00000BBD;  // Animation file references (other format)
#endif
#ifndef CHUNK_ID_BBA
constexpr uint32_t CHUNK_ID_BBA = 0x00000BBA;  // Texture references (other format)
#endif
#ifndef CHUNK_ID_FA4
constexpr uint32_t CHUNK_ID_FA4 = 0x00000FA4;  // Texture references (standard format)
#endif
#ifndef CHUNK_ID_FA5
constexpr uint32_t CHUNK_ID_FA5 = 0x00000FA5;  // Texture filenames (standard format)
#endif
// FA6 is a file reference chunk (equivalent to BBC in "other" format)
// Contains additional filename references including Type 8 sound event files
#ifndef CHUNK_ID_FA6
constexpr uint32_t CHUNK_ID_FA6 = 0x00000FA6;  // Additional file references / sound events (standard format)
#endif
#ifndef CHUNK_ID_FA8
constexpr uint32_t CHUNK_ID_FA8 = 0x00000FA8;  // Animation file references (standard format)
#endif

/**
 * @brief Encoded file reference structure (6 bytes).
 *
 * File references in Guild Wars FFNA files use a special encoding:
 * - id0: Encoded part 1 (uint16)
 * - id1: Encoded part 2 (uint16)
 * - flags: Additional flags (uint16)
 *
 * Decode formula: file_id = (id0 - 0xFF00FF) + (id1 * 0xFF00)
 */
#pragma pack(push, 1)
struct FileRef
{
    uint16_t id0;
    uint16_t id1;
    uint16_t flags;

    /**
     * @brief Decodes the file reference to a file ID.
     *
     * @return Decoded file ID for DAT file lookup.
     */
    uint32_t DecodeFileId() const
    {
        // Formula: (id0 - 0xff00ff) + (id1 * 0xff00)
        int32_t result = static_cast<int32_t>(id0) - 0xFF00FF;
        result += static_cast<int32_t>(id1) * 0xFF00;
        return static_cast<uint32_t>(result);
    }

    /**
     * @brief Alternative decoding formula observed in some files.
     */
    uint32_t DecodeFileIdAlt() const
    {
        // Alternative formula: direct combination
        return (static_cast<uint32_t>(id1) << 16) | id0;
    }
};
#pragma pack(pop)

static_assert(sizeof(FileRef) == 6, "FileRef must be 6 bytes!");

/**
 * @brief Texture reference structure for BBA/FA4 chunks (16 bytes).
 */
#pragma pack(push, 1)
struct TextureRef
{
    uint16_t id0;
    uint16_t id1;
    uint32_t unknown0;
    uint32_t unknown1;
    uint32_t unknown2;

    uint32_t DecodeFileId() const
    {
        // Formula: (id0 - 0xff00ff) + (id1 * 0xff00)
        int32_t result = static_cast<int32_t>(id0) - 0xFF00FF;
        result += static_cast<int32_t>(id1) * 0xFF00;
        return static_cast<uint32_t>(result);
    }
};
#pragma pack(pop)

static_assert(sizeof(TextureRef) == 16, "TextureRef must be 16 bytes!");

/**
 * @brief Parsed file reference with type information.
 */
struct ParsedFileRef
{
    uint32_t fileId;       // Decoded file ID
    uint16_t flags;        // Original flags
    uint32_t chunkType;    // Chunk type it was found in
    size_t index;          // Index within the chunk

    enum class Type
    {
        Unknown,
        Texture,
        Animation,
        SubModel,
        Material
    };

    Type type = Type::Unknown;
};

/**
 * @brief Parser for file references in FFNA files.
 *
 * Handles parsing of:
 * - BBB/FA5: Texture filename references (6 bytes each)
 * - BBC/FA6: Additional filename references (6 bytes each)
 * - BBD: Animation file references
 * - BBA/FA4: Texture references with metadata (16 bytes each)
 */
class FileReferenceParser
{
public:
    /**
     * @brief Parses file references from a BBB/BBC/FA5/FA6 chunk.
     *
     * These chunks contain 6-byte file reference entries.
     *
     * @param data Chunk data (after chunk ID and size).
     * @param dataSize Chunk size.
     * @param outRefs Output vector of parsed references.
     * @return true if parsing succeeded.
     */
    static bool ParseFileNameRefs(const uint8_t* data, size_t dataSize,
                                  std::vector<ParsedFileRef>& outRefs)
    {
        // Header: unknown (4 bytes) + count (4 bytes)
        if (dataSize < 8)
        {
            return false;
        }

        uint32_t unknown, count;
        std::memcpy(&unknown, &data[0], sizeof(uint32_t));
        std::memcpy(&count, &data[4], sizeof(uint32_t));

        // Validate count
        size_t maxEntries = (dataSize - 8) / sizeof(FileRef);
        if (count > maxEntries)
        {
            count = static_cast<uint32_t>(maxEntries);
        }

        outRefs.reserve(count);
        size_t offset = 8;

        for (uint32_t i = 0; i < count; i++)
        {
            if (offset + sizeof(FileRef) > dataSize)
            {
                break;
            }

            FileRef ref;
            std::memcpy(&ref, &data[offset], sizeof(FileRef));
            offset += sizeof(FileRef);

            ParsedFileRef parsed;
            parsed.fileId = ref.DecodeFileId();
            parsed.flags = ref.flags;
            parsed.index = i;
            parsed.type = ParsedFileRef::Type::Texture;

            outRefs.push_back(parsed);
        }

        return true;
    }

    /**
     * @brief Parses texture references from a BBA/FA4 chunk.
     *
     * These chunks contain 16-byte texture reference entries with metadata.
     *
     * @param data Chunk data (after chunk ID and size).
     * @param dataSize Chunk size.
     * @param outRefs Output vector of parsed references.
     * @return true if parsing succeeded.
     */
    static bool ParseTextureRefs(const uint8_t* data, size_t dataSize,
                                 std::vector<ParsedFileRef>& outRefs)
    {
        // Calculate number of entries
        size_t numEntries = dataSize / sizeof(TextureRef);
        outRefs.reserve(numEntries);

        for (size_t i = 0; i < numEntries; i++)
        {
            size_t offset = i * sizeof(TextureRef);
            if (offset + sizeof(TextureRef) > dataSize)
            {
                break;
            }

            TextureRef ref;
            std::memcpy(&ref, &data[offset], sizeof(TextureRef));

            ParsedFileRef parsed;
            parsed.fileId = ref.DecodeFileId();
            parsed.flags = 0;
            parsed.index = i;
            parsed.type = ParsedFileRef::Type::Texture;

            outRefs.push_back(parsed);
        }

        return true;
    }

    /**
     * @brief Parses animation references from a BBD chunk.
     *
     * @param data Chunk data (after chunk ID and size).
     * @param dataSize Chunk size.
     * @param outRefs Output vector of parsed references.
     * @return true if parsing succeeded.
     */
    static bool ParseAnimationRefs(const uint8_t* data, size_t dataSize,
                                   std::vector<ParsedFileRef>& outRefs)
    {
        // Animation references may have a different format
        // For now, try the standard 6-byte format
        return ParseFileNameRefs(data, dataSize, outRefs);
    }

    /**
     * @brief Scans an FFNA file for all file references.
     *
     * @param fileData Complete FFNA file data.
     * @param fileSize File size.
     * @param outRefs Output vector of all parsed references.
     * @return true if any references were found.
     */
    static bool ScanForFileRefs(const uint8_t* fileData, size_t fileSize,
                                std::vector<ParsedFileRef>& outRefs)
    {
        // Verify FFNA signature
        if (fileSize < 5 || fileData[0] != 'f' || fileData[1] != 'f' ||
            fileData[2] != 'n' || fileData[3] != 'a')
        {
            return false;
        }

        // Scan all chunks
        size_t offset = 5;
        while (offset + 8 <= fileSize)
        {
            uint32_t chunkId, chunkSize;
            std::memcpy(&chunkId, &fileData[offset], sizeof(uint32_t));
            std::memcpy(&chunkSize, &fileData[offset + 4], sizeof(uint32_t));

            if (chunkId == 0 || chunkSize == 0 || offset + 8 + chunkSize > fileSize)
            {
                break;
            }

            const uint8_t* chunkData = &fileData[offset + 8];

            // Parse based on chunk type
            std::vector<ParsedFileRef> chunkRefs;
            bool parsed = false;

            switch (chunkId)
            {
            case CHUNK_ID_BBB:
            case CHUNK_ID_BBC:
            case CHUNK_ID_FA5:
                parsed = ParseFileNameRefs(chunkData, chunkSize, chunkRefs);
                break;

            case CHUNK_ID_BBA:
            case CHUNK_ID_FA4:
                parsed = ParseTextureRefs(chunkData, chunkSize, chunkRefs);
                break;

            case CHUNK_ID_BBD:
            case CHUNK_ID_FA8:
                parsed = ParseAnimationRefs(chunkData, chunkSize, chunkRefs);
                for (auto& ref : chunkRefs)
                {
                    ref.type = ParsedFileRef::Type::Animation;
                }
                break;
            }

            if (parsed)
            {
                for (auto& ref : chunkRefs)
                {
                    ref.chunkType = chunkId;
                    outRefs.push_back(std::move(ref));
                }
            }

            offset += 8 + chunkSize;
        }

        return !outRefs.empty();
    }

    /**
     * @brief Gets all texture file IDs from an FFNA file.
     *
     * @param fileData Complete FFNA file data.
     * @param fileSize File size.
     * @return Vector of texture file IDs.
     */
    static std::vector<uint32_t> GetTextureFileIds(const uint8_t* fileData, size_t fileSize)
    {
        std::vector<ParsedFileRef> refs;
        ScanForFileRefs(fileData, fileSize, refs);

        std::vector<uint32_t> textureIds;
        for (const auto& ref : refs)
        {
            if (ref.type == ParsedFileRef::Type::Texture)
            {
                textureIds.push_back(ref.fileId);
            }
        }

        return textureIds;
    }

    /**
     * @brief Gets all animation file IDs from an FFNA file.
     *
     * @param fileData Complete FFNA file data.
     * @param fileSize File size.
     * @return Vector of animation file IDs.
     */
    static std::vector<uint32_t> GetAnimationFileIds(const uint8_t* fileData, size_t fileSize)
    {
        std::vector<ParsedFileRef> refs;
        ScanForFileRefs(fileData, fileSize, refs);

        std::vector<uint32_t> animIds;
        for (const auto& ref : refs)
        {
            if (ref.type == ParsedFileRef::Type::Animation)
            {
                animIds.push_back(ref.fileId);
            }
        }

        return animIds;
    }
};

} // namespace GW::Parsers
