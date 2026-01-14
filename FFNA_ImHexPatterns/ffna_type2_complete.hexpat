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
    u8 boneGroupCount;          // 0x18: Max 4, each 28 bytes
    u8 textureGroupCount;       // 0x19: CRITICAL - 0=OLD UV, >0=MODERN UV
    u16 textureCount;           // 0x1A: Number of texture refs
    u8 maxBoneWeights;          // 0x1C: Max weights per vertex
    u8 maxBoneIndices;          // 0x1D: Max bone indices
    u16 materialCount;          // 0x1E: Number of materials
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
    u8 lodDistanceScale;        // 0x14
    u8 unknown_0x15;
    u8 unknown_0x16;
    u8 unknown_0x17;
    u8 shaderCount;             // 0x18: Number of shaders
    u8 materialBaseCount;       // 0x19
    u16 textureRefCount;        // 0x1A
    u8 shaderParamCount;        // 0x1C: Params per shader
    u8 materialConstCount;      // 0x1D
    u16 textureRangeCount;      // 0x1E
    u32 uvSetFlags;             // 0x20
    u32 vertexGroupCount;       // 0x24
    u32 colorDataCount;         // 0x28
    u32 normalDataCount;        // 0x2C
    u8 boneInfoCount;           // 0x30: 28 bytes each
    u8 skeletonBoneCount;       // 0x31: From classFlags & 0x200
    u8 skeletonWeightCount;     // 0x32
    u8 padding_0x33;
    u32 emitterDataSize;        // 0x34
    float boundingSphereX;      // 0x38
    float boundingSphereY;      // 0x3C
    float boundingSphereZ;      // 0x40
    float boundingSphereRadius; // 0x44
    u32 animationCount;         // 0x48
    u16 lodSubmeshCount;        // 0x4C
    u16 extLodSubmeshCount;     // 0x4E
    u16 boneDataEntryCount;     // 0x50
    u16 attachmentPointCount;   // 0x52
};

// Submesh Header for FA0 (36 bytes = 0x24)
struct SubmeshHeader_FA0 {
    u16 materialIndex;
    u16 submeshFlags;
    u32 indexCount;
    u32 indexBufferOffset;      // Or secondary index count
    u32 vertexCount;
    u32 vertexFormat;           // FVF flags
    u32 uvSetCount;
    u32 vertexColorCount;
    u32 normalCount;
    u32 triangleGroupCount;
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
u8 g_boneGroupCount;
u16 g_materialCount;

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

struct Chunk_BB8_Geometry {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;
    g_chunkDataSize = chunkSize;

    GeometryHeader_BB8 header [[name("Geometry Header (48 bytes)")]];

    // Store header values for submesh parsing
    g_isOldUVFormat = (header.textureGroupCount == 0);
    g_textureGroupCount = header.textureGroupCount;
    g_boneGroupCount = header.boneGroupCount;
    g_materialCount = header.materialCount;

    // Bone groups (if classFlags.HAS_BONE_GROUPS)
    if (header.classFlags.HAS_BONE_GROUPS && header.boneGroupCount > 0) {
        u32 boneGroupArrayCount [[name("Bone Group Count")]];
        if (boneGroupArrayCount <= 4) {
            BoneGroup boneGroups[boneGroupArrayCount] [[name("Bone Groups")]];
        }
    }

    // Bone weights (if classFlags.HAS_BONE_WEIGHTS)
    if (header.classFlags.HAS_BONE_WEIGHTS) {
        u32 boneWeightSetCount [[name("Bone Weight Set Count")]];
        // Each set has 24-byte header + variable data
        // We'll just read the raw data for now
    }

    // Remaining geometry data (materials, textures, submeshes, etc.)
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u32 remainingSize = chunkSize - consumed;
        u8 geometryData[remainingSize] [[name("Geometry Data (materials, submeshes, etc.)")]];
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

struct Chunk_FA0_Geometry {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;

    GeometryHeader_FA0 header [[name("Geometry Header (84 bytes)")]];

    // Bone info structures
    if (header.boneInfoCount > 0) {
        BoneGroup boneInfos[header.boneInfoCount] [[name("Bone Info Structures")]];
    }

    // Shader definitions
    if (header.shaderCount > 0) {
        ShaderDefinition shaders[header.shaderCount] [[name("Shader Definitions")]];

        // Per-shader texture flags
        if (header.shaderParamCount > 0) {
            u16 textureFlags[header.shaderParamCount] [[name("Texture Flags")]];
            u8 textureIndices[header.shaderParamCount] [[name("Texture Indices")]];
            u32 transformHashes[header.shaderParamCount] [[name("Transform Hashes")]];
            u8 blendStates[header.shaderParamCount] [[name("Blend States")]];
            u8 uvChannels[header.shaderParamCount] [[name("UV Channels")]];

            if (header.uvSetFlags != 0) {
                u8 extraUvData[header.shaderParamCount] [[name("Extra UV Data")]];
            }
        }
    }

    // Remaining geometry data
    u32 consumed_fa0 = $ - g_chunkDataStart;
    if (consumed_fa0 < chunkSize) {
        u32 remainingSize = chunkSize - consumed_fa0;
        u8 remainingData[remainingSize] [[name("Submesh & Vertex Data")]];
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
