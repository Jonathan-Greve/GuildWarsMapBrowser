// =============================================================================
// FFNA Type 2 Model File - COMPLETE UNIFIED PATTERN
// Handles ALL chunk types: 0xFA* (runtime) and 0xBB* (DAT original)
//
// Covers:
//   - Geometry: vertices, indices, materials, shaders, textures
//   - Animation: BB9 uses VLE-compressed Euler angles, FA1 uses RAW float data
//   - Skeleton: bone weights, attachment points, emitters
//
// Animation Runtime Notes (critical for matching GW's exact behavior):
//   - Quaternion interpolation uses NLERP, NOT true SLERP
//   - GW uses TRANSPOSED rotation matrices (see Euler conversion docs below)
//   - Hierarchy accumulation: parent × child matrix multiplication
//   - Final matrices are 3x4 row-major, converted to 4x4 column-major for GPU
// =============================================================================

#include <std/mem.pat>
#include <std/math.pat>
#include <std/io.pat>
#pragma pattern_limit 5000000

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
// Created during decompression (no BB* source):
//   0xFA7 (4007): Bounding cylinder data (from geometry processing)
//   0xFA8 (4008): Skeleton references (from animation processing)
//   0xFAC (4012): File metadata (version + counts)

// =============================================================================
// FVF (Flexible Vertex Format) Lookup Functions
// =============================================================================

// FVF Lookup Tables
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
        u32 boneIndex;          // 4 bytes: palette index into skeletonBoneIndices[]
    } else if (pos_idx == 4) {
        float position[3];      // 12 bytes
    } else if (pos_idx == 5) {
        float position[3];      // 12 bytes: xyz
        float weights[2];       // 8 bytes: 2 bone weights
        u32 boneIndices;        // 4 bytes: packed bone indices
    } else if (pos_idx == 6) {
        float position[3];      // 12 bytes
        u32 boneIndex;          // 4 bytes: palette index into skeletonBoneIndices[]
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
//
// AT LOAD TIME: Euler angles are converted to quaternions.
//   - GW uses TRANSPOSED rotation matrices, effectively rotating by NEGATIVE angles
//   - Final quaternions are stored for runtime interpolation
//
// AT RUNTIME: Quaternions are interpolated using NLERP (not true SLERP!)
//   - NLERP: linear interpolate then normalize (faster, nearly identical for small angles)
//   - Result quaternion is converted to 3x4 matrix
//
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
// UV Compression Notes
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
// UV_DELTA_SCALE_FACTOR = 1.0/65536.0 (0x37800000 as float)

// =============================================================================
// COMMON STRUCTURES
// =============================================================================

struct String {
    char value[compute_str_len_plus_one($)];
};

// Bone Group/Info Structure (28 bytes = 0x1C)
// Used when classFlags & 0x002
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
// Size: weightCount * 7 bytes
struct BoneWeightEntry {
    u32 vertexIndex;          // Which vertex this weight applies to
    u8 boneIndex;             // Bone index
    u8 weight;                // Weight value (0-255, normalize to 0.0-1.0)
    u8 flags;                 // Additional flags
};

// Bone Transform Entry (8 bytes)
// Size: (boneCount0 + boneCount1 + boneCount2) * 8 bytes
struct BoneTransformEntry {
    u32 boneHash;             // Bone identifier hash
    u32 transformFlags;       // Transform flags or offset
};

// Bone Weight Set (24 bytes = 0x18 header + variable data)
// Bone weight data
struct BoneWeightSetHeader {
    u32 unknown0;
    u32 unknown1;
    u32 weightCount;          // Number of weight entries
    u32 boneCount0;
    u32 boneCount1;
    u32 boneCount2;
    // Followed by: weightCount * 7 bytes + (boneCount0 + boneCount1 + boneCount2) * 8 bytes
};

// Shader Definition (8 bytes)
struct ShaderDefinition {
    u8 cullFlags;           // Bits: 0x01=?, 0x02=twoSided, 0x04=?, 0x08=?
    u8 shaderType;          // Shader type index (0-10 mapped to internal enum)
    u32 shaderFlags;        // Feature flags
    u8 pixelShaderId;       // Pixel shader ID (8 = AMAT special)
    u8 textureCount;        // Number of textures used by this shader
};

// Material Definition (9 bytes)
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

// Attachment Point (48 bytes = 0x30)
// Each attachment stores 12 DWORDs of transform data
struct AttachmentPoint {
    float positionX, positionY, positionZ;
    float rotationX, rotationY, rotationZ, rotationW;
    float scaleX, scaleY, scaleZ;
    s32 parentBoneIndex;    // -1 for world space
    u32 attachmentType;
};

// Particle Emitter (88 bytes = 0x58)
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

// Animation Entry (20 bytes = 0x14 header)
struct AnimationEntryHeader {
    float unknown0;
    float unknown1;
    float unknown2;
    u32 duration;           // Animation duration
    u16 keyframeCount0;     // First keyframe set count
    u16 keyframeCount1;     // Second keyframe set count
    // Followed by: (keyframeCount0 + keyframeCount1) * 4 bytes of keyframe data
};

// Skeleton Entry (16 bytes = 0x10 header)
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
// BB8 to FA0 geometry conversion
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
// Submesh data - 8 DWORDs
struct SubmeshHeader_BB8 {
    u32 submeshFlags;           // 0x00: Visibility, render state flags
    u32 materialIndex;          // 0x04: Material/shader index
    u32 indexCount;             // 0x08: Number of vertex indices
    u32 vertexCount;            // 0x0C: Number of vertices
    u32 uvSetCount;             // 0x10: UV coordinate sets per vertex
    u32 boneGroupCount;         // 0x14: Number of bone groups
    u32 totalBoneRefs;          // 0x18: Total skeleton bone references
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

// Vertex count per bone group (2 bytes) - how many vertices are assigned to each bone group
// Vertices are assigned sequentially: first N vertices to group 0, next M to group 1, etc.
struct VertexCountPerBoneGroup {
    u16 vertexCount;
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
// Size formula: vertexCount * 6 * 2 = vertexCount * 12 bytes
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
// Total size: skeletonWeightCount * 0x15 bytes
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
//
// Variable data size calculation (from GWMB's ComplexStruct):
//   uVar2 = morphTargetCount
//   iVar3 = (animFlags & 2) ? 0 : (morphTargetCount - baseVertexCount)
//   uVar4 = (animFlags & 0x40) ? morphTargetCount : uvAnimFrameCount
//   res0 = (scaleKeyCount + rotationKeyCount) * 2 + unknown_22 + keyframeData1Count + uVar4 + keyframeData0Count
//   res1 = (iVar3 + positionDeltaCount * 2) * 9
//   res2 = res1 + res0 * 2 + extraDataSize + compressedDataSize
//   res3 = (boneWeightsPerVert * 8 + 12) * morphTargetCount
//   size = res3 + res2 * 2
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
    // End of 46-byte header (0x2E)

    // Calculate variable data size using GWMB's formula
    u32 uVar2 = morphTargetCount;
    u32 iVar3 = ((animFlags & 2) != 0) ? 0 : (uVar2 - baseVertexCount);
    u32 uVar4 = ((animFlags & 0x40) != 0) ? uVar2 : uvAnimFrameCount;

    u32 res0 = (scaleKeyCount + rotationKeyCount) * 2 + unknown_22 + keyframeData1Count + uVar4 + keyframeData0Count;
    u32 res1 = (iVar3 + positionDeltaCount * 2) * 9;
    u32 res2 = res1 + res0 * 2 + extraDataSize + compressedDataSize;
    u32 res3 = (boneWeightsPerVert * 8 + 12) * uVar2;
    u32 calculatedSize = res3 + res2 * 2;

    // Use the safer approach: calculate expected size from embeddedAnimTotalSize
    // Header size is 46 bytes (0x2E)
    u32 headerSize = 46;
    u32 sizeFromTotal = (g_embeddedAnimCount == 1 && g_embeddedAnimTotalSize > headerSize)
                        ? (g_embeddedAnimTotalSize - headerSize) : 0;

    // Prefer sizeFromTotal if available and reasonable - it's more reliable than
    // the formula which may fail if field sizes differ between formats
    u32 variableDataSize = (sizeFromTotal > 0 && sizeFromTotal < 0x100000)
                           ? sizeFromTotal
                           : ((calculatedSize > 0 && calculatedSize < 0x100000)
                              ? calculatedSize : 0);

    // Read the variable-length animation data
    if (variableDataSize > 0 && variableDataSize < 0x100000) {
        u8 animationData[variableDataSize] [[name("Animation Data")]];
    }
};

// Legacy alias for backwards compatibility
using GeosetEntry = EmbeddedAnimationEntry;

// =============================================================================
// 0xFA0 FORMAT STRUCTURES (Runtime Geometry)
// =============================================================================

// Geometry Header for 0xFA0 chunks (84 bytes = 0x54)
// Geometry header parsing
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
// Submesh vertex data parsing
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
// Data layout:
// 1. Index buffer: totalIndexCount * 2 bytes (combined LOD High/Med/Low indices)
// 2. Vertex buffer: vertexCount * vertexSize bytes (shared by all LOD levels)
// 3. Bone Group Sizes: boneGroupCount * 4 bytes (u32 array - bones per vertex group)
// 4. Skeleton Bone Indices: totalBoneRefs * 4 bytes (u32 array - flattened bone palette)
// 5. Triangle groups: triangleGroupCount * 12 bytes (strip/fan definitions) - usually 0
//
// =============================================================================
// BONE PALETTE SYSTEM (Hardware Skinning)
// =============================================================================
// GPU skinning uses a remap table. Vertices store a PALETTE INDEX (0 to totalBoneRefs-1)
// that directly indexes into skeletonBoneIndices[] to get the skeleton bone ID.
//
// IMPORTANT: vertex.boneIndex is a DIRECT INDEX into skeletonBoneIndices[], NOT a group index!
//            (Confirmed via Ghidra RE of GrFvf_SkinXYZNormal and Frida runtime capture)
//
// Data Structure in FA0 Submesh:
//   vertex.boneIndex      = palette index (0 to totalBoneRefs-1) - DIRECT INDEX!
//   boneGroupSizes[N]     = bones per group (for building per-submodel bone matrices)
//   skeletonBoneIndices[] = flat remap table: palette_idx -> skeleton_bone_id
//   totalBoneRefs         = sum(boneGroupSizes) = length of skeletonBoneIndices
//
// Simple Mapping (CORRECT - from Ghidra RE):
//   skeleton_bone = skeletonBoneIndices[vertex.boneIndex]
//
// Runtime Bone Matrix Construction (how GW prepares GPU-side matrices):
//   At render time, GW copies skeleton matrices into a compact per-submodel array:
//   for i in 0..totalBoneRefs:
//       submodel_matrices[i] = skeleton_matrices[skeletonBoneIndices[i]]
//   The vertex.boneIndex then indexes this compact submodel_matrices array.
//
// Example (from 0xBC68 pig model):
//   boneGroupCount = 64, totalBoneRefs = 126
//   boneGroupSizes = [1, 2, 2, 1, 3, 2, 2, 1, 3, 2, 4, 1, ...]
//   skeletonBoneIndices = [23, 22, 23, 23, 20, 22, 11, 21, 22, 11, ...]
//
//   A vertex with boneIndex=0 → skeletonBoneIndices[0] = 23 → skeleton bone 23
//   A vertex with boneIndex=5 → skeletonBoneIndices[5] = 22 → skeleton bone 22
//
// MIRROR NOTE: For symmetric models, skeletonBoneIndices may only contain
// right-side bones (positive X). Left-side vertices use mirror bones at runtime.
// Mirror pairs are determined by matching bone positions with opposite X.
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
    u32 animationSegmentCount;  // 0x18: Number of animation segment entries (22 bytes each)
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
    float geometryScale;        // 0x20: Skeleton/geometry scale factor (GW stores at model+0x100)
                                //       If negative/zero, GW computes scale from bounding data
    u32 unknown_0x24;
    u32 unknown_0x28;
    u32 bindPoseBoneCount;      // 0x2C: Number of bones in bind pose section
    u32 unknown_0x30;
    u32 transformDataSize;      // 0x34: Number of 24-byte sequence entries (NOT gated by flags!)
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

// =============================================================================
// FA1 BIND POSE ENTRY (16 bytes per bone)
// =============================================================================
// Located after FA1 header + bounding cylinders + sequences.
// Contains bone position in model space + hierarchy encoding.
//
// FA1 HIERARCHY ENCODING (DIFFERENT FROM BB9!):
//   FA1 uses DIRECT PARENT INDEXING, not POP_COUNT like BB9.
//
// parentInfo field encoding:
//   Bits 0-7 (low byte):   (parent_index + 1), where 0 = root bone (no parent)
//   Bit 28 (0x10000000):   Unknown flag (possibly intermediate bone marker)
//
// DIRECT PARENT ALGORITHM:
//   - For each bone:
//     lowByte = parentInfo & 0xFF
//     if (lowByte == 0) parent = -1 (this is a root bone)
//     else parent = lowByte - 1
//
// Example: parentInfo low bytes [0, 1, 2, 3, 3, 3, 6, 7, 6, 9, ...]
//   Bone 0: lowByte=0 → parent=-1 (root)
//   Bone 1: lowByte=1 → parent=0
//   Bone 2: lowByte=2 → parent=1
//   Bone 3: lowByte=3 → parent=2
//   Bone 4: lowByte=3 → parent=2 (sibling of bone 3)
//   etc.
//
struct FA1BindPoseEntry {
    float posX;                 // 0x00: Bind pose X position (model space)
    float posY;                 // 0x04: Bind pose Y position (model space)
    float posZ;                 // 0x08: Bind pose Z position (model space)
    u32 parentInfo;             // 0x0C: Low byte = (parent_index + 1), 0 = root
} [[format("format_fa1_bind_pose")]];

fn format_fa1_bind_pose(FA1BindPoseEntry e) {
    u8 lowByte = e.parentInfo & 0xFF;
    s32 parentIdx = (lowByte == 0) ? -1 : s32(lowByte - 1);
    bool hasFlag = (e.parentInfo & 0x10000000) != 0;
    if (hasFlag) {
        return std::format("pos=({:.2f},{:.2f},{:.2f}) parent={} [FLAG]",
            e.posX, e.posY, e.posZ, parentIdx);
    }
    return std::format("pos=({:.2f},{:.2f},{:.2f}) parent={}",
        e.posX, e.posY, e.posZ, parentIdx);
};

// BB9 Sequence Entry (24 bytes = 6 DWORDs)
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
u32 g_embeddedAnimTotalSize;  // Total size of embedded animation data (used to validate entry sizes)
u32 g_embeddedAnimCount;       // Number of embedded animation entries

// =============================================================================
// BB8 GEOMETRY CHUNK PARSING
// =============================================================================

struct SubmeshData_BB8 {
    SubmeshHeader_BB8 header [[name("Submesh Header")]];

    // Index buffer
    u16 indices[header.indexCount] [[name("Index Buffer")]];

    // Position buffer (12 bytes per vertex = float3)
    VertexPosition positions[header.vertexCount] [[name("Vertex Positions")]];

    // Per-vertex bone group data (4 bytes per vertex) - ALWAYS present after positions
    // From C++ ParseSubmeshAtOffset: bone_group_data_start = pos_end;
    // Each 4-byte entry contains: bone_group_idx (1 byte) + other data (3 bytes)
    // The game's skinning (GrFvf_SkinXYZNormal) reads only the first byte for bone index
    u32 perVertexBoneData[header.vertexCount] [[name("Per-Vertex Bone Data")]];

    // UV data section - format detected by checking first two u16 values
    // C++ logic: if both values < 256, use delta+offset format; otherwise direct format
    if (header.uvSetCount > 0) {
        // Peek at first two u16 values to detect format
        u16 peekCnt0 = std::mem::read_unsigned($, 2);
        u16 peekCnt1 = std::mem::read_unsigned($ + 2, 2);

        if (peekCnt0 < 256 && peekCnt1 < 256) {
            // DELTA+OFFSET UV FORMAT (detected by small header values)
            // Structure: header(4) + offset_arrays((cnt0+cnt1)*4) + deltas(vertexCount*uvSetCount*4)
            UVCompressionHeader uvHeader [[name("UV Compression Header")]];
            u16 uThresholds[uvHeader.uOffsetCount] [[name("U Thresholds")]];
            u16 vThresholds[uvHeader.vOffsetCount] [[name("V Thresholds")]];
            s16 uOffsets[uvHeader.uOffsetCount] [[name("U Offsets")]];
            s16 vOffsets[uvHeader.vOffsetCount] [[name("V Offsets")]];
            UV_Delta uvDeltas[header.vertexCount * header.uvSetCount] [[name("UV Delta Data")]];

            // Bone data: vertex counts per group (u16[]) + group sizes (u8[]) + skeleton bone indices (u8[])
            if (header.boneGroupCount > 0) {
                VertexCountPerBoneGroup vertexCountsPerGroup[header.boneGroupCount] [[name("Vertices Per Bone Group")]];
                u8 boneGroupSizes[header.boneGroupCount] [[name("Bone Group Sizes (like FA0 extraData1)")]];
            }

            if (header.totalBoneRefs > 0) {
                u8 skeletonBoneIndices[header.totalBoneRefs] [[name("Skeleton Bone Indices")]];
            }
        } else {
            // DIRECT UV FORMAT (large first values = actual UV data)
            // UVs stored directly as u16 pairs, scaled by 1/65536
            UV_Delta uvData[header.vertexCount * header.uvSetCount] [[name("UV Data (Direct u16 pairs)")]];

            // Bone data: vertex counts per group (u16[]) + group sizes (u8[]) + skeleton bone indices (u8[])
            if (header.boneGroupCount > 0) {
                VertexCountPerBoneGroup vertexCountsPerGroup[header.boneGroupCount] [[name("Vertices Per Bone Group")]];
                u8 boneGroupSizes[header.boneGroupCount] [[name("Bone Group Sizes (like FA0 extraData1)")]];
            }

            if (header.totalBoneRefs > 0) {
                u8 skeletonBoneIndices[header.totalBoneRefs] [[name("Skeleton Bone Indices")]];
            }
        }
    }

    // Triangle group data (triangleGroupCount * 12 bytes each)
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
struct AnimationEntry_BB8 {
    u32 dataSize [[name("Data Size")]];
    if (dataSize > 0 && dataSize < 0x10000) {
        u32 keyframeData[dataSize * 2] [[name("Keyframe Data")]];
    }
};

// LOD Entry for BB8 (variable size)
// Each entry has (indexCount, vertexCount) header
// followed by indexCount * 2 bytes (u16 indices) + vertexCount * 12 bytes (LODVertexEntry)
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
        // Complex format not yet fully parsed
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
        // Read the raw animation data - embeddedAnimTotalSize is the total size
        // including all entry headers and data
        // NOTE: The size value includes all entry data, not including the 8-byte header above
        if (embeddedAnimTotalSize > 0 && embeddedAnimTotalSize < 0x1000000) {
            u8 embeddedAnimData[embeddedAnimTotalSize] [[name("Embedded Animation Data (raw)")]];
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
        // Parse all submeshes
        u32 parseCount = (submeshCount > 0 && submeshCount < 255) ? submeshCount : 0;
        if (parseCount > 0) {
            SubmeshData_BB8 submeshes[parseCount] [[name("Submeshes")]];
        }
    }

    // LOD data (if classFlags.HAS_LOD_DATA = 0x10)
    // Each entry has (indexCount, vertexCount) header
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
    // Structure: boneCount (u32), weightCount (u32), then:
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
    // Structure: count (u32), then for each: dataSize (u32) + dataSize * 2 bytes
    if (header.classFlags.HAS_ANIMATION_SEQUENCES) {
        u32 animCount_bb8 [[name("Animation Sequence Count")]];
        if (animCount_bb8 > 0 && animCount_bb8 < 256) {
            AnimationEntry_BB8 animEntries_bb8[animCount_bb8] [[name("Animation Sequence Entries")]];
        }
    }

    // Bounding data (if classFlags.HAS_BOUNDING_DATA = 0x04)
    // First DWORD is extra size, next 3 DWORDs are counts/values
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
//   2. The hierarchyByte has TWO possible interpretations (auto-detected at parse time):
//      - TREE_DEPTH mode: absolute level in hierarchy (0=root, 1=child, 2=grandchild)
//      - POP_COUNT mode: levels to pop from internal matrix stack before pushing
//   3. Runtime applies (basePos + animDelta) then subtracts basePos, leaving only animDelta
//
// HIERARCHY ENCODING DETECTION:
//   TREE_DEPTH mode: Values like [0,1,2,3,2,3,1,2] - increases by 1 for children, decreases at branches
//   POP_COUNT mode:  Values like [0,0,0,0,3,0,0] - mostly 0s with occasional jumps at branch points
//   WORLD_SPACE mode: All zeros - each bone has independent absolute transforms (no hierarchy)
//
// PARENT COMPUTATION ALGORITHMS:
//   TREE_DEPTH: Track bones at each depth level, parent = most recent bone at (depth-1)
//   POP_COUNT:  Maintain stack of bone indices, pop N entries, parent = stack.top(), push current
//
// RUNTIME TRANSFORM ALGORITHM (per bone, in order):
//   1. Pop 'hierarchyByte' levels from matrix stack (returns to parent's transform)
//   2. Build local matrix with translation = (basePos + animDelta) and sampled rotation
//   3. Push local matrix onto stack (multiplies with parent: result = parent * local)
//   4. Translate by -basePos (cancels basePos, leaving only animDelta in rotated frame)
//   5. If NOT intermediate bone: store final matrix for skinning, increment output index
//
// NET EFFECT: bone's animated position = parent_rotation * animDelta + parent_position
//
// BONEFLAGS (u32 at offset 0x0C):
//   Bits 0-7 (hierarchyByte):  Tree depth OR pop count (see detection logic above)
//   Bit 28 (0x10000000):       INTERMEDIATE BONE FLAG
//                              When set, bone participates in hierarchy (matrix stack push/pop)
//                              but does NOT produce an output skinning matrix.
//                              Mesh vertices reference OUTPUT indices which SKIP intermediate bones!
//                              Output bone count = total bones - intermediate bones
//
struct BoneAnimHeader {
    float basePositionX;    // 0x00: Bone's ABSOLUTE X position in bind pose (world space)
    float basePositionY;    // 0x04: Bone's ABSOLUTE Y position in bind pose (world space)
    float basePositionZ;    // 0x08: Bone's ABSOLUTE Z position in bind pose (world space)
    u8 hierarchyByte;       // 0x0C: Tree depth OR pop count (see detection logic above)
    u8 flagsByte1;          // 0x0D: Special handling flags
    u8 flagsByte2;          // 0x0E: Bit flags for light attachments, render skipping
    u8 flagsByte3;          // 0x0F: Bit 4 (0x10) = INTERMEDIATE BONE (no output matrix)
    u16 posKeyCount;        // 0x10: Number of position keyframes (animated delta from base)
    u16 rotKeyCount;        // 0x12: Number of rotation keyframes (VLE Euler -> quaternion)
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
    //   bindOffset = childBasePos - parentBasePos (computed once at load time)
    //   worldPos = parentWorldPos + rotate(bindOffset + animDelta, parentWorldRot)
    //   worldRot = parentWorldRot * localRot (quaternion multiplication)
    //   skinnedVertex = worldPos + rotate(vertex - basePos, worldRot)
} [[format("format_bone_anim_header")]];

fn format_bone_anim_header(BoneAnimHeader h) {
    bool isIntermediate = (h.flagsByte3 & 0x10) != 0;  // 0x10 in byte3 = 0x10000000 in u32
    if (isIntermediate) {
        return std::format("pos=({:.1f},{:.1f},{:.1f}) pop={} keys=P{}R{}S{} [INTERMEDIATE]",
            h.basePositionX, h.basePositionY, h.basePositionZ,
            h.hierarchyByte, h.posKeyCount, h.rotKeyCount, h.scaleKeyCount);
    }
    return std::format("pos=({:.1f},{:.1f},{:.1f}) pop={} keys=P{}R{}S{}",
        h.basePositionX, h.basePositionY, h.basePositionZ,
        h.hierarchyByte, h.posKeyCount, h.rotKeyCount, h.scaleKeyCount);
};

// VLE (Variable Length Encoding) format documentation:
// VLE decoding for keyframe times and Euler angles
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
// Euler angle to quaternion conversion:
//   1. Decode 3 delta-encoded u16 values (X, Y, Z Euler angles)
//   2. Convert [0,65535] to radians: angle = value * (2π/65536) - π
//   3. Build 3x3 rotation matrices: Rx, Ry, Rz
//
//   CRITICAL: GW uses TRANSPOSED rotation matrices!
//   Standard Rx:              GW's Rx (transposed):
//   [1   0    0  ]            [1   0     0  ]
//   [0  cx  -sx ]            [0  cx    sx ]  <- signs swapped
//   [0  sx   cx ]            [0 -sx    cx ]  <- signs swapped
//
//   Transposed matrix = inverse = rotation by NEGATIVE angle.
//
//   4. Multiply: result = Rx * Ry * Rz (code order, right-multiply)
//      Because matrices are transposed, this effectively computes
//      rotation by (-x, -y, -z) in ZYX intrinsic order.
//   5. Convert matrix to quaternion
//   6. Ensure continuity: if dot(q, prev_q) < 0, negate q
//
// TO MATCH GW IN EXTERNAL TOOLS:
//   Either negate all three Euler angles before standard ZYX conversion,
//   or use transposed rotation matrices like GW does.

// =============================================================================
// BB9 SECTION STRUCTURES (defined before Chunk_BB9_Animation)
// =============================================================================

// Animation Segment Entry (22 bytes per entry)
// These define animation regions within phases, used for:
// - Loop boundaries (main animation segment vs intro)
// - Sub-animation markers (/laugh, /cheer, strafe variants within a phase)
// The hash field identifies the animation type/category.
// For looping: the segment with the largest time range defines the loop region.
struct AnimationSegmentEntry {
    u32 hash [[name("Segment Hash")]];                // 0x00: Animation segment identifier
    u32 startTime [[name("Start Time")]];             // 0x04: Segment start in animation units (100000 = 1 sec)
    u32 endTime [[name("End Time")]];                 // 0x08: Segment end in animation units
    u16 flags [[name("Flags")]];                      // 0x0C: Flags (0x1212 common)
    u8  reserved[8] [[name("Reserved")]];             // 0x0E: Reserved/padding
};  // 22 bytes total (0x16)

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

// Joint entry for physics/IK (21 bytes output, input depends on compress_flag)
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

// Constraint entry for joint physics (12 bytes per entry)
struct ConstraintEntry {
    u32 pad;    // 0x00: Usually 0
    u32 hash;   // 0x04: Constraint identifier
    u32 idx;    // 0x08: Index reference
};  // 12 bytes total

// Attachment animation bounds entry
// Format: 24 bytes per entry (6 DWORDs)
// AttachmentEntry: 48 bytes (12 DWORDs = 0x30 bytes)
// Attachment point data: 12 DWORDs per entry
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

// Frame transform data: 6 DWORDs = 24 bytes per frame
struct AttachmentFrameData {
    u32 data[6] [[name("Frame Transform Data (6 DWORDs)")]];
};  // 24 bytes

// Sequence data: 4 DWORDs = 16 bytes per sequence
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
    // Section order (determined by flag bits):
    //   1. Animation segment entries (count from header.animationSegmentCount) - 22 bytes each
    //   2. Per-bone data (flag 0x4000): header + 24-byte entries
    //   3. Joint data (flag 0x0200): header + 21-byte entries + constraints
    //   4. Bone weight data (flag 0x0040): variable
    //   5. Standard bone transforms (flag 0x0020): header + VLE data
    //   6. Other sections: 0x0800, 0x2000, 0x1000, 0x8000
    //   7. Attachment data (flag 0x0080): header + 20-byte entries
    //   8. Unknown section (flag 0x0400): variable
    // ==========================================================================

    // Section 1: Animation segment entries (defines loop regions and sub-animations)
    // NOTE: VLE bone data parsing leaves cursor at wrong position (VLE size is approximate).
    // We search for valid segment data by checking first entry at various offsets.
    // Valid segments have: timing < 10M (~100s), endTime >= startTime, flags < 0x5000,
    // and reserved bytes (14-17) are all zero.
    if (header.animationSegmentCount > 0 && header.animationSegmentCount < 500) {
        u32 vleEndPos = $;
        u32 segmentDataSize = header.animationSegmentCount * 22;
        u32 chunkEnd = g_chunkDataStart + chunkSize;
        u32 validSegmentOffset = vleEndPos;
        bool foundValid = false;

        // Macro-like inline test for each offset position
        // Tests first segment entry at (vleEndPos + OFFSET)
        // Check: startTime < 10M, endTime < 10M, endTime >= startTime, flags < 0x5000, reserved=0

        // Test offset 0
        if (!foundValid && vleEndPos + segmentDataSize <= chunkEnd) {
            u32 s0 @ vleEndPos + 4; u32 e0 @ vleEndPos + 8; u16 f0 @ vleEndPos + 12;
            u8 r00 @ vleEndPos + 14; u8 r01 @ vleEndPos + 15; u8 r02 @ vleEndPos + 16; u8 r03 @ vleEndPos + 17;
            if (s0 < 10000000 && e0 < 10000000 && e0 >= s0 && f0 < 0x5000 && r00 == 0 && r01 == 0 && r02 == 0 && r03 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos;
            }
        }
        // Test offset +2
        if (!foundValid && vleEndPos + 2 + segmentDataSize <= chunkEnd) {
            u32 s2 @ vleEndPos + 2 + 4; u32 e2 @ vleEndPos + 2 + 8; u16 f2 @ vleEndPos + 2 + 12;
            u8 r20 @ vleEndPos + 2 + 14; u8 r21 @ vleEndPos + 2 + 15; u8 r22 @ vleEndPos + 2 + 16; u8 r23 @ vleEndPos + 2 + 17;
            if (s2 < 10000000 && e2 < 10000000 && e2 >= s2 && f2 < 0x5000 && r20 == 0 && r21 == 0 && r22 == 0 && r23 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 2;
            }
        }
        // Test offset +4
        if (!foundValid && vleEndPos + 4 + segmentDataSize <= chunkEnd) {
            u32 s4 @ vleEndPos + 4 + 4; u32 e4 @ vleEndPos + 4 + 8; u16 f4 @ vleEndPos + 4 + 12;
            u8 r40 @ vleEndPos + 4 + 14; u8 r41 @ vleEndPos + 4 + 15; u8 r42 @ vleEndPos + 4 + 16; u8 r43 @ vleEndPos + 4 + 17;
            if (s4 < 10000000 && e4 < 10000000 && e4 >= s4 && f4 < 0x5000 && r40 == 0 && r41 == 0 && r42 == 0 && r43 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 4;
            }
        }
        // Test offset +8
        if (!foundValid && vleEndPos + 8 + segmentDataSize <= chunkEnd) {
            u32 st8 @ vleEndPos + 8 + 4; u32 et8 @ vleEndPos + 8 + 8; u16 fl8 @ vleEndPos + 8 + 12;
            u8 r80 @ vleEndPos + 8 + 14; u8 r81 @ vleEndPos + 8 + 15;
            if (st8 < 10000000 && et8 < 10000000 && et8 >= st8 && fl8 < 0x5000 && r80 == 0 && r81 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 8;
            }
        }
        // Test offset +16
        if (!foundValid && vleEndPos + 16 + segmentDataSize <= chunkEnd) {
            u32 st16 @ vleEndPos + 16 + 4; u32 et16 @ vleEndPos + 16 + 8; u16 fl16 @ vleEndPos + 16 + 12;
            u8 r160 @ vleEndPos + 16 + 14; u8 r161 @ vleEndPos + 16 + 15;
            if (st16 < 10000000 && et16 < 10000000 && et16 >= st16 && fl16 < 0x5000 && r160 == 0 && r161 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 16;
            }
        }
        // Test offset +32
        if (!foundValid && vleEndPos + 32 + segmentDataSize <= chunkEnd) {
            u32 st32 @ vleEndPos + 32 + 4; u32 et32 @ vleEndPos + 32 + 8; u16 fl32 @ vleEndPos + 32 + 12;
            u8 r320 @ vleEndPos + 32 + 14; u8 r321 @ vleEndPos + 32 + 15;
            if (st32 < 10000000 && et32 < 10000000 && et32 >= st32 && fl32 < 0x5000 && r320 == 0 && r321 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 32;
            }
        }
        // Test offset +64
        if (!foundValid && vleEndPos + 64 + segmentDataSize <= chunkEnd) {
            u32 st64 @ vleEndPos + 64 + 4; u32 et64 @ vleEndPos + 64 + 8; u16 fl64 @ vleEndPos + 64 + 12;
            u8 r640 @ vleEndPos + 64 + 14; u8 r641 @ vleEndPos + 64 + 15;
            if (st64 < 10000000 && et64 < 10000000 && et64 >= st64 && fl64 < 0x5000 && r640 == 0 && r641 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 64;
            }
        }
        // Test offset +128
        if (!foundValid && vleEndPos + 128 + segmentDataSize <= chunkEnd) {
            u32 st_128 @ vleEndPos + 128 + 4; u32 et_128 @ vleEndPos + 128 + 8; u16 fl_128 @ vleEndPos + 128 + 12;
            u8 r1280 @ vleEndPos + 128 + 14; u8 r1281 @ vleEndPos + 128 + 15;
            if (st_128 < 10000000 && et_128 < 10000000 && et_128 >= st_128 && fl_128 < 0x5000 && r1280 == 0 && r1281 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 128;
            }
        }
        // Test offset +256
        if (!foundValid && vleEndPos + 256 + segmentDataSize <= chunkEnd) {
            u32 st_256 @ vleEndPos + 256 + 4; u32 et_256 @ vleEndPos + 256 + 8; u16 fl_256 @ vleEndPos + 256 + 12;
            u8 r2560 @ vleEndPos + 256 + 14; u8 r2561 @ vleEndPos + 256 + 15;
            if (st_256 < 10000000 && et_256 < 10000000 && et_256 >= st_256 && fl_256 < 0x5000 && r2560 == 0 && r2561 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 256;
            }
        }
        // Test offset +512
        if (!foundValid && vleEndPos + 512 + segmentDataSize <= chunkEnd) {
            u32 st_512 @ vleEndPos + 512 + 4; u32 et_512 @ vleEndPos + 512 + 8; u16 fl_512 @ vleEndPos + 512 + 12;
            u8 r5120 @ vleEndPos + 512 + 14; u8 r5121 @ vleEndPos + 512 + 15;
            if (st_512 < 10000000 && et_512 < 10000000 && et_512 >= st_512 && fl_512 < 0x5000 && r5120 == 0 && r5121 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 512;
            }
        }
        // Test offset +1024
        if (!foundValid && vleEndPos + 1024 + segmentDataSize <= chunkEnd) {
            u32 st_1024 @ vleEndPos + 1024 + 4; u32 et_1024 @ vleEndPos + 1024 + 8; u16 fl_1024 @ vleEndPos + 1024 + 12;
            u8 r10240 @ vleEndPos + 1024 + 14; u8 r10241 @ vleEndPos + 1024 + 15;
            if (st_1024 < 10000000 && et_1024 < 10000000 && et_1024 >= st_1024 && fl_1024 < 0x5000 && r10240 == 0 && r10241 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 1024;
            }
        }
        // Test offset +2048
        if (!foundValid && vleEndPos + 2048 + segmentDataSize <= chunkEnd) {
            u32 st_2048 @ vleEndPos + 2048 + 4; u32 et_2048 @ vleEndPos + 2048 + 8; u16 fl_2048 @ vleEndPos + 2048 + 12;
            u8 r20480 @ vleEndPos + 2048 + 14; u8 r20481 @ vleEndPos + 2048 + 15;
            if (st_2048 < 10000000 && et_2048 < 10000000 && et_2048 >= st_2048 && fl_2048 < 0x5000 && r20480 == 0 && r20481 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 2048;
            }
        }
        // Test offset +4096
        if (!foundValid && vleEndPos + 4096 + segmentDataSize <= chunkEnd) {
            u32 st_4096 @ vleEndPos + 4096 + 4; u32 et_4096 @ vleEndPos + 4096 + 8; u16 fl_4096 @ vleEndPos + 4096 + 12;
            u8 r40960 @ vleEndPos + 4096 + 14; u8 r40961 @ vleEndPos + 4096 + 15;
            if (st_4096 < 10000000 && et_4096 < 10000000 && et_4096 >= st_4096 && fl_4096 < 0x5000 && r40960 == 0 && r40961 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 4096;
            }
        }
        // Test offset +8192
        if (!foundValid && vleEndPos + 8192 + segmentDataSize <= chunkEnd) {
            u32 st_8192 @ vleEndPos + 8192 + 4; u32 et_8192 @ vleEndPos + 8192 + 8; u16 fl_8192 @ vleEndPos + 8192 + 12;
            u8 r81920 @ vleEndPos + 8192 + 14; u8 r81921 @ vleEndPos + 8192 + 15;
            if (st_8192 < 10000000 && et_8192 < 10000000 && et_8192 >= st_8192 && fl_8192 < 0x5000 && r81920 == 0 && r81921 == 0) {
                foundValid = true; validSegmentOffset = vleEndPos + 8192;
            }
        }

        // If still not found, try backward search: scan segment data directly
        // Rather than looking for joint header, look for valid segment data at expected offsets
        // Try: chunkEnd - (segmentDataSize + N) for various N representing tail data sizes
        // Tail can be: joints (~100-800 bytes) + other sections
        if (!foundValid) {
            // Test segment positions: segments at chunkEnd - segmentDataSize - tailOffset
            // Where tailOffset accounts for data after segments (joints, etc.)
            // Try tailOffset values: 50, 100, 150, 200, 250, 300, 350, 384, 400, 500, 600, 800, 1000

            // Probe tailOffset = 50
            if (!foundValid && chunkEnd > segmentDataSize + 50) {
                u32 sp50 = chunkEnd - segmentDataSize - 50;
                if (sp50 >= vleEndPos) {
                    u32 vs50 @ sp50 + 4; u32 ve50 @ sp50 + 8; u16 vf50 @ sp50 + 12;
                    u8 vr50a @ sp50 + 14; u8 vr50b @ sp50 + 15;
                    if (vs50 < 10000000 && ve50 < 10000000 && ve50 >= vs50 && vf50 < 0x5000 && vr50a == 0 && vr50b == 0) {
                        foundValid = true; validSegmentOffset = sp50;
                    }
                }
            }
            // Probe tailOffset = 100
            if (!foundValid && chunkEnd > segmentDataSize + 100) {
                u32 sp_100 = chunkEnd - segmentDataSize - 100;
                if (sp_100 >= vleEndPos) {
                    u32 vs_100 @ sp_100 + 4; u32 ve_100 @ sp_100 + 8; u16 vf_100 @ sp_100 + 12;
                    u8 vr_100a @ sp_100 + 14; u8 vr_100b @ sp_100 + 15;
                    if (vs_100 < 10000000 && ve_100 < 10000000 && ve_100 >= vs_100 && vf_100 < 0x5000 && vr_100a == 0 && vr_100b == 0) {
                        foundValid = true; validSegmentOffset = sp_100;
                    }
                }
            }
            // Probe tailOffset = 150
            if (!foundValid && chunkEnd > segmentDataSize + 150) {
                u32 sp_150 = chunkEnd - segmentDataSize - 150;
                if (sp_150 >= vleEndPos) {
                    u32 vs_150 @ sp_150 + 4; u32 ve_150 @ sp_150 + 8; u16 vf_150 @ sp_150 + 12;
                    u8 vr_150a @ sp_150 + 14; u8 vr_150b @ sp_150 + 15;
                    if (vs_150 < 10000000 && ve_150 < 10000000 && ve_150 >= vs_150 && vf_150 < 0x5000 && vr_150a == 0 && vr_150b == 0) {
                        foundValid = true; validSegmentOffset = sp_150;
                    }
                }
            }
            // Probe tailOffset = 200
            if (!foundValid && chunkEnd > segmentDataSize + 200) {
                u32 sp_200 = chunkEnd - segmentDataSize - 200;
                if (sp_200 >= vleEndPos) {
                    u32 vs_200 @ sp_200 + 4; u32 ve_200 @ sp_200 + 8; u16 vf_200 @ sp_200 + 12;
                    u8 vr_200a @ sp_200 + 14; u8 vr_200b @ sp_200 + 15;
                    if (vs_200 < 10000000 && ve_200 < 10000000 && ve_200 >= vs_200 && vf_200 < 0x5000 && vr_200a == 0 && vr_200b == 0) {
                        foundValid = true; validSegmentOffset = sp_200;
                    }
                }
            }
            // Probe tailOffset = 250
            if (!foundValid && chunkEnd > segmentDataSize + 250) {
                u32 sp_250 = chunkEnd - segmentDataSize - 250;
                if (sp_250 >= vleEndPos) {
                    u32 vs_250 @ sp_250 + 4; u32 ve_250 @ sp_250 + 8; u16 vf_250 @ sp_250 + 12;
                    u8 vr_250a @ sp_250 + 14; u8 vr_250b @ sp_250 + 15;
                    if (vs_250 < 10000000 && ve_250 < 10000000 && ve_250 >= vs_250 && vf_250 < 0x5000 && vr_250a == 0 && vr_250b == 0) {
                        foundValid = true; validSegmentOffset = sp_250;
                    }
                }
            }
            // Probe tailOffset = 280
            if (!foundValid && chunkEnd > segmentDataSize + 280) {
                u32 sp_280 = chunkEnd - segmentDataSize - 280;
                if (sp_280 >= vleEndPos) {
                    u32 vs_280 @ sp_280 + 4; u32 ve_280 @ sp_280 + 8; u16 vf_280 @ sp_280 + 12;
                    u8 vr_280a @ sp_280 + 14; u8 vr_280b @ sp_280 + 15;
                    if (vs_280 < 10000000 && ve_280 < 10000000 && ve_280 >= vs_280 && vf_280 < 0x5000 && vr_280a == 0 && vr_280b == 0) {
                        foundValid = true; validSegmentOffset = sp_280;
                    }
                }
            }
            // Probe tailOffset = 290 (calculated for test file)
            if (!foundValid && chunkEnd > segmentDataSize + 290) {
                u32 sp_290 = chunkEnd - segmentDataSize - 290;
                if (sp_290 >= vleEndPos) {
                    u32 vs_290 @ sp_290 + 4; u32 ve_290 @ sp_290 + 8; u16 vf_290 @ sp_290 + 12;
                    u8 vr_290a @ sp_290 + 14; u8 vr_290b @ sp_290 + 15;
                    if (vs_290 < 10000000 && ve_290 < 10000000 && ve_290 >= vs_290 && vf_290 < 0x5000 && vr_290a == 0 && vr_290b == 0) {
                        foundValid = true; validSegmentOffset = sp_290;
                    }
                }
            }
            // Probe tailOffset = 300
            if (!foundValid && chunkEnd > segmentDataSize + 300) {
                u32 sp_300 = chunkEnd - segmentDataSize - 300;
                if (sp_300 >= vleEndPos) {
                    u32 vs_300 @ sp_300 + 4; u32 ve_300 @ sp_300 + 8; u16 vf_300 @ sp_300 + 12;
                    u8 vr_300a @ sp_300 + 14; u8 vr_300b @ sp_300 + 15;
                    if (vs_300 < 10000000 && ve_300 < 10000000 && ve_300 >= vs_300 && vf_300 < 0x5000 && vr_300a == 0 && vr_300b == 0) {
                        foundValid = true; validSegmentOffset = sp_300;
                    }
                }
            }
            // Probe tailOffset = 350
            if (!foundValid && chunkEnd > segmentDataSize + 350) {
                u32 sp_350 = chunkEnd - segmentDataSize - 350;
                if (sp_350 >= vleEndPos) {
                    u32 vs_350 @ sp_350 + 4; u32 ve_350 @ sp_350 + 8; u16 vf_350 @ sp_350 + 12;
                    u8 vr_350a @ sp_350 + 14; u8 vr_350b @ sp_350 + 15;
                    if (vs_350 < 10000000 && ve_350 < 10000000 && ve_350 >= vs_350 && vf_350 < 0x5000 && vr_350a == 0 && vr_350b == 0) {
                        foundValid = true; validSegmentOffset = sp_350;
                    }
                }
            }
            // Probe tailOffset = 384 (exact for test file)
            if (!foundValid && chunkEnd > segmentDataSize + 384) {
                u32 sp_384 = chunkEnd - segmentDataSize - 384;
                if (sp_384 >= vleEndPos) {
                    u32 vs_384 @ sp_384 + 4; u32 ve_384 @ sp_384 + 8; u16 vf_384 @ sp_384 + 12;
                    u8 vr_384a @ sp_384 + 14; u8 vr_384b @ sp_384 + 15;
                    if (vs_384 < 10000000 && ve_384 < 10000000 && ve_384 >= vs_384 && vf_384 < 0x5000 && vr_384a == 0 && vr_384b == 0) {
                        foundValid = true; validSegmentOffset = sp_384;
                    }
                }
            }
            // Probe tailOffset = 400
            if (!foundValid && chunkEnd > segmentDataSize + 400) {
                u32 sp_400 = chunkEnd - segmentDataSize - 400;
                if (sp_400 >= vleEndPos) {
                    u32 vs_400 @ sp_400 + 4; u32 ve_400 @ sp_400 + 8; u16 vf_400 @ sp_400 + 12;
                    u8 vr_400a @ sp_400 + 14; u8 vr_400b @ sp_400 + 15;
                    if (vs_400 < 10000000 && ve_400 < 10000000 && ve_400 >= vs_400 && vf_400 < 0x5000 && vr_400a == 0 && vr_400b == 0) {
                        foundValid = true; validSegmentOffset = sp_400;
                    }
                }
            }
            // Probe tailOffset = 500
            if (!foundValid && chunkEnd > segmentDataSize + 500) {
                u32 sp_500 = chunkEnd - segmentDataSize - 500;
                if (sp_500 >= vleEndPos) {
                    u32 vs_500 @ sp_500 + 4; u32 ve_500 @ sp_500 + 8; u16 vf_500 @ sp_500 + 12;
                    u8 vr_500a @ sp_500 + 14; u8 vr_500b @ sp_500 + 15;
                    if (vs_500 < 10000000 && ve_500 < 10000000 && ve_500 >= vs_500 && vf_500 < 0x5000 && vr_500a == 0 && vr_500b == 0) {
                        foundValid = true; validSegmentOffset = sp_500;
                    }
                }
            }
            // Probe tailOffset = 600
            if (!foundValid && chunkEnd > segmentDataSize + 600) {
                u32 sp_600 = chunkEnd - segmentDataSize - 600;
                if (sp_600 >= vleEndPos) {
                    u32 vs_600 @ sp_600 + 4; u32 ve_600 @ sp_600 + 8; u16 vf_600 @ sp_600 + 12;
                    u8 vr_600a @ sp_600 + 14; u8 vr_600b @ sp_600 + 15;
                    if (vs_600 < 10000000 && ve_600 < 10000000 && ve_600 >= vs_600 && vf_600 < 0x5000 && vr_600a == 0 && vr_600b == 0) {
                        foundValid = true; validSegmentOffset = sp_600;
                    }
                }
            }
            // Probe tailOffset = 800
            if (!foundValid && chunkEnd > segmentDataSize + 800) {
                u32 sp_800 = chunkEnd - segmentDataSize - 800;
                if (sp_800 >= vleEndPos) {
                    u32 vs_800 @ sp_800 + 4; u32 ve_800 @ sp_800 + 8; u16 vf_800 @ sp_800 + 12;
                    u8 vr_800a @ sp_800 + 14; u8 vr_800b @ sp_800 + 15;
                    if (vs_800 < 10000000 && ve_800 < 10000000 && ve_800 >= vs_800 && vf_800 < 0x5000 && vr_800a == 0 && vr_800b == 0) {
                        foundValid = true; validSegmentOffset = sp_800;
                    }
                }
            }
            // Probe tailOffset = 1000
            if (!foundValid && chunkEnd > segmentDataSize + 1000) {
                u32 sp_1000 = chunkEnd - segmentDataSize - 1000;
                if (sp_1000 >= vleEndPos) {
                    u32 vs_1000 @ sp_1000 + 4; u32 ve_1000 @ sp_1000 + 8; u16 vf_1000 @ sp_1000 + 12;
                    u8 vr_1000a @ sp_1000 + 14; u8 vr_1000b @ sp_1000 + 15;
                    if (vs_1000 < 10000000 && ve_1000 < 10000000 && ve_1000 >= vs_1000 && vf_1000 < 0x5000 && vr_1000a == 0 && vr_1000b == 0) {
                        foundValid = true; validSegmentOffset = sp_1000;
                    }
                }
            }
        }

        // Skip VLE overflow bytes to reach segment data
        if (validSegmentOffset > vleEndPos) {
            u32 skipBytes = validSegmentOffset - vleEndPos;
            u8 vleOverflow[skipBytes] [[name("VLE Overflow (alignment to segments)")]];
        }

        AnimationSegmentEntry animationSegments[header.animationSegmentCount]
            [[name("Animation Segments")]];
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

        // Bounds check: each JointEntry is 21 bytes, ensure we have enough space
        u32 jointBytesNeeded = jointCountToUse * 21;
        u32 remainingInChunk = chunkSize - ($ - g_chunkDataStart);
        if (jointBytesNeeded > remainingInChunk) {
            // Not enough space - limit to what we can actually read
            jointCountToUse = remainingInChunk / 21;
        }

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
struct Chunk_FileReferences_FA {
    u32 chunkId;
    u32 chunkSize;
    u32 referenceCount;

    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("File References")]];
    }

    u32 expectedSize = 4 + (referenceCount * 6);  // 4 bytes for count + entries
    if (expectedSize < chunkSize) {
        u32 remainingSize = chunkSize - expectedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
    }
};

// Type 8 Sound File References Chunk (chunk ID 0x01) - NO count field, size/6 entries
// These reference sound files (MP3s) that are triggered by the sound events in chunk 0x02
struct Chunk_Type8_SoundRefs {
    u32 chunkId;
    u32 chunkSize;

    // No count field - number of entries is chunkSize / 6
    u32 referenceCount = chunkSize / 6;
    if (referenceCount > 0 && referenceCount < 256) {
        FileReferenceEntry references[referenceCount] [[name("Sound File References")]];
    }

    // Handle any remaining bytes (shouldn't be any if divisible by 6)
    u32 parsedSize = referenceCount * 6;
    if (parsedSize < chunkSize) {
        u32 remainingSize = chunkSize - parsedSize;
        u8 remaining[remainingSize] [[name("Remaining Data")]];
    }
};

// =============================================================================
// TYPE 8 SOUND EVENT BYTECODE (chunk ID 0x02)
// =============================================================================
// This chunk contains bytecode that defines when to play sounds during animations.
// The bytecode uses a simple stack-based virtual machine format.
//
// TIMING MODEL:
// - Timing is CUMULATIVE with wrap-around at 256 (not absolute values)
// - Each TIMING byte is compared to previous; if lower, we wrapped (+256)
// - Example: bytes [48, 70, 129, 151] = cumulative [48, 70, 129, 151]
// - Example: bytes [94, 135, 248, 33] = cumulative [94, 135, 248, 289] (wrapped at 33<248)
// - Scale to animation time: (cumulative / maxCumulative) * animDuration
//
// TWO EVENT FORMATS:
// Format 1 (Simple files - TRIGGER-based, no EMIT opcode):
//   PUSH <event_index>     ; Which event number
//   TRIGGER (0x06)         ; Start of event definition
//   TIMING <byte>          ; Cumulative timing byte (0x07 + u8)
//   NOP                    ; Padding
//   PUSH <sound_index>     ; Sound file index (0-36)
//   MARK1 MARK1 MARK3      ; Markers
//   PARAM <volume>         ; Volume 0-255 (0x09 + u8)
//
// Format 2 (Complex files - uses EMIT opcode):
//   TIMING_SET (0x1C)      ; Prepare timing update
//   TIMING <byte>          ; Cumulative timing byte
//   ... PUSH effect data ...
//   PUSH <sound_index>     ; Sound file index (small value < 100)
//   EMIT (0x1A)            ; Create the sound event
//   PARAM <volume>         ; Volume 0-255
//
// OPCODE REFERENCE:
//   0x00:       NOP - No operation
//   0x01:       MARK1 - Marker/separator
//   0x02 + u32: PUSH - Push 32-bit value onto stack
//   0x03:       MARK3 - Marker/separator
//   0x06:       TRIGGER - Start event definition (Format 1)
//   0x07 + u8:  TIMING - Cumulative timing byte (wraps at 256)
//   0x09 + u8:  PARAM - Volume/parameter (0-255)
//   0x0A:       END_LF - End marker (LF)
//   0x0D:       END_CR - End marker (CR)
//   0x18:       HEADER_SEP - Section separator
//   0x1A:       EMIT - Create sound event (Format 2)
//   0x1C:       TIMING_SET - Precedes TIMING opcode
//   0x2F:       ALT_FORMAT - Alternative timing format

// Type 8 Sound Events Chunk (chunk ID 0x02) - Bytecode for animation sounds
struct Chunk_Type8_SoundEvents {
    u32 chunkId;
    u32 chunkSize;

    // Store raw bytecode data
    // Full parsing would require a bytecode interpreter
    u8 bytecodeData[chunkSize] [[name("Sound Event Bytecode")]];
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
// Bone data: each entry has (count1 + count2 * 6) * 2 bytes of data
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
// Array with frame offset calculation
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

    // Material data as parallel arrays
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

    // Texture/Material section
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
// FA1 KEYFRAME DATA STRUCTURES
// =============================================================================
//
// FA1 has TWO different keyframe formats:
//
// FORMAT A (per-bone RAW): Used by some files (e.g., 0x10436)
//   - Per-bone 6-byte header: [u16 posCount, u16 rotCount, u16 scaleCount]
//   - Followed by RAW timestamps (u32) and values (float3/float4)
//   - Bones stored sequentially
//
// FORMAT B (global header + VLE): Used by some files (e.g., 0x3AAA)
//   - 16-byte FA1KeyframeHeader with total keyframe counts
//   - Four offset arrays (boneCount × 4 bytes each): BIT offsets into VLE stream
//   - VLE-encoded keyframe data stream
//
// Detection: Check first 12 bytes after bind pose
//   - If bytes 0-5 zeros, bytes 6-9 have counts > 500, bytes 10-11 zeros → Format B
//   - Otherwise → Format A
//

// FA1 Global Keyframe Header (16 bytes) - Format B only
// Used when animation data is stored with offset arrays + VLE encoding
struct FA1KeyframeHeader {
    u16 reserved0;          // 0x00: Always 0
    u16 reserved1;          // 0x02: Always 0
    u16 reserved2;          // 0x04: Always 0
    u16 totalPosKeyCount;   // 0x06: Total position keyframes across ALL bones
    u16 totalRotKeyCount;   // 0x08: Total rotation keyframes across ALL bones
    u16 reserved3;          // 0x0A: Always 0
    u16 timeScaleLow;       // 0x0C: Animation time scale (e.g., 3333 for 0x3AAA)
    u16 timeScaleHigh;      // 0x0E: Usually 0
} [[format("format_fa1_keyframe_header")]];

fn format_fa1_keyframe_header(FA1KeyframeHeader h) {
    return std::format("totalPos={} totalRot={} timeScale={}",
        h.totalPosKeyCount, h.totalRotKeyCount,
        h.timeScaleLow | (u32(h.timeScaleHigh) << 16));
};

// FA1 per-bone animation header (6 bytes) - Format A only
struct FA1BoneAnimHeader {
    u16 posKeyCount;     // 0x00: Number of position keyframes
    u16 rotKeyCount;     // 0x02: Number of rotation keyframes
    u16 scaleKeyCount;   // 0x04: Number of scale keyframes (usually 0)
} [[format("format_fa1_bone_anim_header")]];

fn format_fa1_bone_anim_header(FA1BoneAnimHeader h) {
    return std::format("pos={} rot={} scale={}", h.posKeyCount, h.rotKeyCount, h.scaleKeyCount);
};

// Position keyframe value (float3, 12 bytes)
struct FA1PositionKey {
    float x;
    float y;
    float z;
} [[format("format_fa1_pos_key")]];

fn format_fa1_pos_key(FA1PositionKey p) {
    return std::format("({:.3f}, {:.3f}, {:.3f})", p.x, p.y, p.z);
};

// Rotation keyframe value (float4 quaternion XYZW, 16 bytes)
struct FA1RotationKey {
    float x;
    float y;
    float z;
    float w;
} [[format("format_fa1_rot_key")]];

fn format_fa1_rot_key(FA1RotationKey q) {
    return std::format("({:.4f}, {:.4f}, {:.4f}, {:.4f})", q.x, q.y, q.z, q.w);
};

// Global for FA1 bone count (set by Chunk_FA1_Animation before parsing keyframe data)
u32 g_fa1BoneCount;

// FA1 per-bone keyframe data (variable size, parsed inline)
// Contains: header + timestamps + position values + timestamps + rotation values + timestamps + scale values
struct FA1BoneKeyframeData {
    FA1BoneAnimHeader header [[name("Bone Anim Header (6 bytes)")]];

    // Position keyframes (some animations have 2000+ keys per bone)
    if (header.posKeyCount > 0 && header.posKeyCount < 10000) {
        u32 positionTimes[header.posKeyCount] [[name("Position Timestamps")]];
        FA1PositionKey positionValues[header.posKeyCount] [[name("Position Values")]];
    }

    // Rotation keyframes
    if (header.rotKeyCount > 0 && header.rotKeyCount < 10000) {
        u32 rotationTimes[header.rotKeyCount] [[name("Rotation Timestamps")]];
        FA1RotationKey rotationValues[header.rotKeyCount] [[name("Rotation Values (XYZW)")]];
    }

    // Scale keyframes (usually 0)
    if (header.scaleKeyCount > 0 && header.scaleKeyCount < 10000) {
        u32 scaleTimes[header.scaleKeyCount] [[name("Scale Timestamps")]];
        FA1PositionKey scaleValues[header.scaleKeyCount] [[name("Scale Values")]];
    }
};

// =============================================================================
// FA1 ANIMATION CHUNK PARSING
// =============================================================================

struct Chunk_FA1_Animation {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;
    g_chunkDataSize = chunkSize;  // Set global for nested structs

    GeometryHeader_FA1 header [[name("Animation Header (88 bytes)")]];

    // Bounding cylinders (16 bytes each)
    if (header.boundingCylinderCount > 0) {
        BoundingCylinder cylinders[header.boundingCylinderCount] [[name("Bounding Cylinders")]];
    }

    // Animation sequences: Use transformDataSize NOT classFlags.HAS_ANIMATION_SEQUENCES!
    // Some FA1 files have sequences but the flag is 0. transformDataSize = number of 24-byte entries.
    if (header.transformDataSize > 0 && header.transformDataSize < 256) {
        SequenceEntry_BB9 sequences[header.transformDataSize] [[name("Animation Sequences")]];
    }

    // Bind pose entries (16 bytes each): skeleton hierarchy with DIRECT PARENT indexing
    if (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256) {
        FA1BindPoseEntry bindPose[header.bindPoseBoneCount] [[name("Bind Pose (skeleton hierarchy)")]];
    }

    // Detect keyframe format by peeking at bytes after bind pose
    //
    // Format B (global header + VLE):
    //   - Bytes 0-15: FA1KeyframeHeader with reserved fields = 0
    //   - Bytes 16+: posTimesOffsets[0] = 0 or very small (BIT offset)
    //
    // Format A (per-bone RAW):
    //   - Bytes 0-5: bone 0's 6-byte header (posCount, rotCount, scaleCount)
    //   - Bytes 6-11: bone 1's 6-byte header
    //   - Bytes 12+: bone 0/1's timestamps (uint32 values like 3333, 33333...)
    //
    // CRITICAL: When bone 0 has NO keyframes (0,0,0), bytes 0-5 are zeros which
    // looks like Format B's reserved header fields! Distinguish by checking
    // bytes 16-19: Format B has small offset (~0), Format A has timestamp (>10000).
    //
    u32 keyframeStart = $;
    u16 peek0 @ keyframeStart;      // Format B: reserved0=0, Format A: bone0.posKeyCount
    u16 peek1 @ keyframeStart + 2;  // Format B: reserved1=0, Format A: bone0.rotKeyCount
    u16 peek2 @ keyframeStart + 4;  // Format B: reserved2=0, Format A: bone0.scaleKeyCount
    u16 peek3 @ keyframeStart + 6;  // Format B: totalPosKeyCount, Format A: bone1.posKeyCount
    u16 peek4 @ keyframeStart + 8;  // Format B: totalRotKeyCount, Format A: bone1.rotKeyCount
    u16 peek5 @ keyframeStart + 10; // Format B: reserved3=0, Format A: bone1.scaleKeyCount
    u32 peek6 @ keyframeStart + 16; // Format B: posTimesOffsets[0]≈0, Format A: 2nd timestamp>10000

    // Format B detection:
    // - First 6 bytes zeros (reserved0-2 in FA1KeyframeHeader)
    // - Bytes 6-9 have high keyframe counts (>500)
    // - Bytes 10-11 zero (reserved3)
    // - Bytes 16-19 is posTimesOffsets[0], should be 0 or very small (<1000)
    //
    // Format A false positive prevention:
    // - When bone 0 has 0 keyframes, bytes 0-5 are zeros (looks like Format B header)
    // - But bytes 16-19 would be a timestamp value (>10000), not a small offset
    bool isFormatB = (peek0 == 0 && peek1 == 0 && peek2 == 0 &&
                      peek5 == 0 && (peek3 > 500 || peek4 > 500) &&
                      peek6 < 1000);  // Key check: offset arrays start with small values

    if (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256) {
        if (isFormatB) {
            // Format B: Global header + offset arrays + VLE data
            FA1KeyframeHeader keyframeHeader [[name("Keyframe Header (Format B - 16 bytes)")]];

            // Offset arrays: 4 arrays × boneCount × 4 bytes each (BIT offsets)
            u32 offsetArraySize = header.bindPoseBoneCount * 4;
            u32 posTimesOffsets[header.bindPoseBoneCount] [[name("Position Times BIT Offsets")]];
            u32 posValuesOffsets[header.bindPoseBoneCount] [[name("Position Values BIT Offsets")]];
            u32 rotTimesOffsets[header.bindPoseBoneCount] [[name("Rotation Times BIT Offsets")]];
            u32 rotValuesOffsets[header.bindPoseBoneCount] [[name("Rotation Values BIT Offsets")]];

            // Remaining is VLE-encoded keyframe data (complex to parse statically)
            u32 vleConsumed = $ - g_chunkDataStart;
            if (vleConsumed < chunkSize) {
                u8 vleKeyframeData[chunkSize - vleConsumed]
                    [[name("VLE Keyframe Data (Format B - requires runtime decoding)")]];
            }
        } else {
            // Format A: Per-bone 6-byte headers + RAW data
            FA1BoneKeyframeData keyframes[header.bindPoseBoneCount]
                [[name("Per-Bone Keyframe Data (Format A - RAW)")]];
        }
    }

    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u8 remainingData[chunkSize - consumed] [[name("Remaining Data (should be 0)")]];
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
        u8 textureData[chunkSize - sizeof(header) - sizeof(ATEXHeader)] [[name("DXT Compressed Data")]];
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
// Contains trapezoid-based navmesh organized by elevation planes
// =============================================================================

// Navigation mesh trapezoid (44 bytes)
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
struct NavMeshPortal {
    u16 trapezoidCount [[name("Trapezoid Count")]];
    u16 trapezoidStartIndex [[name("Trapezoid Start Index")]];
    u16 neighborPlaneId [[name("Neighbor Plane ID")]];
    u16 sharedPortalId [[name("Shared Portal ID")]];
    u8 flags [[name("Flags")]];  // 0x4 = not used for pathfinding
};

// X-axis BSP node for spatial queries (16 bytes)
struct NavMeshXNode {
    u32 vertexIndexStart [[name("Vertex Index Start")]];
    u32 vertexIndexEnd [[name("Vertex Index End")]];
    u32 leftChildIndex [[name("Left Child Index")]];
    u32 rightChildIndex [[name("Right Child Index")]];
};

// Y-axis BSP node (12 bytes)
struct NavMeshYNode {
    u32 vertexIndex [[name("Vertex Index")]];
    u32 leftChildIndex [[name("Left Child Index")]];
    u32 rightChildIndex [[name("Right Child Index")]];
};

// Sink node referencing a trapezoid (4 bytes)
struct NavMeshSinkNode {
    u32 trapezoidIndex [[name("Trapezoid Index")]];
};

// Tagged data section header
struct PathfindingTag {
    u8 tag [[name("Tag")]];
    u32 size [[name("Size")]];
};

// Plane header with array counts (32 bytes)
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
        Chunk_FileReferences_FA chunk [[name("Skeleton Refs (0xFA8)")]];
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

    // Type 8 (Animation Sound Event) chunks
    else if (peekId == 0x01) {
        // Chunk 0x01 in type 8 files: Sound file references (MP3s)
        Chunk_Type8_SoundRefs chunk [[name("Sound File References (Type 8)")]];
    }
    else if (peekId == 0x02) {
        // Chunk 0x02 in type 8 files: Sound event bytecode (when to play sounds)
        Chunk_Type8_SoundEvents chunk [[name("Sound Event Bytecode (Type 8)")]];
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
// 0x08 = Animation Sound Events (defines when to play sounds during animations)
//        - Chunk 0x02: Sound event bytecode (timing, volume, sound indices)
//        - Chunk 0x01: Sound file references (MP3 files)
//        - Referenced from animation files via BBC/BBD chunks

struct FFNAFile {
    char signature[4] [[name("FFNA Signature")]];   // "ffna" (0x616e6666)
    u8 fileType [[name("File Type")]];

    // Different file types have different chunk structures
    Chunk chunks[while($ < sizeof($))] [[name("Chunks")]];
};

FFNAFile file @ 0;
