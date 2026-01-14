// =============================================================================
// FFNA Type 2 Model File - COMPLETE UNIFIED PATTERN
// Handles ALL chunk types: 0xFA* (runtime) and 0xBB* (DAT original)
//
// Reverse engineered from Gw.exe using Ghidra MCP:
//   - Model_ParseGeometryHeader (0x00769370)
//   - Model_ParseMaterialData (0x00768e00)
//   - Model_ParseBoneData (0x007691c0)
//   - Model_ParseShaderData (0x00769930)
//   - MdlDecomp_ConvertSubmesh (0x0076dbe0)
//   - MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0 (0x0076d380)
//   - MdlDecomp_ConvertGeometryChunk_0xBB9_to_0xFA1 (0x0076e780)
//   - MdlDecomp_ConvertAllChunks (0x00771090)
//   - MdlFvf_CalculateVertexFormat (0x0077b130)
//   - MdlFvf_BuildVertexBuffer (0x0077ae90)
// =============================================================================

#include <std/mem.pat>
#include <std/math.pat>
#include <std/io.pat>
#pragma pattern_limit 500000

// =============================================================================
// CHUNK ID REFERENCE
// =============================================================================
// Original DAT format (0xBB* range) -> Runtime format (0xFA* range):
//   0xBB8 (3000) -> 0xFA0 (4000): Geometry
//   0xBB9 (3001) -> 0xFA1 (4001): Animation/Skeleton
//   0xBBA (3002) -> 0xFA4 (4004): Texture references (16-byte entries)
//   0xBBB (3003) -> 0xFA5 (4005): Texture filenames
//   0xBBC (3004) -> 0xFA6 (4006): Additional filenames
//   0xBBE (3006) -> 0xFA9 (4009): Version/header data
//   0xBBF (3007) -> 0xFAB (4011): Index buffer data
//   0xBC0 (3008) -> 0xFAD (4013): Additional data
//   0xBC1 (3009) -> 0xFAE (4014): Additional data
//
// Special chunks (inline textures):
//   0xFA3 (4003): Inline ATEX DXT3 texture
//   0xFAA (4010): Inline ATEX DXTA texture
//   0xFAC (4012): File metadata

// =============================================================================
// FVF (Flexible Vertex Format) Lookup Tables
// From MdlFvf_ConvertToGrFVF and GetVertexSizeFromFVF
// =============================================================================

// Get position/weight component size based on FVF bits 8-11 and 12-15
fn get_fvf_pos_weight_size(u32 idx) {
    // fvf_array_0: { 0x0, 0x8, 0x8, 0x10, 0x8, 0x10, 0x10, 0x18,
    //                0x8, 0x10, 0x10, 0x18, 0x10, 0x18, 0x18, 0x20,
    //                0x0, 0x0, 0x0, 0x1, 0xFFFFFFFF, 0xFFFFFFFF }
    if (idx == 0) return 0x0;
    if (idx == 1) return 0x8;
    if (idx == 2) return 0x8;
    if (idx == 3) return 0x10;
    if (idx == 4) return 0x8;
    if (idx == 5) return 0x10;
    if (idx == 6) return 0x10;
    if (idx == 7) return 0x18;
    if (idx == 8) return 0x8;
    if (idx == 9) return 0x10;
    if (idx == 10) return 0x10;
    if (idx == 11) return 0x18;
    if (idx == 12) return 0x10;
    if (idx == 13) return 0x18;
    if (idx == 14) return 0x18;
    if (idx == 15) return 0x20;
    if (idx == 16) return 0x0;
    if (idx == 17) return 0x0;
    if (idx == 18) return 0x0;
    if (idx == 19) return 0x1;
    return 0xFFFFFFFF;
};

// Get normal component size based on FVF bits 4-6
fn get_fvf_normal_size(u32 idx) {
    // fvf_array_1: { 0x0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24 }
    if (idx == 0) return 0x0;
    if (idx == 1) return 0xC;
    if (idx == 2) return 0xC;
    if (idx == 3) return 0x18;
    if (idx == 4) return 0xC;
    if (idx == 5) return 0x18;
    if (idx == 6) return 0x18;
    if (idx == 7) return 0x24;
    return 0x0;
};

// Get UV component size based on FVF bits 0-3
fn get_fvf_uv_size(u32 idx) {
    // fvf_array_2: { 0x0, 0xC, 0x4, 0x10, 0xC, 0x18, 0x10, 0x1C,
    //                0x4, 0x10, 0x8, 0x14, 0x10, 0x1C, 0x14, 0x20 }
    if (idx == 0) return 0x0;
    if (idx == 1) return 0xC;
    if (idx == 2) return 0x4;
    if (idx == 3) return 0x10;
    if (idx == 4) return 0xC;
    if (idx == 5) return 0x18;
    if (idx == 6) return 0x10;
    if (idx == 7) return 0x1C;
    if (idx == 8) return 0x4;
    if (idx == 9) return 0x10;
    if (idx == 10) return 0x8;
    if (idx == 11) return 0x14;
    if (idx == 12) return 0x10;
    if (idx == 13) return 0x1C;
    if (idx == 14) return 0x14;
    if (idx == 15) return 0x20;
    return 0x0;
};

// Convert DAT FVF to DirectX FVF format
fn get_fvf(u32 dat_fvf) {
    return (dat_fvf & 0xff0) << 4 | dat_fvf >> 8 & 0x30 | dat_fvf & 0xf;
};

// Calculate vertex size from FVF flags
fn get_vertex_size_from_fvf(u32 fvf) {
    return get_fvf_pos_weight_size(fvf >> 0xc & 0xf) +
           get_fvf_pos_weight_size(fvf >> 8 & 0xf) +
           get_fvf_normal_size(fvf >> 4 & 7) +
           get_fvf_uv_size(fvf & 0xf);
};

fn compute_str_len_plus_one(u32 address) {
    u32 counter = 0;
    while (counter < 256) {
        u8 curr_char @ address + counter;
        if (curr_char == 0) return counter + 1;
        counter += 1;
    }
    return counter;
};

// =============================================================================
// ClassFlags Bitmask (offset 0x08 in geometry headers)
// From MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0
// =============================================================================
bitfield ClassFlags {
    HAS_SKELETON : 1;          // 0x001: Has skeleton reference
    HAS_BONE_GROUPS : 1;       // 0x002: Bone group data (max 4, 28 bytes each)
    HAS_BOUNDING_DATA : 1;     // 0x004: Bounding box/sphere data
    HAS_SUBMESH_DATA : 1;      // 0x008: Submesh definitions present
    HAS_LOD_DATA : 1;          // 0x010: Level-of-detail data
    HAS_GEOSET_DATA : 1;       // 0x020: Geometry set/vertex buffer data
    HAS_BONE_WEIGHTS : 1;      // 0x040: Per-vertex bone weights
    HAS_MORPH_DATA : 1;        // 0x080: Morph target/attachment data
    HAS_ANIMATION_DATA : 1;    // 0x100: Animation sequence data
    HAS_SKELETON_DATA : 1;     // 0x200: Skeleton/joint hierarchy (max 32 bones, 255 weights)
    HAS_EXTENDED_LOD : 1;      // 0x400: Extended LOD data
    HAS_CLOTH_DATA : 1;        // 0x800: Cloth simulation data
    HAS_SCALE_DATA : 1;        // 0x1000: Scale transform data
    RESERVED : 19;
};

// =============================================================================
// String Type (null-terminated)
// =============================================================================
struct String {
    char value[compute_str_len_plus_one($)];
};

// =============================================================================
// UV Compression Notes (from MdlDecomp_ConvertSubmesh at 0x0076dbe0)
// =============================================================================
// Two vertex data formats exist based on textureGroupCount (offset 0x19 in BB8):
//
// OLD format (textureGroupCount == 0):
//   - UV data (uint16 pairs) comes immediately after positions
//   - UV values are normalized: final_uv = raw_uv / 65536.0
//
// MODERN format (textureGroupCount > 0):
//   - After positions: per-vertex extra data (4 bytes each - colors/weights)
//   - UV uses delta+offset compression
//   - UV Header: count0 (u16), count1 (u16)
//   - Offset arrays: u_thresholds[count0], v_thresholds[count1],
//                    u_offsets[count0], v_offsets[count1]
//   - Delta data: (u_delta, v_delta) uint16 pairs per vertex
//   - Final UV = delta * (1/65536) + offset
//
// Scale constant at 0x00a399b8: 0x37800000 = 1.0f/65536.0f

// =============================================================================
// COMMON STRUCTURES
// =============================================================================

// Bone Info Structure (28 bytes = 0x1C)
struct BoneInfo {
    float offsetX;
    float offsetY;
    float offsetZ;
    u32 parentBoneIndex;
    u32 childCount;
    u32 flags;
    u32 boneId;
};

// Shader Definition (8 bytes) - From Model_ParseShaderData at 0x00769930
struct ShaderDefinition {
    u8 cullMode;        // 0=back-face cull, 1=no culling, 2=front-face cull
    u8 blendFlags;
    u32 shaderFlags;
    u8 pixelShaderId;
    u8 uvChannelCount;
};

// Material Definition (9 bytes) - From Model_ParseMaterialData at 0x00768e00
struct MaterialDefinition {
    u16 renderFlags;        // Render state (0x60A -> 0x622 indicates transform)
    u16 blendFlags;         // 0x600 = standard blend
    u8 textureStageIndex;
    u8 alphaTestRef;        // Alpha test reference (0-255)
    u8 sortPriority;
    u8 lodBias;
    u8 uvChannel;
};

// Texture Filename Entry (6 bytes)
// Hash decode: file_hash = (id0 - 0xFF00FF) + (id1 * 0xFF00)
struct TextureFilenameEntry {
    u16 id0;
    u16 id1;
    u16 unknown;
};

// Attachment Point (48 bytes = 0x30) - From Model_ParseAttachmentPoints
struct AttachmentPoint {
    float positionX, positionY, positionZ;
    float rotationX, rotationY, rotationZ, rotationW;
    float scaleX, scaleY, scaleZ;
    s32 parentBoneIndex;
    u32 attachmentType;
    u32 reserved[2];
};

// Particle Emitter (88 bytes = 0x58) - From Model_ParseEmitterData
struct ParticleEmitter {
    u32 emitterType;
    s32 parentBoneIndex;
    u32 unknown_0x08;
    float positionX, positionY, positionZ;
    float directionX, directionY, directionZ;
    float startSize, endSize;
    float startSpeed, endSpeed;
    float lifetime;
    float emissionRate;
    u8 startColorR, startColorG, startColorB, startColorA;
    u8 endColorR, endColorG, endColorB, endColorA;
    u32 textureIndex;
    u32 blendMode;
    u32 unknown_0x50;
    u8 particlesPerEmission;
    u8 reserved[3];
};

// ATEX Inline Texture Header
struct ATEXHeader {
    char signature[4];  // "ATEX"
    char format[4];     // "DXT3", "DXTA", "DXT1", "DXT5", etc.
    u16 width;
    u16 height;
    u32 dataSize;
    u32 mipLevelCount;
};

// =============================================================================
// 0xFA* FORMAT STRUCTURES (Runtime/Converted Format)
// =============================================================================

// Geometry Header for 0xFA0 chunks (84 bytes = 0x54)
struct GeometryHeader_FA0 {
    u32 modelVersion;           // 0x00: Usually 0x26 for standard models
    u32 fileId;                 // 0x04: File ID in DAT archive
    ClassFlags classFlags;      // 0x08: Data section presence flags
    u32 boundingBoxId;          // 0x0C
    u32 collisionMeshId;        // 0x10
    u8 lodDistanceScale;        // 0x14
    u8 unknown_0x15;
    u8 unknown_0x16;
    u8 unknown_0x17;
    u8 shaderCount;             // 0x18: Number of shader definitions
    u8 materialBaseCount;       // 0x19
    u16 textureRefCount;        // 0x1A
    u8 shaderParamCount;        // 0x1C: Shader parameter count per shader
    u8 materialConstCount;      // 0x1D
    u16 textureRangeCount;      // 0x1E
    u32 uvSetFlags;             // 0x20
    u32 vertexGroupCount;       // 0x24
    u32 colorDataCount;         // 0x28
    u32 normalDataCount;        // 0x2C
    u8 boneInfoCount;           // 0x30: Bone structs (28 bytes each)
    u8 padding_0x31[3];
    u32 emitterDataSize;        // 0x34
    float boundingSphereX;      // 0x38
    float boundingSphereY;
    float boundingSphereZ;
    float boundingSphereRadius;
    u32 frameDataCount;         // 0x48
    u32 submeshCount;           // 0x4C
    u16 collisionSubmeshCount;  // 0x50
    u16 boneDataEntryCount;     // 0x52
    u16 attachmentPointCount;   // (overlaps with padding)
};

// Animation/Extended Geometry Header for 0xFA1 chunks
struct GeometryHeader_FA1 {
    u32 modelVersion;
    u32 fileId;
    ClassFlags classFlags;
    u32 boundingBoxId;
    u32 collisionMeshId;
    u32 boundingCylinderCount;  // 0x14
    u8 sequenceKeyframeCountLo; // 0x18
    u8 sequenceKeyframeCountHi;
    u16 unknown_0x1A;
    u8 unknown_0x1C;
    u8 unknown_0x1D;
    u8 unknown_0x1E;
    u8 unknown_0x1F;
    float boundingSphereX;
    float boundingSphereY;
    float boundingSphereZ;
    float boundingSphereRadius;
    u32 animationCount;         // 0x30
    u32 transformDataSize;      // 0x34
    u32 skeletonNodeCount;      // 0x38
    u32 weightDataCount;
    u32 vertexGroupCount;
    u32 unknown_0x44;
    u32 submeshCount;           // 0x48
    u16 attachmentDataCount;
    u16 padding_0x4E;
    u16 boneDataCount;
    u16 additionalCount;
};

// Submesh Header for FA0 (36 bytes = 0x24)
struct SubmeshHeader_FA0 {
    u16 materialIndex;
    u16 submeshFlags;
    u32 indexCount;
    u32 indexBufferOffset;
    u32 vertexCount;
    u32 vertexFormat;       // FVF-style flags
    u32 uvSetCount;
    u32 vertexColorCount;
    u32 normalCount;
    u32 triangleGroupCount;
};

// FVF-based Vertex
struct Vertex<auto dat_fvf> {
    u32 FVF = get_fvf(dat_fvf);
    u32 vertex_size = get_vertex_size_from_fvf(FVF);
    u32 used_bytes = 0;

    if (FVF & 1) {
        used_bytes += 12;
        float positionX, positionY, positionZ;
    }
    if (FVF & 2) {
        used_bytes += 4;
        u32 vertexGroupIndex;
    }
    if (FVF & 4) {
        used_bytes += 12;
        float normalX, normalY, normalZ;
    }
    float additionalData[(vertex_size - used_bytes) / 4];
};

// =============================================================================
// 0xBB* FORMAT STRUCTURES (Original DAT Format)
// =============================================================================

// Geometry Header for 0xBB8 chunks (48 bytes = 0x30)
struct GeometryHeader_BB8 {
    u32 geometryFlags;          // 0x00
    u32 reserved_0x04;          // 0x04
    ClassFlags classFlags;      // 0x08
    u32 modelHash0;             // 0x0C
    u32 modelHash1;             // 0x10
    u32 unknown_0x14;           // 0x14
    u8 boneGroupCount;          // 0x18: Max 4, 28 bytes each
    u8 textureGroupCount;       // 0x19: CRITICAL - 0=OLD UV format, >0=MODERN
    u16 textureCount;           // 0x1A
    u8 maxBoneWeights;          // 0x1C
    u8 maxBoneIndices;          // 0x1D
    u16 materialCount;          // 0x1E
    u32 boneWeightSetCount;     // 0x20
    u32 outputFlags;            // 0x24
    float submeshScaleX;        // 0x28
    float submeshScaleY;        // 0x2C
};

// Animation/Skeleton Header for 0xBB9 chunks
struct AnimationHeader_BB9 {
    u32 typeMarker;     // Usually 0x26
    u32 fileId;
    u32 flags;          // Typically 0x10
    u32 modelHash0;
    u32 modelHash1;
};

// Submesh Header for BB8 (24 bytes = 0x18)
struct SubmeshHeader_BB8 {
    u32 indexCount;
    u32 vertexCount;
    u32 uvSetCount;
    u32 vertexGroupCount;
    u32 vertexColorCount;
    u32 normalCount;
};

// Vertex Position (12 bytes)
struct VertexPosition {
    float x, y, z;
};

// Per-Vertex Extra Data (4 bytes) - colors/weights
struct VertexExtraData {
    u8 data[4];
};

// UV Coordinate Pair - OLD format (4 bytes)
struct UV_Direct {
    u16 u;  // U * 65536
    u16 v;  // V * 65536
};

// UV Delta Pair - MODERN format (4 bytes)
struct UV_Delta {
    u16 uDelta;
    u16 vDelta;
};

// UV Compression Header for MODERN format
struct UVCompressionHeader {
    u16 uOffsetCount;
    u16 vOffsetCount;
};

// =============================================================================
// GLOBAL STATE FOR PARSING
// =============================================================================
u32 g_submeshOffset;
u32 g_numIndices;
u32 g_numVertices;
u32 g_geomDataSize;
bool g_isOldFormat;

fn find_submesh_header(u32 dataStart, u32 dataSize) {
    u32 offset = 0;
    while (offset < std::math::min(dataSize, 300)) {
        if (offset + 48 > dataSize) break;

        u32 indexCount = std::mem::read_unsigned(dataStart + offset, 4);
        u32 vertexCount = std::mem::read_unsigned(dataStart + offset + 4, 4);

        if (indexCount >= 6 && indexCount <= 100000 &&
            vertexCount >= 3 && vertexCount <= 50000 &&
            indexCount >= vertexCount) {

            u32 idxStart = offset + 24;
            u32 idxSize = indexCount * 2;
            u32 posStart = idxStart + idxSize;
            u32 posSize = vertexCount * 12;

            if (posStart + posSize <= dataSize) {
                u16 idx0 = std::mem::read_unsigned(dataStart + idxStart, 2);
                u16 idx1 = std::mem::read_unsigned(dataStart + idxStart + 2, 2);
                u16 idx2 = std::mem::read_unsigned(dataStart + idxStart + 4, 2);
                if (idx0 < vertexCount && idx1 < vertexCount && idx2 < vertexCount) {
                    g_submeshOffset = offset;
                    g_numIndices = indexCount;
                    g_numVertices = vertexCount;
                    return true;
                }
            }
        }
        offset = offset + 1;
    }
    g_submeshOffset = 0;
    g_numIndices = 0;
    g_numVertices = 0;
    return false;
};

// =============================================================================
// BB8 SUBMESH STRUCTURES
// =============================================================================

struct Submesh_OldFormat {
    SubmeshHeader_BB8 header [[name("Submesh Header")]];
    u16 indices[header.indexCount] [[name("Index Buffer")]];
    VertexPosition positions[header.vertexCount] [[name("Vertex Positions")]];
    UV_Direct uvCoords[header.vertexCount * (header.uvSetCount > 0 ? header.uvSetCount : 1)]
        [[name("UV Coordinates (direct format)")]];
};

struct Submesh_ModernFormat {
    SubmeshHeader_BB8 header [[name("Submesh Header")]];
    u16 indices[header.indexCount] [[name("Index Buffer")]];
    VertexPosition positions[header.vertexCount] [[name("Vertex Positions")]];
    VertexExtraData extraData[header.vertexCount] [[name("Per-Vertex Data (color/weights)")]];
    UVCompressionHeader uvHeader [[name("UV Compression Header")]];
    u16 uvOffsetArrays[(uvHeader.uOffsetCount + uvHeader.vOffsetCount) * 2]
        [[name("UV Offset Arrays")]];
    UV_Delta uvDeltas[header.vertexCount * (header.uvSetCount > 0 ? header.uvSetCount : 1)]
        [[name("UV Delta Data")]];
};

struct GeometryDataParsed_BB8 {
    u32 dataStart = $;
    bool found = find_submesh_header(dataStart, g_geomDataSize);

    if (g_submeshOffset > 0) {
        u8 preSubmeshData[g_submeshOffset] [[name("Pre-Submesh Data (materials, textures, etc.)")]];
    }

    if (found) {
        if (g_isOldFormat) {
            Submesh_OldFormat submesh [[name("Submesh (OLD UV format)")]];
            u32 uvSets = std::mem::read_unsigned(dataStart + g_submeshOffset + 8, 4);
            if (uvSets == 0 || uvSets > 8) uvSets = 1;
            u32 consumed = g_submeshOffset + 24 + g_numIndices * 2 +
                           g_numVertices * 12 + g_numVertices * uvSets * 4;
            if (consumed < g_geomDataSize) {
                u8 remainingData[g_geomDataSize - consumed] [[name("Remaining Data")]];
            }
        } else {
            Submesh_ModernFormat submesh [[name("Submesh (MODERN UV format)")]];
            u32 consumed = g_submeshOffset + 24 + g_numIndices * 2 + g_numVertices * 12;
            consumed = consumed + g_numVertices * 4;
            u16 uCnt = std::mem::read_unsigned(dataStart + consumed, 2);
            u16 vCnt = std::mem::read_unsigned(dataStart + consumed + 2, 2);
            consumed = consumed + 4 + (uCnt + vCnt) * 4;
            u32 uvSets = std::mem::read_unsigned(dataStart + g_submeshOffset + 8, 4);
            if (uvSets == 0 || uvSets > 8) uvSets = 1;
            consumed = consumed + g_numVertices * uvSets * 4;
            if (consumed < g_geomDataSize) {
                u8 remainingData[g_geomDataSize - consumed] [[name("Remaining Data")]];
            }
        }
    } else {
        u8 rawGeometryData[g_geomDataSize] [[name("Raw Geometry Data (submesh not found)")]];
    }
};

// =============================================================================
// CHUNK DEFINITIONS
// =============================================================================

// 0xBB8 - Geometry (DAT format)
struct Chunk_BB8_Geometry {
    u32 chunkId;
    u32 chunkSize;
    GeometryHeader_BB8 header [[name("Geometry Header (48 bytes)")]];

    g_geomDataSize = chunkSize - sizeof(GeometryHeader_BB8);
    g_isOldFormat = (header.textureGroupCount == 0);

    GeometryDataParsed_BB8 geometryData [[name("Geometry Data")]];
};

// 0xBB9 - Animation/Skeleton (DAT format)
struct Chunk_BB9_Animation {
    u32 chunkId;
    u32 chunkSize;
    AnimationHeader_BB9 header [[name("Animation Header")]];
    u8 animationData[chunkSize - sizeof(AnimationHeader_BB9)] [[name("Animation Data")]];
};

// 0xBBA - Texture References (DAT format)
struct Chunk_BBA_TextureRefs {
    u32 chunkId;
    u32 chunkSize;
    u32 version;
    u32 entryCount;
    u8 textureRefData[(chunkSize > 8) ? (chunkSize - 8) : 0] [[name("Texture Reference Data")]];
};

// 0xBBB/0xBBC - Texture Filenames (DAT format)
struct Chunk_BBB_TextureFilenames {
    u32 chunkId;
    u32 chunkSize;
    u32 unknown;
    u32 filenameCount;
    TextureFilenameEntry filenames[filenameCount];
    if (chunkSize > 8 + filenameCount * 6) {
        u8 remaining[chunkSize - 8 - filenameCount * 6];
    }
};

// 0xBC0/0xBC1 - Additional Data (DAT format)
struct Chunk_BC0_AdditionalData {
    u32 chunkId;
    u32 chunkSize;
    u32 version;
    u32 entryCount;
    u8 data[chunkSize - 8] [[name("Additional Data Entries")]];
};

// 0xFA0 - Geometry (Runtime format)
struct Chunk_FA0_Geometry {
    u32 chunkId;
    u32 chunkSize;
    u32 chunkDataStart = $;

    GeometryHeader_FA0 header [[name("Geometry Header (84 bytes)")]];

    if (header.boneInfoCount > 0) {
        BoneInfo boneInfos[header.boneInfoCount] [[name("Bone Info Structures")]];
    }

    u8 remainingData[chunkSize - ($ - chunkDataStart)] [[name("Remaining Geometry Data")]];
};

// 0xFA1 - Animation/Skeleton (Runtime format)
struct Chunk_FA1_Animation {
    u32 chunkId;
    u32 chunkSize;
    GeometryHeader_FA1 header [[name("Animation Header")]];

    u32 headerSize = sizeof(GeometryHeader_FA1);
    u32 dataRemaining = chunkSize - headerSize;

    if (header.boundingCylinderCount > 0 && dataRemaining >= header.boundingCylinderCount * 16) {
        u8 boundingCylinders[header.boundingCylinderCount * 16] [[name("Bounding Cylinders")]];
        dataRemaining = dataRemaining - header.boundingCylinderCount * 16;
    }

    if (dataRemaining > 0) {
        u8 remainingData[dataRemaining] [[name("Animation/Transform Data")]];
    }
};

// 0xFA3/0xFAA - Inline ATEX Texture
struct Chunk_ATEX {
    u32 chunkId;
    u32 chunkSize;
    ATEXHeader header [[name("ATEX Header")]];
    u8 textureData[chunkSize - sizeof(ATEXHeader)] [[name("DXT Compressed Texture Data")]];
};

// 0xFA4 - Texture References (Runtime format)
struct Chunk_FA4_TextureRefs {
    u32 chunkId;
    u32 chunkSize;
    u32 version;
    u32 entryCount;
    // 16-byte entries
    u8 textureRefs[chunkSize - 8] [[name("Texture Reference Entries (16 bytes each)")]];
};

// 0xFA5/0xFA6/0xFAD - Texture Filenames (Runtime format)
struct Chunk_FA5_TextureFilenames {
    u32 chunkId;
    u32 chunkSize;
    u32 count;
    u32 actualCount = std::math::min(count, (chunkSize - 4) / sizeof(TextureFilenameEntry));
    TextureFilenameEntry filenames[actualCount] [[name("Texture Filename Entries")]];
    if (chunkSize > 4 + actualCount * sizeof(TextureFilenameEntry)) {
        u8 remaining[chunkSize - 4 - actualCount * sizeof(TextureFilenameEntry)];
    }
};

// 0xFAC - File Metadata
struct Chunk_FAC_Metadata {
    u32 chunkId;
    u32 chunkSize;
    u32 version;
    u32 fileId;
    u8 metaData[chunkSize - 8] [[name("File Metadata")]];
};

// Generic Unknown Chunk
struct UnknownChunk {
    u32 chunkId;
    u32 chunkSize;
    u8 data[chunkSize] [[name("Unknown Data")]];
};

// =============================================================================
// CHUNK DISPATCHER
// =============================================================================
struct Chunk {
    u32 peekId = std::mem::read_unsigned($, 4);

    // 0xBB* range (DAT original format)
    if (peekId == 0xBB8) {
        Chunk_BB8_Geometry chunk [[name("Geometry (0xBB8 - DAT format)")]];
    }
    else if (peekId == 0xBB9) {
        Chunk_BB9_Animation chunk [[name("Animation (0xBB9 - DAT format)")]];
    }
    else if (peekId == 0xBBA) {
        Chunk_BBA_TextureRefs chunk [[name("Texture Refs (0xBBA - DAT format)")]];
    }
    else if (peekId == 0xBBB || peekId == 0xBBC) {
        Chunk_BBB_TextureFilenames chunk [[name("Texture Filenames (0xBBB/BBC - DAT format)")]];
    }
    else if (peekId == 0xBBE) {
        UnknownChunk chunk [[name("Version Data (0xBBE - DAT format)")]];
    }
    else if (peekId == 0xBBF) {
        UnknownChunk chunk [[name("Index Buffer (0xBBF - DAT format)")]];
    }
    else if (peekId == 0xBC0 || peekId == 0xBC1) {
        Chunk_BC0_AdditionalData chunk [[name("Additional Data (0xBC0/BC1 - DAT format)")]];
    }

    // 0xFA* range (Runtime/converted format)
    else if (peekId == 0xFA0) {
        Chunk_FA0_Geometry chunk [[name("Geometry (0xFA0 - Runtime format)")]];
    }
    else if (peekId == 0xFA1) {
        Chunk_FA1_Animation chunk [[name("Animation (0xFA1 - Runtime format)")]];
    }
    else if (peekId == 0xFA3 || peekId == 0xFAA) {
        Chunk_ATEX chunk [[name("Inline ATEX Texture")]];
    }
    else if (peekId == 0xFA4) {
        Chunk_FA4_TextureRefs chunk [[name("Texture Refs (0xFA4 - Runtime format)")]];
    }
    else if (peekId == 0xFA5 || peekId == 0xFA6 || peekId == 0xFAD) {
        Chunk_FA5_TextureFilenames chunk [[name("Texture Filenames (0xFA5/FA6/FAD)")]];
    }
    else if (peekId == 0xFA7) {
        UnknownChunk chunk [[name("Unknown (0xFA7)")]];
    }
    else if (peekId == 0xFA9) {
        UnknownChunk chunk [[name("Version Data (0xFA9)")]];
    }
    else if (peekId == 0xFAB) {
        UnknownChunk chunk [[name("Index Buffer (0xFAB)")]];
    }
    else if (peekId == 0xFAC) {
        Chunk_FAC_Metadata chunk [[name("File Metadata (0xFAC)")]];
    }
    else if (peekId == 0xFAE) {
        UnknownChunk chunk [[name("Additional Data (0xFAE)")]];
    }

    // Unknown chunk - parse generically
    else {
        UnknownChunk chunk [[name("Unknown Chunk")]];
    }
};

// =============================================================================
// MAIN FILE STRUCTURE
// =============================================================================
struct FFNAModelFile {
    char signature[4] [[name("FFNA Signature")]];   // "ffna" (0x616e6666)
    u8 fileType [[name("File Type")]];               // 2 = model, 5 = model variant

    Chunk chunks[while($ < sizeof($))] [[name("Chunks")]];
};

FFNAModelFile modelFile @ 0;
