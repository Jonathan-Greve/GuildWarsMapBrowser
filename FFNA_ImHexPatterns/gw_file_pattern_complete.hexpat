// =============================================================================
// FFNA Type 2 Model File - COMPLETE UNIFIED PATTERN
// Handles ALL chunk types: 0xFA* (runtime) and 0xBB* (DAT original)
//
// Reverse engineered from Gw.exe using Ghidra:
//   - Model_ParseGeometryHeader (0x00769950)
//   - Model_ParseMaterialData (0x00768e00)
//   - Model_ParseBoneData (0x007691c0)
//   - Model_ParseShaderData (0x00769930)
//   - Model_ParseAttachmentPoints (0x00769d00)
//   - Model_ParseEmitterData (0x00769e70)
//   - Model_ParseAnimationData (0x0076aa40)
//   - Model_ParseSkeletonData (0x0076aba0)
//   - MdlDecomp_ConvertSubmesh (0x0076e1c0)
//   - MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0 (0x0076d960)
//   - MdlDecomp_ConvertGeometryChunk_0xBB9_to_0xFA1 (0x0076ed60)
//   - MdlDecomp_ConvertAllChunks (0x00771090)
//   - MdlDecomp_ConvertBoneWeights (0x0076e4e0)
//   - MdlDecomp_ParseSubmeshVertexData (0x0076b6a0)
//   - Model_DecompressQuaternionKeys (0x00770e60)
//   - Model_ExpandUnsignedDeltaVLE (0x00771440)
//   - Model_ExpandSignedDeltaVLE (0x00771340)
//   - Model_UpdateSkeletonTransforms (0x00754670)
//   - Model_ApplyBoneTransform (0x007561e0)
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
// Already FA* format in DAT (no BB* source):
//   0xFA3 (4003): Inline ATEX DXT3 texture
//   0xFAA (4010): Inline ATEX DXTA texture
//
// Created during MdlDecomp_ConvertAllChunks (no BB* source):
//   0xFA7 (4007): Bounding cylinder data (from geometry processing)
//   0xFA8 (4008): Skeleton references (from animation processing)
//   0xFAC (4012): File metadata (version + counts)

// =============================================================================
// FVF (Flexible Vertex Format) Lookup Functions
// From MdlFvf_ConvertToGrFVF and GetVertexSizeFromFVF @ 0x0065f440
// =============================================================================

// FVF Lookup Tables from Gw.exe
// fvf_array_0: UV/TexCoord sizes - indexed by bits 8-11 and 12-15 of FVF
fn get_fvf_uv_size(u32 idx) {
    if (idx == 0) return 0x00;
    if (idx == 1) return 0x08;   // float2 UV (8 bytes)
    if (idx == 2) return 0x08;
    if (idx == 3) return 0x10;   // 2x float2 (16 bytes)
    if (idx == 4) return 0x08;
    if (idx == 5) return 0x10;
    if (idx == 6) return 0x10;
    if (idx == 7) return 0x18;   // 3x float2 (24 bytes)
    if (idx == 8) return 0x08;
    if (idx == 9) return 0x10;
    if (idx == 10) return 0x10;
    if (idx == 11) return 0x18;
    if (idx == 12) return 0x10;
    if (idx == 13) return 0x18;
    if (idx == 14) return 0x18;
    if (idx == 15) return 0x20;  // 4x float2 (32 bytes)
    // Special entries 16-21
    if (idx == 16) return 0x00;
    if (idx == 17) return 0x00;
    if (idx == 18) return 0x00;
    if (idx == 19) return 0x01;  // Special marker
    if (idx == 20) return 0xFFFFFFFF;
    if (idx == 21) return 0xFFFFFFFF;
    return 0x00;
};

// fvf_array_1: Normal/Tangent sizes - indexed by bits 4-6 of FVF
fn get_fvf_normal_size(u32 idx) {
    if (idx == 0) return 0x00;   // No normals
    if (idx == 1) return 0x0C;   // float3 normal (12 bytes)
    if (idx == 2) return 0x0C;
    if (idx == 3) return 0x18;   // float3 normal + float3 tangent (24 bytes)
    if (idx == 4) return 0x0C;
    if (idx == 5) return 0x18;
    if (idx == 6) return 0x18;
    if (idx == 7) return 0x24;   // normal + tangent + bitangent (36 bytes)
    return 0x00;
};

// fvf_array_2: Position/Weight sizes - indexed by bits 0-3 of FVF
fn get_fvf_position_size(u32 idx) {
    if (idx == 0) return 0x00;   // No position
    if (idx == 1) return 0x0C;   // float3 position (12 bytes)
    if (idx == 2) return 0x04;   // compressed position
    if (idx == 3) return 0x10;   // float3 + 1 weight (16 bytes)
    if (idx == 4) return 0x0C;   // float3
    if (idx == 5) return 0x18;   // float3 + 2 weights (24 bytes)
    if (idx == 6) return 0x10;   // float3 + 1 weight
    if (idx == 7) return 0x1C;   // float3 + 3 weights + 4 bone indices (28 bytes)
    if (idx == 8) return 0x04;   // compressed
    if (idx == 9) return 0x10;
    if (idx == 10) return 0x08;
    if (idx == 11) return 0x14;
    if (idx == 12) return 0x10;
    if (idx == 13) return 0x1C;
    if (idx == 14) return 0x14;
    if (idx == 15) return 0x20;  // 32 bytes
    return 0x00;
};

// Convert DAT FVF format to internal GR FVF format
fn get_fvf(u32 dat_fvf) {
    return (dat_fvf & 0xff0) << 4 | (dat_fvf >> 8) & 0x30 | dat_fvf & 0xf;
};

// Calculate vertex size from FVF flags
fn get_vertex_size_from_fvf(u32 fvf) {
    return get_fvf_uv_size((fvf >> 12) & 0xf) +    // bits 12-15: second texcoord set
           get_fvf_uv_size((fvf >> 8) & 0xf) +     // bits 8-11: first texcoord set
           get_fvf_normal_size((fvf >> 4) & 0x7) + // bits 4-6: normal/tangent flags
           get_fvf_position_size(fvf & 0xf);       // bits 0-3: position/weight flags
};

// =============================================================================
// VERTEX STRUCTURE (FVF-based)
// =============================================================================

// Vertex structure that parses based on DAT FVF format
// The position component (pos_idx) can include bone binding and normal data
// for skinned meshes, reducing the need for separate normal component
struct Vertex<auto dat_fvf> {
    u32 fvf = get_fvf(dat_fvf);
    u32 pos_idx = fvf & 0xf;
    u32 norm_idx = (fvf >> 4) & 0x7;
    u32 uv1_idx = (fvf >> 8) & 0xf;
    u32 uv2_idx = (fvf >> 12) & 0xf;

    // Position component (based on pos_idx)
    // For skinned meshes, this includes position + bone data + normal
    if (pos_idx == 1) {
        float position[3];      // 12 bytes: xyz
    } else if (pos_idx == 2) {
        u32 compressedPos;      // 4 bytes: compressed
    } else if (pos_idx == 3) {
        float position[3];      // 12 bytes: xyz
        u32 boneIndex;          // 4 bytes: bone group index (Ghidra may call this "group")
    } else if (pos_idx == 4) {
        float position[3];      // 12 bytes
    } else if (pos_idx == 5) {
        float position[3];      // 12 bytes: xyz
        float weights[2];       // 8 bytes: 2 bone weights
        u32 boneIndices;        // 4 bytes: packed bone indices
    } else if (pos_idx == 6) {
        float position[3];      // 12 bytes
        u32 boneIndex;          // 4 bytes: bone group index
    } else if (pos_idx == 7) {
        float position[3];      // 12 bytes: xyz
        u32 boneIndex;          // 4 bytes: single bone index (0, 1, 2, etc.)
        float normal[3];        // 12 bytes: unit normal vector (included in pos component)
    } else if (pos_idx == 8) {
        u32 compressedPos;      // 4 bytes
    } else if (pos_idx >= 9 && pos_idx <= 15) {
        u8 posData[get_fvf_position_size(pos_idx)];
    }

    // Separate Normal/Tangent component (based on norm_idx)
    // Only used when normal is NOT included in position component
    if (norm_idx == 1 || norm_idx == 2 || norm_idx == 4) {
        float normal[3];        // 12 bytes: normal xyz
    } else if (norm_idx == 3 || norm_idx == 5 || norm_idx == 6) {
        float normal[3];        // 12 bytes: normal xyz
        float tangent[3];       // 12 bytes: tangent xyz
    } else if (norm_idx == 7) {
        float normal[3];        // 12 bytes
        float tangent[3];       // 12 bytes
        float bitangent[3];     // 12 bytes
    }

    // UV Set 1 (based on uv1_idx)
    if (uv1_idx == 1 || uv1_idx == 2 || uv1_idx == 4 || uv1_idx == 8) {
        float uv[2];            // 8 bytes: uv
    } else if (uv1_idx == 3 || uv1_idx == 5 || uv1_idx == 6 || uv1_idx == 9 || uv1_idx == 10 || uv1_idx == 12) {
        float uv[4];            // 16 bytes: 2x uv
    } else if (uv1_idx == 7 || uv1_idx == 11 || uv1_idx == 13 || uv1_idx == 14) {
        float uv[6];            // 24 bytes: 3x uv
    } else if (uv1_idx == 15) {
        float uv[8];            // 32 bytes: 4x uv
    }

    // UV Set 2 (based on uv2_idx)
    if (uv2_idx == 1 || uv2_idx == 2 || uv2_idx == 4 || uv2_idx == 8) {
        float uv2[2];           // 8 bytes
    } else if (uv2_idx == 3 || uv2_idx == 5 || uv2_idx == 6 || uv2_idx == 9 || uv2_idx == 10 || uv2_idx == 12) {
        float uv2[4];           // 16 bytes
    } else if (uv2_idx == 7 || uv2_idx == 11 || uv2_idx == 13 || uv2_idx == 14) {
        float uv2[6];           // 24 bytes
    } else if (uv2_idx == 15) {
        float uv2[8];           // 32 bytes
    }
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
// VLE (Variable Length Encoding) DECODER FUNCTIONS
// =============================================================================
// From Model_ExpandUnsignedDeltaVLE @ 0x00771440
// From Model_ExpandSignedDeltaVLE @ 0x00771340
//
// VLE Byte Format:
//   First byte: [Cont:1][Sign:1][Data:6]
//   Subsequent: [Cont:1][Data:7]
//
// Returns: (value, bytes_consumed)

// Read a single VLE value and return the number of bytes consumed
fn vle_get_byte_count(u32 address) {
    u8 b0 @ address;
    if ((b0 & 0x80) == 0) return 1;

    u8 b1 @ address + 1;
    if ((b1 & 0x80) == 0) return 2;

    u8 b2 @ address + 2;
    if ((b2 & 0x80) == 0) return 3;

    u8 b3 @ address + 3;
    if ((b3 & 0x80) == 0) return 4;

    return 5;  // Max 5 bytes
};

// Decode VLE value at address (returns raw unsigned value, NOT delta-decoded)
fn vle_decode_value(u32 address) {
    u8 b0 @ address;
    u32 value = b0 & 0x3F;  // First 6 bits

    if ((b0 & 0x80) == 0) return value;

    u8 b1 @ address + 1;
    value = value | ((b1 & 0x7F) << 6);
    if ((b1 & 0x80) == 0) return value;

    u8 b2 @ address + 2;
    value = value | ((b2 & 0x7F) << 13);
    if ((b2 & 0x80) == 0) return value;

    u8 b3 @ address + 3;
    value = value | ((b3 & 0x7F) << 20);
    if ((b3 & 0x80) == 0) return value;

    u8 b4 @ address + 4;
    value = value | (b4 << 27);
    return value;
};

// Check if VLE value at address has positive sign (bit 6 of first byte)
fn vle_is_positive(u32 address) {
    u8 b0 @ address;
    return (b0 & 0x40) != 0;
};

// Calculate total bytes needed to read N VLE values starting at address
fn vle_calc_total_bytes(u32 address, u32 count) {
    u32 total = 0;
    u32 i = 0;
    while (i < count) {
        total = total + vle_get_byte_count(address + total);
        i = i + 1;
    }
    return total;
};

// =============================================================================
// VLE VALUE STRUCTURES
// =============================================================================

// Forward declaration for BoneAnimHeader (defined later in BB9 section)
using BoneAnimHeader;

// Single VLE-encoded value (variable 1-5 bytes)
// Displays raw value and sign for inspection
struct VLEValue {
    u32 byteCount = vle_get_byte_count($);
    u32 rawValue = vle_decode_value($);
    bool signPositive = vle_is_positive($);
    u8 bytes[byteCount] [[name("VLE Bytes")]];
} [[format("format_vle_value")]];

fn format_vle_value(VLEValue v) {
    if (v.signPositive) {
        return std::format("+{} ({} bytes)", v.rawValue, v.byteCount);
    }
    return std::format("-{} ({} bytes)", v.rawValue, v.byteCount);
};

// Position keyframe (12 bytes = float3)
struct PositionKeyframe {
    float x, y, z;
} [[format("format_pos_keyframe")]];

fn format_pos_keyframe(PositionKeyframe p) {
    return std::format("({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
};

// Rotation keyframe: 3 VLE-encoded Euler angle deltas (variable size, 3-15 bytes)
// Each component is VLE delta-encoded, representing Euler angles [0, 65535] -> [-π, π]
// Decoding: new_value = prev ± value (wraps at 16 bits)
struct RotationKeyframe {
    VLEValue eulerX [[name("Euler X (VLE delta)")]];
    VLEValue eulerY [[name("Euler Y (VLE delta)")]];
    VLEValue eulerZ [[name("Euler Z (VLE delta)")]];
};

// Scale keyframe (12 bytes = float3)
struct ScaleKeyframe {
    float x, y, z;
} [[format("format_scale_keyframe")]];

fn format_scale_keyframe(ScaleKeyframe s) {
    return std::format("({:.2f}, {:.2f}, {:.2f})", s.x, s.y, s.z);
};

// Complete bone keyframe data (variable size)
// Parses all VLE-encoded times and values for a single bone
struct BoneKeyframeData<auto posKeyCount, auto rotKeyCount, auto scaleKeyCount> {
    // Position keyframes
    if (posKeyCount > 0) {
        VLEValue positionTimes[posKeyCount] [[name("Position Times (VLE)")]];
        PositionKeyframe positionValues[posKeyCount] [[name("Position Values")]];
    }

    // Rotation keyframes
    if (rotKeyCount > 0) {
        VLEValue rotationTimes[rotKeyCount] [[name("Rotation Times (VLE)")]];
        RotationKeyframe rotationValues[rotKeyCount] [[name("Rotation Values (Delta u16)")]];
    }

    // Scale keyframes
    if (scaleKeyCount > 0) {
        VLEValue scaleTimes[scaleKeyCount] [[name("Scale Times (VLE)")]];
        ScaleKeyframe scaleValues[scaleKeyCount] [[name("Scale Values")]];
    }
};

// Complete bone animation entry (header + keyframe data)
struct BoneAnimationEntry {
    BoneAnimHeader header [[name("Bone Header (22 bytes)")]];

    // Parse keyframe data if any keyframes exist
    if (header.posKeyCount > 0 || header.rotKeyCount > 0 || header.scaleKeyCount > 0) {
        BoneKeyframeData<header.posKeyCount, header.rotKeyCount, header.scaleKeyCount>
            keyframes [[name("Keyframe Data")]];
    }
};

// =============================================================================
// ClassFlags Bitmask (offset 0x08 in geometry headers)
// From MdlDecomp_ConvertGeometryChunk_0xBB8_to_0xFA0
// =============================================================================
bitfield ClassFlags {
    HAS_SKELETON : 1;              // 0x001: Has skeleton reference
    HAS_BONE_GROUPS : 1;           // 0x002: Bone group data (max 4, 28 bytes each)
    HAS_BOUNDING_DATA : 1;         // 0x004: Bounding box/sphere data
    HAS_ATTACHMENT_DATA : 1;       // 0x008: Has attachment points data (f0x52 section)
    HAS_LOD_DATA : 1;              // 0x010: Level-of-detail data
    HAS_EMBEDDED_ANIMATION : 1;    // 0x020: Embedded vertex animation (morph targets)
    HAS_BONE_WEIGHTS : 1;          // 0x040: Per-vertex bone weights
    HAS_MORPH_DATA : 1;            // 0x080: Morph target/attachment data
    HAS_ANIMATION_SEQUENCES : 1;   // 0x100: Animation sequence data (internal to model)
    HAS_SKELETON_DATA : 1;         // 0x200: Skeleton/joint hierarchy (max 32 bones)
    HAS_EXTENDED_LOD : 1;          // 0x400: Extended LOD data
    HAS_CLOTH_DATA : 1;            // 0x800: Cloth simulation data
    HAS_SCALE_DATA : 1;            // 0x1000: Scale transform data
    UNKNOWN_0x2000 : 1;            // 0x2000
    UNKNOWN_0x4000 : 1;            // 0x4000
    HAS_TEX_COORDS : 1;            // 0x8000: Texture coordinates
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

// Single Bone Weight Entry (7 bytes)
// From MdlDecomp_ConvertBoneWeights: weightCount * 7 bytes
struct BoneWeightEntry {
    u32 vertexIndex;          // Which vertex this weight applies to
    u8 boneIndex;             // Bone index
    u8 weight;                // Weight value (0-255, normalize to 0.0-1.0)
    u8 flags;                 // Additional flags
};

// Bone Transform Entry (8 bytes)
// From MdlDecomp_ConvertBoneWeights: (boneCount0 + boneCount1 + boneCount2) * 8 bytes
struct BoneTransformEntry {
    u32 boneHash;             // Bone identifier hash
    u32 transformFlags;       // Transform flags or offset
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

// File Reference Entry (6 bytes)
// Used in FA5/FA6/BBB/BBC chunks to reference external files
// Can be textures, models, sounds, or other assets for composite models
// Decode to file ID: (id0 - 0xff00ff) + (id1 * 0xff00)
struct FileReferenceEntry {
    u16 id0;                // Encoded part 1
    u16 id1;                // Encoded part 2
    u16 unknown;            // Usually 0, possibly flags or index
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

// Bounding Entry Type A (88 bytes) - References external objects
struct BoundingEntryTypeA {
    u32 entryType;          // 0x00: Type flag (usually 1)
    u32 refCount;           // 0x04: Reference count or index
    u32 objectIndex;        // 0x08: Index into some object array
    u32 objectHash;         // 0x0C: Hash identifier
    float radius;           // 0x10: Bounding radius
    u32 padding1[7];        // 0x14-0x2F: Usually zeros
    float dimX;             // 0x30: Dimension X
    float dimY;             // 0x34: Dimension Y
    float dimZ;             // 0x38: Dimension Z
    u32 padding2[5];        // 0x3C-0x4F: Usually zeros
    u32 flags;              // 0x50: Additional flags
    u32 reserved;           // 0x54: Usually 0
};

// Bounding Entry Type B (80 bytes) - Bounding cylinder/box definition
struct BoundingEntryTypeB {
    u32 entryType;          // 0x00: Type flag (usually 1)
    float posX;             // 0x04: Position X
    float posY;             // 0x08: Position Y
    float posZ;             // 0x0C: Position Z
    u32 padding1[2];        // 0x10-0x17: Usually zeros
    float scale;            // 0x18: Scale factor
    u32 padding2;           // 0x1C: Usually 0
    float boundsVal0;       // 0x20: Bounds value 0
    float boundsVal1;       // 0x24: Bounds value 1
    float boundsVal2;       // 0x28: Bounds value 2
    u32 padding3[5];        // 0x2C-0x3F: Usually zeros
    float extentNegX;       // 0x40: Negative X extent
    float extentPosX;       // 0x44: Positive X extent
    float scaleX;           // 0x48: Scale X
    float scaleY;           // 0x4C: Scale Y
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

// RGB Color (3 bytes) - used in submesh color data
struct ColorRGB {
    u8 r, g, b;
};

// Triangle Group Entry (12 bytes) - face group with material assignment
struct TriangleGroup {
    u32 startIndex;         // Starting triangle index
    u32 triangleCount;      // Number of triangles
    u32 materialIndex;      // Material for this group
};

// Animation Keyframe (8 bytes) - from Buffer_CalcFrameOffset
// Each frame in animation data is 8 bytes
struct AnimationKeyframe {
    u32 frameTime;          // Keyframe time (ticks or normalized)
    u32 frameData;          // Transform index or packed data
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

// LOD Vertex Entry (12 bytes) - compressed vertex for LOD levels
// From LOD data formula: vertexCount * 6 * 2 = vertexCount * 12 bytes
struct LODVertexEntry {
    s16 posX;                   // Position X (normalized/scaled)
    s16 posY;                   // Position Y (normalized/scaled)
    s16 posZ;                   // Position Z (normalized/scaled)
    s16 normalX;                // Normal X (normalized)
    s16 normalY;                // Normal Y (normalized)
    s16 normalZ;                // Normal Z (normalized)
};

// Skeleton Node Entry (16 bytes per node)
// Used by both BB8 and FA0 skeleton parsing
struct SkeletonNode {
    float posX;                 // Position/offset X
    float posY;                 // Position/offset Y
    float posZ;                 // Position/offset Z
    u32 boneIndex;              // Bone identifier/index
};

// BB8 Skeleton Weight Entry (0x15 = 21 bytes)
// From MdlDecomp: skeletonWeightCount * 0x15 bytes
struct SkeletonWeightEntry_BB8 {
    u32 boneIndex;              // Bone reference
    float weightValue;          // Weight influence
    u32 vertexStart;            // Starting vertex
    u32 vertexCount;            // Number of affected vertices
    u8 flags;                   // Additional flags
    u32 transformOffset;        // Offset to transform data
};

// Embedded Animation Entry (0x2E = 46 bytes header + variable data)
// Used when classFlags.HAS_EMBEDDED_ANIMATION (0x20) is set
// Stores morph target / vertex animation data for models that don't use external
// skeletal animation files (BB9/FA1). Vertices deform directly via morph targets.
//
// anim_flags bits:
//   0x02 = Has compressed vertex deltas
//   0x40 = UV animation uses morph_target_count instead of uv_anim_frame_count
//   0x80000000 = Usually set
struct EmbeddedAnimationEntry {
    u32 keyframeData0Count;     // 0x00: Count for keyframe data section 0
    u32 keyframeData1Count;     // 0x04: Count for keyframe data section 1
    u32 unknown_08;             // 0x08: Unknown
    u32 animFlags;              // 0x0C: Animation flags (see above)
    u16 totalFrameCount;        // 0x10: Total frames in animation
    u8 unknown_12;              // 0x12: Unknown
    u8 boneWeightsPerVert;      // 0x13: Number of bone weights per vertex
    u16 morphTargetCount;       // 0x14: Number of morph target keyframes
    u32 compressedDataSize;     // 0x16: Size of compressed animation data
    u32 uvAnimFrameCount;       // 0x1A: UV animation frame count
    u32 positionDeltaCount;     // 0x1E: Position delta keyframe count
    u16 unknown_22;             // 0x22: Unknown
    u16 rotationKeyCount;       // 0x24: Rotation keyframe count
    u16 scaleKeyCount;          // 0x26: Scale keyframe count
    u16 baseVertexCount;        // 0x28: Base pose vertex count
    u32 extraDataSize;          // 0x2A: Extra animation data size
};

// Legacy alias for backwards compatibility
using GeosetEntry = EmbeddedAnimationEntry;

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
// From MdlDecomp_ParseSubmeshVertexData @ 0x0076b6a0
struct SubmeshHeader_FA0 {
    u16 materialIndex;          // 0x00: Material/shader index
    u16 submeshFlags;           // 0x02: Visibility, render flags
    u32 indexCount;             // 0x04: LOD0 (highest detail) index count
    u32 indexCount2;            // 0x08: LOD1 (medium detail) - adds to buffer if != indexCount
    u32 indexCount3;            // 0x0C: LOD2 (lowest detail) - adds to buffer if != indexCount2
    u32 vertexCount;            // 0x10: Number of vertices (shared across all LOD levels)
    u32 vertexFormat;           // 0x14: FVF flags (see get_fvf() for conversion)
    u32 boneGroupCount;         // 0x18: Number of bone groups (see BONE GROUP PALETTE docs above)
    u32 totalBoneRefs;          // 0x1C: sum(boneGroupSizes) - total entries in skeletonBoneIndices
    u32 triangleGroupCount;     // 0x20: Triangle strip/fan groups (usually 0 for indexed meshes)
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
// 2. Vertex buffer: vertexCount * vertexSize bytes (shared by all LOD levels)
// 3. Bone Group Sizes: boneGroupCount * 4 bytes (u32 array - bones per vertex group)
// 4. Skeleton Bone Indices: totalBoneRefs * 4 bytes (u32 array - flattened bone palette)
// 5. Triangle groups: triangleGroupCount * 12 bytes (strip/fan definitions) - usually 0
//
// =============================================================================
// BONE GROUP PALETTE SYSTEM (Hardware Skinning)
// =============================================================================
// GPU skinning optimization - vertices store a GROUP INDEX, not direct bone IDs.
// This allows the GPU to batch vertices by bone group for efficient matrix uploads.
//
// IMPORTANT: vertex.boneIndex is NOT a skeleton bone ID! It's a group index.
//
// Data Structure in FA0 Submesh:
//   vertex.boneIndex      = bone GROUP index (0 to boneGroupCount-1)
//   boneGroupSizes[N]     = number of skeleton bones affecting group N
//   skeletonBoneIndices[] = flattened array of all skeleton bone IDs for all groups
//   totalBoneRefs         = sum(boneGroupSizes) = length of skeletonBoneIndices
//
// Mapping Algorithm (to get skeleton bone for a vertex):
//   1. Read group_idx = vertex.boneIndex
//   2. Compute offset = sum(boneGroupSizes[0..group_idx-1])
//   3. skeleton_bone = skeletonBoneIndices[offset]  (first bone of the group)
//   4. For multi-bone blending, read boneGroupSizes[group_idx] bones starting at offset
//
// Python Implementation:
//   group_to_bone = []
//   offset = 0
//   for group_size in bone_group_sizes:
//       group_to_bone.append(skeleton_bone_indices[offset])
//       offset += group_size
//   skeleton_bone = group_to_bone[vertex.bone_group_idx]
//
// Example (from 0x1C805 female NPC model):
//   boneGroupCount = 99, totalBoneRefs = 161
//   boneGroupSizes = [1, 2, 2, 1, 1, 2, ...]
//   skeletonBoneIndices = [10, 9, 10, 8, 9, 9, ...]
//
//   Group 0: size=1 → bone 10 at offset 0
//   Group 1: size=2 → bones 9,10 at offset 1
//   Group 2: size=2 → bones 8,9 at offset 3
//   Group 3: size=1 → bone 9 at offset 5
//
// LINEAR BLEND SKINNING FORMULA:
//   bind_offset = vertex.position - bone.base_position
//   animated_vertex = bone.world_pos + quat_rotate(bone.world_rot, bind_offset)
// =============================================================================
struct SubmeshData_FA0 {
    SubmeshHeader_FA0 header [[name("Submesh Header")]];

    // Calculate total index count using the game's logic
    u32 totalIndexCount = calc_total_index_count(header.indexCount, header.indexCount2, header.indexCount3);

    // Index buffer (u16 indices)
    if (totalIndexCount > 0 && totalIndexCount < 100000) {
        u16 indices[totalIndexCount] [[name("Index Buffer")]];
    }

    // Vertex buffer - parsed using FVF-based Vertex struct
    // Vertices come IMMEDIATELY after indices (no gap)
    if (header.vertexCount > 0 && header.vertexCount < 100000) {
        Vertex<header.vertexFormat> vertices[header.vertexCount] [[name("Vertex Buffer")]];
    }

    // Extra data section 1 (4 bytes each - bone group sizes: how many bones per group)
    if (header.boneGroupCount > 0 && header.boneGroupCount < 100000) {
        u32 extraData1[header.boneGroupCount] [[name("Bone Group Sizes")]];
    }

    // Extra data section 2 (4 bytes each - skeleton bone indices for all groups)
    if (header.totalBoneRefs > 0 && header.totalBoneRefs < 100000) {
        u32 extraData2[header.totalBoneRefs] [[name("Skeleton Bone Indices")]];
    }

    // Triangle groups (12 bytes each based on formula derivation)
    if (header.triangleGroupCount > 0 && header.triangleGroupCount < 1000) {
        TriangleGroup triangleGroups[header.triangleGroupCount] [[name("Triangle Groups")]];
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

// BB9 Sequence Entry (24 bytes = 6 DWORDs)
// From MdlDecomp: puVar6 = puVar7 + count * 6 (uint* arithmetic)
//
// ANIMATION SEQUENCE SYSTEM:
// Each animation FILE contains ONE animation (e.g., walk cycle, attack, idle).
// Sequences are PHASES within that single animation, not separate animations.
//
// Examples of phases within a single animation:
//   Walk cycle: [left_step] → [right_step] → [left_step] (loops)
//   Attack:     [wind_up] → [swing] → [follow_through] → [recovery]
//   Idle:       [breathe_in] → [breathe_out] → [shift_weight]
//
// This allows the game to:
//   - Trigger events at specific phases (damage during swing, footstep sounds)
//   - Interrupt at safe points (cancel during wind_up, not during swing)
//   - Loop specific portions (walk cycles)
//   - Blend between animations knowing which phase you're in
//
// Time Calculation:
//   total_frames = sum(seq.frameCount for all sequences)
//   time_per_frame = (max_keyframe_time - min_keyframe_time) / (total_frames - 1)
//   seq.start_frame = sum of all previous sequences' frameCounts
//   seq.time_range = [min_time + start_frame*tpf, min_time + (start_frame+frameCount)*tpf]
//
// Note: Animation doesn't return to bind pose at end - the game blends to next animation.
//
struct SequenceEntry_BB9 {
    u32 animationId;        // Animation hash (unknown mapping to names)
    float boundX;           // Movement bounds (often -0.0)
    float boundY;           // Movement bounds
    float boundZ;           // Movement bounds
    u32 frameCount;         // Frames in this sequence (used to calculate time range)
    u32 sequenceIndex;      // Sequence grouping index
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

    // Position buffer (12 bytes per vertex = float3)
    VertexPosition positions[header.vertexCount] [[name("Vertex Positions")]];

    // Per-vertex extra data (4 bytes per vertex) - ALWAYS present
    // This is part of the fixed buffer layout: (indexCount + vertexCount * 8) * 2 bytes
    // Which equals: indexCount * 2 + vertexCount * 16 = indices + positions(12) + extra(4)
    VertexExtraData extraData[header.vertexCount] [[name("Per-Vertex Extra Data")]];

    // UV data section - always uses compression format with header + thresholds + offsets + deltas
    if (header.uvSetCount > 0) {
        UVCompressionHeader uvHeader [[name("UV Compression Header")]];
        u16 uThresholds[uvHeader.uOffsetCount] [[name("U Thresholds")]];
        u16 vThresholds[uvHeader.vOffsetCount] [[name("V Thresholds")]];
        s16 uOffsets[uvHeader.uOffsetCount] [[name("U Offsets")]];
        s16 vOffsets[uvHeader.vOffsetCount] [[name("V Offsets")]];
        UV_Delta uvDeltas[header.vertexCount * header.uvSetCount] [[name("UV Delta Data")]];
    }

    // Color data (colorCount * 3 bytes each - RGB)
    // From Ghidra: colorCount * 3 bytes in buffer size formula
    if (header.colorCount > 0) {
        ColorRGB colors[header.colorCount] [[name("Vertex Colors (RGB)")]];
    }

    // Normal indices (normalCount * 1 byte each)
    // From Ghidra: normalCount bytes in buffer size formula
    if (header.normalCount > 0) {
        u8 normalIndices[header.normalCount] [[name("Normal Indices")]];
    }

    // Triangle group data (triangleGroupCount * 12 bytes each)
    // From Ghidra: triGroupCount * 3 * 4 bytes in buffer size formula
    if (header.triangleGroupCount > 0) {
        TriangleGroup triangleGroups[header.triangleGroupCount] [[name("Triangle Groups")]];
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

// Animation Entry for BB8 (variable size)
// Each entry: dataSize (u32), then dataSize * 2 DWORDs of keyframe data
// From Ghidra: puVar11 = puVar9 + *puVar11 * 2; (uint* arithmetic = *8 bytes)
struct AnimationEntry_BB8 {
    u32 dataSize [[name("Data Size")]];
    if (dataSize > 0 && dataSize < 0x10000) {
        u32 keyframeData[dataSize * 2] [[name("Keyframe Data")]];
    }
};

// LOD Entry for BB8 (variable size)
// From Ghidra: each entry has (indexCount, vertexCount) header
// followed by (indexCount + vertexCount * 6) * 2 bytes of data
// Data structure: indexCount * 2 bytes (u16 indices) + vertexCount * 12 bytes (LODVertexEntry)
struct LODEntry_BB8 {
    u32 indexCount [[name("Index Count")]];
    u32 vertexCount [[name("Vertex Count")]];

    // Index buffer (u16 each)
    if (indexCount > 0 && indexCount < 100000) {
        u16 indices[indexCount] [[name("LOD Index Buffer")]];
    }
    // Vertex buffer (12 bytes per vertex)
    if (vertexCount > 0 && vertexCount < 100000) {
        LODVertexEntry vertices[vertexCount] [[name("LOD Vertex Buffer")]];
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

    // Embedded animation data (if classFlags.HAS_EMBEDDED_ANIMATION = 0x20)
    // Models with this flag use vertex animation (morph targets) instead of external skeletal animation
    if (header.classFlags.HAS_EMBEDDED_ANIMATION) {
        u32 embeddedAnimTotalSize [[name("Embedded Animation Total Size")]];
        u32 embeddedAnimCount [[name("Embedded Animation Count")]];
        if (embeddedAnimCount > 0 && embeddedAnimCount < 256) {
            EmbeddedAnimationEntry embeddedAnims[embeddedAnimCount] [[name("Embedded Animation Entries")]];
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

    // Submeshes (if classFlags.HAS_ATTACHMENT_DATA = 0x08)
    // Note: In BB8 format, this flag controls submesh data presence
    if (header.classFlags.HAS_ATTACHMENT_DATA) {
        u32 submeshCount [[name("Submesh Count")]];
        if (submeshCount > 0 && submeshCount < 255) {
            SubmeshData_BB8 submeshes[submeshCount] [[name("Submeshes")]];
        }
    }

    // LOD data (if classFlags.HAS_LOD_DATA = 0x10)
    // From Ghidra: each entry has (indexCount, vertexCount) header
    // followed by (indexCount + vertexCount * 6) * 2 bytes of data
    if (header.classFlags.HAS_LOD_DATA) {
        u32 lodCount_bb8 [[name("LOD Count")]];
        if (lodCount_bb8 > 0 && lodCount_bb8 < 256) {
            LODEntry_BB8 lodEntries_bb8[lodCount_bb8] [[name("LOD Entries")]];
        }
    }

    // Extended LOD data (if classFlags.HAS_EXTENDED_LOD = 0x400)
    // Similar structure to LOD but with 3 header values instead of 2
    if (header.classFlags.HAS_EXTENDED_LOD) {
        u32 extLodCount_bb8 [[name("Extended LOD Count")]];
        // Each entry: unknown0, indexCount, vertexCount, then (indexCount + vertexCount * 6) * 2 bytes
        // Using raw bytes for now until structure is verified
        if (extLodCount_bb8 > 0 && extLodCount_bb8 < 256) {
            u8 extLodData[$ - g_chunkDataStart < chunkSize ? 0 : 0] [[name("Extended LOD Data (placeholder)")]];
        }
    }

    // Skeleton data (if classFlags.HAS_SKELETON_DATA = 0x200)
    // From Ghidra: boneCount (u32), weightCount (u32), then:
    //   boneCount * 16 bytes + weightCount * 0x15 bytes
    if (header.classFlags.HAS_SKELETON_DATA) {
        u32 skeletonBoneCount_bb8 [[name("Skeleton Bone Count")]];
        u32 skeletonWeightCount_bb8 [[name("Skeleton Weight Count")]];
        if (skeletonBoneCount_bb8 > 0 && skeletonBoneCount_bb8 <= 32) {
            SkeletonNode skeletonNodes_bb8[skeletonBoneCount_bb8] [[name("Skeleton Nodes")]];
        }
        if (skeletonWeightCount_bb8 > 0 && skeletonWeightCount_bb8 < 256) {
            SkeletonWeightEntry_BB8 skeletonWeights_bb8[skeletonWeightCount_bb8] [[name("Skeleton Weight Entries")]];
        }
    }

    // Animation sequence data (if classFlags.HAS_ANIMATION_SEQUENCES = 0x100)
    // Internal animation sequences stored within the model (not external BB9/FA1 files)
    // From Ghidra: count (u32), then for each: dataSize (u32) + dataSize * 2 bytes
    if (header.classFlags.HAS_ANIMATION_SEQUENCES) {
        u32 animCount_bb8 [[name("Animation Sequence Count")]];
        if (animCount_bb8 > 0 && animCount_bb8 < 256) {
            AnimationEntry_BB8 animEntries_bb8[animCount_bb8] [[name("Animation Sequence Entries")]];
        }
    }

    // Bounding data (if classFlags.HAS_BOUNDING_DATA = 0x04)
    // From Ghidra: first DWORD is extra size, next 3 DWORDs are counts/values
    // Total = 16 bytes header + extraSize bytes
    // Extra data structure: count1 * 88 bytes (type A) + count2 * 80 bytes (type B)
    if (header.classFlags.HAS_BOUNDING_DATA) {
        u32 boundingExtraSize [[name("Bounding Extra Size")]];
        u32 boundingCountA [[name("Bounding Count Type A (88 bytes each)")]];
        u32 boundingCountB [[name("Bounding Count Type B (80 bytes each)")]];
        u32 boundingValue3 [[name("Bounding Value 3")]];
        if (boundingExtraSize > 0 && boundingExtraSize < 0x100000) {
            // Type A entries (88 bytes each) - external object references
            if (boundingCountA > 0 && boundingCountA < 256) {
                BoundingEntryTypeA boundingEntriesA[boundingCountA] [[name("Bounding Entries Type A")]];
            }
            // Type B entries (80 bytes each) - bounding cylinder/box definitions
            if (boundingCountB > 0 && boundingCountB < 256) {
                BoundingEntryTypeB boundingEntriesB[boundingCountB] [[name("Bounding Entries Type B")]];
            }
        }
    }

    // Remaining data (if any)
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u32 remainingSize = chunkSize - consumed;
        u8 remainingData[remainingSize] [[name("Remaining Unparsed Data")]];
    }
};

// =============================================================================
// BB9 ANIMATION CHUNK PARSING
// =============================================================================

// BB9 Flag Bits:
// 0x0001: Unknown
// 0x0008: HAS_SEQUENCE_DATA - animation sequence entries
// 0x0010: HAS_BONE_TRANSFORMS_EXTENDED - compressed bone animation data
// 0x0020: HAS_BONE_TRANSFORMS - standard bone transforms
// 0x0040: HAS_BONE_WEIGHT_DATA
// 0x0080: HAS_ATTACHMENT_DATA
// 0x0100: HAS_SOUND_EFFECT_DATA (0x16 byte entries -> 0x17 output)
// 0x0200: HAS_JOINT_DATA
// 0x0400: Unknown section
// 0x0800: Unknown section (count * 2 DWORDs)
// 0x1000: Unknown section (count * 5 bytes each)
// 0x2000: Unknown section (count * 5 bytes each)
// 0x4000: HAS_PER_BONE_DATA - variable size per-bone entries
// 0x8000: HAS_TEXTURE_COORDINATES

// Bone Transform Header (8 bytes) - for flag 0x10
struct BoneTransformHeader_BB9 {
    u32 boneCount;          // Number of bone transform entries
    u32 unknown;            // Secondary header value
};

// Per-Bone Animation Header (22 bytes) - appears after BoneTransformHeader_BB9
// Each bone has this header followed by variable-length VLE-compressed keyframe data
//
// KEY DISCOVERIES:
//   1. base_position is ABSOLUTE world coordinate in bind pose, NOT relative to parent
//   2. hierarchyDepth is used ONLY for matrix stack ops during animation playback
//   3. Parent-child relationships are computed from depth changes (see algorithm below)
//
// PARENT COMPUTATION ALGORITHM:
//   - If depth INCREASES from previous bone: previous bone is parent (child relationship)
//   - If depth DECREASES or stays same: look up bone at depth-1 (sibling relationship)
//   - Multiple bones at same depth are siblings sharing the same parent
//
struct BoneAnimHeader {
    float basePositionX;    // 0x00: Bone's ABSOLUTE X position in bind pose (world space)
    float basePositionY;    // 0x04: Bone's ABSOLUTE Y position in bind pose (world space)
    float basePositionZ;    // 0x08: Bone's ABSOLUTE Z position in bind pose (world space)
    u8 hierarchyDepth;      // 0x0C: Hierarchy depth for matrix stack push/pop during animation
    u8 flagsByte1;          // 0x0D: Additional flags (unused?)
    u8 flagsByte2;          // 0x0E: Additional flags (unused?)
    u8 flagsByte3;          // 0x0F: Additional flags (unused?)
    u16 posKeyCount;        // 0x10: Number of position keyframes (animated delta from base)
    u16 rotKeyCount;        // 0x12: Number of rotation keyframes (Euler -> quaternion)
    u16 scaleKeyCount;      // 0x14: Number of scale keyframes
    // Total: 22 bytes (0x16)
    //
    // Followed by VLE-compressed keyframe data in this order:
    //   1. Position times: VLE unsigned delta-of-delta (posKeyCount values)
    //   2. Position values: float3 * posKeyCount (12 bytes each, delta from base_position)
    //   3. Rotation times: VLE unsigned delta-of-delta (rotKeyCount values)
    //   4. Rotation values: VLE signed delta per component (3 VLE values per key)
    //   5. Scale times: VLE unsigned delta-of-delta (scaleKeyCount values)
    //   6. Scale values: float3 * scaleKeyCount (12 bytes each)
    //
    // ANIMATION SKINNING FORMULA:
    //   vertex_offset = vertex.position - bone.base_position  (in bind pose)
    //   animated_vertex = bone.world_position + quaternion_rotate(bone.world_rotation, vertex_offset)
};

// VLE (Variable Length Encoding) format documentation:
// From Model_ExpandUnsignedDeltaVLE @ 0x00771440 and Model_ExpandSignedDeltaVLE @ 0x00771340
//
// UNSIGNED DELTA-OF-DELTA VLE (for keyframe TIMES):
//   First byte format:
//     Bit 7 (0x80): Continuation flag (1 = more bytes follow)
//     Bit 6 (0x40): Sign (0 = negative delta, 1 = positive delta)
//     Bits 0-5 (0x3F): 6 bits of value
//   Subsequent bytes:
//     Bit 7: Continuation flag
//     Bits 0-6: 7 bits of value
//
//   Decoding (up to 5 bytes):
//     value = byte0[0:5] | byte1[0:6]<<6 | byte2[0:6]<<13 | byte3[0:6]<<20 | byte4<<27
//     delta = (sign_positive) ? +value : -value
//     new_value = (2 * prev1 - prev2) + delta  // Second-order prediction
//
// SIGNED DELTA VLE (for Euler angle COMPONENTS):
//   Same byte format, simpler delta:
//     new_value = (sign_subtract) ? (previous - value) : (previous + value)
//     Result wraps at 16 bits: (result & 0xFFFF)
//
// EULER ANGLE TO QUATERNION CONVERSION:
// From Model_DecompressQuaternionKeys @ 0x00770e60:
//   1. Decode 3 delta-encoded u16 values (X, Y, Z Euler angles)
//   2. Convert [0,65535] to radians: angle = value * (2π/65536) - π
//   3. Build 3x3 rotation matrices: Rx, Ry, Rz
//   4. Multiply: result = Rz * Ry * Rx (ZYX order)
//   5. Convert matrix to quaternion
//   6. Ensure continuity: if dot(q, prev_q) < 0, negate q

// =============================================================================
// BB9 SECTION STRUCTURES (defined before Chunk_BB9_Animation)
// =============================================================================

// Effect/Sound entry - triggered during animation playback
// Discovered format: 22 bytes per entry
struct EffectSoundEntry {
    u32 hash [[name("Effect/Sound Hash")]];           // 0x00: Effect or sound identifier
    u16 triggerTime [[name("Trigger Time")]];         // 0x04: Time to trigger (animation units)
    u16 unknown_0x06 [[name("Unknown 0x06")]];        // 0x06
    u32 unknown_0x08 [[name("Unknown 0x08")]];        // 0x08
    u32 unknown_0x0C [[name("Unknown 0x0C")]];        // 0x0C
    u32 unknown_0x10 [[name("Unknown 0x10")]];        // 0x10: Often bone index or 0xFFFFFFFF
    u16 unknown_0x14 [[name("Unknown 0x14")]];        // 0x14
};  // 22 bytes total

// Per-bone data entry - maps animation hashes to skeleton bone indices
// Discovered format: 24 bytes per entry (not 12 as originally thought)
struct PerBoneEntry {
    u32 animationHash [[name("Animation Hash")]];      // 0x00: Animation sequence identifier
    u32 boneIndex [[name("Bone Index")]];              // 0x04: Target skeleton bone
    u32 unknown_0x08 [[name("Unknown 0x08")]];         // 0x08: Usually 0
    u32 unknown_0x0C [[name("Unknown 0x0C")]];         // 0x0C
    u32 unknown_0x10 [[name("Unknown 0x10")]];         // 0x10
    u32 unknown_0x14 [[name("Unknown 0x14")]];         // 0x14
};  // 24 bytes total

// Per-bone data section header
struct PerBoneHeader {
    u32 count [[name("Per-Bone Entry Count")]];
    u32 unknown [[name("Unknown")]];                   // Often 0
};

// Joint entry for physics/IK
// From Ghidra: OUTPUT is always 21 bytes, INPUT depends on compress_flag
struct JointEntry {
    u32 hash [[name("Joint Hash")]];                   // 0x00: Joint identifier
    u16 jointIndex [[name("Joint Index")]];            // 0x04: Index in joint array
    u16 jointType [[name("Joint Type")]];              // 0x06: Type/flags
    u8 reserved[9] [[name("Reserved")]];               // 0x08-0x10: Usually zeros
    u32 timeValue [[name("Time Value")]];              // 0x11: Time in animation units (3333, 6666, etc)
};  // 21 bytes total

// Joint data section header
// IMPORTANT: jointCount often UNDERSTATES actual joint entries (e.g., 25 in header vs 44 actual)
// The constraintCount field is actually a TIME VALUE (e.g., 13333), NOT a count!
// Must scan for valid entries or calculate from remaining bytes.
struct JointHeader {
    u16 jointCount [[name("Joint Count (often understated!)")]];
    u16 compressFlag [[name("Compress Flag")]];        // ~(this >> 16) & 1 for input size
    u32 timeValue [[name("Time Value (NOT constraint count!)")]];  // Actually animation time, not count
};

// Constraint entry for joint physics
// From Ghidra: 12 bytes per entry (count * 0xC)
struct ConstraintEntry {
    u32 pad;    // 0x00: Usually 0
    u32 hash;   // 0x04: Constraint identifier
    u32 idx;    // 0x08: Index reference
};  // 12 bytes total

// Attachment animation bounds entry
// Format: 24 bytes per entry (6 DWORDs)
// AttachmentEntry: 48 bytes (12 DWORDs = 0x30 bytes)
// From Model_ParseAttachmentPoints: param_3 * 0xc (12 DWORDs per entry)
// After entries, there's frame transform data (6 DWORDs each) and sequence data (4 DWORDs each)
struct AttachmentEntry {
    u32 field_0x00 [[name("Field 0x00")]];             // 0x00: Example: 4
    u32 field_0x04 [[name("Field 0x04")]];             // 0x04: Example: 5
    float field_0x08 [[name("Field 0x08")]];           // 0x08: Example: 0.33 (float)
    float field_0x0c [[name("Field 0x0c")]];           // 0x0C: Example: 0.33 (float)
    float field_0x10 [[name("Field 0x10")]];           // 0x10: Example: 12.0 (float)
    u32 field_0x14 [[name("Field 0x14")]];             // 0x14: Example: 8
    u32 field_0x18 [[name("Field 0x18")]];             // 0x18: Usually 0
    float field_0x1c [[name("Field 0x1c")]];           // 0x1C: Example: 2000.0 (float)
    float field_0x20 [[name("Field 0x20")]];           // 0x20: Example: 35.0 (float)
    float field_0x24 [[name("Field 0x24")]];           // 0x24: Example: 0.6 (float)
    u32 frameDataCount [[name("Frame Data Count")]];   // 0x28: Count for frame transform data (6 DWORDs each)
    u32 seqDataCount [[name("Seq Data Count")]];       // 0x2C: Count for sequence data (4 DWORDs each)
};  // 48 bytes total (0x30)

// Attachment data section header
struct AttachmentHeader {
    u32 count [[name("Attachment Count")]];
};

// Frame transform data: 6 DWORDs = 24 bytes
// From Ghidra: puVar7 * 6 (where puVar7 is totalFrameCount)
struct AttachmentFrameData {
    u32 data[6] [[name("Frame Transform Data (6 DWORDs)")]];
};  // 24 bytes

// Sequence data: 4 DWORDs = 16 bytes
// From Ghidra: iVar4 * 4 (where iVar4 is totalSeqCount)
struct AttachmentSeqData {
    u32 data[4] [[name("Sequence Data (4 DWORDs)")]];
};  // 16 bytes

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

    // Bone transforms extended (if flags & 0x10)
    // Contains full VLE-compressed bone animation data (headers + keyframe times/values)
    if ((header.flags & 0x10) != 0) {
        BoneTransformHeader_BB9 boneTransformHeader [[name("Bone Transform Header")]];

        // Parse bone animation entries (header + VLE keyframe data for each bone)
        if (boneTransformHeader.boneCount > 0 && boneTransformHeader.boneCount <= 256) {
            BoneAnimationEntry bones[boneTransformHeader.boneCount]
                [[name("Bone Animation Data (header + VLE keyframes)")]];
        }
    }

    // ==========================================================================
    // REMAINING ANIMATION DATA (after VLE bone data)
    // Section order from Ghidra MdlDecomp_ConvertGeometryChunk_BB9_to_FA1:
    //   1. Effect/Sound entries (count from header.unknown_0x18) - 22 bytes each
    //   2. Per-bone data (flag 0x4000): header + 24-byte entries
    //   3. Joint data (flag 0x0200): header + 21-byte entries + constraints
    //   4. Bone weight data (flag 0x0040): variable
    //   5. Standard bone transforms (flag 0x0020): header + VLE data
    //   6. Other sections: 0x0800, 0x2000, 0x1000, 0x8000
    //   7. Attachment data (flag 0x0080): header + 20-byte entries
    //   8. Unknown section (flag 0x0400): variable
    // ==========================================================================

    // Section 1: Effect/Sound entries (always present if unknown_0x18 > 0)
    if (header.unknown_0x18 > 0 && header.unknown_0x18 < 100) {
        EffectSoundEntry effectSoundEntries[header.unknown_0x18]
            [[name("Effect/Sound Entries")]];
    }

    // Section 2: Per-bone data (flag 0x4000)
    if ((header.flags & 0x4000) != 0) {
        PerBoneHeader perBoneHeader [[name("Per-Bone Header")]];
        if (perBoneHeader.count > 0 && perBoneHeader.count < 256) {
            PerBoneEntry perBoneEntries[perBoneHeader.count]
                [[name("Per-Bone Entries")]];
        }
    }

    // Section 3: Joint data (flag 0x0200)
    // NOTE: Header jointCount often UNDERSTATES actual entries (e.g., 25 vs 44).
    // The header's second field (timeValue) is NOT a constraint count - it's a time value!
    // For reliable parsing: use header count for joints, calculate constraints from remaining space.
    if ((header.flags & 0x0200) != 0) {
        JointHeader jointHeader [[name("Joint Header")]];

        // Use header's stated joint count (may miss some entries, but avoids corruption)
        // The timeValue field was incorrectly labeled constraintCount in earlier analysis
        u32 jointCountToUse = jointHeader.jointCount;
        if (jointCountToUse > 256) jointCountToUse = 256;

        if (jointCountToUse > 0) {
            JointEntry jointEntries[jointCountToUse]
                [[name("Joint Entries (using header count - may be incomplete)")]];
        }

        // Calculate remaining space for additional data (extra joints + constraints)
        u32 remainingAfterJoints = chunkSize - ($ - g_chunkDataStart);

        // Calculate EXACT attachment reserve by searching for valid count
        // Attachment section is at the very end: header(4) + count*24 bytes
        // The count field is a small positive number (typically 1-10)
        // Estimate attachment section size for reserve calculation
        // Attachment section: count (4) + entries (48 each) + frame/seq data
        // Fallback to ~100 bytes if we can't estimate
        u32 attachmentReserve = 100;

        // Parse remaining joint+constraint data as generic bytes for now
        // The exact split between extra joints and constraints requires scanning
        if (remainingAfterJoints > attachmentReserve) {
            u32 extraDataSize = remainingAfterJoints - attachmentReserve;
            u8 jointConstraintOverflow[extraDataSize]
                [[name("Extra Joint/Constraint Data (header understated joint count)")]];
        }
    }

    // Section 4: Standard bone transforms (flag 0x0020)
    // Format: count (2 DWORDs) + per-entry (24 bytes header + VLE times + float3 values)
    // VLE makes static parsing complex - capture remaining space minus attachment reserve
    if ((header.flags & 0x0020) != 0) {
        u32 remainingForBoneTransforms = chunkSize - ($ - g_chunkDataStart);
        // Reserve space for attachment if that flag is also set
        u32 boneTransformReserve = 0;
        if ((header.flags & 0x0080) != 0) {
            boneTransformReserve = 52;  // header(4) + ~2 entries(48)
        }
        if (remainingForBoneTransforms > boneTransformReserve) {
            u32 boneTransformDataSize = remainingForBoneTransforms - boneTransformReserve;
            u8 standardBoneTransformData[boneTransformDataSize]
                [[name("Standard Bone Transform Data (flag 0x20 - VLE encoded)")]];
        }
    }

    // Section 5: Attachment data (flag 0x0080)
    // From Ghidra + Frida dynamic analysis:
    // - Count at start (4 bytes)
    // - Each entry is 48 bytes (12 DWORDs)
    // - Entry fields [10] and [11] are frameDataCount and seqDataCount
    // - After entries: frameData (24 bytes each) + seqData (16 bytes each)
    //
    // PROBLEM: Previous sections may not be fully parsed, leaving cursor wrong.
    // SOLUTION: Search backward from chunk end for valid attachment count.
    // The attachment section is ALWAYS at the end of the BB9 chunk.
    if ((header.flags & 0x0080) != 0) {
        u32 attachChunkEnd = g_chunkDataStart + chunkSize;
        u32 attachCurrentPos = $;
        u32 attachFoundCount = 0;
        u32 attachStart = 0;

        // Search for attachment section by looking for count value that matches position
        // For count N, the section is at: attachChunkEnd - totalSize
        // Where totalSize = 4 + N*48 + variable (frame+seq data)
        //
        // Since frame/seq data sizes are unknown, we estimate with count*48+4 minimum
        // and scan for a valid count value (1-20) at plausible positions

        // Minimum attachment section size: 4 (header) + 48 (1 entry) = 52 bytes
        // Maximum reasonable: 4 + 20*48 + 100*24 + 100*16 = 4 + 960 + 2400 + 1600 = 4964 bytes

        // Scan for count values 1-10 at positions that make sense
        // At position P, if value V is there, section size is at least: 4 + V*48

        // Try count = 1 (minimum size ~52-100 bytes from end)
        u32 test1Pos = attachChunkEnd - 52;
        if (test1Pos >= attachCurrentPos) {
            u32 val1 @ test1Pos;
            if (val1 == 1) { attachFoundCount = 1; attachStart = test1Pos; }
        }

        // Try count = 2 (minimum size ~100 bytes from end)
        if (attachFoundCount == 0) {
            u32 test2Pos = attachChunkEnd - 100;
            if (test2Pos >= attachCurrentPos) {
                u32 val2 @ test2Pos;
                if (val2 == 2) { attachFoundCount = 2; attachStart = test2Pos; }
            }
        }

        // Try count = 3
        if (attachFoundCount == 0) {
            u32 test3Pos = attachChunkEnd - 148;
            if (test3Pos >= attachCurrentPos) {
                u32 val3 @ test3Pos;
                if (val3 == 3) { attachFoundCount = 3; attachStart = test3Pos; }
            }
        }

        // If count search failed, fall back to reading remaining data
        if (attachFoundCount > 0 && attachStart > attachCurrentPos) {
            // Skip unparsed data between current position and attachment section
            u32 skipSize = attachStart - attachCurrentPos;
            u8 unparsedPreAttachment[skipSize] [[name("Unparsed Data (before attachment)")]];

            // Now parse attachment section
            AttachmentHeader attachmentHeader [[name("Attachment Header")]];
            AttachmentEntry attachmentEntries[attachFoundCount] [[name("Attachment Entries")]];

            // Calculate total frame and sequence counts from entries
            u32 totalFrameCount = 0;
            u32 totalSeqCount = 0;
            if (attachFoundCount >= 1) { totalFrameCount = totalFrameCount + attachmentEntries[0].frameDataCount; totalSeqCount = totalSeqCount + attachmentEntries[0].seqDataCount; }
            if (attachFoundCount >= 2) { totalFrameCount = totalFrameCount + attachmentEntries[1].frameDataCount; totalSeqCount = totalSeqCount + attachmentEntries[1].seqDataCount; }
            if (attachFoundCount >= 3) { totalFrameCount = totalFrameCount + attachmentEntries[2].frameDataCount; totalSeqCount = totalSeqCount + attachmentEntries[2].seqDataCount; }

            // Parse frame transform data (6 DWORDs = 24 bytes each)
            if (totalFrameCount > 0 && totalFrameCount < 100) {
                AttachmentFrameData frameData[totalFrameCount] [[name("Frame Transform Data")]];
            }

            // Parse sequence data (4 DWORDs = 16 bytes each)
            if (totalSeqCount > 0 && totalSeqCount < 100) {
                AttachmentSeqData seqData[totalSeqCount] [[name("Sequence Data")]];
            }
        }
    }

    // Capture any remaining unparsed data
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u32 remainingSize = chunkSize - consumed;
        u8 remainingAnimData[remainingSize] [[name("Remaining Unparsed Data")]];
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

// Animation File References Chunk (BBD/0xBBD)
// References external animation files that should be loaded alongside this animation.
// Uses same file reference format as BBC (version + count + 6-byte entries).
// Decode file ID: (id0 - 0xff00ff) + (id1 * 0xff00)
struct Chunk_BBD_AnimationRefs {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                    // Must be 1
    u32 referenceCount;             // Number of animation file references

    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("Animation File References")]];
    }

    u32 expectedSize = 8 + (referenceCount * 6);  // 8 bytes for version+count + entries
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
    }
};

// File References Chunk (BBB/BBC) - BB* format has version + count
// BB* (original DAT format): version (must be 1) + count + entries
// References external files for composite models: textures, sub-models, sounds, etc.
struct Chunk_FileReferences_BB {
    u32 chunkId;
    u32 chunkSize;
    u32 version;                    // Must be 1
    u32 referenceCount;             // Number of file reference entries

    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("File References")]];
    }

    u32 expectedSize = 8 + (referenceCount * 6);  // 8 bytes for version+count + entries
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
    }
};

// File References Chunk (FA5/FA6/FAD/FAE) - FA* format is just count + entries
// From Ghidra: conversion copies data starting after version, so runtime format
// is just count + entries (version stripped)
struct Chunk_FileReferences_FA {
    u32 chunkId;
    u32 chunkSize;
    u32 referenceCount;             // Number of file reference entries (no version)

    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("File References")]];
    }

    u32 expectedSize = 4 + (referenceCount * 6);  // 4 bytes for count + entries
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
    }
};

// Type 8 Asset References Chunk (chunk ID 0x01) - NO count field, size/6 entries
struct Chunk_Type8_AssetRefs {
    u32 chunkId;
    u32 chunkSize;

    // No count field - number of entries is chunkSize / 6
    u32 referenceCount = chunkSize / 6;
    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("Asset References")]];
    }

    // Handle any remaining bytes (shouldn't be any if divisible by 6)
    u32 parsedSize = referenceCount * 6;
    if (parsedSize < chunkSize) {
        u32 remainingSize = chunkSize - parsedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
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

    // Weight data (7 bytes per weight entry)
    if (weightCount > 0 && weightCount < 1000) {
        BoneWeightEntry weights[weightCount] [[name("Bone Weight Entries")]];
    }
    // Bone transform data (8 bytes per bone entry)
    u32 totalBoneCount = boneCount0 + boneCount1 + boneCount2;
    if (totalBoneCount > 0 && totalBoneCount < 1000) {
        BoneTransformEntry boneTransforms[totalBoneCount] [[name("Bone Transform Entries")]];
    }
};

// LOD Entry (variable size)
// From Model_ParseBoneData: each entry has (count1 + count2 * 6) * 2 bytes of data
// Data structure: indexCount * 2 bytes (u16 indices) + vertexCount * 12 bytes (LODVertexEntry)
struct LODEntry_FA0 {
    u32 indexCount [[name("Index Count")]];
    u32 vertexCount [[name("Vertex Count")]];

    // Index buffer (u16 each)
    if (indexCount > 0 && indexCount < 100000) {
        u16 indices[indexCount] [[name("LOD Index Buffer")]];
    }
    // Vertex buffer (12 bytes per vertex)
    if (vertexCount > 0 && vertexCount < 100000) {
        LODVertexEntry vertices[vertexCount] [[name("LOD Vertex Buffer")]];
    }
};

// Skeleton Weight Entry (0x54 = 84 bytes)
// Contains bone-to-vertex weight mapping with transform data
struct SkeletonWeightEntry {
    // Transform matrix (4x4 floats = 64 bytes)
    float transform[16];
    // Additional weight/bone data (20 bytes)
    u32 boneIndex;              // Which bone this entry refers to
    u32 vertexStart;            // Starting vertex index
    u32 vertexCount;            // Number of vertices affected
    float influence;            // Weight influence factor
    u32 flags;                  // Additional flags
};

// Animation Frame Entry (variable size)
// From Model_ParseArrayWithCallback with Buffer_CalcFrameOffset
// Each entry has a frameCount, followed by frameCount * 8 bytes of frame data
struct AnimFrameEntry_FA0 {
    u32 frameCount [[name("Frame Count")]];
    if (frameCount > 0 && frameCount < 0x10000) {
        AnimationKeyframe keyframes[frameCount] [[name("Animation Keyframes")]];
    }
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

    // Embedded animation data (if classFlags.HAS_EMBEDDED_ANIMATION = 0x20)
    // Models with this flag use vertex animation (morph targets) instead of external skeletal animation
    if (header.classFlags.HAS_EMBEDDED_ANIMATION) {
        u32 embeddedAnimTotalSize [[name("Embedded Animation Total Size")]];
        u32 embeddedAnimCount [[name("Embedded Animation Count")]];
        // Each entry is 0x2E (46) byte header + variable data
        if (embeddedAnimCount > 0 && embeddedAnimCount < 256) {
            EmbeddedAnimationEntry embeddedAnims[embeddedAnimCount] [[name("Embedded Animation Entries")]];
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
        SkeletonNode skeletonNodes[header.skeletonBoneCount] [[name("Skeleton Nodes")]];
        if (header.skeletonWeightCount > 0) {
            SkeletonWeightEntry skeletonWeights[header.skeletonWeightCount] [[name("Skeleton Weight Entries")]];
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
// MAP FILE CHUNKS (FFNA Type 3)
// =============================================================================
// Map files use chunk IDs in the 0x20xxxxxx (full data) and 0x10xxxxxx (stub) ranges.
// Reference: Gw.exe Ghidra analysis + gw1-map-viewer/src/loaders/
//
// Chunk ID Reference (0x20xxxxxx = full, 0x10xxxxxx = stub):
//   0x20000000: MapHeaderChunk - Basic map header
//   0x20000002: TerrainChunk - Heightmap, textures, shadow map
//   0x20000003: ZoneChunk - Map zones/regions for area triggers
//   0x20000004: PropInfoChunk - Prop instance placements
//   0x20000006: UnknownChunk06
//   0x20000007: UnknownChunk07
//   0x20000008: PathfindingChunk - Navigation mesh (trapezoids, BSP, portals)
//   0x20000009: EnvironmentChunk - Fog, lighting, sky, water settings
//   0x2000000A: UnknownChunk0A
//   0x2000000C: MapInfoChunk - Map bounds and parameters
//   0x2000000E: UnknownChunk0E
//   0x2000000F: UnknownChunk0F
//   0x20000010: ShoreChunk - Water shorelines with wave animation
//   0x20000011: EnvironmentExtraChunk
//   0x20000012: AudioZoneChunk - Map audio/music zones
//   0x20000013: UnknownChunk13
//   0x20000014: UnknownChunk14
//   0x21000002: TerrainFilenamesChunk
//   0x21000003: ZoneFilenamesChunk
//   0x21000004: PropFilenamesChunk
//   0x21000006: UnknownFilenamesChunk06
//   0x21000009: EnvironmentFilenamesChunk
//   0x21000010: ShoreFilenamesChunk
//   0x21000012: AudioFilenamesChunk

// =============================================================================
// SHARED MAP STRUCTURES
// =============================================================================

// World coordinate bounds for the map
struct MapBounds {
    float minX [[name("Min X")]];
    float minY [[name("Min Y")]];
    float maxX [[name("Max X")]];
    float maxY [[name("Max Y")]];
};

// 2D vertex used throughout map data
struct MapVertex2D {
    float x;
    float y;
};

// File reference (decode: (id0 - 0xff00ff) + (id1 * 0xff00))
struct MapFileRef {
    u16 id0;
    u16 id1;
};

// File reference with padding
struct MapFileRefPadded {
    MapFileRef fileRef;
    u16 reserved;
};

// =============================================================================
// 0x20000000 - MAP HEADER CHUNK
// =============================================================================

struct MapHeaderChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 headerData[chunkSize] [[name("Header Data")]];
};

// =============================================================================
// 0x2000000C - MAP INFO CHUNK (Bounds & Parameters)
// =============================================================================

struct MapInfoChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 signature [[name("Signature")]];
    u8 version [[name("Version")]];
    MapBounds bounds [[name("Map Bounds")]];
    u32 unknown0;
    u32 unknown1;
    u32 unknown2;
    u32 unknown3;
    u32 unknown4;
};

// =============================================================================
// 0x20000002 - TERRAIN CHUNK (Heightmap & Textures)
// Verified from Gw.exe terrain loading functions
// =============================================================================

// Per-tile lighting/shadow data for 32x32 tile blocks
struct TerrainTileBlockData {
    u32 dataSize [[name("Data Size")]];
    u8 lightingData[dataSize] [[name("Lighting Data")]];
    u8 shadowData[128] [[name("Shadow Data")]];
};

struct TerrainChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];

    // Header
    u32 signature [[name("Signature")]];
    u32 version [[name("Version")]];

    // Tag 0: Grid dimensions
    u8 tag0 [[name("Tag 0")]];
    u32 tag0Size [[name("Tag 0 Size")]];
    u32 gridDimX [[name("Grid Dim X")]];
    u32 gridDimY [[name("Grid Dim Y")]];
    float cellSize [[name("Cell Size")]];
    float maxSlopeAngle [[name("Max Slope Angle")]];  // < 1.570796 (90 degrees)
    u16 unknown0;
    float unknown1;
    float unknown2;

    // Tag 1: Heightmap data
    u8 tag1 [[name("Tag 1 (Heightmap)")]];
    u32 heightmapSizeBytes [[name("Heightmap Size")]];
    float heightmap[gridDimX * gridDimY] [[name("Heightmap")]];

    // Tag 2: Texture indices per tile
    u8 tag2 [[name("Tag 2 (Textures)")]];
    u32 textureIndexCount [[name("Texture Index Count")]];
    u8 textureIndices[textureIndexCount] [[name("Texture Indices")]];

    // Tag 3: Unknown small data
    u8 tag3 [[name("Tag 3")]];
    u32 tag3Size [[name("Tag 3 Size")]];
    u8 tag3DataCount;
    u8 tag3Data[tag3DataCount] [[name("Tag 3 Data")]];

    // Tag 4: Unknown small data
    u8 tag4 [[name("Tag 4")]];
    u32 tag4Size [[name("Tag 4 Size")]];
    u8 tag4DataCount;
    u8 tag4Data[tag4DataCount] [[name("Tag 4 Data")]];

    // Tag 5: Unknown data
    u8 tag5 [[name("Tag 5")]];
    u32 tag5Size [[name("Tag 5 Size")]];
    u8 tag5Data[tag5Size] [[name("Tag 5 Data")]];

    // Tag 6: Shadow map
    u8 tag6 [[name("Tag 6 (Shadow Map)")]];
    u32 shadowMapCount [[name("Shadow Map Count")]];
    u8 shadowMap[shadowMapCount] [[name("Shadow Map")]];

    // Tag 7: Per-32x32 tile block data
    u8 tag7 [[name("Tag 7 (Tile Blocks)")]];
    u32 tag7Size [[name("Tag 7 Size")]];
    TerrainTileBlockData tileBlocks[gridDimX * gridDimY / 32 / 32] [[name("Tile Blocks")]];

    u32 unknown3;
    u8 terminator;

    // Remaining data
    u8 remainingData[chunkSize - 76 - sizeof(heightmap) - sizeof(textureIndices)
        - sizeof(tag3Data) - sizeof(tag4Data) - sizeof(tag5Data)
        - sizeof(shadowMap) - sizeof(tileBlocks) - 5] [[name("Remaining Data")]];
};

// =============================================================================
// 0x20000003 - ZONE CHUNK (Map Regions)
// Defines zones for area triggers, map names, etc.
// =============================================================================

struct ZoneTaggedElement {
    u8 tag;
    u16 size;
    u16 unknown;
    u8 data[size] [[name("Zone Element Data")]];
};

struct ZoneTaggedElement1 {
    u8 tag;
    u32 size;
    u32 zoneCount [[name("Zone Count")]];
};

struct ZoneRegion {
    u32 vertexCount [[name("Vertex Count")]];
    MapVertex2D vertices[vertexCount] [[name("Boundary Vertices")]];
    u8 zoneData[28] [[name("Zone Properties")]];
    u8 terminator;
};

struct ZoneChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];

    u32 signature [[name("Signature")]];  // 0x59220329
    u32 version [[name("Version")]];      // 0xA0

    ZoneTaggedElement element0 [[name("Element 0")]];
    ZoneTaggedElement element1 [[name("Element 1")]];
    ZoneTaggedElement1 element2 [[name("Element 2 (Zone Count)")]];
    ZoneTaggedElement element3 [[name("Element 3")]];

    if (element2.zoneCount > 0) {
        u32 unknown0;
        ZoneRegion zones[element2.zoneCount] [[name("Zone Regions")]];
        u8 remainingData[chunkSize - 8 - sizeof(element0) - sizeof(element1)
            - sizeof(element2) - sizeof(element3) - 4 - sizeof(zones)] [[name("Remaining Data")]];
    } else {
        u8 remainingData[chunkSize - 8 - sizeof(element0) - sizeof(element1)
            - sizeof(element2) - sizeof(element3)] [[name("Remaining Data")]];
    }
};

// =============================================================================
// 0x20000004 - PROP INFO CHUNK (Prop Placements)
// Contains positions and transforms for all static props
// =============================================================================

// Per-prop bounding/collision vertices
struct PropBoundingVertex {
    float x;
    float y;
};

struct PropInstance {
    u16 filenameIndex [[name("Filename Index")]];
    float posX [[name("Position X")]];
    float posY [[name("Position Y")]];
    float posZ [[name("Position Z")]];
    float rotationF4;
    float rotationF5;
    float rotationF6;
    float sinAngle [[name("Sin Angle")]];
    float cosAngle [[name("Cos Angle")]];
    float rotationF9;
    float scale [[name("Scale")]];
    float unknown;
    u8 flags;
    u8 boundingVertexCount [[name("Bounding Vertex Count")]];
    PropBoundingVertex boundingVertices[boundingVertexCount] [[name("Bounding Vertices")]];
};

struct PropArray {
    u16 propCount [[name("Prop Count")]];
    PropInstance props[propCount] [[name("Props")]];
};

struct PropVertexData {
    float x;
    float y;
    float z;
    u32 unknown0;
    u32 unknown1;
    u32 unknown2;
};

struct PropVertexDataSection {
    u8 tag;
    u32 sizeBytes [[name("Size (bytes)")]];
    u32 vertexCount [[name("Vertex Count")]];
    PropVertexData vertices[vertexCount] [[name("Vertices")]];
};

struct PropIndexSection {
    u8 tag;
    u32 sizeBytes [[name("Size (bytes)")]];
    u32 indexCount [[name("Index Count")]];
    u16 indices[indexCount] [[name("Indices")]];
};

struct PropVertexSection2 {
    u8 tag;
    u32 sizeBytes [[name("Size (bytes)")]];
    u32 vertexCount [[name("Vertex Count")]];
    MapVertex2D vertices[vertexCount] [[name("Vertices")]];
};

struct PropLinkEntry {
    u16 unknown;
    u16 propIndex [[name("Prop Index")]];
};

struct PropLinkSection {
    u8 tag;
    u32 sizeBytes [[name("Size (bytes)")]];
    u16 entryCount [[name("Entry Count")]];
    PropLinkEntry entries[entryCount] [[name("Prop Links")]];
};

struct PropInfoChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];

    u32 signature [[name("Signature")]];
    u16 version [[name("Version")]];  // 0x11 or 0x12
    u32 propArraySizeBytes [[name("Prop Array Size")]];

    PropArray propArray [[name("Prop Array")]];
    PropVertexDataSection vertexData [[name("Vertex Data")]];
    PropIndexSection indexData [[name("Index Data")]];
    PropVertexSection2 vertexData2 [[name("Vertex Data 2")]];
    PropLinkSection propLinks [[name("Prop Links")]];

    u8 remainingData[chunkSize - 10 - sizeof(propArray) - sizeof(vertexData)
        - sizeof(indexData) - sizeof(vertexData2) - sizeof(propLinks)] [[name("Remaining Data")]];
};

// =============================================================================
// 0x21000002/3/4/6/9/10/12 - FILENAME CHUNKS
// Contains file references for assets
// =============================================================================

struct FilenameChunkHeader {
    u32 signature [[name("Signature")]];
    u8 version [[name("Version")]];
};

struct FilenameEntry {
    MapFileRef fileRef [[name("File Reference")]];
    u16 unknown;
};

struct FilenameChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    FilenameChunkHeader header [[name("Header")]];
    FilenameEntry entries[(chunkSize - sizeof(header)) / 6] [[name("Filename Entries")]];
    u8 remainingData[chunkSize - sizeof(header) - sizeof(entries)] [[name("Remaining Data")]];
};

// 0x21000009 - Environment filenames (special format)
struct EnvironmentFilenameChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u16 unknown0;
    u16 unknown1;
    u8 unknown2;
    MapFileRefPadded filenames[(chunkSize - 5) / sizeof(MapFileRefPadded)] [[name("Filenames")]];
};

// =============================================================================
// 0x20000006/7 - UNKNOWN CHUNKS
// =============================================================================

struct UnknownChunk06 {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct UnknownChunk07 {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

// =============================================================================
// 0x20000008 - PATHFINDING CHUNK (Navigation Mesh)
// Verified from Gw.exe: PathChunk_ReadHeader @ 0x006f9040
// Contains trapezoid-based navmesh organized by elevation planes
// =============================================================================

// Navigation mesh trapezoid (44 bytes)
// Verified from Path_import_trapezoids @ 0x006fa750
struct NavMeshTrapezoid {
    // Neighbor indices (PLANE-LOCAL, 0xFFFFFFFF = no neighbor)
    u32 neighborTopLeft [[name("Neighbor Top-Left")]];
    u32 neighborTopRight [[name("Neighbor Top-Right")]];
    u32 neighborBottomLeft [[name("Neighbor Bottom-Left")]];
    u32 neighborBottomRight [[name("Neighbor Bottom-Right")]];

    // Portal indices (PLANE-LOCAL, 0xFFFF = no portal)
    u16 portalLeft [[name("Portal Left")]];
    u16 portalRight [[name("Portal Right")]];

    // World-space coordinates
    float yTop [[name("Y Top")]];
    float yBottom [[name("Y Bottom")]];
    float xTopLeft [[name("X Top-Left")]];
    float xTopRight [[name("X Top-Right")]];
    float xBottomLeft [[name("X Bottom-Left")]];
    float xBottomRight [[name("X Bottom-Right")]];
};

// Portal connecting different elevation planes (9 bytes)
// Verified from PathData_ImportPortals @ 0x006fa1b0
struct NavMeshPortal {
    u16 trapezoidCount [[name("Trapezoid Count")]];
    u16 trapezoidStartIndex [[name("Trapezoid Start Index")]];
    u16 neighborPlaneId [[name("Neighbor Plane ID")]];
    u16 sharedPortalId [[name("Shared Portal ID")]];
    u8 flags [[name("Flags")]];  // 0x4 = not used for pathfinding
};

// X-axis BSP node for spatial queries (16 bytes)
// Verified from Path_import_x_nodes @ 0x006fa470
struct NavMeshXNode {
    u32 vertexIndexStart [[name("Vertex Index Start")]];
    u32 vertexIndexEnd [[name("Vertex Index End")]];
    u32 leftChildIndex [[name("Left Child Index")]];
    u32 rightChildIndex [[name("Right Child Index")]];
};

// Y-axis BSP node (12 bytes)
// Verified from Path_import_y_nodes @ 0x006fa5c0
struct NavMeshYNode {
    u32 vertexIndex [[name("Vertex Index")]];
    u32 leftChildIndex [[name("Left Child Index")]];
    u32 rightChildIndex [[name("Right Child Index")]];
};

// Sink node referencing a trapezoid (4 bytes)
// Verified from Path_import_sink_nodes @ 0x006fa3f0
struct NavMeshSinkNode {
    u32 trapezoidIndex [[name("Trapezoid Index")]];
};

// Tagged data section header
struct PathfindingTag {
    u8 tag [[name("Tag")]];
    u32 size [[name("Size")]];
};

// Plane header with array counts (32 bytes)
// Verified from PathChunk_AllocArrays @ 0x006f9aa0
struct NavMeshPlaneHeader {
    u32 polyDataCount [[name("Poly Data Count")]];
    u32 edgeVectorCount [[name("Edge Vector Count")]];
    u32 trapezoidCount [[name("Trapezoid Count")]];
    u32 xNodeCount [[name("X-Node Count")]];
    u32 yNodeCount [[name("Y-Node Count")]];
    u32 sinkNodeCount [[name("Sink Node Count")]];
    u32 portalCount [[name("Portal Count")]];
    u32 portalTrapezoidCount [[name("Portal Trapezoid Count")]];
};

// Complete pathfinding plane (one elevation level)
// Tag order from function pointer table at 0xbadfe4-0xbae00c:
// 0 -> 11 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 10 -> 9
struct NavMeshPlane {
    // Tag 0: Header
    PathfindingTag headerTag [[name("Header Tag")]];
    NavMeshPlaneHeader header [[name("Header")]];

    // Tag 11: Polygon boundary data
    PathfindingTag polyDataTag [[name("Poly Data Tag")]];
    MapVertex2D polyData[header.polyDataCount] [[name("Poly Data")]];

    // Tag 1: Edge vectors
    PathfindingTag edgeVectorsTag [[name("Edge Vectors Tag")]];
    MapVertex2D edgeVectors[header.edgeVectorCount] [[name("Edge Vectors")]];

    // Tag 2: Trapezoids (navmesh geometry)
    PathfindingTag trapezoidsTag [[name("Trapezoids Tag")]];
    NavMeshTrapezoid trapezoids[header.trapezoidCount] [[name("Trapezoids")]];

    // Tag 3: Root node type (0=X, 1=Y, 2=Sink)
    PathfindingTag rootNodeTag [[name("Root Node Tag")]];
    u8 rootNodeType [[name("Root Node Type")]];

    // Tag 4: X-axis BSP nodes
    PathfindingTag xNodesTag [[name("X-Nodes Tag")]];
    NavMeshXNode xNodes[header.xNodeCount] [[name("X-Nodes")]];

    // Tag 5: Y-axis BSP nodes
    PathfindingTag yNodesTag [[name("Y-Nodes Tag")]];
    NavMeshYNode yNodes[header.yNodeCount] [[name("Y-Nodes")]];

    // Tag 6: Sink nodes
    PathfindingTag sinkNodesTag [[name("Sink Nodes Tag")]];
    NavMeshSinkNode sinkNodes[header.sinkNodeCount] [[name("Sink Nodes")]];

    // Tag 10: Portal trapezoid indices
    PathfindingTag portalTrapsTag [[name("Portal Trapezoids Tag")]];
    u32 portalTrapezoidIndices[header.portalTrapezoidCount] [[name("Portal Trapezoid Indices")]];

    // Tag 9: Portals
    PathfindingTag portalsTag [[name("Portals Tag")]];
    NavMeshPortal portals[header.portalCount] [[name("Portals")]];
};

// Static obstacle (tree, rock, pillar, etc.)
// Circular collision primitive blocking character movement
// From Ghidra analysis of PathObstacle_Import @ 0x006f8e20
struct StaticObstacle {
    float x [[name("X")]];           // World X coordinate
    float y [[name("Y")]];           // World Y coordinate (Z in 3D)
    float radius [[name("Radius")]]; // Collision radius
};

// Obstacle grid cell for spatial partitioning
// Each cell contains an index into the obstacle array, allowing
// fast lookup of nearby obstacles during pathfinding
struct ObstacleGridCell {
    u8 obstacleIndex;   // Index into obstacle array (0xFF = empty)
    u8 nextCell;        // Chain to next cell with obstacle
    u8 flags;           // Cell flags
};

// Tag 13 obstacle data with grid-based spatial partitioning
// Grid accelerates obstacle lookups during navmesh queries
struct PathfindingObstacleData {
    u16 gridWidth [[name("Grid Width")]];
    u16 gridHeight [[name("Grid Height")]];
    u16 obstacleCount [[name("Obstacle Count")]];

    // Spatial partitioning grid for fast obstacle lookups
    ObstacleGridCell cells[gridWidth * gridHeight] [[name("Grid Cells")]];

    // Actual obstacle collision data
    StaticObstacle obstacles[obstacleCount] [[name("Static Obstacles")]];
};

// Main pathfinding chunk
// Verified from PathChunk_ReadHeader @ 0x006f9040
struct PathfindingChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];

    // Header (signature 0xEEFE704C)
    u32 signature [[name("Signature")]];
    u32 version [[name("Version")]];
    u32 sequence [[name("Sequence")]];

    // Tag 7: Preamble/validation
    PathfindingTag preambleTag [[name("Preamble Tag")]];
    u8 preambleData[preambleTag.size] [[name("Preamble Data")]];

    // Tag 8: All plane data
    PathfindingTag planesTag [[name("Planes Tag")]];
    u32 planeCount [[name("Plane Count")]];
    NavMeshPlane planes[planeCount] [[name("Planes")]];

    // Tag 12: Node properties
    PathfindingTag nodePropsTag [[name("Node Properties Tag")]];
    u8 nodePropsData[nodePropsTag.size] [[name("Node Properties")]];

    // Tag 13: Static obstacles (trees, rocks, etc.)
    // Grid-based spatial partitioning for collision detection
    PathfindingTag obstaclesTag [[name("Obstacles Tag")]];
    PathfindingObstacleData obstacles [[name("Obstacles")]];

    // Tag 14: State data
    PathfindingTag stateTag [[name("State Tag")]];
    u8 stateData[stateTag.size] [[name("State Data")]];

    // Terminator (0xFF)
    PathfindingTag terminatorTag [[name("Terminator")]];
};

// 0x10000008 - Pathfinding stub (minimal boundary data only)
struct PathfindingChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 signature [[name("Signature")]];  // 0xEEFE704C
    u8 boundaryData[chunkSize - 4] [[name("Boundary Data")]];
};

// =============================================================================
// 0x20000009 - ENVIRONMENT INFO CHUNK
// Fog, lighting, sky, water rendering settings
// =============================================================================

// Lighting mode settings
struct EnvLightingMode {
    u8 unknown0;
    u8 unknown1;
    s32 unknown2;
    u8 unknown3;
    u8 unknown4;
    u16 unknown5;
};

// Sky/atmosphere settings
struct EnvSkySettings {
    u8 skyBrightness [[name("Sky Brightness")]];
    u8 skySaturation [[name("Sky Saturation")]];
    u8 colorScale [[name("Color Scale")]];  // 0xFF = grayscale, 0 = normal
    u16 unknown0;
    u8 unknown1;
};

// Fog settings
struct EnvFogSettings {
    u8 fogBlue [[name("Fog Blue")]];
    u8 fogGreen [[name("Fog Green")]];
    u8 fogRed [[name("Fog Red")]];
    u32 fogDistanceStart [[name("Fog Distance Start")]];
    u32 fogDistanceEnd [[name("Fog Distance End")]];
    s32 fogZStart [[name("Fog Z Start")]];
    s32 fogZEnd [[name("Fog Z End")]];
};

// Ambient and sun lighting
struct EnvLightingSettings {
    u8 ambientBlue [[name("Ambient Blue")]];
    u8 ambientGreen [[name("Ambient Green")]];
    u8 ambientRed [[name("Ambient Red")]];
    u8 ambientIntensity [[name("Ambient Intensity")]];
    u8 sunBlue [[name("Sun Blue")]];
    u8 sunGreen [[name("Sun Green")]];
    u8 sunRed [[name("Sun Red")]];
    u8 sunIntensity [[name("Sun Intensity")]];
};

// Unknown environment data
struct EnvUnknownData4 {
    u8 data[2];
};

// Sky texture settings
struct EnvSkyTextures {
    u8 cloudCylinderScale [[name("Cloud Cylinder Scale")]];
    u16 skyBackgroundIndex [[name("Sky Background Index")]];
    u16 skyClouds0Index [[name("Sky Clouds 0 Index")]];
    u16 skyClouds1Index [[name("Sky Clouds 1 Index")]];
    u16 skySunIndex [[name("Sky Sun Index")]];
    u8 fogSkyHeight [[name("Fog Sky Height")]];
    s16 skyCylinderHeight [[name("Sky Cylinder Height")]];
    s16 skyCylinderRadius [[name("Sky Cylinder Radius")]];
    u8 unknown;
};

// Alternate sky texture format
struct EnvSkyTexturesAlt {
    u8 cloudCylinderScale [[name("Cloud Cylinder Scale")]];
    u16 skyBackgroundIndex [[name("Sky Background Index")]];
    u16 skyClouds0Index [[name("Sky Clouds 0 Index")]];
    u16 skyClouds1Index [[name("Sky Clouds 1 Index")]];
    u16 skySunIndex [[name("Sky Sun Index")]];
    u8 unknown[7];
};

// Water rendering settings
struct EnvWaterSettings {
    u8 unknown[9];
    float unknown1;
    float distortionTexScale [[name("Distortion Tex Scale")]];
    float distortionScale [[name("Distortion Scale")]];
    float distortionTexSpeed [[name("Distortion Tex Speed")]];
    float colorTexScale [[name("Color Tex Scale")]];
    float colorTexSpeed [[name("Color Tex Speed")]];
    float transparency [[name("Transparency")]];
    float unknown2;
    float unknown3;
    u8 color0Blue [[name("Color 0 Blue")]];
    u8 color0Green [[name("Color 0 Green")]];
    u8 color0Red [[name("Color 0 Red")]];
    u8 color0Alpha [[name("Color 0 Alpha")]];
    u8 color1Blue [[name("Color 1 Blue")]];
    u8 color1Green [[name("Color 1 Green")]];
    u8 color1Red [[name("Color 1 Red")]];
    u8 color1Alpha [[name("Color 1 Alpha")]];
    u16 colorTextureIndex [[name("Color Texture Index")]];
    u16 distortionTextureIndex [[name("Distortion Texture Index")]];
};

// Wind settings
struct EnvWindSettings {
    u8 windDir0 [[name("Wind Direction 0")]];
    u8 windDir1 [[name("Wind Direction 1")]];
    u8 windSpeed0 [[name("Wind Speed 0")]];
    u8 windSpeed1 [[name("Wind Speed 1")]];
};

// Additional environment data
struct EnvExtraData {
    u8 unknown[9];
    float unknown1;
    float unknown2;
    u8 data[15];
};

struct EnvironmentChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];

    u32 signature [[name("Signature")]];  // 0x92991030
    u16 version0 [[name("Version 0")]];
    u16 version1 [[name("Version 1")]];

    u8 tag0;
    u16 lightingModeCount [[name("Lighting Mode Count")]];
    EnvLightingMode lightingModes[lightingModeCount] [[name("Lighting Modes")]];

    u8 tag1;
    u16 skySettingsCount [[name("Sky Settings Count")]];
    EnvSkySettings skySettings[skySettingsCount] [[name("Sky Settings")]];

    u8 tag2;
    u16 fogSettingsCount [[name("Fog Settings Count")]];
    EnvFogSettings fogSettings[fogSettingsCount] [[name("Fog Settings")]];

    u8 tag3;
    u16 lightingSettingsCount [[name("Lighting Settings Count")]];
    EnvLightingSettings lightingSettings[lightingSettingsCount] [[name("Lighting Settings")]];

    u8 tag4;
    u16 unknownData4Count;
    EnvUnknownData4 unknownData4[unknownData4Count];

    u8 tag5;
    u16 skyTextureCount [[name("Sky Texture Count")]];
    if (version1 > 0) {
        EnvSkyTexturesAlt skyTextures[skyTextureCount] [[name("Sky Textures")]];
    } else {
        EnvSkyTextures skyTextures[skyTextureCount] [[name("Sky Textures")]];
    }

    u8 tag6;
    u16 waterSettingsCount [[name("Water Settings Count")]];
    EnvWaterSettings waterSettings[waterSettingsCount] [[name("Water Settings")]];

    u8 tag7;
    u16 windSettingsCount [[name("Wind Settings Count")]];
    EnvWindSettings windSettings[windSettingsCount] [[name("Wind Settings")]];

    u8 unknown0;
    u16 unknown1[8];
    u8 unknown2;

    u8 tag8;
    u8 extraDataCount;
    EnvExtraData extraData[extraDataCount] [[name("Extra Data")]];

    u16 unknown3;
    u16 extraCount;
    u8 extraBytes[5 * extraCount];

    // Variable trailing data
    u8 trailingData[chunkSize - ($ - addressof(signature))] [[name("Trailing Data")]];
};

// =============================================================================
// 0x2000000A/E/F - UNKNOWN CHUNKS
// =============================================================================

struct UnknownChunk0A {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct UnknownChunk0E {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct UnknownChunk0F {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

// =============================================================================
// 0x20000010 - SHORE CHUNK (Water Shorelines)
// Wave animation along water edges
// =============================================================================

struct ShoreSegment {
    float unknown0;
    float shoreHeight [[name("Shore Height")]];
    float waveAlpha [[name("Wave Alpha")]];      // 0-1, intensity of waves
    float waveSpeed [[name("Wave Speed")]];      // Animation speed
    u32 vertexCount [[name("Vertex Count")]];
    MapVertex2D vertices[vertexCount] [[name("Shore Vertices")]];
    u32 unknown1;
};

struct ShoreChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    char signature[4] [[name("Signature")]];
    u32 unknown0;
    u32 segmentCount [[name("Segment Count")]];
    ShoreSegment segments[segmentCount] [[name("Shore Segments")]];
    u8 remainingData[chunkSize - 12 - 5 - sizeof(segments)] [[name("Remaining Data")]];
    u32 unknown1;
    u8 terminator;
};

// =============================================================================
// 0x20000011/12/13/14 - ADDITIONAL CHUNKS
// =============================================================================

struct EnvironmentExtraChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct AudioZoneChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct UnknownChunk13 {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

struct UnknownChunk14 {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Data")]];
};

// =============================================================================
// 0x10xxxxxx - STUB CHUNKS (Lightweight reference files)
// These contain minimal data - just basic metadata, no full content
// =============================================================================

struct MapHeaderChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct TerrainChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct ZoneChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct PropInfoChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct EnvironmentChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct ShoreChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct FilenameChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct GenericMapChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
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
        Chunk_FileReferences_BB chunk [[name("File References (0xBBB/BBC)")]];
    }
    else if (peekId == 0xBBD) {
        Chunk_BBD_AnimationRefs chunk [[name("Animation File Refs (0xBBD)")]];
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
        Chunk_FileReferences_FA chunk [[name("File References (Component Assets)")]];
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

    // Type 8 (Asset Descriptor) chunks
    else if (peekId == 0x01) {
        // Chunk 0x01 in type 8 files: File references to actual assets (no count field)
        Chunk_Type8_AssetRefs chunk [[name("Asset References (Type 8)")]];
    }
    else if (peekId == 0x02) {
        // Chunk 0x02 in type 8 files: Descriptor metadata
        UnknownChunk chunk [[name("Descriptor Metadata (Type 8)")]];
    }

    // ========== MAP CHUNKS (0x20xxxxxx - Full data) ==========
    else if (peekId == 0x20000000) {
        MapHeaderChunk mapHeader [[name("Map Header")]];
    }
    else if (peekId == 0x20000002) {
        TerrainChunk terrain [[name("Terrain (Heightmap/Textures)")]];
    }
    else if (peekId == 0x20000003) {
        ZoneChunk zones [[name("Zone Regions")]];
    }
    else if (peekId == 0x20000004) {
        PropInfoChunk propInfo [[name("Prop Placements")]];
    }
    else if (peekId == 0x20000006) {
        UnknownChunk06 unknown06 [[name("Unknown Chunk 06")]];
    }
    else if (peekId == 0x20000007) {
        UnknownChunk07 unknown07 [[name("Unknown Chunk 07")]];
    }
    else if (peekId == 0x20000008) {
        PathfindingChunk pathfinding [[name("Pathfinding (NavMesh)")]];
    }
    else if (peekId == 0x20000009) {
        EnvironmentChunk environment [[name("Environment (Fog/Lighting/Water)")]];
    }
    else if (peekId == 0x2000000A) {
        UnknownChunk0A unknown0A [[name("Unknown Chunk 0A")]];
    }
    else if (peekId == 0x2000000C) {
        MapInfoChunk mapInfo [[name("Map Info (Bounds)")]];
    }
    else if (peekId == 0x2000000E) {
        UnknownChunk0E unknown0E [[name("Unknown Chunk 0E")]];
    }
    else if (peekId == 0x2000000F) {
        UnknownChunk0F unknown0F [[name("Unknown Chunk 0F")]];
    }
    else if (peekId == 0x20000010) {
        ShoreChunk shore [[name("Shore (Water Edges)")]];
    }
    else if (peekId == 0x20000011) {
        EnvironmentExtraChunk envExtra [[name("Environment Extra")]];
    }
    else if (peekId == 0x20000012) {
        AudioZoneChunk audioZones [[name("Audio Zones")]];
    }
    else if (peekId == 0x20000013) {
        UnknownChunk13 unknown13 [[name("Unknown Chunk 13")]];
    }
    else if (peekId == 0x20000014) {
        UnknownChunk14 unknown14 [[name("Unknown Chunk 14")]];
    }
    // 0x21xxxxxx - Map filename/asset reference chunks
    else if (peekId == 0x21000002) {
        FilenameChunk terrainFilenames [[name("Terrain Texture Filenames")]];
    }
    else if (peekId == 0x21000003) {
        FilenameChunk zoneFilenames [[name("Zone Filenames")]];
    }
    else if (peekId == 0x21000004) {
        FilenameChunk propFilenames [[name("Prop Model Filenames")]];
    }
    else if (peekId == 0x21000006) {
        FilenameChunk unknown06Filenames [[name("Unknown 06 Filenames")]];
    }
    else if (peekId == 0x21000009) {
        EnvironmentFilenameChunk envFilenames [[name("Environment Filenames")]];
    }
    else if (peekId == 0x21000010) {
        FilenameChunk shoreFilenames [[name("Shore Filenames")]];
    }
    else if (peekId == 0x21000012) {
        FilenameChunk audioFilenames [[name("Audio Filenames")]];
    }

    // ========== MAP CHUNKS (0x10xxxxxx - Stub/reference files) ==========
    // These are lightweight files with minimal data, used for map selection/metadata
    else if (peekId == 0x10000000) {
        MapHeaderChunkStub mapHeaderStub [[name("Map Header (Stub)")]];
    }
    else if (peekId == 0x10000002) {
        TerrainChunkStub terrainStub [[name("Terrain (Stub)")]];
    }
    else if (peekId == 0x10000003) {
        ZoneChunkStub zonesStub [[name("Zones (Stub)")]];
    }
    else if (peekId == 0x10000004) {
        PropInfoChunkStub propInfoStub [[name("Props (Stub)")]];
    }
    else if (peekId == 0x10000006) {
        GenericMapChunkStub unknown06Stub [[name("Unknown 06 (Stub)")]];
    }
    else if (peekId == 0x10000007) {
        GenericMapChunkStub unknown07Stub [[name("Unknown 07 (Stub)")]];
    }
    else if (peekId == 0x10000008) {
        PathfindingChunkStub pathfindingStub [[name("Pathfinding (Stub)")]];
    }
    else if (peekId == 0x10000009) {
        EnvironmentChunkStub environmentStub [[name("Environment (Stub)")]];
    }
    else if (peekId == 0x1000000A) {
        GenericMapChunkStub unknown0AStub [[name("Unknown 0A (Stub)")]];
    }
    else if (peekId == 0x1000000C) {
        MapInfoChunk mapInfoStub [[name("Map Info (Stub)")]];
    }
    else if (peekId == 0x1000000E) {
        GenericMapChunkStub unknown0EStub [[name("Unknown 0E (Stub)")]];
    }
    else if (peekId == 0x1000000F) {
        GenericMapChunkStub unknown0FStub [[name("Unknown 0F (Stub)")]];
    }
    else if (peekId == 0x10000010) {
        ShoreChunkStub shoreStub [[name("Shore (Stub)")]];
    }
    else if (peekId == 0x10000011) {
        GenericMapChunkStub envExtraStub [[name("Environment Extra (Stub)")]];
    }
    else if (peekId == 0x10000012) {
        GenericMapChunkStub audioStub [[name("Audio (Stub)")]];
    }
    else if (peekId == 0x10000013) {
        GenericMapChunkStub unknown13Stub [[name("Unknown 13 (Stub)")]];
    }
    else if (peekId == 0x10000014) {
        GenericMapChunkStub unknown14Stub [[name("Unknown 14 (Stub)")]];
    }
    // 0x11xxxxxx - Map filename stub chunks
    else if (peekId == 0x11000002 || peekId == 0x11000003 || peekId == 0x11000004 ||
             peekId == 0x11000006 || peekId == 0x11000009 || peekId == 0x11000010 ||
             peekId == 0x11000012) {
        FilenameChunkStub filenamesStub [[name("Filenames (Stub)")]];
    }

    // Unknown chunk
    else {
        UnknownChunk chunk [[name("Unknown Chunk")]];
    }
};

// =============================================================================
// MAIN FILE STRUCTURE
// =============================================================================

// File Types:
// 0x02 = Model/Geometry file (most common)
// 0x05 = Variant/LOD file
// 0x08 = Asset Descriptor (references other files, used in composite models)

struct FFNAFile {
    char signature[4] [[name("FFNA Signature")]];   // "ffna" (0x616e6666)
    u8 fileType [[name("File Type")]];

    // Different file types have different chunk structures
    Chunk chunks[while($ < sizeof($))] [[name("Chunks")]];
};

FFNAFile file @ 0;
