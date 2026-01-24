#pragma once
#include "FFNAType.h"
#include "FFNA_ModelFile.h"
#include "AtexReader.h"
#include <array>
#include <stdint.h>
#include <span>
#include <cmath>
#include <fstream>
#include <set>

// Debug logging for BB8 parsing
inline void LogBB8Debug(const char* msg)
{
    static std::ofstream log_file("bb8_debug.log", std::ios::app);
    if (log_file.is_open())
    {
        log_file << msg << std::flush;
    }
}

// =============================================================================
// FFNA Type 2 "Other" Model File Parser
// These model files use chunk IDs in the 0xBB* range instead of 0xFA* range
// They also contain inline ATEX textures instead of file references
//
// NOTE: The 0xBB8 geometry chunk uses a DIFFERENT internal format than 0xFA0.
// The game's FUN_0076d380 converts this format to standard 0xFA0 at runtime.
// Full geometry parsing is not implemented - we focus on inline texture extraction.
// =============================================================================

// Chunk ID mappings (Other -> Standard):
// 0xBB8 (3000) -> 0xFA0 (4000): Geometry (different internal format!)
// 0xBB9 (3001) -> 0xFA1 (4001): Animation/Skeleton
// 0xBBA (3002) -> 0xFA4 (4004): Metadata (version info)
// 0xBBB (3003) -> 0xFA5 (4005): Texture filenames
// 0xBBC (3004) -> 0xFA6 (4006): Additional filenames
// 0xBC0 (3008): Additional data
// 0xFA3 (4003): Inline ATEX DXT3 texture
// 0xFAA (4010): Inline ATEX DXTA texture

constexpr uint32_t CHUNK_ID_GEOMETRY_OTHER = 0x00000BB8;
constexpr uint32_t CHUNK_ID_ANIMATION_OTHER = 0x00000BB9;
constexpr uint32_t CHUNK_ID_METADATA_OTHER = 0x00000BBA;
constexpr uint32_t CHUNK_ID_TEXTURE_FILENAMES_OTHER = 0x00000BBB;
constexpr uint32_t CHUNK_ID_ADDITIONAL_FILENAMES_OTHER = 0x00000BBC;
constexpr uint32_t CHUNK_ID_ADDITIONAL_DATA_OTHER = 0x00000BC0;
constexpr uint32_t CHUNK_ID_INLINE_ATEX_DXT3 = 0x00000FA3;
constexpr uint32_t CHUNK_ID_INLINE_ATEX_DXTA = 0x00000FAA;

// =============================================================================
// Texture filename structure for "other" format (0xBBB chunk)
// Different from standard format - entries are 6 bytes:
//   Bytes 0-1: id0 (encoded filename part 1)
//   Bytes 2-3: id1 (encoded filename part 2)
//   Bytes 4-5: unknown (usually 0)
// Decode: file_hash = (id0 - 0xFF00FF) + (id1 * 0xFF00)
// =============================================================================
#pragma pack(push, 1)
struct TextureFileNameOther
{
    uint16_t id0;         // Encoded filename part 1
    uint16_t id1;         // Encoded filename part 2
    uint16_t unknown;     // Usually 0

    TextureFileNameOther() = default;

    TextureFileNameOther(uint32_t offset, const unsigned char* data)
    {
        std::memcpy(&id0, &data[offset], sizeof(id0));
        std::memcpy(&id1, &data[offset + 2], sizeof(id1));
        std::memcpy(&unknown, &data[offset + 4], sizeof(unknown));
    }
};
#pragma pack(pop)

static_assert(sizeof(TextureFileNameOther) == 6, "TextureFileNameOther must be 6 bytes!");

struct TextureFileNamesChunkOther
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t unknown;                    // Unknown field (usually small value)
    uint32_t num_texture_filenames;      // Actual count of texture filename entries
    uint32_t actual_num_texture_filenames;
    std::vector<TextureFileNameOther> texture_filenames;
    std::vector<uint8_t> chunk_data;

    TextureFileNamesChunkOther() = default;

    TextureFileNamesChunkOther(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
                               bool& textures_parsed_correctly)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        std::memcpy(&unknown, &data[offset + 8], sizeof(unknown));
        std::memcpy(&num_texture_filenames, &data[offset + 12], sizeof(num_texture_filenames));

        // Use num_texture_filenames as the actual count
        // Chunk layout: unknown (4) + num_filenames (4) + filenames (6 bytes each)
        actual_num_texture_filenames = num_texture_filenames;

        // Validate count doesn't exceed chunk bounds
        uint32_t max_entries = (chunk_size - 8) / sizeof(TextureFileNameOther);
        if (actual_num_texture_filenames > max_entries)
        {
            actual_num_texture_filenames = max_entries;
        }

        uint32_t curr_offset = offset + 16;  // Skip: chunk_id(4) + chunk_size(4) + unknown(4) + num_filenames(4)
        texture_filenames.resize(actual_num_texture_filenames);

        for (uint32_t i = 0; i < actual_num_texture_filenames; ++i)
        {
            texture_filenames[i] = TextureFileNameOther(curr_offset, data);
            curr_offset += sizeof(TextureFileNameOther);
        }

        // Calculate remaining bytes: chunk_size - (unknown + num_filenames + filenames)
        size_t used_bytes = 8 + (sizeof(TextureFileNameOther) * actual_num_texture_filenames);
        if (chunk_size > used_bytes)
        {
            size_t remaining_bytes = chunk_size - used_bytes;
            if (curr_offset + remaining_bytes <= data_size_bytes)
            {
                chunk_data.resize(remaining_bytes);
                std::memcpy(chunk_data.data(), &data[curr_offset], remaining_bytes);
            }
        }
    }
};

// =============================================================================
// Header structure for 0xBB8 geometry chunk
// Based on reverse engineering of MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0
//
// ClassFlags bitmask (offset 0x08) controls which data sections exist:
//   0x002: Bone group data
//   0x004: Bounding box data
//   0x008: Submesh data (most important - contains vertex/index data)
//   0x010: LOD data
//   0x020: Vertex buffer data
//   0x040: Bone weights
//   0x080: Morph target data
//   0x100: Animation data
//   0x200: Skeleton data
//   0x400: Extended LOD data
// =============================================================================

// UV scale factor used for decompressing 16-bit UV coordinates
// Value: 0x3ef00010 as float ≈ 0.46875f (approximately 15/32)
constexpr float UV_SCALE_FACTOR = 0.46875f;

// IMPORTANT: The main geometry header is 48 bytes (0x30), NOT 64 bytes!
// This was verified from disassembly at 0x0076d3bd: LEA EAX, [ESI + 0x30]
#pragma pack(push, 1)
struct ChunkBB8_Header
{
    uint32_t field_0x00;            // 0x00: Type/version (usually contains flags)
    uint32_t field_0x04;            // 0x04: Usually 0
    uint32_t class_flags;           // 0x08: Bitmask controlling which data sections exist
    uint32_t signature0;            // 0x0C: Model signature part 1
    uint32_t signature1;            // 0x10: Model signature part 2
    uint32_t field_0x14;            // 0x14
    uint8_t num_bone_groups;        // 0x18: Number of bone groups (max 4)
    uint8_t num_texture_groups;     // 0x19: Number of texture groups
    uint16_t num_textures;          // 0x1A: Number of textures
    uint8_t num_bone_weights;       // 0x1C: Number of bone weights
    uint8_t num_bone_indices;       // 0x1D: Number of bone indices
    uint16_t num_materials;         // 0x1E: Number of materials
    uint32_t num_bone_weight_sets;  // 0x20: Number of bone weight sets
    uint32_t class_flags_output;    // 0x24: Output class flags (stored in output)
    float scale_x;                  // 0x28: X scale factor for submeshes
    float scale_y;                  // 0x2C: Y scale factor for submeshes
    // Total: 0x30 (48) bytes - NOT 64 bytes!

    ChunkBB8_Header() = default;
    ChunkBB8_Header(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }

    // Check if submesh data is present
    bool HasSubmeshData() const { return (class_flags & 0x008) != 0; }
    bool HasBoneGroups() const { return (class_flags & 0x002) != 0; }
    bool HasBoundingBox() const { return (class_flags & 0x004) != 0; }
    bool HasLODData() const { return (class_flags & 0x010) != 0; }
    bool HasVertexBuffer() const { return (class_flags & 0x020) != 0; }
    bool HasBoneWeights() const { return (class_flags & 0x040) != 0; }
    bool HasMorphTargets() const { return (class_flags & 0x080) != 0; }
    bool HasAnimation() const { return (class_flags & 0x100) != 0; }
    bool HasSkeleton() const { return (class_flags & 0x200) != 0; }
    bool HasExtendedLOD() const { return (class_flags & 0x400) != 0; }
};
#pragma pack(pop)

static_assert(sizeof(ChunkBB8_Header) == 0x30, "ChunkBB8_Header must be 48 bytes!");

// Texture group structure for "other" format (9 bytes)
// Similar to UnknownTexStruct0 (8 bytes) but with an extra byte that may contain blend info
#pragma pack(push, 1)
// This structure maps directly to UnknownTexStruct1 (uts1) in the 0xFA0 format.
// When 0xBB8 is converted to 0xFA0, these 9 bytes are copied as-is.
// Note: The blend_flag for rendering does NOT come from this structure!
// For MODERN format (with texture_groups), blend_flag defaults to 0.
// For OLD format (no texture_groups), blend_flag defaults to 8.
struct TextureGroupOther
{
    uint16_t some_flags0;         // 0x00-0x01: Flags (maps to uts1.some_flags0)
    uint16_t some_flags1;         // 0x02-0x03: Flags (maps to uts1.some_flags1)
    uint8_t f0x4;                 // 0x04: Unknown (maps to uts1.f0x4)
    uint8_t f0x5;                 // 0x05: Unknown (maps to uts1.f0x5)
    uint8_t num_textures_to_use;  // 0x06: Number of textures to use for this submesh
    uint8_t f0x7;                 // 0x07: Unknown (maps to uts1.f0x7)
    uint8_t f0x8;                 // 0x08: Unknown (maps to uts1.f0x8) - NOT blend_flag!
};
#pragma pack(pop)
static_assert(sizeof(TextureGroupOther) == 9, "TextureGroupOther must be 9 bytes!");


// Submesh header for 0xBB8 format (24 bytes = 6 DWORDs)
// Discovered through binary analysis of actual model files.
// Data after this header:
//   - Index buffer: num_indices * 2 bytes (uint16 indices)
//   - Position buffer: num_vertices * 12 bytes (float triplets)
#pragma pack(push, 1)
struct SubmeshBB8_Header
{
    uint32_t num_indices;           // 0x00: Number of indices (16-bit)
    uint32_t num_vertices;          // 0x04: Number of vertices
    uint32_t num_uv_sets;           // 0x08: Number of UV coordinate sets per vertex
    uint32_t num_vertex_groups;     // 0x0C: Number of vertex groups/bone groups
    uint32_t num_colors;            // 0x10: Number of color values
    uint32_t num_normals;           // 0x14: Number of normals/additional data
    // Total: 0x18 (24) bytes

    SubmeshBB8_Header() = default;
    SubmeshBB8_Header(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }

    // Calculate total size of submesh data (indices + positions)
    uint32_t GetDataSize() const
    {
        return num_indices * 2 + num_vertices * 12;  // uint16 indices + float3 positions
    }
};
#pragma pack(pop)

static_assert(sizeof(SubmeshBB8_Header) == 0x18, "SubmeshBB8_Header must be 24 bytes!");

// Structure for inline ATEX texture data
struct InlineATEXTexture
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    char signature[4];      // "ATEX"
    char format[4];         // "DXT3", "DXTA", "DXT1", "DXT5", etc.
    uint16_t width;
    uint16_t height;
    uint32_t data_size;
    uint32_t mip_levels;
    std::vector<uint8_t> texture_data;
    int texture_index = -1; // Index for display purposes

    InlineATEXTexture() = default;

    InlineATEXTexture(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
                      bool& parsed_correctly, int index = -1)
    {
        texture_index = index;

        if (offset + 28 > data_size_bytes)
        {
            parsed_correctly = false;
            return;
        }

        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        std::memcpy(signature, &data[offset + 8], 4);
        std::memcpy(format, &data[offset + 12], 4);
        std::memcpy(&width, &data[offset + 16], sizeof(width));
        std::memcpy(&height, &data[offset + 18], sizeof(height));
        std::memcpy(&data_size, &data[offset + 20], sizeof(data_size));
        std::memcpy(&mip_levels, &data[offset + 24], sizeof(mip_levels));

        // Validate signature
        if (signature[0] != 'A' || signature[1] != 'T' ||
            signature[2] != 'E' || signature[3] != 'X')
        {
            parsed_correctly = false;
            return;
        }

        uint32_t texture_data_size = chunk_size - 16; // chunk_size minus ATEX header (16 bytes)
        if (offset + 8 + chunk_size > data_size_bytes)
        {
            parsed_correctly = false;
            return;
        }

        texture_data.resize(texture_data_size);
        std::memcpy(texture_data.data(), &data[offset + 24], texture_data_size);
    }

    // Get format as string
    std::string GetFormatString() const
    {
        return std::string(format, 4);
    }

    // Convert inline ATEX to DatTexture using existing ATEX processing
    DatTexture ToDatTexture() const
    {
        if (texture_data.empty() || width == 0 || height == 0)
        {
            return DatTexture();
        }

        // Build ATEX header + data for ProcessImageFile
        // ATEX header: signature(4) + format(4) + width(2) + height(2) + data_size(4) = 16 bytes
        std::vector<uint8_t> atex_data(16 + texture_data.size());

        // Copy header
        std::memcpy(&atex_data[0], signature, 4);
        std::memcpy(&atex_data[4], format, 4);
        std::memcpy(&atex_data[8], &width, 2);
        std::memcpy(&atex_data[10], &height, 2);
        std::memcpy(&atex_data[12], &data_size, 4);

        // Copy texture data
        std::memcpy(&atex_data[16], texture_data.data(), texture_data.size());

        return ProcessImageFile(atex_data.data(), static_cast<int>(atex_data.size()));
    }
};

// =============================================================================
// Geometry chunk for 0xBB8 format
// This implements parsing based on reverse engineering of the game's
// MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0 function
// =============================================================================
struct GeometryChunkOther
{
    uint32_t chunk_id = 0;
    uint32_t chunk_size = 0;
    ChunkBB8_Header header;
    std::vector<uint8_t> raw_geometry_data; // Raw geometry data for reference

    // Parsed geometry data
    std::vector<GeometryModel> models;
    TextureAndVertexShader tex_and_vertex_shader_struct;
    std::vector<UnknownTexStruct1> uts1;
    std::vector<uint8_t> unknown_tex_stuff1;

    // Texture group data (9 bytes each) - contains blend flags
    std::vector<TextureGroupOther> texture_groups;

    // Per-submesh texture indices (parsed from "bone indices" section)
    // Each inner vector contains the texture indices for that submesh
    std::vector<std::vector<uint8_t>> submesh_texture_indices;

    // BB8 bone palette: skeleton bone IDs indexed by bone group index
    // Extracted from BoneGroup structures at start of geometry data
    // For BB8, vertex.group directly indexes into this array to get skeleton bone ID
    std::vector<uint32_t> bb8_bone_palette;

    // Parsing status
    bool geometry_parsed = false;

    GeometryChunkOther() = default;

    GeometryChunkOther(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
                       bool& parsed_correctly)
    {
        if (offset + 8 > data_size_bytes)
        {
            parsed_correctly = false;
            return;
        }

        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));

        uint32_t curr_offset = offset + 8;

        // Validate chunk bounds
        if (curr_offset + chunk_size > data_size_bytes)
        {
            parsed_correctly = false;
            return;
        }

        // Read the header (0x30 = 48 bytes, NOT 64!)
        if (curr_offset + sizeof(ChunkBB8_Header) > data_size_bytes)
        {
            parsed_correctly = false;
            return;
        }

        header = ChunkBB8_Header(&data[curr_offset]);
        curr_offset += sizeof(ChunkBB8_Header);

        // Store remaining data as raw bytes for reference
        uint32_t remaining_size = chunk_size - sizeof(ChunkBB8_Header);
        if (remaining_size > 0 && curr_offset + remaining_size <= data_size_bytes)
        {
            raw_geometry_data.resize(remaining_size);
            std::memcpy(raw_geometry_data.data(), &data[curr_offset], remaining_size);
        }

        // Attempt to parse geometry based on classFlags
        char debug_msg[256];
        sprintf_s(debug_msg, "GeometryChunkOther: class_flags=0x%X, HasSubmeshData=%d, raw_data_size=%zu\n",
                  header.class_flags, header.HasSubmeshData() ? 1 : 0, raw_geometry_data.size());
        LogBB8Debug(debug_msg);

        if (header.HasSubmeshData() && !raw_geometry_data.empty())
        {
            geometry_parsed = ParseGeometryData(parsed_correctly);
            sprintf_s(debug_msg, "GeometryChunkOther: ParseGeometryData returned %d, models.size=%zu\n",
                      geometry_parsed ? 1 : 0, models.size());
            LogBB8Debug(debug_msg);
        }
    }

private:
    // Parse the raw geometry data based on the 0xBB8 format
    // Based on MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0 at 0x0076d380
    bool ParseGeometryData(bool& parsed_correctly)
    {
        if (raw_geometry_data.empty())
        {
            return false;
        }

        const unsigned char* data = raw_geometry_data.data();
        uint32_t data_size = static_cast<uint32_t>(raw_geometry_data.size());
        uint32_t curr_offset = 0;

        // Parse bone group data if present (0x002 flag)
        // Format: count DWORD + count * 28 bytes (7 DWORDs each)
        // BoneGroup structure (28 bytes):
        //   float offsetX, offsetY, offsetZ (12 bytes)
        //   u32 parentBoneIndex (4 bytes)
        //   u32 childCount (4 bytes)
        //   u32 flags (4 bytes)
        //   u32 boneId (4 bytes) - THIS IS THE SKELETON BONE INDEX!
        if (header.HasBoneGroups())
        {
            if (curr_offset + 4 > data_size) return false;
            uint32_t bone_group_count = *reinterpret_cast<const uint32_t*>(&data[curr_offset]);
            curr_offset += 4;

            char debug_msg[256];
            sprintf_s(debug_msg, "ParseGeometryData: Found %u bone groups (0x002 flag)\n", bone_group_count);
            LogBB8Debug(debug_msg);

            if (bone_group_count > 256) return false; // Reasonable limit
            uint32_t bone_group_size = bone_group_count * 28;
            if (curr_offset + bone_group_size > data_size) return false;

            // Extract boneId from each BoneGroup structure (at offset 24 within each 28-byte struct)
            bb8_bone_palette.clear();
            bb8_bone_palette.reserve(bone_group_count);
            for (uint32_t i = 0; i < bone_group_count; i++)
            {
                uint32_t group_offset = curr_offset + i * 28;
                uint32_t boneId = *reinterpret_cast<const uint32_t*>(&data[group_offset + 24]);
                bb8_bone_palette.push_back(boneId);

                if (i < 10)  // Log first 10 for debugging
                {
                    sprintf_s(debug_msg, "  BoneGroup[%u]: boneId=%u\n", i, boneId);
                    LogBB8Debug(debug_msg);
                }
            }
            if (bone_group_count > 10)
            {
                sprintf_s(debug_msg, "  ... and %u more bone groups\n", bone_group_count - 10);
                LogBB8Debug(debug_msg);
            }

            curr_offset += bone_group_size;
        }

        // Skip bone weight data if present (0x040 flag)
        // This has a complex structure handled by MdlDecomp_ConvertBoneWeights
        if (header.HasBoneWeights())
        {
            // For now, we skip this section - need more RE to parse correctly
            // The size depends on various header fields
        }

        // Parse texture group and material data based on header fields
        // This section exists if num_texture_groups (0x19) < 0xFF
        if (header.num_texture_groups < 0xFF && header.num_textures < 0x100 && header.num_materials < 0x100)
        {
            if (header.num_texture_groups > 0)
            {
                // Parse texture group data: num_texture_groups * 9 bytes each
                uint32_t tex_group_size = header.num_texture_groups * 9;
                if (curr_offset + tex_group_size > data_size) return false;

                texture_groups.resize(header.num_texture_groups);
                std::memcpy(texture_groups.data(), &data[curr_offset], tex_group_size);
                curr_offset += tex_group_size;

                // Log parsed texture groups for debugging
                char debug_msg[256];
                for (size_t i = 0; i < texture_groups.size(); i++)
                {
                    sprintf_s(debug_msg, "GeometryChunkOther: texture_group[%zu] num_textures_to_use=%u, f0x8=%u, f0x5=%u\n",
                              i, texture_groups[i].num_textures_to_use, texture_groups[i].f0x8, texture_groups[i].f0x5);
                    LogBB8Debug(debug_msg);
                }

                // Parse bone index data: (3 or 4) * num_bone_indices bytes
                // This section ALSO contains per-submesh texture indices at offset 8
                uint32_t bone_idx_multiplier = (header.num_bone_weight_sets != 0) ? 4 : 3;
                uint32_t bone_idx_size = bone_idx_multiplier * header.num_bone_indices;
                if (curr_offset + bone_idx_size > data_size) return false;

                // Extract per-submesh texture indices from this section
                // They are stored at offset 8 as uint8 pairs (2 textures per submesh)
                // num_texture_groups tells us how many submeshes there are
                if (bone_idx_size >= 8 + header.num_texture_groups * 2)
                {
                    uint32_t tex_idx_offset = curr_offset + 8;
                    submesh_texture_indices.resize(header.num_texture_groups);

                    for (uint8_t s = 0; s < header.num_texture_groups; s++)
                    {
                        // Each submesh has 2 texture indices
                        uint8_t t0 = data[tex_idx_offset + s * 2];
                        uint8_t t1 = data[tex_idx_offset + s * 2 + 1];
                        submesh_texture_indices[s].push_back(t0);
                        submesh_texture_indices[s].push_back(t1);

                        sprintf_s(debug_msg, "GeometryChunkOther: submesh[%u] texture_indices=[%u, %u]\n",
                                  s, t0, t1);
                        LogBB8Debug(debug_msg);
                    }
                }

                curr_offset += bone_idx_size;

                // Skip texture references: num_textures * 8 bytes
                uint32_t tex_ref_size = header.num_textures * 8;
                if (curr_offset + tex_ref_size > data_size) return false;
                curr_offset += tex_ref_size;

                // Skip texture names (variable length strings)
                for (uint16_t i = 0; i < header.num_textures; i++)
                {
                    while (curr_offset < data_size && data[curr_offset] != 0)
                        curr_offset++;
                    if (curr_offset < data_size) curr_offset++; // Skip null terminator
                }

                // Skip material data: num_materials * 8 bytes
                uint32_t mat_size = header.num_materials * 8;
                if (curr_offset + mat_size > data_size) return false;
                curr_offset += mat_size;
            }
        }

        // Skip vertex buffer metadata if present (0x020 flag)
        if (header.HasVertexBuffer())
        {
            if (curr_offset + 8 > data_size) return false;
            uint32_t vb_size = *reinterpret_cast<const uint32_t*>(&data[curr_offset]);
            curr_offset += 8; // Skip 2 DWORDs of header
            if (curr_offset + vb_size > data_size) return false;
            curr_offset += vb_size;
        }

        // Skip morph target data if present (0x080 flag)
        if (header.HasMorphTargets())
        {
            if (curr_offset + 12 > data_size) return false;
            // Skip 3 DWORDs + variable data - for now just skip
            curr_offset += 12;
            // TODO: Parse morph target data properly
        }

        // Parse submesh data (0x008 flag) - this is the main geometry
        if (header.HasSubmeshData())
        {
            return ParseSubmeshSection(data, data_size, curr_offset, parsed_correctly);
        }

        return false;
    }

    // Parse the submesh section by scanning for the submesh header pattern
    // Based on successful Python script analysis - scan at 2-byte boundaries
    bool ParseSubmeshSection(const unsigned char* data, uint32_t data_size,
                             uint32_t start_offset, bool& parsed_correctly)
    {
        // Scan for submesh header pattern (num_indices, num_vertices followed by valid data)
        // The Python script found headers at non-4-byte-aligned offsets (e.g., 0x1E = 30 bytes)
        return ScanForSubmeshHeader(data, data_size, parsed_correctly);
    }

    // Helper to check if a float value is valid for position data
    static bool IsValidPositionFloat(float f, float max_val = 500.0f)
    {
        return !std::isnan(f) && !std::isinf(f) && std::abs(f) <= max_val;
    }

    // Scan for submesh header pattern at 2-byte boundaries
    // This approach was validated with the Python script on real model files
    // Now scans for ALL submeshes, not just the first one
    bool ScanForSubmeshHeader(const unsigned char* data, uint32_t data_size, bool& parsed_correctly)
    {
        constexpr uint32_t SUBMESH_HEADER_SIZE = 24; // 6 DWORDs
        constexpr uint32_t MAX_SCAN_OFFSET = 300;    // Don't scan too far for FIRST submesh

        // Ensure we have enough data to scan
        if (data_size < 48)
        {
            LogBB8Debug("ScanForSubmeshHeader: data_size < 48, skipping\n");
            return false;
        }

        uint32_t scan_start = 0;
        uint32_t submesh_count = 0;
        bool found_any = false;

        // Loop to find ALL submeshes
        while (scan_start < data_size - 48)
        {
            uint32_t max_offset = (submesh_count == 0)
                ? std::min(data_size - 48, MAX_SCAN_OFFSET)
                : data_size - 48;  // After first, scan all remaining data

            bool found_submesh = false;

            // Scan at 1-byte boundaries - OLD format files can have headers at odd offsets
            // due to variable-size sections (e.g., 17-byte bone data section)
            for (uint32_t test_offset = scan_start; test_offset < max_offset; test_offset += 1)
            {
                if (test_offset + SUBMESH_HEADER_SIZE > data_size)
                    break;

                // Read potential submesh header values
                uint32_t num_indices = *reinterpret_cast<const uint32_t*>(&data[test_offset]);
                uint32_t num_vertices = *reinterpret_cast<const uint32_t*>(&data[test_offset + 4]);

                // Validate counts - indices should be >= vertices typically
                if (num_indices < 6 || num_indices > 100000) continue;
                if (num_vertices < 3 || num_vertices > 50000) continue;
                if (num_indices < num_vertices) continue; // Usually more indices than vertices

                // Calculate buffer positions
                uint32_t idx_start = test_offset + SUBMESH_HEADER_SIZE;
                uint32_t idx_size = num_indices * 2;
                uint32_t pos_start = idx_start + idx_size;
                uint32_t pos_size = num_vertices * 12;

                // Check if data fits
                if (pos_start + pos_size > data_size) continue;

                // Validate indices - all should be < num_vertices
                bool valid_indices = true;
                uint32_t unique_indices = 0;
                uint16_t last_idx = 0xFFFF;
                for (uint32_t i = 0; i < std::min(num_indices, 30u); i++)
                {
                    uint16_t idx = *reinterpret_cast<const uint16_t*>(&data[idx_start + i * 2]);
                    if (idx >= num_vertices)
                    {
                        valid_indices = false;
                        break;
                    }
                    if (idx != last_idx)
                    {
                        unique_indices++;
                        last_idx = idx;
                    }
                }

                if (!valid_indices || unique_indices < 3) continue;

                // Validate positions - should be reasonable floats
                bool valid_positions = true;
                uint32_t non_zero_positions = 0;
                for (uint32_t i = 0; i < std::min(num_vertices, 5u); i++)
                {
                    float x, y, z;
                    std::memcpy(&x, &data[pos_start + i * 12], sizeof(float));
                    std::memcpy(&y, &data[pos_start + i * 12 + 4], sizeof(float));
                    std::memcpy(&z, &data[pos_start + i * 12 + 8], sizeof(float));

                    if (!IsValidPositionFloat(x) || !IsValidPositionFloat(y) || !IsValidPositionFloat(z))
                    {
                        valid_positions = false;
                        break;
                    }

                    if (x != 0.0f || y != 0.0f || z != 0.0f)
                    {
                        non_zero_positions++;
                    }
                }

                if (!valid_positions) continue;
                if (non_zero_positions < 2) continue; // Most positions shouldn't be all zeros

                // Found valid structure - parse it
                char debug_msg[256];
                sprintf_s(debug_msg, "ScanForSubmeshHeader: FOUND submesh[%u] at offset 0x%X, indices=%u, vertices=%u\n",
                          submesh_count, test_offset, num_indices, num_vertices);
                LogBB8Debug(debug_msg);

                uint32_t submesh_end = 0;
                if (ParseSubmeshAtOffset(data, data_size, test_offset, num_indices, num_vertices,
                                         idx_start, pos_start, parsed_correctly, submesh_end))
                {
                    found_any = true;
                    found_submesh = true;
                    submesh_count++;

                    // Continue scanning from after this submesh
                    scan_start = submesh_end;
                    break;
                }
                else
                {
                    // Parsing failed, try next offset
                    continue;
                }
            }

            // If no submesh found in this scan range, we're done
            if (!found_submesh)
                break;
        }

        char debug_msg[256];
        sprintf_s(debug_msg, "ScanForSubmeshHeader: Found %u total submeshes\n", submesh_count);
        LogBB8Debug(debug_msg);

        return found_any;
    }

    // Parse submesh once we've found the header location
    // Returns the end offset of this submesh via submesh_end_out for continued scanning
    bool ParseSubmeshAtOffset(const unsigned char* data, uint32_t data_size,
                              uint32_t header_offset, uint32_t num_indices, uint32_t num_vertices,
                              uint32_t idx_start, uint32_t pos_start, bool& parsed_correctly,
                              uint32_t& submesh_end_out)
    {
        // Read additional header fields
        uint32_t num_uv_sets = *reinterpret_cast<const uint32_t*>(&data[header_offset + 8]);
        uint32_t num_vertex_groups = *reinterpret_cast<const uint32_t*>(&data[header_offset + 12]);
        uint32_t num_colors = *reinterpret_cast<const uint32_t*>(&data[header_offset + 16]);
        uint32_t num_normals = *reinterpret_cast<const uint32_t*>(&data[header_offset + 20]);

        char debug_msg[256];
        sprintf_s(debug_msg, "ParseSubmeshAtOffset: num_uv_sets=%u, num_vertex_groups=%u, num_colors=%u, num_normals=%u\n",
                  num_uv_sets, num_vertex_groups, num_colors, num_normals);
        LogBB8Debug(debug_msg);

        // Clamp UV sets to reasonable range
        if (num_uv_sets > 8) num_uv_sets = 1;

        // Read material/texture group index from 4 bytes before header
        // This tells us which texture indices to use for this submesh
        uint32_t material_index = 0;
        if (header_offset >= 4)
        {
            material_index = *reinterpret_cast<const uint32_t*>(&data[header_offset - 4]);
        }
        sprintf_s(debug_msg, "ParseSubmeshAtOffset: material_index=%u (from offset 0x%X)\n",
                  material_index, header_offset - 4);
        LogBB8Debug(debug_msg);

        GeometryModel model;
        model.unknown = material_index;  // Store material index for texture lookup
        model.num_indices0 = num_indices;
        model.num_indices1 = num_indices;
        model.num_indices2 = num_indices;
        model.num_vertices = num_vertices;
        model.total_num_indices = num_indices;
        model.dat_fvf = GR_FVF_POSITION;

        // Populate bone mapping fields from BB8 bone palette
        // For BB8, each bone group maps directly to a skeleton bone (group size = 1)
        // This makes the extra_data format compatible with ExtractBoneData()
        if (!bb8_bone_palette.empty())
        {
            uint32_t bone_group_count = static_cast<uint32_t>(bb8_bone_palette.size());
            model.u0 = bone_group_count;  // Number of bone groups
            model.u1 = bone_group_count;  // Total bone refs (= bone_group_count since each group has 1 bone)
            model.u2 = 0;  // No triangle groups

            // Build extra_data: [groupSizes...][skeletonBoneIndices...]
            // Format: u0 * 4 bytes of group sizes (all 1s) + u1 * 4 bytes of bone IDs
            model.extra_data.resize((model.u0 + model.u1) * 4);
            uint8_t* extra_ptr = model.extra_data.data();

            // Write group sizes (all 1, since BB8 uses direct mapping)
            for (uint32_t i = 0; i < bone_group_count; i++)
            {
                uint32_t one = 1;
                std::memcpy(extra_ptr + i * 4, &one, sizeof(uint32_t));
            }

            // Write skeleton bone indices
            for (uint32_t i = 0; i < bone_group_count; i++)
            {
                std::memcpy(extra_ptr + (bone_group_count + i) * 4, &bb8_bone_palette[i], sizeof(uint32_t));
            }

            sprintf_s(debug_msg, "ParseSubmeshAtOffset: Populated bone mapping from BB8 palette: u0=%u, u1=%u\n",
                      model.u0, model.u1);
            LogBB8Debug(debug_msg);
        }

        // Read all indices
        model.indices.resize(num_indices);
        for (uint32_t i = 0; i < num_indices; i++)
        {
            model.indices[i] = *reinterpret_cast<const uint16_t*>(&data[idx_start + i * 2]);
        }

        // Read all positions first
        model.vertices.resize(num_vertices);
        for (uint32_t i = 0; i < num_vertices; i++)
        {
            ModelVertex vertex(get_fvf(GR_FVF_POSITION | GR_FVF_GROUP), parsed_correctly, 16);
            vertex.has_position = true;
            vertex.has_group = true;  // BB8 format includes bone group indices

            float x, y, z;
            std::memcpy(&x, &data[pos_start + i * 12], sizeof(float));
            std::memcpy(&y, &data[pos_start + i * 12 + 4], sizeof(float));
            std::memcpy(&z, &data[pos_start + i * 12 + 8], sizeof(float));

            // GW uses different coordinate system - swap Y/Z and negate
            vertex.x = x;
            vertex.z = y;
            vertex.y = -z;

            // Update bounds
            model.minX = std::min(model.minX, vertex.x);
            model.maxX = std::max(model.maxX, vertex.x);
            model.minY = std::min(model.minY, vertex.y);
            model.maxY = std::max(model.maxY, vertex.y);
            model.minZ = std::min(model.minZ, vertex.z);
            model.maxZ = std::max(model.maxZ, vertex.z);
            model.sumX += vertex.x;
            model.sumY += vertex.y;
            model.sumZ += vertex.z;

            model.vertices[i] = vertex;
        }

        // After positions, there's per-vertex extra data (4 bytes each).
        // This section contains bone group index (1 byte) + other data (3 bytes).
        // IMPORTANT: Game's skinning (GrFvf_SkinXYZNormal) reads only 1 BYTE for bone index!
        constexpr float UV_SCALE = 1.0f / 65536.0f;
        uint32_t pos_end = pos_start + num_vertices * 12;
        uint32_t bone_group_data_start = pos_end;

        // Read bone group indices for each vertex (only first byte of each 4-byte entry)
        std::set<uint32_t> unique_groups;
        for (uint32_t i = 0; i < num_vertices; i++)
        {
            if (bone_group_data_start + (i + 1) * 4 <= data_size)
            {
                // Read only the first byte - game's skinning code reads a single byte
                uint8_t bone_group_idx = data[bone_group_data_start + i * 4];
                model.vertices[i].group = static_cast<uint32_t>(bone_group_idx);
                unique_groups.insert(bone_group_idx);
            }
            else
            {
                model.vertices[i].group = 0;
                unique_groups.insert(0);
            }
        }

        // Debug: log unique bone group indices found
        sprintf_s(debug_msg, "ParseSubmeshAtOffset: Found %zu unique bone groups. First few: ", unique_groups.size());
        std::string groups_str = debug_msg;
        int count = 0;
        for (uint32_t g : unique_groups)
        {
            if (count++ >= 10) { groups_str += "..."; break; }
            groups_str += std::to_string(g) + " ";
        }
        groups_str += "\n";
        LogBB8Debug(groups_str.c_str());

        // Per-vertex bone group data: num_vertices * 4 bytes
        uint32_t other_data_size = num_vertices * 4;
        uint32_t uv_section_start = pos_end + other_data_size;
        uint32_t num_uv_verts = num_uv_sets * num_vertices;

        sprintf_s(debug_msg, "ParseSubmeshAtOffset: pos_end=0x%X, skip %u bytes (other data), uv_section_start=0x%X\n",
                  pos_end, other_data_size, uv_section_start);
        LogBB8Debug(debug_msg);

        if (uv_section_start + num_uv_verts * 4 <= data_size)
        {
            // Read UV header: count0 (uint16) and count1 (uint16)
            uint16_t count0 = *reinterpret_cast<const uint16_t*>(&data[uv_section_start]);
            uint16_t count1 = *reinterpret_cast<const uint16_t*>(&data[uv_section_start + 2]);

            // Calculate offset array size: (count0 + count1) * 4 bytes
            // Layout: u_thresholds[count0] + v_thresholds[count1] + u_offsets[count0] + v_offsets[count1]
            uint32_t offset_array_size = (count0 + count1) * 4;
            uint32_t delta_data_start = uv_section_start + 4 + offset_array_size;

            // Validate: check if this format makes sense (small counts, data fits)
            bool use_delta_offset_format = (count0 < 256 && count1 < 256) &&
                                           (delta_data_start + num_uv_verts * 4 <= data_size);

            sprintf_s(debug_msg, "ParseSubmeshAtOffset: count0=%u, count1=%u, offset_array_size=%u, delta_start=0x%X, use_delta=%d\n",
                      count0, count1, offset_array_size, delta_data_start, use_delta_offset_format ? 1 : 0);
            LogBB8Debug(debug_msg);

            if (use_delta_offset_format)
            {
                // Delta+offset format from Ghidra (MdlDecomp_ConvertSubmesh)
                // UV header: count0 (uint16), count1 (uint16)
                // Then arrays: u_thresholds[count0], v_thresholds[count1], u_offsets[count0], v_offsets[count1]
                // Then delta data: (u_delta, v_delta) pairs for each vertex

                uint32_t offset_array_start = uv_section_start + 4;

                const uint16_t* u_thresholds = reinterpret_cast<const uint16_t*>(&data[offset_array_start]);
                const uint16_t* v_thresholds = reinterpret_cast<const uint16_t*>(&data[offset_array_start + count0 * 2]);
                const int16_t* u_offsets = reinterpret_cast<const int16_t*>(&data[offset_array_start + (count0 + count1) * 2]);
                const int16_t* v_offsets = reinterpret_cast<const int16_t*>(&data[offset_array_start + (count0 + count1) * 2 + count0 * 2]);

                LogBB8Debug("  Using DELTA+OFFSET UV format\n");

                for (uint32_t uv_set = 0; uv_set < num_uv_sets && uv_set < 8; uv_set++)
                {
                    uint32_t u_offset_idx = 0;
                    uint32_t v_offset_idx = 0;
                    uint32_t u_counter = 0;
                    uint32_t v_counter = 0;

                    for (uint32_t i = 0; i < num_vertices; i++)
                    {
                        uint32_t delta_offset = delta_data_start + (uv_set * num_vertices + i) * 4;
                        uint16_t u_delta = *reinterpret_cast<const uint16_t*>(&data[delta_offset]);
                        uint16_t v_delta = *reinterpret_cast<const uint16_t*>(&data[delta_offset + 2]);

                        int16_t u_off = (count0 > 0 && u_offset_idx < count0) ? u_offsets[u_offset_idx] : 0;
                        int16_t v_off = (count1 > 0 && v_offset_idx < count1) ? v_offsets[v_offset_idx] : 0;

                        float u = static_cast<float>(u_delta) * UV_SCALE + static_cast<float>(u_off);
                        float v = static_cast<float>(v_delta) * UV_SCALE + static_cast<float>(v_off);

                        model.vertices[i].tex_coord[uv_set][0] = u;
                        model.vertices[i].tex_coord[uv_set][1] = v;
                        model.vertices[i].has_tex_coord[uv_set] = true;
                        if (uv_set + 1 > model.vertices[i].num_texcoords)
                            model.vertices[i].num_texcoords = static_cast<uint8_t>(uv_set + 1);

                        if (i < 5 && uv_set == 0)
                        {
                            char uv_debug[192];
                            sprintf_s(uv_debug, "  Vertex %u UV%u: delta(%u,%u) offset(%d,%d) -> UV(%.4f,%.4f)\n",
                                      i, uv_set, u_delta, v_delta, u_off, v_off, u, v);
                            LogBB8Debug(uv_debug);
                        }

                        // Advance offset indices based on thresholds
                        u_counter++;
                        v_counter++;
                        if (count0 > 0 && u_offset_idx < count0 && u_thresholds[u_offset_idx] == u_counter)
                        {
                            u_offset_idx++;
                            u_counter = 0; // Reset on match
                        }
                        if (count1 > 0 && v_offset_idx < count1 && v_thresholds[v_offset_idx] == v_counter)
                        {
                            v_offset_idx++;
                            v_counter = 0; // Reset on match
                        }
                    }
                }
            }
            else
            {
                // Direct UV format: uint16 pairs normalized by /65536
                LogBB8Debug("  Using DIRECT UV format (uint16 pairs / 65536)\n");

                for (uint32_t uv_set = 0; uv_set < num_uv_sets && uv_set < 8; uv_set++)
                {
                    for (uint32_t i = 0; i < num_vertices; i++)
                    {
                        uint32_t uv_offset = uv_section_start + (uv_set * num_vertices + i) * 4;
                        uint16_t u_raw = *reinterpret_cast<const uint16_t*>(&data[uv_offset]);
                        uint16_t v_raw = *reinterpret_cast<const uint16_t*>(&data[uv_offset + 2]);

                        float u = static_cast<float>(u_raw) * UV_SCALE;
                        float v = 1.0f - static_cast<float>(v_raw) * UV_SCALE;  // Flip V for coordinate system

                        model.vertices[i].tex_coord[uv_set][0] = u;
                        model.vertices[i].tex_coord[uv_set][1] = v;
                        model.vertices[i].has_tex_coord[uv_set] = true;
                        if (uv_set + 1 > model.vertices[i].num_texcoords)
                            model.vertices[i].num_texcoords = static_cast<uint8_t>(uv_set + 1);

                        if (i < 5 && uv_set == 0)
                        {
                            char uv_debug[128];
                            sprintf_s(uv_debug, "  Vertex %u UV%u: raw(%u,%u) -> UV(%.4f,%.4f)\n",
                                      i, uv_set, u_raw, v_raw, u, v);
                            LogBB8Debug(uv_debug);
                        }
                    }
                }
            }
        }
        else
        {
            sprintf_s(debug_msg, "ParseSubmeshAtOffset: Not enough data for UVs (need 0x%X, have %u)\n",
                      uv_section_start + num_uv_verts * 4, data_size);
            LogBB8Debug(debug_msg);

            // Fallback: set default UVs
            for (uint32_t i = 0; i < num_vertices; i++)
            {
                model.vertices[i].tex_coord[0][0] = 0.0f;
                model.vertices[i].tex_coord[0][1] = 0.0f;
                model.vertices[i].has_tex_coord[0] = true;
                model.vertices[i].num_texcoords = 1;
            }
        }

        // Calculate averages
        if (num_vertices > 0)
        {
            model.avgX = model.sumX / num_vertices;
            model.avgY = model.sumY / num_vertices;
            model.avgZ = model.sumZ / num_vertices;
        }

        // Calculate submesh end offset for multi-submesh scanning
        // Layout: header(24) + indices(num_indices*2) + positions(num_vertices*12) + other_data(num_vertices*4) + UV section
        uint32_t pos_end_calc = pos_start + num_vertices * 12;
        uint32_t other_data_calc = num_vertices * 4;
        uint32_t uv_start_calc = pos_end_calc + other_data_calc;

        // Check if we have the UV section
        if (uv_start_calc + 4 <= data_size)
        {
            uint16_t cnt0 = *reinterpret_cast<const uint16_t*>(&data[uv_start_calc]);
            uint16_t cnt1 = *reinterpret_cast<const uint16_t*>(&data[uv_start_calc + 2]);

            // Validate counts are reasonable
            if (cnt0 < 256 && cnt1 < 256)
            {
                uint32_t offset_arr_sz = (cnt0 + cnt1) * 4;
                uint32_t delta_start_calc = uv_start_calc + 4 + offset_arr_sz;
                uint32_t uv_data_size = num_uv_sets * num_vertices * 4;
                submesh_end_out = delta_start_calc + uv_data_size;
            }
            else
            {
                // Direct UV format: just num_uv_sets * num_vertices * 4 bytes of UV data
                submesh_end_out = uv_start_calc + num_uv_sets * num_vertices * 4;
            }
        }
        else
        {
            // No UV section, end after positions + other data
            submesh_end_out = uv_start_calc;
        }

        sprintf_s(debug_msg, "ParseSubmeshAtOffset: parsed %u vertices with %u UV sets, submesh_end=0x%X\n",
                  num_vertices, num_uv_sets, submesh_end_out);
        LogBB8Debug(debug_msg);

        models.push_back(model);
        return true;
    }
};

struct FFNA_ModelFile_Other
{
    char ffna_signature[4];
    FFNAType ffna_type;
    GeometryChunkOther geometry_chunk;
    TextureFileNamesChunkOther texture_filenames_chunk;
    std::vector<InlineATEXTexture> inline_textures;

    bool parsed_correctly = true;
    bool textures_parsed_correctly = true;
    bool has_inline_textures = false;
    bool geometry_format_unsupported = true; // Always true for "other" format

    std::unordered_map<uint32_t, int> riff_chunks;
    std::unordered_set<int> seen_model_ids;

    FFNA_ModelFile_Other() = default;

    FFNA_ModelFile_Other(int offset, std::span<unsigned char>& data)
    {
        uint32_t current_offset = offset;

        if (data.size() < 5)
        {
            parsed_correctly = false;
            return;
        }

        std::memcpy(ffna_signature, &data[offset], sizeof(ffna_signature));
        current_offset += 4;
        std::memcpy(&ffna_type, &data[current_offset], sizeof(ffna_type));
        current_offset += 1;

        // Read all chunks
        while (current_offset + 8 <= data.size())
        {
            GeneralChunk chunk(current_offset, data.data());

            // Validate chunk
            if (chunk.chunk_size == 0 || current_offset + 8 + chunk.chunk_size > data.size())
            {
                break;
            }

            // Store chunk location (only store first occurrence of each chunk type)
            if (riff_chunks.find(chunk.chunk_id) == riff_chunks.end())
            {
                riff_chunks.emplace(chunk.chunk_id, current_offset);
            }

            // Move to the next chunk
            current_offset += 8 + chunk.chunk_size;
        }

        // Parse geometry chunk (0xBB8) - limited parsing
        auto it = riff_chunks.find(CHUNK_ID_GEOMETRY_OTHER);
        if (it != riff_chunks.end())
        {
            int chunk_offset = it->second;
            geometry_chunk = GeometryChunkOther(chunk_offset, data.data(),
                                                static_cast<uint32_t>(data.size_bytes()), parsed_correctly);
        }

        // Parse texture filenames chunk (0xBBB or 0xBBC)
        it = riff_chunks.find(CHUNK_ID_TEXTURE_FILENAMES_OTHER);
        if (it != riff_chunks.end())
        {
            int chunk_offset = it->second;
            texture_filenames_chunk =
                TextureFileNamesChunkOther(chunk_offset, data.data(),
                                      static_cast<uint32_t>(data.size_bytes()), textures_parsed_correctly);
        }
        else
        {
            it = riff_chunks.find(CHUNK_ID_ADDITIONAL_FILENAMES_OTHER);
            if (it != riff_chunks.end())
            {
                int chunk_offset = it->second;
                texture_filenames_chunk =
                    TextureFileNamesChunkOther(chunk_offset, data.data(),
                                          static_cast<uint32_t>(data.size_bytes()), textures_parsed_correctly);
            }
        }

        // Parse ALL inline ATEX textures (0xFA3 and 0xFAA)
        // These can appear multiple times, so we need to scan all chunks
        int tex_index = 0;
        current_offset = offset + 5; // Reset to start of chunks
        while (current_offset + 8 <= data.size())
        {
            uint32_t chunk_id = *reinterpret_cast<const uint32_t*>(&data[current_offset]);
            uint32_t chunk_size = *reinterpret_cast<const uint32_t*>(&data[current_offset + 4]);

            if (chunk_size == 0 || current_offset + 8 + chunk_size > data.size())
            {
                break;
            }

            if (chunk_id == CHUNK_ID_INLINE_ATEX_DXT3 || chunk_id == CHUNK_ID_INLINE_ATEX_DXTA)
            {
                bool tex_parsed = true;
                InlineATEXTexture inline_tex(current_offset, data.data(),
                                             static_cast<uint32_t>(data.size_bytes()), tex_parsed, tex_index);
                if (tex_parsed && inline_tex.width > 0 && inline_tex.height > 0)
                {
                    inline_textures.push_back(inline_tex);
                    has_inline_textures = true;
                    tex_index++;
                }
            }

            current_offset += 8 + chunk_size;
        }
    }

    // Check if this file uses the "other" format (0xBB* chunks)
    bool IsOtherFormat() const
    {
        return riff_chunks.contains(CHUNK_ID_GEOMETRY_OTHER);
    }

    // Get the number of inline textures
    size_t GetInlineTextureCount() const
    {
        return inline_textures.size();
    }

    // Get a specific inline texture as DatTexture
    DatTexture GetInlineTexture(size_t index) const
    {
        if (index < inline_textures.size())
        {
            return inline_textures[index].ToDatTexture();
        }
        return DatTexture();
    }

    // Get all inline textures as DatTextures
    std::vector<DatTexture> GetAllInlineTextures() const
    {
        std::vector<DatTexture> textures;
        textures.reserve(inline_textures.size());
        for (const auto& tex : inline_textures)
        {
            DatTexture dat_tex = tex.ToDatTexture();
            if (dat_tex.width > 0 && dat_tex.height > 0)
            {
                textures.push_back(dat_tex);
            }
        }
        return textures;
    }

    // GetMesh returns a mesh from parsed geometry data if available
    Mesh GetMesh(int model_index, AMAT_file& amat_file)
    {
        char debug_msg[256];

        // Check if we have parsed geometry
        if (!geometry_chunk.geometry_parsed || geometry_chunk.models.empty())
        {
            sprintf_s(debug_msg, "GetMesh: No parsed geometry (parsed=%d, models=%zu)\n",
                      geometry_chunk.geometry_parsed ? 1 : 0, geometry_chunk.models.size());
            LogBB8Debug(debug_msg);
            return Mesh();
        }

        // Validate model index
        if (model_index < 0 || model_index >= static_cast<int>(geometry_chunk.models.size()))
        {
            sprintf_s(debug_msg, "GetMesh: Invalid model_index=%d (models=%zu)\n",
                      model_index, geometry_chunk.models.size());
            LogBB8Debug(debug_msg);
            return Mesh();
        }

        const auto& sub_model = geometry_chunk.models[model_index];
        sprintf_s(debug_msg, "GetMesh: model_index=%d, vertices=%zu, indices=%zu\n",
                  model_index, sub_model.vertices.size(), sub_model.indices.size());
        LogBB8Debug(debug_msg);

        std::vector<GWVertex> vertices;
        std::vector<uint32_t> indices;

        // Build vertex list
        for (size_t i = 0; i < sub_model.vertices.size(); i++)
        {
            const ModelVertex& model_vertex = sub_model.vertices[i];
            GWVertex vertex;

            if (!model_vertex.has_position)
            {
                return Mesh();
            }

            vertex.position = XMFLOAT3(model_vertex.x, model_vertex.y, model_vertex.z);

            if (model_vertex.has_normal)
            {
                vertex.normal = XMFLOAT3(model_vertex.normal_x, model_vertex.normal_y, model_vertex.normal_z);
            }

            if (model_vertex.has_tangent)
            {
                vertex.tangent = XMFLOAT3(model_vertex.tangent_x, model_vertex.tangent_y, model_vertex.tangent_z);
            }

            if (model_vertex.has_bitangent)
            {
                vertex.bitangent = XMFLOAT3(model_vertex.bitangent_x, model_vertex.bitangent_y, model_vertex.bitangent_z);
            }

            // Copy all UV coordinates
            if (model_vertex.has_tex_coord[0])
                vertex.tex_coord0 = XMFLOAT2(model_vertex.tex_coord[0][0], model_vertex.tex_coord[0][1]);
            if (model_vertex.has_tex_coord[1])
                vertex.tex_coord1 = XMFLOAT2(model_vertex.tex_coord[1][0], model_vertex.tex_coord[1][1]);
            if (model_vertex.has_tex_coord[2])
                vertex.tex_coord2 = XMFLOAT2(model_vertex.tex_coord[2][0], model_vertex.tex_coord[2][1]);
            if (model_vertex.has_tex_coord[3])
                vertex.tex_coord3 = XMFLOAT2(model_vertex.tex_coord[3][0], model_vertex.tex_coord[3][1]);
            if (model_vertex.has_tex_coord[4])
                vertex.tex_coord4 = XMFLOAT2(model_vertex.tex_coord[4][0], model_vertex.tex_coord[4][1]);
            if (model_vertex.has_tex_coord[5])
                vertex.tex_coord5 = XMFLOAT2(model_vertex.tex_coord[5][0], model_vertex.tex_coord[5][1]);
            if (model_vertex.has_tex_coord[6])
                vertex.tex_coord6 = XMFLOAT2(model_vertex.tex_coord[6][0], model_vertex.tex_coord[6][1]);
            if (model_vertex.has_tex_coord[7])
                vertex.tex_coord7 = XMFLOAT2(model_vertex.tex_coord[7][0], model_vertex.tex_coord[7][1]);

            // Log first few vertices' UVs
            if (i < 3)
            {
                sprintf_s(debug_msg, "GetMesh: vertex[%d] has_uv0=%d UV0=(%.4f,%.4f) tex_coord0=(%.4f,%.4f)\n",
                    i, model_vertex.has_tex_coord[0] ? 1 : 0,
                    model_vertex.tex_coord[0][0], model_vertex.tex_coord[0][1],
                    vertex.tex_coord0.x, vertex.tex_coord0.y);
                LogBB8Debug(debug_msg);
            }

            vertices.push_back(vertex);
        }

        // Build index list
        for (uint32_t i = 0; i < sub_model.num_indices0 && i < sub_model.indices.size(); i += 3)
        {
            if (i + 2 >= sub_model.indices.size())
            {
                break;
            }

            uint32_t index0 = sub_model.indices[i];
            uint32_t index1 = sub_model.indices[i + 1];
            uint32_t index2 = sub_model.indices[i + 2];

            if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
            {
                continue;
            }

            indices.push_back(index0);
            indices.push_back(index1);
            indices.push_back(index2);
        }

        // Use same indices for all LOD levels since we don't have LOD info
        std::vector<uint32_t> indices1 = indices;
        std::vector<uint32_t> indices2 = indices;

        // Set up texture pairs based on parsed submesh_texture_indices
        std::vector<uint8_t> uv_coords_indices;
        std::vector<uint8_t> tex_indices;
        std::vector<uint8_t> blend_flags;
        std::vector<uint16_t> texture_types;

        // Get total number of textures from filename chunk
        size_t total_textures = texture_filenames_chunk.texture_filenames.size();
        uint8_t num_vertex_uvs = sub_model.vertices.empty() ? 1 : sub_model.vertices[0].num_texcoords;

        // Use material index from sub_model.unknown to look up correct texture group
        uint32_t material_index = sub_model.unknown;

        sprintf_s(debug_msg, "GetMesh: model_index=%d, material_index=%u, total_textures=%zu, num_vertex_uvs=%u, submesh_texture_indices.size=%zu\n",
                  model_index, material_index, total_textures, num_vertex_uvs, geometry_chunk.submesh_texture_indices.size());
        LogBB8Debug(debug_msg);

        // Determine blend_flag based on format:
        // - MODERN format (with texture_groups): blend_flag defaults to 0 (opaque)
        // - OLD format (without texture_groups): blend_flag defaults to 8 (alpha blend)
        // Note: In MODERN format (uts1), byte[8] is f0x8, NOT the blend_flag!
        // The blend_flag for modern format comes from AMAT files, not texture_groups.
        uint8_t blend_flag;
        BlendState blend_state;

        if (geometry_chunk.texture_groups.empty())
        {
            // OLD format: default to alpha blend (8)
            blend_flag = 8;
            blend_state = BlendState::AlphaBlend;
            sprintf_s(debug_msg, "GetMesh: OLD format, using default blend_flag=8 (alpha blend)\n");
            LogBB8Debug(debug_msg);
        }
        else
        {
            // MODERN format: default to opaque (0)
            blend_flag = 0;
            blend_state = BlendState::Opaque;
            sprintf_s(debug_msg, "GetMesh: MODERN format, using default blend_flag=0 (opaque)\n");
            LogBB8Debug(debug_msg);
        }

        // Use the parsed per-submesh texture indices, indexed by material_index
        if (material_index < geometry_chunk.submesh_texture_indices.size())
        {
            const auto& this_submesh_textures = geometry_chunk.submesh_texture_indices[material_index];

            sprintf_s(debug_msg, "GetMesh: submesh[%d] material_index=%u has %zu texture indices, blend_flag=%u\n",
                      model_index, material_index, this_submesh_textures.size(), blend_flag);
            LogBB8Debug(debug_msg);

            for (size_t i = 0; i < this_submesh_textures.size(); i++)
            {
                uint8_t global_tex_idx = this_submesh_textures[i];

                // Validate texture index
                if (global_tex_idx >= total_textures)
                {
                    sprintf_s(debug_msg, "GetMesh: WARNING submesh[%d] tex[%zu]=%u >= total_textures=%zu, clamping\n",
                              model_index, i, global_tex_idx, total_textures);
                    LogBB8Debug(debug_msg);
                    global_tex_idx = 0;
                }

                uint8_t uv_set_index = static_cast<uint8_t>(i % num_vertex_uvs);
                uv_coords_indices.push_back(uv_set_index);
                tex_indices.push_back(global_tex_idx);

                uint8_t tex_blend_flag = (i == 0) ? blend_flag : 7;
                blend_flags.push_back(tex_blend_flag);
                texture_types.push_back(0xFFFF);

                sprintf_s(debug_msg, "GetMesh: submesh[%d] tex_pair[%zu] global_tex_index=%u, uv_set=%u, blend_flag=%u\n",
                          model_index, i, global_tex_idx, uv_set_index, tex_blend_flag);
                LogBB8Debug(debug_msg);
            }
        }
        else
        {
            // Fallback: no parsed texture indices for this material_index
            size_t num_textures_to_use = std::min(static_cast<size_t>(num_vertex_uvs), total_textures);

            sprintf_s(debug_msg, "GetMesh: submesh[%d] material_index=%u has no parsed texture indices, using %zu textures (num_vertex_uvs=%u, total=%zu)\n",
                      model_index, material_index, num_textures_to_use, num_vertex_uvs, total_textures);
            LogBB8Debug(debug_msg);

            for (size_t i = 0; i < num_textures_to_use; i++)
            {
                uint8_t uv_set_index = static_cast<uint8_t>(i);
                uv_coords_indices.push_back(uv_set_index);
                tex_indices.push_back(static_cast<uint8_t>(i));
                uint8_t tex_blend_flag = (i == 0) ? blend_flag : 7;
                blend_flags.push_back(tex_blend_flag);
                texture_types.push_back(0xFFFF);
            }
        }

        // If no textures for this submesh, add a placeholder pair
        if (tex_indices.empty())
        {
            uv_coords_indices.push_back(0);
            tex_indices.push_back(0);
            blend_flags.push_back(blend_flag);
            texture_types.push_back(0xFFFF);
        }

        sprintf_s(debug_msg, "GetMesh: Final mesh vertices=%zu, indices=%zu, tex_pairs=%zu\n",
                  vertices.size(), indices.size(), tex_indices.size());
        LogBB8Debug(debug_msg);

        return Mesh(vertices, indices, indices1, indices2, uv_coords_indices, tex_indices,
                    blend_flags, texture_types, false, blend_state,
                    static_cast<int>(tex_indices.size()));
    }
};

// Utility function to check if a file uses the "other" model format
inline bool IsOtherModelFormat(std::span<unsigned char>& data)
{
    if (data.size() < 13)
    {
        return false;
    }

    // Check FFNA signature and type
    if (data[0] != 'f' || data[1] != 'f' || data[2] != 'n' || data[3] != 'a')
    {
        return false;
    }

    if (data[4] != 2) // Must be type 2 (model file)
    {
        return false;
    }

    // Check first chunk ID - if it's 0xBB8, it's an "other" format
    uint32_t first_chunk_id = *reinterpret_cast<const uint32_t*>(&data[5]);
    return first_chunk_id == CHUNK_ID_GEOMETRY_OTHER;
}
