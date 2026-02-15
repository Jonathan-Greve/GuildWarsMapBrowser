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

struct FloatRange {
    float min;
    float max;
};

// Cloud definition inside FA0 emitterData block (88 bytes = 0x58)
// Confirmed via Model_ParseEmitterData + Model_build_particle_clouds.
struct ParticleCloudDef_FA0 {
    u32 cloudFlags;                 // 0x00: runtime flags (bit0=quad geometry, bit1=alt orientation mode)
    s32 emitterTypeOverride;        // 0x04: GrCloud_Create arg0 override; -1 => model fallback
    u32 materialIndex;              // 0x08: material lookup index (particle texture is model-specific)
    u32 scaleRampRef;               // 0x0C: optional external scale-ramp reference
    float particleScale;            // 0x10: base particle scale
    float reserved_0x14;            // 0x14: present in file, not consumed in model particle build path
    float createArg5;               // 0x18: GrCloud_Create arg5 -> GrParticle(+0x68)
    float motionCoeffA[3];          // 0x1C..0x24: GrCloud_Create arg3 -> GrParticle(+0x50), scaled by invMass
    float motionCoeffB[3];          // 0x28..0x30: GrCloud_Create arg4 -> GrParticle(+0x5C), additive accel term
    FloatRange particleLifetime;    // 0x34
    u32 flipbookCellsX;             // 0x3C (0 disables flipbook)
    u32 flipbookCellsY;             // 0x40
    u32 flipbookStart;              // 0x44
    u32 flipbookCount;              // 0x48
    float flipbookFps;              // 0x4C
    u32 linkedEmitterCount;         // 0x50
    u32 particleEntryCount;         // 0x54: collision/obstacle entry count (0x18 bytes each in tail)
};

// Emitter definition inside FA0 emitterData block (80 bytes = 0x50)
// Confirmed via Model_ParseEmitterData + GrCloud_AddEmitter/GrParticle_AddEmitter.
struct ParticleEmitterDef_FA0 {
    s32 emitterSourceOrMode;        // often -1/0/small mode/index values
    float localOffsetX;             // local emitter offset payload (mode=1 root emitters can be source-transform driven)
    float localOffsetY;
    float localOffsetZ;
    // Bone-group index (NOT a bone index). Resolved to a bone via bind-pose flag bits 0x1F00.
    u32 boneGroupIndex;             // 0x10: low byte used for emitter sort/grouping
    u32 shapeType;                  // 0x14: file enum (0..4), remapped by DAT_00a3722c => 0,1,2,4,3
    float emissionPeriod;           // 0x18: GrParticle_AddEmitter arg0 (runtime trace)
    FloatRange shapeRadius;         // 0x1C
    float basisOrDirection[3];      // 0x24: local basis/direction vector
    FloatRange velocityJitter;      // 0x30
    FloatRange radialVelocity;      // 0x38
    FloatRange tangentialVelocity;  // 0x40
    FloatRange particleMass;        // 0x48
};

// Per-cloud collision plane payload in FA0 particle tail.
// Runtime consumes these as obstacle records in GrParticle_CollideObstacles.
struct ParticleObstaclePlane_FA0 {
    float normalX;
    float normalY;
    float normalZ;
    float planeD;
};

enum ParticleObstacleType_FA0 : u32 {
    OBSTACLE_REFLECT = 0,           // elastic reflection
    OBSTACLE_DAMP = 1,              // damp velocity by factor
    OBSTACLE_STOP = 2,              // zero velocity
    OBSTACLE_KILL = 3               // kill particle
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

// Bounding Entry Type B (80 bytes) - BB8 weapon emitter entry
// Observed in BB8 weapon files: this 0x50-byte record matches the
// ParticleEmitterDef_FA0 layout (same offsets/size) for the fields GW consumes.
struct BoundingEntryTypeB {
    u32 emitterSourceOrMode;    // 0x00: mode/source selector (observed 0/2 in weapons)
    float localOffsetX;         // 0x04
    float localOffsetY;         // 0x08
    float localOffsetZ;         // 0x0C
    // Observed in BB8 weapon files:
    //   emitterMeta[0] = source/mode index (boneGroupIndex in FA0 terms)
    //   emitterMeta[1] = shape selector (shapeType in FA0 terms)
    u32 emitterMeta[2];         // 0x10-0x17
    float emissionPeriod;       // 0x18
    float shapeRadiusMin;       // 0x1C
    float shapeRadiusMax;       // 0x20
    float basisOrDirection[3];  // 0x24-0x2F
    FloatRange velocityJitter;  // 0x30-0x37
    FloatRange radialVelocity;  // 0x38-0x3F
    FloatRange tangentialVelocity; // 0x40-0x47
    FloatRange particleMass;       // 0x48-0x4F
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
    u8 boneInfoCount;               // 0x30: Bone info structs (28 bytes each)
    u8 legacyNodeCount;             // 0x31: legacy section A count (0x10-byte entries)
    u8 legacyNodeDataCount;         // 0x32: legacy section B count (0x54-byte entries)
    u8 padding_0x33;                // 0x33
    u32 emitterDataSize;            // 0x34: particle payload size
    u32 cloudCount;                 // 0x38: count of 0x58 cloud records
    u32 cloudEmitterCount;          // 0x3C: count of 0x50 emitter records
    u32 cloudObstacleEntryCount;    // 0x40: count of obstacle entry records
    u32 submeshCount;               // 0x44
    u32 animationCount;             // 0x48
    u16 lodCount;                   // 0x4C
    u16 reserved_0x4E;              // 0x4E
    u16 boneDataEntryCount;         // 0x50
    u16 attachmentPointCount;       // 0x52
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

// FA0 emitterData payload size, confirmed from runtime parser:
// cloudCount*0x58 + cloudEmitterCount*0x50 + cloudObstacleEntryCount*0x18
fn calc_particle_block_size(u32 cloudCount, u32 emitterCount, u32 obstacleEntryCount) {
    return cloudCount * 0x58 + emitterCount * 0x50 + obstacleEntryCount * 0x18;
};

struct ParticleDataBlock_FA0<auto cloudCount, auto emitterCount, auto obstacleEntryCount, auto payloadSize> {
    if (cloudCount > 0 && cloudCount < 0x10000) {
        ParticleCloudDef_FA0 clouds[cloudCount] [[name("Cloud Definitions (0x58)")]];
    }
    if (emitterCount > 0 && emitterCount < 0x10000) {
        ParticleEmitterDef_FA0 emitters[emitterCount] [[name("Emitter Definitions (0x50)")]];
    }
    if (obstacleEntryCount > 0 && obstacleEntryCount < 0x10000) {
        // File layout is planes + damping + type; runtime repacks each entry to [plane, type, damping].
        ParticleObstaclePlane_FA0 obstaclePlanes[obstacleEntryCount] [[name("Obstacle Planes (0x10)")]];
        float obstacleDamping[obstacleEntryCount] [[name("Obstacle Damping (float)")]];
        ParticleObstacleType_FA0 obstacleType[obstacleEntryCount] [[name("Obstacle Type")]];
    }

    u32 consumed = $ - g_chunkDataStart;
    u32 payloadStart = g_chunkDataSize - payloadSize;
    if (consumed < payloadStart + payloadSize) {
        u32 remaining = payloadStart + payloadSize - consumed;
        u8 payloadRemainder[remaining] [[name("Particle Payload Remainder")]];
    }
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
    u16 animationSegmentCount;  // 0x18: Number of 23-byte animation segment entries (FA1 has padding)
    u16 reserved_0x1A;          // 0x1A: Reserved/unused
    u32 unknown_0x1C;
    float geometryScale;        // 0x20: Skeleton/geometry scale factor (GW stores at model+0x100)
                                //       If negative/zero, GW computes scale from bounding data
    u32 unknown_0x24;
    u32 unknown_0x28;
    u32 bindPoseBoneCount;      // 0x2C: Number of bones in bind pose section
    u32 unknown_0x30;
    u32 transformDataSize;      // 0x34: Number of 24-byte sequence entries (NOT gated by flags!)
    u32 submeshCount;           // 0x38
    u32 unknown_0x3C;           // 0x3C: Event/phase count in skeleton-data variants (count * 5 bytes)
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
// FA1 HIERARCHY ENCODING:
//   Two encodings are observed:
//   1) Direct-parent (common in external FA1 animation files)
//      lowByte = parent_index + 1, where 0 = root
//   2) Pop-count (seen in some embedded/model FA1 variants)
//      lowByte = levels to pop from matrix stack
//   Mode selection is deterministic from hierarchy-byte stream validity
//   (no offset scanning/search required).
//
// parentInfo field:
//   Bits 0-7 (low byte):   hierarchy payload byte (mode-dependent)
//   Bit 28 (0x10000000):   Intermediate-bone marker
//
// DIRECT PARENT ALGORITHM:
//   - For each bone:
//     lowByte = parentInfo & 0xFF
//     if (lowByte == 0) parent = -1 (this is a root bone)
//     else parent = lowByte - 1
//
// POP COUNT ALGORITHM:
//   - Maintain matrix-stack depth while iterating bones in order
//   - Pop lowByte levels, parent = stack.top(), then push current bone
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
    u32 parentInfo;             // 0x0C: Low byte = hierarchy payload, high bits = flags
} [[format("format_fa1_bind_pose")]];

fn format_fa1_bind_pose(FA1BindPoseEntry e) {
    u8 lowByte = e.parentInfo & 0xFF;
    s32 directParentIdx = (lowByte == 0) ? -1 : s32(lowByte - 1);
    bool hasFlag = (e.parentInfo & 0x10000000) != 0;
    if (hasFlag) {
        return std::format("pos=({:.2f},{:.2f},{:.2f}) low={} directParent={} [FLAG]",
            e.posX, e.posY, e.posZ, lowByte, directParentIdx);
    }
    return std::format("pos=({:.2f},{:.2f},{:.2f}) low={} directParent={}",
        e.posX, e.posY, e.posZ, lowByte, directParentIdx);
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
    // In BB8 weapon files without usable bind/base positions, bound* can carry
    // source-indexed emitter anchor vectors (often the negated localOffset vector).
    float boundX;
    float boundY;
    float boundZ;
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
        u32 consumedInChunk = $ - g_chunkDataStart;
        u32 remainingInChunk = (consumedInChunk <= g_chunkDataSize) ? (g_chunkDataSize - consumedInChunk) : 0;
        if (embeddedAnimTotalSize > 0 && embeddedAnimTotalSize < 0x1000000 &&
            embeddedAnimTotalSize <= remainingInChunk) {
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
//   2. hierarchyByte is interpreted deterministically from stream validity:
//      - DIRECT_PARENT mode: value = parent_index + 1, 0 = root
//      - POP_COUNT mode: levels to pop from internal matrix stack before pushing
//      - If both are valid, choose the less-fragmented hierarchy (fewer roots).
//      - TREE_DEPTH remains a fallback for malformed legacy streams.
//   3. Runtime applies (basePos + animDelta) then subtracts basePos, leaving only animDelta
//
// HIERARCHY ENCODING DETECTION:
//   DIRECT_PARENT: Values like [0,1,2,3,4,5,2,7,...] (parent = value - 1)
//   POP_COUNT:     Values like [0,0,0,0,3,0,0] (stack pop counts)
//   AMBIGUOUS:     If both pass, prefer the mode with fewer roots and avoid root-heavy trees.
//   TREE_DEPTH:    Values like [0,1,2,3,2,3,1,2] (absolute depth levels, legacy fallback)
//   WORLD_SPACE mode: All zeros - each bone has independent absolute transforms (no hierarchy)
//
// PARENT COMPUTATION ALGORITHMS:
//   DIRECT_PARENT: parent = (value == 0) ? -1 : value - 1
//   POP_COUNT:     Maintain stack of bone indices, pop N entries, parent = stack.top(), push current
//   TREE_DEPTH:    Track bones at each depth level, parent = most recent bone at (depth-1)
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
//   Bits 0-7 (hierarchyByte):  Direct-parent OR pop-count OR tree-depth payload
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
    u8 hierarchyByte;       // 0x0C: Hierarchy payload byte (mode-dependent)
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
        return std::format("pos=({:.1f},{:.1f},{:.1f}) hier={} keys=P{}R{}S{} [INTERMEDIATE]",
            h.basePositionX, h.basePositionY, h.basePositionZ,
            h.hierarchyByte, h.posKeyCount, h.rotKeyCount, h.scaleKeyCount);
    }
    return std::format("pos=({:.1f},{:.1f},{:.1f}) hier={} keys=P{}R{}S{}",
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

// BB9 Animation Segment Entry (22 bytes per entry)
// Engine runtime fields:
//   hash, startTime, endTime, phaseStartIndex, phaseEndIndex, loopStartOffset, transitionParam
struct AnimationSegmentEntry {
    u32 hash [[name("Segment Hash")]];                // 0x00: Animation segment identifier
    u32 startTime [[name("Start Time")]];             // 0x04: Segment start in animation units (100000 = 1 sec)
    u32 endTime [[name("End Time")]];                 // 0x08: Segment end in animation units
    u8 phaseStartIndex [[name("Phase Start Index")]]; // 0x0C
    u8 phaseEndIndex [[name("Phase End Index")]];     // 0x0D (exclusive)
    u32 loopStartOffset [[name("Loop Start Offset")]];// 0x0E
    float transitionParam [[name("Transition Param")]];// 0x12
};  // 22 bytes total (0x16)

// FA1 Animation Segment Entry (23 bytes per entry)
// Ghidra (Model_ParseGeometryChunk0xFA1):
// - segmentType is copied as a runtime selector field.
// - 0 = local clip segment (uses keyframes in this FA1 chunk)
// - >0 = external segment source (typically references from FA8 chunk, 1-based)
//   so FA1 files can dispatch many segment definitions across chained clips.
struct FA1AnimationSegmentEntry {
    u8 segmentType [[name("Segment Type (0=local, >0=external source)")]]; // 0x00
    u32 hash [[name("Segment Hash")]];                // 0x01: Animation segment identifier
    u32 startTime [[name("Start Time")]];             // 0x05: Segment start in animation units
    u32 endTime [[name("End Time")]];                 // 0x09: Segment end in animation units
    u8 phaseStartIndex [[name("Phase Start Index")]]; // 0x0D
    u8 phaseEndIndex [[name("Phase End Index")]];     // 0x0E (exclusive)
    u32 loopStartOffset [[name("Loop Start Offset")]];// 0x0F
    float transitionParam [[name("Transition Param")]];// 0x13
};  // 23 bytes total (0x17)

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
// Keep post-joint bytes as raw tail until this section is fully reversed.
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

    // Phase timing data (flag 0x0100) - appears between bone transforms and segments
    // Structure: count (u32) + timeValues (u32[count]) + phaseFlags (u8[count])
    // The time values appear to be phase boundaries or transition points
    // The phase flags indicate phase types or transition modes
    if ((header.flags & 0x0100) != 0) {
        u32 phaseTimingCount [[name("Phase Timing Count")]];
        if (phaseTimingCount > 0 && phaseTimingCount < 256) {
            u32 phaseTimeValues[phaseTimingCount] [[name("Phase Time Values")]];
            u8 phaseFlags[phaseTimingCount] [[name("Phase Flags")]];
        }
    }

    // ==========================================================================
    // REMAINING ANIMATION DATA (after bone data and phase timing)
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

    // Animation segment entries (defines loop regions and sub-animations)
    // With phase timing data properly parsed, segments should follow directly
    if (header.animationSegmentCount > 0 && header.animationSegmentCount < 500) {
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
    // Deterministic parse only: consume count-derived entries from header.
    // Any unresolved tail bytes are left raw (no searching/scanning heuristics).
    if ((header.flags & 0x0200) != 0) {
        JointHeader jointHeader [[name("Joint Header")]];

        u32 jointCountToUse = jointHeader.jointCount;
        if (jointCountToUse > 256) jointCountToUse = 256;

        u32 remainingInChunk = chunkSize - ($ - g_chunkDataStart);
        u32 maxJointCountBySize = remainingInChunk / 21;
        if (jointCountToUse > maxJointCountBySize) {
            jointCountToUse = maxJointCountBySize;
        }

        if (jointCountToUse > 0) {
            JointEntry jointEntries[jointCountToUse]
                [[name("Joint Entries (count-derived deterministic parse)")]];
        }

        // Preserve unresolved post-joint bytes deterministically as raw data.
        u32 remainingAfterJoints = chunkSize - ($ - g_chunkDataStart);
        if (remainingAfterJoints > 0) {
            u8 jointConstraintOverflow[remainingAfterJoints]
                [[name("Post-Joint Tail (unresolved, no scan)")]];
        }
    }

    // Sections controlled by 0x0020/0x0080 remain unresolved in this pattern revision.
    // Keep deterministic parsing by treating remaining bytes as opaque tail data.
    if (($ - g_chunkDataStart) < chunkSize &&
        (((header.flags & 0x0020) != 0) || ((header.flags & 0x0080) != 0))) {
        u32 unresolvedTail = chunkSize - ($ - g_chunkDataStart);
        u8 unresolvedAnimTail[unresolvedTail]
            [[name("Unresolved Anim Tail (flags 0x20/0x80, no scan)")]];
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
    u32 firstDword @ $;

    // Runtime files are mixed:
    // - Variant A: [u32 count] + count * 6-byte entries
    // - Variant B: raw 6-byte entries only (no count prefix)
    if (firstDword > 0 && firstDword < 0x10000 && (4 + firstDword * 6) <= chunkSize) {
        u32 referenceCount;
        if (referenceCount > 0 && referenceCount < 0x10000) {
            FileReferenceEntry references[referenceCount] [[name("File References")]];
        }

        u32 expectedSize = 4 + (referenceCount * 6);
        if (expectedSize < chunkSize) {
            u32 remainingSize = chunkSize - expectedSize;
            u8 remaining[remainingSize] [[name("Remaining Data")]];
        }
    } else {
        u32 referenceCountRaw = chunkSize / 6;
        if (referenceCountRaw > 0 && referenceCountRaw < 0x10000) {
            FileReferenceEntry referencesRaw[referenceCountRaw]
                [[name("File References (No Count Prefix)")]];
        }

        u32 parsedSizeRaw = referenceCountRaw * 6;
        if (parsedSizeRaw < chunkSize) {
            u32 remainingSizeRaw = chunkSize - parsedSizeRaw;
            u8 remainingRaw[remainingSizeRaw] [[name("Remaining Data")]];
        }
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
    g_chunkDataSize = chunkSize;

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

    // Parse the main body conservatively up to emitter payload start.
    // This keeps parsing deterministic even when submesh internals vary across revisions.
    u32 emitterPayloadOffset = (header.emitterDataSize <= chunkSize) ? (chunkSize - header.emitterDataSize) : chunkSize;
    u32 consumedBeforeEmitter = $ - g_chunkDataStart;
    if (consumedBeforeEmitter < emitterPayloadOffset) {
        u8 geometryBody[emitterPayloadOffset - consumedBeforeEmitter]
            [[name("Geometry Body (Submeshes/LOD/Anim/Legacy Sections)")]];
    }

    // Emitter/particle payload (tail block at end of FA0 chunk)
    if (header.emitterDataSize > 0 && header.emitterDataSize <= chunkSize) {
        u32 expectedParticleBytes =
            calc_particle_block_size(header.cloudCount, header.cloudEmitterCount, header.cloudObstacleEntryCount);

        if (header.cloudCount < 0x10000 &&
            header.cloudEmitterCount < 0x10000 &&
            header.cloudObstacleEntryCount < 0x10000 &&
            expectedParticleBytes <= header.emitterDataSize) {
            ParticleDataBlock_FA0<header.cloudCount, header.cloudEmitterCount,
                                  header.cloudObstacleEntryCount, header.emitterDataSize>
                emitterData [[name("Emitter/Particle Data")]];
        } else {
            u8 emitterDataRaw[header.emitterDataSize] [[name("Emitter/Particle Data (Raw)")]];
        }
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
// Ghidra evidence (Model_ParseGeometryChunk0xFA1 @ 0x0076a9c0):
// - FA1 keyframes are parsed as PER-BONE RAW data (no format switch).
// - After keyframes, the parser advances deterministically:
//   1) section 0x38 callback array: submeshCount entries, each 12 + (dword@+4 * 4) bytes
//   2) phase timing block: (header.unknown_0x3C & 0xFFFF) * 5 bytes
//   3) segment table: animationSegmentCount * 23 bytes
//

// FA1 per-bone animation header (6 bytes)
struct FA1BoneAnimHeader {
    u16 posKeyCount;            // 0x00: Position key count
    u16 rotKeyCount;            // 0x02: Rotation key count
    u16 scaleKeyCount;          // 0x04: Scale key count
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

// FA1 per-bone keyframe data (variable size):
// [6-byte header]
// [pos times u32[posKeyCount]]
// [pos values float3[posKeyCount]]
// [rot times u32[rotKeyCount]]
// [rot values float4[rotKeyCount]]
// [scale times u32[scaleKeyCount]]
// [scale values float3[scaleKeyCount]]
struct FA1BoneKeyframeData {
    FA1BoneAnimHeader header [[name("Bone Anim Header (6 bytes)")]];

    if (header.posKeyCount > 0 && header.posKeyCount < 10000) {
        u32 positionTimes[header.posKeyCount] [[name("Position Timestamps")]];
        FA1PositionKey positionValues[header.posKeyCount] [[name("Position Values")]];
    }

    if (header.rotKeyCount > 0 && header.rotKeyCount < 10000) {
        u32 rotationTimes[header.rotKeyCount] [[name("Rotation Timestamps")]];
        FA1RotationKey rotationValues[header.rotKeyCount] [[name("Rotation Values (XYZW)")]];
    }

    if (header.scaleKeyCount > 0 && header.scaleKeyCount < 10000) {
        u32 scaleTimes[header.scaleKeyCount] [[name("Scale Timestamps")]];
        FA1PositionKey scaleValues[header.scaleKeyCount] [[name("Scale Values")]];
    }
};

// Engine section at header offset 0x38:
// parsed with Model_ParseArrayWithCallback(..., 0x0C, Buffer_CalcWeightOffset)
// entry size = 12 + (u32 at +4 * 4)
struct FA1Section0x38Entry {
    u32 field0 [[name("Field 0x00")]];
    u32 extraWordCount [[name("Extra Word Count")]];
    u32 field8 [[name("Field 0x08")]];
    if (extraWordCount > 0 && extraWordCount < 0x10000) {
        u32 extraWords[extraWordCount] [[name("Extra Words")]];
    }
};

// Engine section 0x3C layout:
//   u32 phaseTimeValues[count]
//   u8  phaseFlags[count]
// The parser multiplies count by 5 to advance to the segment table, but the
// bytes are stored as two contiguous arrays (not interleaved 5-byte structs).

// =============================================================================
// FA1 ANIMATION CHUNK PARSING
// =============================================================================

struct Chunk_FA1_Animation {
    u32 chunkId;
    u32 chunkSize;
    g_chunkDataStart = $;
    g_chunkDataSize = chunkSize;

    GeometryHeader_FA1 header [[name("Animation Header (88 bytes)")]];

    // Bounding cylinders (16 bytes each)
    if (header.boundingCylinderCount > 0) {
        BoundingCylinder cylinders[header.boundingCylinderCount] [[name("Bounding Cylinders")]];
    }

    // Sequence entries are controlled by transformDataSize in FA1
    if (header.transformDataSize > 0 && header.transformDataSize < 256) {
        SequenceEntry_BB9 sequences[header.transformDataSize] [[name("Animation Sequences")]];
    }

    // Bind pose entries
    if (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256) {
        FA1BindPoseEntry bindPose[header.bindPoseBoneCount] [[name("Bind Pose (skeleton hierarchy)")]];
    }

    // Per-bone RAW keyframe blocks
    if (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256) {
        FA1BoneKeyframeData keyframes[header.bindPoseBoneCount]
            [[name("Per-Bone Keyframe Data (RAW)")]];
    }

    // Deterministic section at 0x38: submeshCount callback entries.
    if (header.submeshCount > 0 && header.submeshCount < 4096) {
        FA1Section0x38Entry section38[header.submeshCount] [[name("FA1 Section 0x38 Entries")]];
    }

    // Deterministic section at 0x3C:
    // low 16 bits are entry count, encoded as u32[count] + u8[count].
    u32 phaseEntryCount = header.unknown_0x3C & 0xFFFF;
    if (phaseEntryCount > 0 && phaseEntryCount < 65536) {
        u32 phaseTimeValues[phaseEntryCount] [[name("FA1 Phase Time Values (u32 array)")]];
        u8 phaseFlags[phaseEntryCount] [[name("FA1 Phase Flags (u8 array)")]];
    }

    // Deterministic segment table (23 bytes each)
    if (header.animationSegmentCount > 0 && header.animationSegmentCount < 500) {
        FA1AnimationSegmentEntry fa1Segments[header.animationSegmentCount]
            [[name("FA1 Animation Segments")]];
        // NOTE: A high segment count is expected for dispatch clips.
        // Most entries may be external (segmentType > 0) and are not local keyframe ranges.
    }

    // Keep tail deterministic but untyped until each callback section is fully decoded.
    u32 consumed = $ - g_chunkDataStart;
    if (consumed < chunkSize) {
        u8 remainingTailData[chunkSize - consumed] [[name("Remaining Tail Data")]];
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

// 0x10000008 - Pathfinding stage-1 (bloat source)
// Compact boundary representation used by PathData bloat path.
struct PathfindingChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 chunkEnd = addressof(chunkId) + 8 + chunkSize;

    if (chunkSize >= 19) {
        u32 signature [[name("Signature")]];          // 0xEEFE704C
        u8 formatVersion [[name("Format Version")]]; // 0x0C
        u32 sequence [[name("Sequence")]];
        u8 tag7 [[name("Boundary Tag")]];             // 0x07
        u16 boundaryPointCount [[name("Boundary Point Count")]];

        if (($ + boundaryPointCount * 8 + 7) <= chunkEnd) {
            MapVertex2D boundaryPoints[boundaryPointCount] [[name("Boundary Points")]];
            u8 tag14 [[name("Tail Tag")]];            // 0x0E
            u32 boundaryHash [[name("Boundary Hash")]];
            u8 reserved0 [[name("Reserved")]];        // 0x00
            u8 terminator [[name("Terminator")]];     // 0xFF
        } else {
            u8 boundaryData[chunkEnd - $] [[name("Boundary Data (Raw)")]];
        }
    } else {
        u8 rawData[chunkSize] [[name("Raw Data")]];
    }
};

// =============================================================================
// 0x20000009 - ENVIRONMENT INFO CHUNK
// Fog, lighting, sky, water rendering settings
// =============================================================================

// Tag 0: sky mode table (entry size = 0x0A)
// Parsed by EnvData_ParseSkyTextureSettings / EnvData_ParseParams into runtime offsets:
//   +0 byte -> runtime +0x14 (normalized 0..1)
//   +1 byte -> runtime +0x18 (normalized 0..1)
//   +2 u32  -> runtime +0x1C (raw cloud alpha value used by Env_create_cloud_texture)
//   +6 byte -> runtime +0x20 (1 + byte * (15/256))
//   +7 byte -> runtime +0x24 (normalized 0..1)
//   +8 u16  -> runtime +0x28 (cloud alpha texture filename index, 0xFFFF = none)
struct EnvSkyModeParams {
    u8 layerParam0 [[name("Layer Param 0")]];
    u8 layerParam1 [[name("Layer Param 1")]];
    u32 cloudAlphaValueRaw [[name("Cloud Alpha Value (Raw)")]];
    u8 skyHazeScaleByte [[name("Sky Haze Scale Byte")]];
    u8 layerBlendByte [[name("Layer Blend Byte")]];
    u16 cloudAlphaTextureIndex [[name("Cloud Alpha Texture Index")]];
};

// Tag 1: sky color/fog blend settings (entry size = 0x06)
struct EnvSkySettings {
    u8 skyBlue [[name("Sky Blue")]];
    u8 skyGreen [[name("Sky Green")]];
    u8 skyRed [[name("Sky Red")]];
    u16 skyRangeParam [[name("Sky Range Param")]];
    u8 skyAlphaParam [[name("Sky Alpha Param")]];
};

// Fog settings
struct EnvFogSettings {
    u8 fogBlue [[name("Fog Blue")]];
    u8 fogGreen [[name("Fog Green")]];
    u8 fogRed [[name("Fog Red")]];
    u32 fogDistanceStart [[name("Fog Distance Start")]];
    u32 fogDistanceEnd [[name("Fog Distance End")]];
    s32 fogHeightStart [[name("Fog Height Start")]];
    s32 fogHeightEnd [[name("Fog Height End")]];
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

// Sky reference table (tag 4).
// Usually indexed by EnvMainSettings.skyTextureRefIndex / EnvSkyLayer.skyTextureRefIndex.
// 0xFFFF means "no tag4 cloud dome texture"; runtime may still use
// EnvSkyModeParams.cloudAlphaTextureIndex (tag0) as fallback cloud layer.
struct EnvSkyRefSettings {
    u16 cloudTextureIndex [[name("Cloud Dome Texture Index")]];
};

// Sky texture settings
struct EnvSkyTextures {
    u8 skyLayer0ScaleByte [[name("Sky Layer 0 Scale Byte")]];
    u16 skyBackgroundIndex [[name("Sky Background Index")]];
    u16 skyClouds0Index [[name("Sky Clouds 0 Index")]];
    u16 skyClouds1Index [[name("Sky Clouds 1 Index")]];
    u16 skySunIndex [[name("Sky Sun Index")]];
    u8 skyLayer1ScaleByte [[name("Sky Layer 1 Scale Byte")]];
    u16 skyScrollVelocityRaw0 [[name("Sky Scroll Velocity Raw 0 (U16)")]];
    u16 skyScrollVelocityRaw1 [[name("Sky Scroll Velocity Raw 1 (U16)")]];
    u8 sunSizeScaleByte [[name("Sun Size Scale Byte")]];
};

// Alternate sky texture format
struct EnvSkyTexturesAlt {
    u8 skyLayer0ScaleByte [[name("Sky Layer 0 Scale Byte")]];
    u16 skyBackgroundIndex [[name("Sky Background Index")]];
    u16 skyClouds0Index [[name("Sky Clouds 0 Index")]];
    u16 skyClouds1Index [[name("Sky Clouds 1 Index")]];
    u16 skySunIndex [[name("Sky Sun Index")]];
    u8 skyLayer1ScaleByte [[name("Sky Layer 1 Scale Byte")]];
    u16 skyScrollVelocityRaw0 [[name("Sky Scroll Velocity Raw 0 (U16)")]];
    u16 skyScrollVelocityRaw1 [[name("Sky Scroll Velocity Raw 1 (U16)")]];
    u8 sunSizeScaleByte [[name("Sun Size Scale Byte")]];
    u8 version16ExtraByte [[name("Version16 Extra Byte")]];
};

// Water rendering settings (per-environment water preset).
//
// Notes (confirmed via Gw.exe RE + gwgfxdump captures):
// - absorption BGRA bytes map to water shader constant "waterAbsorption" (PS c1) after conversion to float RGBA / 255.
// - pattern BGRA bytes map to water shader constant "waterPattern" (PS c0) after conversion to float RGBA / 255.
// - fresnel/specularScale influence "waterFresnel" and "waterSpecularColor" constants depending on technique.
// - texture indices refer to the EnvironmentFilenameChunk (env filenames table) for the active environment:
//   - patternTextureIndex -> stage3 base/pattern texture (often DXT1)
//   - bumpDuDvTextureIndex -> stage0 DuDv/bump texture (often V8U8) used for texbem perturbation
//
// Layout here is the *file* layout. The runtime typically expands/aligns this into a larger struct and also derives
// additional values (e.g. scroll direction from wind) before MapWater_render consumes it.
struct EnvWaterParams {
    u8 mode [[name("Mode")]];
    u8 flags [[name("Flags")]];
    u8 reserved0[3];

    float surfaceBaseHeight [[name("Surface Base Height")]]; // likely water plane height; verify per-map
    float reloadSentinel [[name("Reload Sentinel")]];        // unknown; often 0.0; sometimes non-zero (forces regen?)

    float waveAmplitude [[name("Wave Amplitude")]];
    float secondaryTexScale [[name("Secondary Tex Scale")]];
    float waveScale [[name("Wave Scale")]];
    float secondarySpeed [[name("Secondary Speed")]];
    float primaryTexScale [[name("Primary Tex Scale")]];
    float primarySpeed [[name("Primary Speed")]];
    float fresnel [[name("Fresnel")]];
    float specularScale [[name("Specular Scale")]];

    // File is stored BGRA. Runtime constant is float RGBA/255.
    u8 absorptionB [[name("Absorption B")]];
    u8 absorptionG [[name("Absorption G")]];
    u8 absorptionR [[name("Absorption R")]];
    u8 absorptionA [[name("Absorption A")]];

    // File is stored BGRA. Runtime constant is float RGBA/255.
    u8 patternB [[name("Pattern B")]];
    u8 patternG [[name("Pattern G")]];
    u8 patternR [[name("Pattern R")]];
    u8 patternA [[name("Pattern A")]];

    u16 patternTextureIndex [[name("Pattern Texture Index (Stage3)")]];
    u16 bumpDuDvTextureIndex [[name("Bump DuDv Texture Index (Stage0)")]];
};

// Wind settings
struct EnvWindSettings {
    u8 windDir0 [[name("Wind Direction 0")]];
    u8 windDir1 [[name("Wind Direction 1")]];
    u8 windSpeed0 [[name("Wind Speed 0")]];
    u8 windSpeed1 [[name("Wind Speed 1")]];
};

// Tag 8: Global environment selection (fixed-size 0x11 record)
struct EnvMainSettings {
    u16 skyParamsIndex [[name("Sky Params Index")]];
    u16 fogParamsIndex [[name("Fog Params Index")]];
    u16 cloudParamsIndex [[name("Cloud Params Index")]];
    u16 lightParamsIndex [[name("Light Params Index")]];
    u16 skyTextureRefIndex [[name("Sky Texture Ref Index")]];
    u16 skyTextureSettingsIndex [[name("Sky Texture Settings Index")]];
    u16 waterSettingsIndex [[name("Water Settings Index")]];
    u16 windSettingsIndex [[name("Wind Settings Index")]];
    u8 skyBrightnessBias [[name("Sky Brightness Bias")]];
};

// Tag 9: Optional sky layers (size = 0x20 per entry)
// Runtime semantics (EnvData_ParseParams):
// - index value 0 means "inherit from global/main env" for that field
// - non-zero value means "override with indexed table entry"
// The override mask is stored in the runtime object and consumed by Env_update_frame.
struct EnvSkyLayer {
    u16 skyParamsIndex [[name("Sky Params Index")]];
    u16 fogParamsIndex [[name("Fog Params Index")]];
    u16 cloudParamsIndex [[name("Cloud Params Index")]];
    u16 lightParamsIndex [[name("Light Params Index")]];
    u16 skyTextureRefIndex [[name("Sky Texture Ref Index")]];
    u16 skyTextureSettingsIndex [[name("Sky Texture Settings Index")]];
    u16 waterSettingsIndex [[name("Water Settings Index")]];
    u16 windSettingsIndex [[name("Wind Settings Index")]];
    s32 offsetX [[name("Offset X")]];
    s32 offsetY [[name("Offset Y")]];
    // Used by Map_check_point_in_region as squared-distance thresholds:
    // - circular regions: distSq <= heightSquared, fade starts at widthSquared
    // - polygon regions: edge fade weight uses minDistSq / widthSquared
    u32 widthSquared [[name("Width Squared")]];
    u32 heightSquared [[name("Height Squared")]];
};

// Tag 0x0B: environment tag records (5 bytes each)
// Runtime: (tagId -> envIndex) hash map used by Env_fade_to_tagged.
// flags bit0 marks the tagged entry as enabled/valid.
struct EnvTagEntry {
    u16 tagId [[name("Tag ID")]];
    u16 envIndex [[name("Environment Index")]];
    u8 flags [[name("Flags")]];
};

// Tag 0x0C: environment polygon records (variable size)
struct EnvPolygonEntry {
    u8 vertexCount [[name("Vertex Count")]];
    u16 envIndex [[name("Environment Index")]];
    MapVertex2D vertices[vertexCount] [[name("Vertices")]];
};

struct EnvironmentChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 chunkEnd = addressof(chunkId) + 8 + chunkSize;

    u32 signature [[name("Signature")]];  // 0x92991030
    u16 version0 [[name("Version 0")]];
    u16 version1 [[name("Version 1")]];

    u8 tag0;
    u16 lightingModeCount [[name("Lighting Mode Count")]];
    EnvSkyModeParams skyModeParams[lightingModeCount] [[name("Sky Mode Params")]];

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
    u16 skyRefSettingsCount [[name("Sky Ref Settings Count")]];
    EnvSkyRefSettings skyRefSettings[skyRefSettingsCount] [[name("Sky Ref Settings")]];

    u8 tag5;
    u16 skyTextureCount [[name("Sky Texture Count")]];
    if (version1 > 0) {
        EnvSkyTexturesAlt skyTextures[skyTextureCount] [[name("Sky Textures")]];
    } else {
        EnvSkyTextures skyTextures[skyTextureCount] [[name("Sky Textures")]];
    }

    u8 tag6;
    u16 waterSettingsCount [[name("Water Settings Count")]];
    EnvWaterParams waterSettings[waterSettingsCount] [[name("Water Settings")]];

    u8 tag7;
    u16 windSettingsCount [[name("Wind Settings Count")]];
    EnvWindSettings windSettings[windSettingsCount] [[name("Wind Settings")]];

    // Tag 8 has no count, always a single fixed-size record.
    u8 tag8;
    EnvMainSettings mainSettings [[name("Main Settings")]];

    // Tag 9 uses standard count + fixed-size entries.
    u8 tag9;
    u16 skyLayerCount [[name("Sky Layer Count")]];
    EnvSkyLayer skyLayers[skyLayerCount] [[name("Sky Layers")]];

    // Optional tag 0x0B
    if (($ < chunkEnd) && (std::mem::read_unsigned($, 1) == 0x0B)) {
        u8 tag11 [[name("Tag 0x0B")]];
        u16 tag11Count [[name("Tag 0x0B Count")]];
        EnvTagEntry tag11Entries[tag11Count] [[name("Environment Tags")]];
    }

    // Optional tag 0x0C
    if (($ < chunkEnd) && (std::mem::read_unsigned($, 1) == 0x0C)) {
        u8 tag12 [[name("Tag 0x0C")]];
        u16 tag12Count [[name("Tag 0x0C Count")]];
        EnvPolygonEntry tag12Entries[tag12Count] [[name("Environment Polygons")]];
    }

    // Optional terminator
    if (($ < chunkEnd) && (std::mem::read_unsigned($, 1) == 0xFF)) {
        u8 terminator [[name("Terminator (0xFF)")]];
    }

    // Any unmodeled bytes left in chunk
    if ($ < chunkEnd) {
        u8 trailingData[chunkEnd - $] [[name("Trailing Data")]];
    }
};

// =============================================================================
// 0x2000000A/E/F - UNKNOWN CHUNKS
// =============================================================================

struct UnknownChunk0A {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 chunkEnd = addressof(chunkId) + 8 + chunkSize;

    // Observed:
    // - stage-2 (0x2000000A): 0x090923F8, 0x00000000, 0xFF, 0x00000000
    // - stage-1 (0x1000000A): 0x090923F8, 0x00000000, 0xFF
    if (chunkSize >= 8) {
        u32 signature [[name("Signature")]];          // 0x090923F8
        u32 version [[name("Version")]];              // observed 0
        if ($ < chunkEnd) {
            u8 terminator [[name("Terminator")]];     // 0xFF
        }
        if (($ + 4) <= chunkEnd) {
            u32 trailingZero [[name("Trailing Zero")]];
        }
        if ($ < chunkEnd) {
            u8 trailingData[chunkEnd - $] [[name("Trailing Data")]];
        }
    } else {
        u8 data[chunkSize] [[name("Data")]];
    }
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
    u32 chunkEnd = addressof(chunkId) + 8 + chunkSize;

    char signature[4] [[name("Signature")]];  // "ROHS" ("SHOR")
    u32 formatVersion [[name("Format Version")]];  // observed 3
    u32 segmentCount [[name("Segment Count")]];
    ShoreSegment segments[segmentCount] [[name("Shore Segments")]];

    if (($ + 5) <= chunkEnd) {
        u8 terminator [[name("Terminator")]];      // 0xFF
        u32 trailingZero [[name("Trailing Zero")]]; // 0
    }

    if ($ < chunkEnd) {
        u8 trailingData[chunkEnd - $] [[name("Trailing Data")]];
    }
};

// =============================================================================
// 0x20000011/12/13/14 - ADDITIONAL CHUNKS
// =============================================================================

struct EnvironmentExtraChunk {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u32 chunkEnd = addressof(chunkId) + 8 + chunkSize;

    // Signature/version are stable in both stage-2 and stage-1 variants.
    char signature[4] [[name("Signature")]];  // "THGS"
    u32 version [[name("Version")]];          // observed 2

    // stage-1 (0x10000011): chunkSize = 9 -> just terminator after header
    // stage-2 (0x20000011): payload + terminator + trailing zero u32
    if (($ < chunkEnd) && (chunkSize == 9)) {
        u8 terminator [[name("Terminator")]]; // 0xFF
    } else if (($ < chunkEnd) && (chunkSize >= 13)) {
        if (chunkSize > 13) {
            u8 data[chunkSize - 13] [[name("Data")]];
        }
        u8 terminator [[name("Terminator")]]; // 0xFF
        u32 trailingZero [[name("Trailing Zero")]]; // 0
    }

    if ($ < chunkEnd) {
        u8 trailingData[chunkEnd - $] [[name("Trailing Data")]];
    }
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
// 0x10xxxxxx - STAGE-1/BLOAT CHUNKS
// These are compact pre-bloat map payloads used by MapData_BloatAndImport*.
// =============================================================================

struct MapHeaderChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    u8 data[chunkSize] [[name("Stub Data")]];
};

struct TerrainChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    if (chunkSize >= 8) {
        u32 signature [[name("Signature")]];          // 0x87821134
        u16 version [[name("Version")]];              // observed 0x0011
        u16 stripFormat [[name("Strip Format")]];     // observed mostly 0x6088
        if (chunkSize > 8) {
            u8 bloatPayload[chunkSize - 8] [[name("Bloat Payload")]];
        }
    } else {
        u8 data[chunkSize] [[name("Stub Data")]];
    }
};

struct ZoneChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    if (chunkSize >= 18) {
        u32 signature [[name("Signature")]];           // 0x59220320
        u16 version [[name("Version")]];               // observed 0x010A
        u16 stripDataSize [[name("Strip Data Size")]];
        u16 reserved0 [[name("Reserved 0")]];          // observed 0
        u16 zonePathCount [[name("Zone Path Count")]]; // matches embedded UTF-16 zone INI refs
        u16 reserved1 [[name("Reserved 1")]];          // observed 0
        u16 flags [[name("Flags")]];                   // observed 0x0001 / 0x0102
        u16 reserved2 [[name("Reserved 2")]];          // observed 0
        if (chunkSize > 18) {
            u8 bloatPayload[chunkSize - 18] [[name("Bloat Payload")]];
        }
    } else {
        u8 data[chunkSize] [[name("Stub Data")]];
    }
};

struct PropInfoChunkStub {
    u32 chunkId [[name("Chunk ID")]];
    u32 chunkSize [[name("Chunk Size")]];
    if (chunkSize >= 10) {
        u32 signature [[name("Signature")]];              // 0x39583392
        u16 version [[name("Version")]];                  // observed 0x0011
        u32 stripSectionSize [[name("Strip Section Size")]];
        if (chunkSize > 10) {
            u8 bloatPayload[chunkSize - 10] [[name("Bloat Payload")]];
        }
    } else {
        u8 data[chunkSize] [[name("Stub Data")]];
    }
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
        EnvironmentChunk environmentStage1 [[name("Environment (Stage-1/Bloat)")]];
    }
    else if (peekId == 0x1000000A) {
        UnknownChunk0A unknown0AStub [[name("Unknown 0A (Stage-1)")]];
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
        ShoreChunk shoreStage1 [[name("Shore (Stage-1/Bloat)")]];
    }
    else if (peekId == 0x10000011) {
        EnvironmentExtraChunk envExtraStage1 [[name("Environment Extra (Stage-1)")]];
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

    // Different file types have different chunk structures.
    // Require a complete chunk header and a declared payload that fits in remaining bytes.
    Chunk chunks[while(
        $ + 8 <= sizeof($) &&
        std::mem::read_unsigned($ + 4, 4) <= (sizeof($) - $ - 8)
    )] [[name("Chunks")]];
};

FFNAFile file @ 0;
