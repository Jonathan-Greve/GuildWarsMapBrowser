// =============================================================================
// FFNA Type 2 Model File - COMPLETE UNIFIED PATTERN
// Handles ALL chunk types: 0xFA* (runtime) and 0xBB* (DAT original)
//
// Reverse engineered from Gw.exe using Ghidra MCP:
//   - Model_ParseGeometryHeader (0x00769370)
//   - Model_ParseMaterialData (0x00768e00)
//   - Model_ParseBoneData (0x007691c0)
//   - Model_ParseShaderData (0x00769930)
//   - Model_ParseAttachmentPoints (0x00769d00)
//   - Model_ParseEmitterData (0x00769e70)
//   - Model_ParseAnimationData (0x0076aa40)
//   - Model_ParseSkeletonData (0x0076aba0)
//   - MdlDecomp_ConvertSubmesh (0x0076dbe0)
//   - MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0 (0x0076d380)
//   - MdlDecomp_ConvertGeometryChunk_0xBB9_to_0xFA1 (0x0076e780)
//   - MdlDecomp_ConvertAllChunks (0x00771090)
//   - MdlDecomp_ConvertBoneWeights (0x0076e4e0)
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
// Special chunks:
//   0xFA3 (4003): Inline ATEX DXT3 texture
//   0xFA7 (4007): Bounding cylinder data
//   0xFA8 (4008): Skeleton references
//   0xFAA (4010): Inline ATEX DXTA texture
//   0xFAC (4012): File metadata

// =============================================================================
// FVF (Flexible Vertex Format) Lookup Functions
// From MdlFvf_ConvertToGrFVF and GetVertexSizeFromFVF
// =============================================================================

fn get_fvf_pos_weight_size(u32 idx) {
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
    return 0x0;
};

fn get_fvf_normal_size(u32 idx) {
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

fn get_fvf_uv_size(u32 idx) {
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

fn get_fvf(u32 dat_fvf) {
    return (dat_fvf & 0xff0) << 4 | dat_fvf >> 8 & 0x30 | dat_fvf & 0xf;
};

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
    HAS_SKELETON_DATA : 1;     // 0x200: Skeleton/joint hierarchy (max 32 bones)
    HAS_EXTENDED_LOD : 1;      // 0x400: Extended LOD data
    HAS_CLOTH_DATA : 1;        // 0x800: Cloth simulation data
    HAS_SCALE_DATA : 1;        // 0x1000: Scale transform data
    UNKNOWN_0x2000 : 1;        // 0x2000
    UNKNOWN_0x4000 : 1;        // 0x4000
    HAS_TEX_COORDS : 1;        // 0x8000: Texture coordinates
    RESERVED : 16;
};

// =============================================================================
// UV Compression Notes (from MdlDecomp_ConvertSubmesh at 0x0076dbe0)
// =============================================================================
// Two UV formats based on textureGroupCount (offset 0x19 in BB8 header):
//
// OLD format (textureGroupCount == 0):
//   - UV data immediately after positions (no extra data gap)
//   - UV values normalized: final_uv = raw_uv / 65536.0
//
// MODERN format (textureGroupCount > 0):
//   - After positions: per-vertex extra data (4 bytes each)
//   - UV compression header: uOffsetCount (u16), vOffsetCount (u16)
//   - U thresholds[uOffsetCount], V thresholds[vOffsetCount]
//   - U offsets[uOffsetCount], V offsets[vOffsetCount]
//   - UV deltas: (u_delta, v_delta) per vertex
//   - Final UV = delta * (1.0/65536.0) + offset
//
// UV_DELTA_SCALE_FACTOR at 0x00a399b8 = 1.0/65536.0 (0x37800000)

// =============================================================================
// COMMON STRUCTURES
// =============================================================================

struct String {
    char value[compute_str_len_plus_one($)];
};

// Bone Group/Info Structure (28 bytes = 0x1C)
// From Model_ParseGeometryHeader when classFlags & 0x002
struct BoneGroup {
    float offsetX;
    float offsetY;
    float offsetZ;
    u32 parentBoneIndex;      // -1 for root
    u32 childCount;
    u32 flags;
    u32 boneId;
};

// Bone Weight Set (24 bytes = 0x18 header + variable data)
// From MdlDecomp_ConvertBoneWeights
struct BoneWeightSetHeader {
    u32 unknown0;
    u32 unknown1;
    u32 weightCount;          // Number of weight entries
    u32 boneCount0;
    u32 boneCount1;
    u32 boneCount2;
    // Followed by: weightCount * 7 bytes + (boneCount0 + boneCount1 + boneCount2) * 8 bytes
};

// Shader Definition (8 bytes) - From Model_ParseShaderData
struct ShaderDefinition {
    u8 cullFlags;           // Bits: 0x01=?, 0x02=twoSided, 0x04=?, 0x08=?
    u8 shaderType;          // Shader type index (0-10 mapped to internal enum)
    u32 shaderFlags;        // Feature flags
    u8 pixelShaderId;       // Pixel shader ID (8 = AMAT special)
    u8 textureCount;        // Number of textures used by this shader
};

// Material Definition (9 bytes) - From Model_ParseMaterialData
struct MaterialDefinition {
    u16 renderFlags;        // Render state flags (0x60A -> 0x622 conversion)
    u16 blendFlags;         // Alpha blend mode (0x600 = standard)
    u8 textureStageIndex;   // Which texture stage to use
    u8 alphaTestRef;        // Alpha test reference (0-255)
    u8 sortPriority;        // Render sort priority
    u8 lodBias;             // Texture LOD bias
    u8 uvChannel;           // UV channel index
};

// Texture Reference Entry (16 bytes in FA4, 6 bytes in BBB)
struct TextureRefEntry_FA4 {
    u32 textureId;          // DAT file ID
    u32 flags;
    u32 unknown1;
    u32 unknown2;
};

// Texture Filename Entry (6 bytes)
// Hash decode: file_hash = (id0 - 0xFF00FF) + (id1 * 0xFF00)
struct TextureFilenameEntry {
    u16 id0;                // Encoded part 1
    u16 id1;                // Encoded part 2
    u16 unknown;            // Usually 0
};

// Attachment Point (48 bytes = 0x30) - From Model_ParseAttachmentPoints
// Each attachment stores 12 DWORDs of transform data
struct AttachmentPoint {
    float positionX, positionY, positionZ;
    float rotationX, rotationY, rotationZ, rotationW;
    float scaleX, scaleY, scaleZ;
    s32 parentBoneIndex;    // -1 for world space
    u32 attachmentType;
};

// Particle Emitter (88 bytes = 0x58) - From Model_ParseEmitterData
struct ParticleEmitter {
    u32 emitterType;        // Point, sphere, cylinder, etc.
    s32 parentBoneIndex;    // -1 for world space
    u32 flags;
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

// Animation Entry (20 bytes = 0x14 header) - From Model_ParseAnimationData
struct AnimationEntryHeader {
    float unknown0;
    float unknown1;
    float unknown2;
    u32 duration;           // Animation duration
    u16 keyframeCount0;     // First keyframe set count
    u16 keyframeCount1;     // Second keyframe set count
    // Followed by: (keyframeCount0 + keyframeCount1) * 4 bytes of keyframe data
};

// Skeleton Entry (16 bytes = 0x10 header) - From Model_ParseSkeletonData
struct SkeletonEntryHeader {
    float unknown0;
    float unknown1;
    float unknown2;
    u32 boneData;           // Packed bone counts
    // Followed by variable bone data
};

// ATEX Inline Texture Header
struct ATEXHeader {
    char signature[4];      // "ATEX"
    char format[4];         // "DXT3", "DXTA", "DXT1", "DXT5", etc.
    u16 width;
    u16 height;
    u32 dataSize;
    u32 mipLevelCount;
};

// =============================================================================
// 0xBB8 FORMAT STRUCTURES (Original DAT Geometry)
// =============================================================================

// Geometry Header for 0xBB8 chunks (48 bytes = 0x30)
// From MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0
struct GeometryHeader_BB8 {
    u32 geometryFlags;          // 0x00: Flags for this geometry
    u32 reserved_0x04;          // 0x04: Usually 0
    ClassFlags classFlags;      // 0x08: Controls which data sections exist
    u32 modelHash0;             // 0x0C: Model signature part 1
    u32 modelHash1;             // 0x10: Model signature part 2
    u32 unknown_0x14;           // 0x14
    u8 shaderCount;             // 0x18: Number of shaders (each 8 bytes)
    u8 textureGroupCount;       // 0x19: CRITICAL - 0=OLD UV, >0=MODERN UV
    u16 textureCount;           // 0x1A: Number of texture refs (each 8 bytes)
    u8 materialBaseCount;       // 0x1C: Base material count (each 9 bytes)
    u8 maxBoneIndices;          // 0x1D: Max bone indices per vertex
    u16 materialCount;          // 0x1E: Total material count
    u32 boneWeightSetCount;     // 0x20: Bone weight set count
    u32 outputFlags;            // 0x24: Output conversion flags
    float submeshScaleX;        // 0x28: UV scale X
    float submeshScaleY;        // 0x2C: UV scale Y
};

// Submesh Header for BB8 (32 bytes = 0x20)
// From MdlDecomp_ConvertSubmesh - 8 DWORDs
struct SubmeshHeader_BB8 {
    u32 submeshFlags;           // 0x00: Visibility, render state flags
    u32 materialIndex;          // 0x04: Material/shader index
    u32 indexCount;             // 0x08: Number of vertex indices
    u32 vertexCount;            // 0x0C: Number of vertices
    u32 uvSetCount;             // 0x10: UV coordinate sets per vertex
    u32 colorCount;             // 0x14: Vertex color count
    u32 normalCount;            // 0x18: Normal vector count
    u32 triangleGroupCount;     // 0x1C: Triangle strip/fan groups
};

// Vertex Position (12 bytes)
struct VertexPosition {
    float x, y, z;
};

// Per-Vertex Extra Data (4 bytes) - colors or bone weights
struct VertexExtraData {
    u8 data[4];
};

// UV Coordinate - OLD format (4 bytes, direct normalized)
struct UV_Direct {
    u16 u;  // U * 65536
    u16 v;  // V * 65536
};

// UV Delta - MODERN format (4 bytes, delta compressed)
struct UV_Delta {
    u16 uDelta;
    u16 vDelta;
};

// UV Compression Header for MODERN format
struct UVCompressionHeader {
    u16 uOffsetCount;
    u16 vOffsetCount;
};

// Texture Group Definition (9 bytes)
// From shader data parsing
struct TextureGroupDef {
    u8 textureIndex;
    u16 flags;
    u32 transformHash;
    u8 blendState;
    u8 uvChannel;
};

// =============================================================================
// 0xFA0 FORMAT STRUCTURES (Runtime Geometry)
// =============================================================================

// Geometry Header for 0xFA0 chunks (84 bytes = 0x54)
// From Model_ParseGeometryHeader at 0x00769370
struct GeometryHeader_FA0 {
    u32 modelVersion;           // 0x00: Usually 0x26
    u32 fileId;                 // 0x04: DAT file ID
    ClassFlags classFlags;      // 0x08: Data section flags
    u32 boundingBoxId;          // 0x0C
    u32 collisionMeshId;        // 0x10
    u32 unknown_0x14;           // 0x14
    u8 shaderCount;             // 0x18: Number of shaders (8 bytes each)
    u8 textureGroupCount;       // 0x19: Texture groups
    u16 textureCount;           // 0x1A: Number of texture refs
    u8 materialBaseCount;       // 0x1C: Materials for shader/material arrays
    u8 maxBoneIndices;          // 0x1D: Max bone indices
    u16 materialCount;          // 0x1E: Total material count
    u32 unknown_0x20;           // 0x20
    u32 geometryFlags;          // 0x24: Additional flags
    u32 unknown_0x28;           // 0x28
    u32 unknown_0x2C;           // 0x2C
    u8 boneInfoCount;           // 0x30: Bone info structs (28 bytes each)
    u8 skeletonBoneCount;       // 0x31: Skeleton bones
    u8 skeletonWeightCount;     // 0x32: Skeleton weights
    u8 padding_0x33;            // 0x33
    u32 emitterDataSize;        // 0x34: Size of emitter data section
    float boundingSphereX;      // 0x38
    float boundingSphereY;      // 0x3C
    float boundingSphereZ;      // 0x40
    u32 submeshCount;           // 0x44: Number of submeshes
    u32 animationCount;         // 0x48: Animation entries
    u16 lodCount;               // 0x4C: LOD entries
    u16 extLodCount;            // 0x4E: Extended LOD entries
    u16 boneDataEntryCount;     // 0x50: Bone data entries
    u16 attachmentPointCount;   // 0x52: Attachment points
};

// Submesh Header for FA0 (36 bytes = 0x24)
// From MdlDecomp_ParseSubmeshVertexData
struct SubmeshHeader_FA0 {
    u16 materialIndex;          // 0x00: Material/shader index
    u16 submeshFlags;           // 0x02: Visibility, render flags
    u32 indexCount;             // 0x04: LOD High - index count for highest detail
    u32 indexCount2;            // 0x08: LOD Medium - index count (adds to buffer if != indexCount)
    u32 indexCount3;            // 0x0C: LOD Low - index count (adds to buffer if != indexCount2)
    u32 vertexCount;            // 0x10: Number of vertices (shared across all LOD levels)
    u32 vertexFormat;           // 0x14: FVF flags for vertex size calculation
    u32 uvSetCount;             // 0x18: UV set metadata count (NOT per-vertex)
    u32 vertexColorCount;       // 0x1C: Number of vertex colors (u32 each)
    u32 triangleGroupCount;     // 0x20: Triangle strip/fan group count
};

// Helper function to calculate total index count for all LOD levels
// Logic from MdlDecomp_ParseSubmeshVertexData:
// - indexCount = LOD High (always present)
// - indexCount2 = LOD Medium (adds to buffer if != indexCount)
// - indexCount3 = LOD Low (adds to buffer if != indexCount2)
// All LOD levels share the same vertex buffer
fn calc_total_index_count(u32 ic1, u32 ic2, u32 ic3) {
    u32 total = ic1;
    if (ic2 != ic1) {
        total = total + ic2;
    }
    if (ic3 != ic2) {
        total = total + ic3;
    }
    return total;
};

// FA0 Submesh with vertex data
// Data layout per MdlDecomp_ParseSubmeshVertexData:
// 1. Index buffer: totalIndexCount * 2 bytes (combined LOD High/Med/Low indices)
// 2. UV metadata: uvSetCount * 4 bytes (per UV set, NOT per vertex)
// 3. Vertex buffer: vertexCount * vertexSize bytes (shared by all LOD levels)
// 4. Color buffer: colorCount * 4 bytes (RGBA per vertex)
// 5. Triangle groups: triGroupCount * 12 bytes (strip/fan definitions)
struct SubmeshData_FA0 {
    SubmeshHeader_FA0 header [[name("Submesh Header")]];

    // Calculate total index count using the game's logic
    u32 totalIndexCount = calc_total_index_count(header.indexCount, header.indexCount2, header.indexCount3);

    // Index buffer (u16 indices)
    if (totalIndexCount > 0 && totalIndexCount < 100000) {
        u16 indices[totalIndexCount] [[name("Index Buffer")]];
    }

    // UV metadata (4 bytes per UV set - NOT per-vertex UVs)
    if (header.uvSetCount > 0 && header.uvSetCount < 16) {
        u32 uvMetadata[header.uvSetCount] [[name("UV Set Metadata")]];
    }

    // Vertex buffer - size depends on FVF
    // Use the FVF lookup to calculate vertex size
    u32 vertexSize = get_vertex_size_from_fvf(get_fvf(header.vertexFormat));
    if (header.vertexCount > 0 && header.vertexCount < 100000 && vertexSize > 0) {
        u8 vertexData[header.vertexCount * vertexSize] [[name("Vertex Buffer")]];
    }

    // Vertex colors (u32 RGBA each)
    if (header.vertexColorCount > 0 && header.vertexColorCount < 100000) {
        u32 vertexColors[header.vertexColorCount] [[name("Vertex Colors")]];
    }

    // Triangle groups (12 bytes each based on formula derivation)
    if (header.triangleGroupCount > 0 && header.triangleGroupCount < 1000) {
        u8 triangleGroups[header.triangleGroupCount * 12] [[name("Triangle Group Data")]];
    }
};

// =============================================================================
// 0xBB9/0xFA1 FORMAT STRUCTURES (Animation/Skeleton)
// =============================================================================

// Animation Header for 0xBB9 chunks (44 bytes = 0x2C)
struct AnimationHeader_BB9 {
    u32 typeMarker;             // 0x00: Usually 0x26
    u32 fileId;                 // 0x04
    u32 flags;                  // 0x08: Typically 0x10
    u32 modelHash0;             // 0x0C
    u32 modelHash1;             // 0x10
    u32 boundingCylinderCount;  // 0x14
    u32 unknown_0x18;           // 0x18
    u32 unknown_0x1C;           // 0x1C
    u32 sequenceCount;          // 0x20: Number of animation sequences
    u32 unknown_0x24;           // 0x24
    u32 unknown_0x28;           // 0x28
};

// Animation Header for 0xFA1 chunks (88 bytes = 0x58)
struct GeometryHeader_FA1 {
    u32 modelVersion;           // 0x00
    u32 fileId;                 // 0x04
    ClassFlags classFlags;      // 0x08
    u32 boundingBoxId;          // 0x0C
    u32 collisionMeshId;        // 0x10
    u32 boundingCylinderCount;  // 0x14
    u16 sequenceKeyframeCount0; // 0x18
    u16 sequenceKeyframeCount1; // 0x1A
    u32 unknown_0x1C;
    float boundingSphereX;      // 0x20
    float boundingSphereY;      // 0x24
    float boundingSphereZ;      // 0x28
    float boundingSphereRadius; // 0x2C
    u32 unknown_0x30;
    u32 transformDataSize;      // 0x34
    u32 submeshCount;           // 0x38
    u32 unknown_0x3C;
    u16 sequenceCount0;         // 0x40
    u16 sequenceCount1;         // 0x42
    u32 unknown_0x44;
    u32 animationCount;         // 0x48
    u32 skeletonNodeCount;      // 0x4C
    u16 boneDataCount;          // 0x50
    u16 attachmentCount;        // 0x52
    u8 unknown_0x54;
    u8 unknown_0x55;
    u8 unknown_0x56;
    u8 unknown_0x57;
};

// Sequence Entry (22 bytes = 0x16 in BB9)
struct SequenceEntry_BB9 {
    float unknown0;
    float unknown1;
    float unknown2;
    u32 hash;
    u8 frameCount0;
    u8 frameCount1;
    u8 unknown2a;
    u8 unknown2b;
    u16 dataSize;
};

// Bounding Cylinder (16 bytes)
struct BoundingCylinder {
    float centerX, centerY, centerZ;
    float radius;
};

// =============================================================================
// GLOBAL PARSING STATE
// =============================================================================
u32 g_chunkDataSize;
u32 g_chunkDataStart;
bool g_isOldUVFormat;
u8 g_textureGroupCount;

// =============================================================================
// BB8 GEOMETRY CHUNK PARSING
// =============================================================================

struct SubmeshData_BB8 {
    SubmeshHeader_BB8 header [[name("Submesh Header")]];

    // Index buffer
    u16 indices[header.indexCount] [[name("Index Buffer")]];

    // Position buffer
    VertexPosition positions[header.vertexCount] [[name("Vertex Positions")]];

    // Extra per-vertex data (if flags indicate)
    if ((header.submeshFlags & 0x08) != 0) {
        VertexExtraData extraData[header.vertexCount] [[name("Per-Vertex Colors/Weights")]];
    }

    // Additional per-vertex data (if flags indicate)
    if ((header.submeshFlags & 0x10) != 0) {
        VertexExtraData additionalData[header.vertexCount] [[name("Additional Per-Vertex Data")]];
    }

    // UV data section
    if (!g_isOldUVFormat && header.uvSetCount > 0) {
        // MODERN format: UV compression header + offset arrays + delta data
        UVCompressionHeader uvHeader [[name("UV Compression Header")]];
        u16 uThresholds[uvHeader.uOffsetCount] [[name("U Thresholds")]];
        u16 vThresholds[uvHeader.vOffsetCount] [[name("V Thresholds")]];
        s16 uOffsets[uvHeader.uOffsetCount] [[name("U Offsets")]];
        s16 vOffsets[uvHeader.vOffsetCount] [[name("V Offsets")]];
        UV_Delta uvDeltas[header.vertexCount * header.uvSetCount] [[name("UV Delta Data")]];
    } else if (header.uvSetCount > 0) {
        // OLD format: Direct UV values
        UV_Direct uvCoords[header.vertexCount * header.uvSetCount] [[name("UV Coordinates (direct)")]];
    }

    // Normal indices (for normal sharing)
    if (header.normalCount > 0 && header.normalCount != header.vertexCount) {
        u16 normalIndices[header.normalCount] [[name("Normal Indices")]];
    }

    // Color data
    if (header.colorCount > 0) {
        u32 vertexColors[header.colorCount] [[name("Vertex Colors (RGBA)")]];
    }

    // Triangle group data (strip/fan definitions)
    if (header.triangleGroupCount > 0) {
        u8 triangleGroups[header.triangleGroupCount * 3] [[name("Triangle Group Data")]];
    }
};

// Texture Reference (8 bytes) - for BB8 format
struct TextureRef_BB8 {
    u32 textureFileId;          // DAT file ID
    u32 flags;                  // Texture flags
};

// Texture Group Definition (9 bytes)
struct TextureGroupDef_BB8 {
    u8 textureIndex;
    u16 flags;
    u32 transformHash;
    u8 blendState;
    u8 uvChannel;
};

// Material Reference (2 bytes)
struct MaterialRef_BB8 {
    u16 materialIndex;
};

struct Chunk_BB8_Geometry {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;
    g_chunkDataSize = chunkSize;

    GeometryHeader_BB8 header [[name("Geometry Header (48 bytes)")]];

    // Store header values for submesh parsing
    g_isOldUVFormat = (header.textureGroupCount == 0);
    g_textureGroupCount = header.textureGroupCount;

    // Bone groups (if classFlags.HAS_BONE_GROUPS)
    if (header.classFlags.HAS_BONE_GROUPS) {
        u32 boneGroupArrayCount [[name("Bone Group Count")]];
        if (boneGroupArrayCount > 0 && boneGroupArrayCount <= 4) {
            BoneGroup boneGroups[boneGroupArrayCount] [[name("Bone Groups")]];
        }
    }

    // Bone weights (if classFlags.HAS_BONE_WEIGHTS)
    if (header.classFlags.HAS_BONE_WEIGHTS) {
        // Variable-length bone weight data - skip for now, read as raw
        // Would need MdlDecomp_ConvertBoneWeights logic
    }

    // Shader definitions (shaderCount * 8 bytes)
    if (header.shaderCount > 0 && header.shaderCount != 0xFF) {
        ShaderDefinition shaders[header.shaderCount] [[name("Shader Definitions")]];
    }

    // Material definitions (materialBaseCount * 9 bytes)
    if (header.materialBaseCount > 0 && header.materialBaseCount < 0x80) {
        MaterialDefinition materials[header.materialBaseCount] [[name("Material Definitions")]];

        // Extra byte per material if boneWeightSetCount != 0
        if (header.boneWeightSetCount != 0) {
            u8 materialExtraData[header.materialBaseCount] [[name("Material Extra Data")]];
        }
    }

    // Texture group data (if textureGroupCount > 0)
    if (header.textureGroupCount > 0 && header.textureGroupCount != 0xFF) {
        TextureGroupDef_BB8 textureGroups[header.textureGroupCount] [[name("Texture Groups")]];

        // Bone index data: maxBoneIndices * (3 or 4) bytes
        if (header.maxBoneIndices > 0) {
            u32 boneIndexDataSize = (header.boneWeightSetCount != 0 ? 4 : 3) * header.maxBoneIndices;
            u8 boneIndexData[boneIndexDataSize] [[name("Bone Index Data")]];
        }

        // Texture references (textureCount * 8 bytes)
        if (header.textureCount > 0 && header.textureCount < 256) {
            TextureRef_BB8 textureRefs[header.textureCount] [[name("Texture References")]];
        }

        // Texture filename strings (null-terminated)
        // This is variable length - we'll read until we hit materialCount refs

        // Material references (materialCount * 2 bytes)
        if (header.materialCount > 0 && header.materialCount < 256) {
            MaterialRef_BB8 materialRefs[header.materialCount] [[name("Material References")]];
        }
    }

    // Geometry sets (if classFlags.HAS_GEOSET_DATA = 0x20)
    if (header.classFlags.HAS_GEOSET_DATA) {
        u32 geosetCount [[name("Geometry Set Count")]];
        u32 geosetDataSize [[name("Geometry Set Data Size")]];
        if (geosetDataSize > 0 && geosetDataSize < 0x100000) {
            u8 geosetData[geosetDataSize] [[name("Geometry Set Data")]];
        }
    }

    // Morph/attachment data (if classFlags.HAS_MORPH_DATA = 0x80)
    if (header.classFlags.HAS_MORPH_DATA) {
        u32 morphCount0 [[name("Morph Count 0")]];
        u32 morphCount1 [[name("Morph Count 1")]];
        u32 morphCount2 [[name("Morph Count 2")]];
        if (morphCount0 > 0 && morphCount1 > 0 && morphCount2 > 0) {
            u32 morphDataSize = (morphCount1 + morphCount0 * 2) * 6 + morphCount2 * 4;
            if (morphDataSize < 0x100000) {
                u8 morphData[morphDataSize] [[name("Morph Data")]];
            }
        }
    }

    // Submeshes (if classFlags.HAS_SUBMESH_DATA = 0x08)
    if (header.classFlags.HAS_SUBMESH_DATA) {
        u32 submeshCount [[name("Submesh Count")]];
        if (submeshCount > 0 && submeshCount < 255) {
            SubmeshData_BB8 submeshes[submeshCount] [[name("Submeshes")]];
        }
    }

    // Remaining data (LOD, skeleton, animation, bounding)
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u32 remainingSize = chunkSize - consumed;
        u8 remainingData[remainingSize] [[name("Remaining Data (LOD, skeleton, animation, bounding)")]];
    }
};

// =============================================================================
// BB9 ANIMATION CHUNK PARSING
// =============================================================================

struct Chunk_BB9_Animation {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;

    AnimationHeader_BB9 header [[name("Animation Header")]];

    // Bounding cylinders (if boundingCylinderCount > 0)
    if (header.boundingCylinderCount > 0) {
        BoundingCylinder cylinders[header.boundingCylinderCount] [[name("Bounding Cylinders")]];
    }

    // Sequence data (if flags & 0x08)
    if ((header.flags & 0x08) != 0) {
        u32 sequenceCount [[name("Sequence Count")]];
        if (sequenceCount > 0 && sequenceCount < 256) {
            SequenceEntry_BB9 sequences[sequenceCount] [[name("Animation Sequences")]];
        }
    }

    // Remaining animation data
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u32 remainingSize = chunkSize - consumed;
        u8 animationData[remainingSize] [[name("Animation Data")]];
    }
};

// =============================================================================
// TEXTURE CHUNKS
// =============================================================================

struct Chunk_BBA_TextureRefs {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                    // Should be 2
    u32 entryCount;

    if (version == 2 && entryCount > 0 && entryCount < 256) {
        TextureRefEntry_FA4 entries[entryCount] [[name("Texture References")]];
    }

    u32 expectedSize = 8 + (entryCount * 16);
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize];
    }
};

struct Chunk_BBB_TextureFilenames {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                    // Should be 1
    u32 filenameCount;

    if (version == 1 && filenameCount > 0 && filenameCount < 256) {
        TextureFilenameEntry filenames[filenameCount] [[name("Texture Filenames")]];
    }

    u32 expectedSize = 8 + (filenameCount * 6);
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize];
    }
};

// =============================================================================
// FA0 GEOMETRY CHUNK PARSING
// =============================================================================

// Bone Data Entry for FA0 (variable size)
// Header is 24 bytes, followed by variable data
struct BoneDataEntry_FA0 {
    u32 unknown0;
    u32 unknown1;
    u32 weightCount;        // * 7 bytes of weight data
    u32 boneCount0;         // * 8 bytes each
    u32 boneCount1;         // * 8 bytes each
    u32 boneCount2;         // * 8 bytes each

    // Variable data
    if (weightCount > 0 && weightCount < 1000) {
        u8 weightData[weightCount * 7] [[name("Weight Data")]];
    }
    u32 totalBoneCount = boneCount0 + boneCount1 + boneCount2;
    if (totalBoneCount > 0 && totalBoneCount < 1000) {
        u8 boneData[totalBoneCount * 8] [[name("Bone Data")]];
    }
};

// LOD Entry (variable size)
// From Model_ParseBoneData: each entry has (count1 + count2 * 6) * 2 bytes of data
struct LODEntry_FA0 {
    u32 indexCount [[name("Index Count")]];
    u32 vertexCount [[name("Vertex Count")]];

    // Data size = (indexCount + vertexCount * 6) * 2 bytes
    u32 dataSize = (indexCount + vertexCount * 6) * 2;
    if (dataSize > 0 && dataSize < 0x100000) {
        u8 lodData[dataSize] [[name("LOD Geometry Data")]];
    }
};

// Skeleton Node Entry (16 bytes per node)
// From skeleton parsing in Model_ParseGeometryHeader
struct SkeletonNode_FA0 {
    float unknown0;
    float unknown1;
    float unknown2;
    u32 boneIndex;
};

// Animation Frame Entry (variable size)
// From Model_ParseArrayWithCallback with Buffer_CalcFrameOffset
struct AnimFrameEntry_FA0 {
    u32 frameOffset [[name("Frame Data Offset")]];
};

struct Chunk_FA0_Geometry {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;

    GeometryHeader_FA0 header [[name("Geometry Header (84 bytes)")]];

    // Bone info structures (28 bytes each)
    if (header.boneInfoCount > 0 && header.boneInfoCount < 32) {
        BoneGroup boneInfos[header.boneInfoCount] [[name("Bone Info Structures")]];
    }

    // Bone data entries (variable size each)
    if (header.boneDataEntryCount > 0 && header.boneDataEntryCount < 256) {
        BoneDataEntry_FA0 boneDataEntries[header.boneDataEntryCount] [[name("Bone Data Entries")]];
    }

    // Shader definitions (8 bytes each)
    if (header.shaderCount > 0 && header.shaderCount != 0xFF) {
        ShaderDefinition shaders[header.shaderCount] [[name("Shader Definitions")]];
    }

    // Material data as parallel arrays (from Model_ParseShaderData)
    // Total size: materialBaseCount * 9 + (unknown_0x20 != 0 ? materialBaseCount : 0)
    if (header.materialBaseCount > 0 && header.materialBaseCount < 0x80) {
        // Render flags: 2 bytes each
        u16 materialRenderFlags[header.materialBaseCount] [[name("Material Render Flags")]];
        // Texture indices: 1 byte each
        u8 materialTextureIndices[header.materialBaseCount] [[name("Material Texture Indices")]];
        // Transform hashes: 4 bytes each
        u32 materialTransformHashes[header.materialBaseCount] [[name("Material Transform Hashes")]];
        // Blend states: 1 byte each
        u8 materialBlendStates[header.materialBaseCount] [[name("Material Blend States")]];
        // UV channels: 1 byte each
        u8 materialUvChannels[header.materialBaseCount] [[name("Material UV Channels")]];
        // Extra data if unknown_0x20 != 0
        if (header.unknown_0x20 != 0) {
            u8 materialExtraData[header.materialBaseCount] [[name("Material Extra Data")]];
        }
    }

    // Texture/Material section (from Model_ParseMaterialData)
    // textureGroupCount * 9 bytes + maxBoneIndices * (3 or 4) + textureCount * 8 + strings + materialCount * 8
    if (header.textureGroupCount > 0 && header.textureGroupCount != 0xFF) {
        TextureGroupDef_BB8 textureGroups[header.textureGroupCount] [[name("Texture Groups")]];
    }

    // Bone index data
    if (header.maxBoneIndices > 0) {
        u32 boneIndexSize = (header.unknown_0x20 != 0 ? 4 : 3) * header.maxBoneIndices;
        u8 boneIndexData[boneIndexSize] [[name("Bone Index Data")]];
    }

    // Texture references (8 bytes each)
    if (header.textureCount > 0 && header.textureCount < 256) {
        TextureRef_BB8 textureRefs[header.textureCount] [[name("Texture References")]];
    }

    // Texture filename strings
    if (header.textureCount > 0 && header.textureCount < 256) {
        String textureFilenames[header.textureCount] [[name("Texture Filenames")]];
    }

    // Material references (8 bytes each = 2 DWORDs)
    if (header.materialCount > 0 && header.materialCount < 256) {
        u32 materialRefData[header.materialCount * 2] [[name("Material Reference Data")]];
    }

    // Geometry sets (if classFlags.HAS_GEOSET_DATA)
    if (header.classFlags.HAS_GEOSET_DATA) {
        u32 geosetUnknown [[name("Geoset Unknown")]];
        u32 geosetCount [[name("Geometry Set Count")]];
        // Each geoset is 0x2E (46) bytes
        if (geosetCount > 0 && geosetCount < 256) {
            u8 geosetData[geosetCount * 0x2E] [[name("Geometry Set Data")]];
        }
    }

    // Attachment points (if classFlags.HAS_MORPH_DATA)
    if (header.classFlags.HAS_MORPH_DATA && header.attachmentPointCount > 0) {
        AttachmentPoint attachments[header.attachmentPointCount] [[name("Attachment Points")]];
    }

    // Submeshes with full vertex data
    // Each submesh header (36 bytes) is followed immediately by its data:
    // indices (for all 3 LOD levels), UV metadata, vertex buffer, colors, triangle groups
    if (header.submeshCount > 0 && header.submeshCount < 255) {
        SubmeshData_FA0 submeshes[header.submeshCount] [[name("Submeshes")]];
    }

    // LOD entries (if lodCount > 0)
    // Each LOD entry has variable size based on (indexCount + vertexCount * 6) * 2
    if (header.lodCount > 0 && header.lodCount < 256) {
        LODEntry_FA0 lodEntries[header.lodCount] [[name("LOD Entries")]];
    }

    // Skeleton data (if skeletonBoneCount > 0)
    // Layout: skeletonBoneCount * 16 bytes + skeletonWeightCount * 0x54 bytes
    if (header.skeletonBoneCount > 0) {
        SkeletonNode_FA0 skeletonNodes[header.skeletonBoneCount] [[name("Skeleton Nodes")]];
        if (header.skeletonWeightCount > 0) {
            u8 skeletonWeightData[header.skeletonWeightCount * 0x54] [[name("Skeleton Weight Data")]];
        }
    }

    // Animation frame data (if animationCount > 0)
    // Each entry is 4 bytes (offset), followed by variable frame data
    if (header.animationCount > 0 && header.animationCount < 1024) {
        AnimFrameEntry_FA0 animFrames[header.animationCount] [[name("Animation Frame Offsets")]];
    }

    // Emitter data (size from emitterDataSize field)
    if (header.emitterDataSize > 0 && header.emitterDataSize < 0x100000) {
        u8 emitterData[header.emitterDataSize] [[name("Emitter/Particle Data")]];
    }

    // Remaining unparsed data (if any)
    u32 consumed_fa0 = $ - g_chunkDataStart;
    if (consumed_fa0 < chunkSize) {
        u32 remainingSize = chunkSize - consumed_fa0;
        u8 remainingData[remainingSize] [[name("Remaining Unparsed Data")]];
    }
};

// =============================================================================
// FA1 ANIMATION CHUNK PARSING
// =============================================================================

struct Chunk_FA1_Animation {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;

    GeometryHeader_FA1 header [[name("Animation Header (88 bytes)")]];

    // Bounding cylinders
    if (header.boundingCylinderCount > 0) {
        BoundingCylinder cylinders[header.boundingCylinderCount] [[name("Bounding Cylinders")]];
    }

    // Remaining animation data
    u32 consumed_fa1 = $ - g_chunkDataStart;
    if (consumed_fa1 < chunkSize) {
        u32 remainingSize = chunkSize - consumed_fa1;
        u8 animationData[remainingSize] [[name("Animation/Skeleton Data")]];
    }
};

// =============================================================================
// INLINE TEXTURE CHUNK
// =============================================================================

struct Chunk_ATEX {
    u32 chunkId;
    u32 chunkSize;
    ATEXHeader header [[name("ATEX Header")]];
    if (chunkSize > sizeof(ATEXHeader)) {
        u8 textureData[chunkSize - sizeof(ATEXHeader)] [[name("DXT Compressed Data")]];
    }
};

// =============================================================================
// METADATA CHUNK (0xFAC)
// =============================================================================

struct Chunk_FAC_Metadata {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                // Usually 1
    u32 classFlags;             // Copy of geometry classFlags
    u32 boundingCylinderCount;
    if (chunkSize > 12) {
        u8 additionalData[chunkSize - 12] [[name("Additional Metadata")]];
    }
};

// =============================================================================
// INDEX BUFFER CHUNK (0xBBF/0xFAB)
// =============================================================================

struct IndexBufferEntry {
    u32 indexCount;
    u32 indices[indexCount] [[name("Indices")]];
};

struct Chunk_FAB_IndexBuffer {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                // Should be 1
    u32 bufferCount;

    // Each buffer has: indexCount (u32) + indices[indexCount] (u32 each)
    if (chunkSize > 8) {
        u8 indexData[chunkSize - 8] [[name("Index Buffer Data")]];
    }
};

// =============================================================================
// GENERIC CHUNK
// =============================================================================

struct UnknownChunk {
    u32 chunkId;
    u32 chunkSize;
    // Bounds check: don't read past end of file
    u32 maxReadable = std::mem::size() - $;
    u32 safeSize = (chunkSize <= maxReadable) ? chunkSize : maxReadable;
    if (safeSize > 0) {
        u8 data[safeSize] [[name("Unknown Data")]];
    }
};

// =============================================================================
// CHUNK DISPATCHER
// =============================================================================

struct Chunk {
    u32 peekId = std::mem::read_unsigned($, 4);

    // 0xBB* range (DAT original format)
    if (peekId == 0xBB8) {
        Chunk_BB8_Geometry chunk [[name("Geometry (0xBB8)")]];
    }
    else if (peekId == 0xBB9) {
        Chunk_BB9_Animation chunk [[name("Animation (0xBB9)")]];
    }
    else if (peekId == 0xBBA) {
        Chunk_BBA_TextureRefs chunk [[name("Texture Refs (0xBBA)")]];
    }
    else if (peekId == 0xBBB || peekId == 0xBBC) {
        Chunk_BBB_TextureFilenames chunk [[name("Texture Filenames (0xBBB/BBC)")]];
    }
    else if (peekId == 0xBBE) {
        UnknownChunk chunk [[name("Version Data (0xBBE)")]];
    }
    else if (peekId == 0xBBF) {
        Chunk_FAB_IndexBuffer chunk [[name("Index Buffer (0xBBF)")]];
    }
    else if (peekId == 0xBC0 || peekId == 0xBC1) {
        UnknownChunk chunk [[name("Additional Data (0xBC0/BC1)")]];
    }

    // 0xFA* range (Runtime/converted format)
    else if (peekId == 0xFA0) {
        Chunk_FA0_Geometry chunk [[name("Geometry (0xFA0)")]];
    }
    else if (peekId == 0xFA1) {
        Chunk_FA1_Animation chunk [[name("Animation (0xFA1)")]];
    }
    else if (peekId == 0xFA3 || peekId == 0xFAA) {
        Chunk_ATEX chunk [[name("Inline ATEX Texture")]];
    }
    else if (peekId == 0xFA4) {
        Chunk_BBA_TextureRefs chunk [[name("Texture Refs (0xFA4)")]];
    }
    else if (peekId == 0xFA5 || peekId == 0xFA6 || peekId == 0xFAD || peekId == 0xFAE) {
        Chunk_BBB_TextureFilenames chunk [[name("Texture Filenames")]];
    }
    else if (peekId == 0xFA7) {
        UnknownChunk chunk [[name("Bounding Cylinders (0xFA7)")]];
    }
    else if (peekId == 0xFA8) {
        UnknownChunk chunk [[name("Skeleton Refs (0xFA8)")]];
    }
    else if (peekId == 0xFA9) {
        UnknownChunk chunk [[name("Version Data (0xFA9)")]];
    }
    else if (peekId == 0xFAB) {
        Chunk_FAB_IndexBuffer chunk [[name("Index Buffer (0xFAB)")]];
    }
    else if (peekId == 0xFAC) {
        Chunk_FAC_Metadata chunk [[name("File Metadata (0xFAC)")]];
    }

    // Unknown chunk
    else {
        UnknownChunk chunk [[name("Unknown Chunk")]];
    }
};

// =============================================================================
// MAIN FILE STRUCTURE
// =============================================================================

struct FFNAModelFile {
    char signature[4] [[name("FFNA Signature")]];   // "ffna" (0x616e6666)
    u8 fileType [[name("File Type")]];               // 2 = model, 5 = variant

    Chunk chunks[while($ < sizeof($))] [[name("Chunks")]];
};

FFNAModelFile modelFile @ 0;
