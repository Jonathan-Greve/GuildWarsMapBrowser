#pragma once

#include "VLEDecoder.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Skeleton.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <optional>
#include <DirectXMath.h>

using namespace DirectX;

namespace GW::Parsers {

// BB9/FA1 chunk IDs
constexpr uint32_t CHUNK_ID_BB9 = 0x00000BB9;  // Animation data (type 2 "other" format)
constexpr uint32_t CHUNK_ID_FA1 = 0x00000FA1;  // Animation data (type 2 "standard" format)

// BB9 Header flags (different from FA1 ClassFlags!)
constexpr uint32_t BB9_FLAG_HAS_SEQUENCES = 0x0008;         // Animation sequences present
constexpr uint32_t BB9_FLAG_HAS_BONE_TRANSFORMS = 0x0010;   // Bone transform data present

// FA1 ClassFlags (different from BB9 flags!)
constexpr uint32_t FA1_FLAG_HAS_SKELETON = 0x0001;          // Has skeleton reference
constexpr uint32_t FA1_FLAG_HAS_BONE_GROUPS = 0x0002;       // Bone group data
constexpr uint32_t FA1_FLAG_HAS_ATTACHMENT_DATA = 0x0008;   // Attachment points (NOT sequences!)
constexpr uint32_t FA1_FLAG_HAS_LOD_DATA = 0x0010;          // LOD data (NOT bone transforms!)
constexpr uint32_t FA1_FLAG_HAS_ANIMATION_SEQUENCES = 0x0100; // Animation sequences
constexpr uint32_t FA1_FLAG_HAS_SKELETON_DATA = 0x0200;     // Skeleton/joint hierarchy

/**
 * @brief BB9 animation chunk header structure (44 bytes).
 *
 * This is the main header for BB9 (0xBB9) animation chunks.
 */
#pragma pack(push, 1)
struct BB9Header
{
    uint32_t typeMarker;              // 0x00: Type/version marker
    uint32_t fileId;                  // 0x04: File identifier
    uint32_t flags;                   // 0x08: Flags controlling what data sections exist
    uint32_t modelHash0;              // 0x0C: Model signature part 1
    uint32_t modelHash1;              // 0x10: Model signature part 2
    uint32_t boundingCylinderCount;   // 0x14: Number of bounding cylinders
    uint32_t reserved[5];             // 0x18-0x2B: Reserved/unknown fields (5 x 4 = 20 bytes)
    // Total: 6 fields (24) + reserved (20) = 44 (0x2C) bytes

    bool HasSequences() const { return (flags & BB9_FLAG_HAS_SEQUENCES) != 0; }
    bool HasBoneTransforms() const { return (flags & BB9_FLAG_HAS_BONE_TRANSFORMS) != 0; }
};
#pragma pack(pop)

static_assert(sizeof(BB9Header) == 44, "BB9Header must be 44 bytes!");

/**
 * @brief FA1 animation chunk header structure (88 bytes).
 *
 * This is the main header for FA1 (0xFA1) animation chunks.
 * IMPORTANT: FA1 uses ClassFlags which have DIFFERENT meanings than BB9 flags!
 */
#pragma pack(push, 1)
struct FA1Header
{
    uint32_t modelVersion;            // 0x00: Usually 0x26 (38)
    uint32_t fileId;                  // 0x04: File identifier
    uint32_t classFlags;              // 0x08: ClassFlags bitfield (different from BB9!)
    uint32_t boundingBoxId;           // 0x0C: Bounding box reference
    uint32_t collisionMeshId;         // 0x10: Collision mesh reference
    uint32_t boundingCylinderCount;   // 0x14: Number of bounding cylinders
    uint16_t sequenceKeyframeCount0;  // 0x18: Keyframe count 0
    uint16_t sequenceKeyframeCount1;  // 0x1A: Keyframe count 1
    uint32_t unknown_0x1C;            // 0x1C: Unknown
    float geometryScale;              // 0x20: Skeleton/geometry scale factor. If <=0, computed from bounding data
    float unknown_0x24;               // 0x24: Unknown (often 0)
    float unknown_0x28;               // 0x28: Unknown (often 1.0)
    uint32_t bindPoseBoneCount;       // 0x2C: Number of bones in bind pose section (discovered via RE)
    uint32_t unknown_0x30;            // 0x30: Unknown (often 3)
    uint32_t transformDataSize;       // 0x34: Transform data size
    uint32_t submeshCount;            // 0x38: Submesh count
    uint32_t unknown_0x3C;            // 0x3C: Unknown
    uint16_t sequenceCount0;          // 0x40: Sequence count 0
    uint16_t sequenceCount1;          // 0x42: Sequence count 1
    uint32_t unknown_0x44;            // 0x44: Unknown
    uint32_t animationCount;          // 0x48: Animation count
    uint32_t skeletonNodeCount;       // 0x4C: Skeleton node count
    uint16_t boneDataCount;           // 0x50: Bone data count
    uint16_t attachmentCount;         // 0x52: Attachment count
    uint8_t unknown_0x54;             // 0x54: Unknown
    uint8_t unknown_0x55;             // 0x55: Unknown
    uint8_t unknown_0x56;             // 0x56: Unknown
    uint8_t unknown_0x57;             // 0x57: Unknown
    // Total: 88 bytes (0x58)

    bool HasSkeleton() const { return (classFlags & FA1_FLAG_HAS_SKELETON) != 0; }
    bool HasAnimationSequences() const { return (classFlags & FA1_FLAG_HAS_ANIMATION_SEQUENCES) != 0; }
    bool HasSkeletonData() const { return (classFlags & FA1_FLAG_HAS_SKELETON_DATA) != 0; }
};
#pragma pack(pop)

static_assert(sizeof(FA1Header) == 88, "FA1Header must be 88 bytes!");

/**
 * @brief Animation sequence entry structure (24 bytes).
 */
#pragma pack(push, 1)
struct BB9SequenceEntry
{
    uint32_t animationId;   // 0x00: Animation hash identifier
    float boundX;           // 0x04: Bounding X
    float boundY;           // 0x08: Bounding Y
    float boundZ;           // 0x0C: Bounding Z
    uint32_t frameCount;    // 0x10: Number of frames in this sequence
    uint32_t sequenceIndex; // 0x14: Sequence grouping index
    // Total: 24 bytes
};
#pragma pack(pop)

static_assert(sizeof(BB9SequenceEntry) == 24, "BB9SequenceEntry must be 24 bytes!");

/**
 * @brief Bone animation header structure (22 bytes per bone).
 */
#pragma pack(push, 1)
struct BB9BoneAnimHeader
{
    float baseX;           // 0x00: Absolute X position in bind pose
    float baseY;           // 0x04: Absolute Y position in bind pose
    float baseZ;           // 0x08: Absolute Z position in bind pose
    uint32_t boneFlags;    // 0x0C: Flags (low byte = hierarchy depth)
    uint16_t posKeyCount;  // 0x10: Number of position keyframes
    uint16_t rotKeyCount;  // 0x12: Number of rotation keyframes
    uint16_t scaleKeyCount;// 0x14: Number of scale keyframes
    // Total: 22 bytes

    uint8_t GetHierarchyDepth() const { return static_cast<uint8_t>(boneFlags & 0xFF); }
};
#pragma pack(pop)

static_assert(sizeof(BB9BoneAnimHeader) == 22, "BB9BoneAnimHeader must be 22 bytes!");

/**
 * @brief FA1 bind pose entry structure (16 bytes per bone).
 *
 * Found in FA1 chunks after the header + bounding cylinders.
 * Contains bind pose position and explicit parent information.
 *
 * Parent encoding:
 * - Low byte (0-255): parent bone index, or 0 for chain mode
 * - High byte flags: 0x10 indicates "parent is 0" explicitly (branch to root)
 * - When low byte = 0 AND no flag: parent = previous bone (chain)
 * - When low byte > 0: parent = low byte value (explicit override)
 * - When flag 0x10 set: parent = 0 (branch back to root)
 */
#pragma pack(push, 1)
struct FA1BindPoseEntry
{
    float posX;             // 0x00: Bind pose X position
    float posY;             // 0x04: Bind pose Y position
    float posZ;             // 0x08: Bind pose Z position
    uint32_t parentInfo;    // 0x0C: Parent index + flags
    // Total: 16 bytes

    /**
     * @brief Gets the explicit parent index from the parentInfo field.
     *
     * @param boneIndex The index of this bone (for chain mode).
     * @return Parent bone index, or -1 for root.
     */
    int32_t GetParentIndex(size_t boneIndex) const
    {
        constexpr uint32_t FLAG_BRANCH_TO_ROOT = 0x10000000;

        if (boneIndex == 0)
        {
            return -1;  // First bone is always root
        }

        uint8_t lowByte = static_cast<uint8_t>(parentInfo & 0xFF);

        if (parentInfo & FLAG_BRANCH_TO_ROOT)
        {
            // Flag means parent = 0 (branch back to root/body center)
            return 0;
        }
        else if (lowByte > 0)
        {
            // Explicit parent override
            return static_cast<int32_t>(lowByte);
        }
        else
        {
            // Chain mode: parent = previous bone
            return static_cast<int32_t>(boneIndex - 1);
        }
    }
};
#pragma pack(pop)

static_assert(sizeof(FA1BindPoseEntry) == 16, "FA1BindPoseEntry must be 16 bytes!");

/**
 * @brief FA1 keyframe header structure (16 bytes).
 *
 * Found in FA1 chunks after bind pose entries for models that use
 * the FA1-specific VLE keyframe format (not BB9-style per-bone headers).
 *
 * This format is used by models like 0xBC68 (pig) where animation data
 * is stored as:
 *   1. This header (16 bytes)
 *   2. Bone offset table (bindPoseBoneCount × 4 bytes) - bit offsets into VLE stream
 *   3. VLE-encoded keyframe stream
 */
#pragma pack(push, 1)
struct FA1KeyframeHeader
{
    uint16_t reserved0;         // 0x00: Always 0
    uint16_t reserved1;         // 0x02: Always 0
    uint16_t reserved2;         // 0x04: Always 0
    uint16_t rotationKeyCount;  // 0x06: Total rotation keyframes across all bones
    uint16_t positionKeyCount;  // 0x08: Total position keyframes across all bones
    uint16_t reserved3;         // 0x0A: Always 0
    uint16_t reserved4;         // 0x0C: Always 0
    uint16_t reserved5;         // 0x0E: Always 0
    // Total: 16 bytes

    /**
     * @brief Checks if this looks like a valid FA1 keyframe header.
     *
     * Valid headers have zeros in reserved fields and reasonable key counts.
     */
    bool IsValid() const
    {
        return reserved0 == 0 && reserved1 == 0 && reserved2 == 0 &&
               reserved3 == 0 && reserved4 == 0 && reserved5 == 0 &&
               rotationKeyCount < 5000 && positionKeyCount < 5000 &&
               (rotationKeyCount > 0 || positionKeyCount > 0);
    }
};
#pragma pack(pop)

static_assert(sizeof(FA1KeyframeHeader) == 16, "FA1KeyframeHeader must be 16 bytes!");

/**
 * @brief Parser for BB9/FA1 animation chunks.
 *
 * Parses animation data including:
 * - Animation sequences (idle, walk, run, etc.)
 * - Bone transform keyframes (position, rotation, scale)
 * - Bone hierarchy from depth values
 *
 * Uses VLE decoding for compressed keyframe data.
 */
class BB9AnimationParser
{
public:
    /**
     * @brief Parses FA1 bind pose entries and extracts parent array.
     *
     * FA1 chunks contain explicit bind pose data with parent indices,
     * which is more accurate than deriving hierarchy from BB9 hierarchyByte.
     *
     * @param data Pointer to FA1 chunk data (starting at header).
     * @param dataSize Size of the chunk data.
     * @param outParents Output vector for parent indices.
     * @param outBindPositions Output vector for bind positions.
     * @return Number of bones parsed, or 0 on failure.
     */
    static size_t ParseFA1BindPose(const uint8_t* data, size_t dataSize,
                                    std::vector<int32_t>& outParents,
                                    std::vector<XMFLOAT3>& outBindPositions)
    {
        if (dataSize < sizeof(FA1Header))
        {
            return 0;
        }

        FA1Header header;
        std::memcpy(&header, data, sizeof(FA1Header));

        // Bone count is at offset 0x2C (bindPoseBoneCount field)
        uint32_t boneCount = header.bindPoseBoneCount;

        // Validate bone count
        if (boneCount == 0 || boneCount > 256)
        {
            return 0;
        }

        // Calculate offset to bind pose data: header + bounding cylinders
        size_t bindPoseOffset = sizeof(FA1Header) + (header.boundingCylinderCount * 16);

        // Check if there's enough data for all bind pose entries
        size_t bindPoseSize = boneCount * sizeof(FA1BindPoseEntry);
        if (bindPoseOffset + bindPoseSize > dataSize)
        {
            return 0;
        }

        outParents.clear();
        outParents.reserve(boneCount);
        outBindPositions.clear();
        outBindPositions.reserve(boneCount);

        // Parse each bind pose entry
        for (size_t i = 0; i < boneCount; i++)
        {
            FA1BindPoseEntry entry;
            std::memcpy(&entry, &data[bindPoseOffset + i * sizeof(FA1BindPoseEntry)], sizeof(FA1BindPoseEntry));

            int32_t parent = entry.GetParentIndex(i);
            outParents.push_back(parent);

            // Transform coordinates: (x, y, z) -> (x, -z, y) for DirectX
            outBindPositions.push_back({
                entry.posX,
                -entry.posZ,
                entry.posY
            });
        }

        return boneCount;
    }

    /**
     * @brief Parses a BB9/FA1 animation chunk from raw data.
     *
     * @param data Pointer to chunk data (after chunk ID and size).
     * @param dataSize Size of the chunk data.
     * @return Parsed animation clip, or nullopt on failure.
     */
    static std::optional<Animation::AnimationClip> Parse(const uint8_t* data, size_t dataSize)
    {
        if (dataSize < sizeof(BB9Header))
        {
            return std::nullopt;
        }

        // Parse header
        BB9Header header;
        std::memcpy(&header, data, sizeof(BB9Header));

        Animation::AnimationClip clip;
        clip.modelHash0 = header.modelHash0;
        clip.modelHash1 = header.modelHash1;

        // Try to read geometry scale from header offset 0x20
        // For FA1: this is the actual geometry scale factor
        // For BB9: this is reserved[2], which may contain scale data
        // GW stores this at model+0x100 and uses it to scale bone positions
        // If the value is 0 or negative, GW computes scale from bounding data
        float headerScale;
        std::memcpy(&headerScale, &header.reserved[2], sizeof(float));
        bool needsAutoScale = !(headerScale > 0.001f && headerScale < 100.0f);
        clip.geometryScale = needsAutoScale ? 1.0f : headerScale;

        size_t offset = sizeof(BB9Header);

        // Skip bounding cylinders (16 bytes each)
        offset += header.boundingCylinderCount * 16;

        // Parse sequences if present
        uint32_t cumulativeFrames = 0;
        if (header.HasSequences())
        {
            if (offset + 4 > dataSize) return std::nullopt;

            uint32_t sequenceCount;
            std::memcpy(&sequenceCount, &data[offset], sizeof(uint32_t));
            offset += 4;

            for (uint32_t i = 0; i < sequenceCount; i++)
            {
                if (offset + sizeof(BB9SequenceEntry) > dataSize) return std::nullopt;

                BB9SequenceEntry entry;
                std::memcpy(&entry, &data[offset], sizeof(BB9SequenceEntry));
                offset += sizeof(BB9SequenceEntry);

                Animation::AnimationSequence seq;
                seq.hash = entry.animationId;
                seq.name = "seq_" + std::to_string(i);
                seq.frameCount = entry.frameCount;
                seq.sequenceIndex = entry.sequenceIndex;
                seq.bounds = {entry.boundX, entry.boundY, entry.boundZ};

                clip.sequences.push_back(seq);
                cumulativeFrames += entry.frameCount;
            }
        }
        clip.totalFrames = cumulativeFrames > 0 ? cumulativeFrames : 100;

        // Parse bone transforms if present
        if (header.HasBoneTransforms())
        {
            if (offset + 8 > dataSize) return std::nullopt;

            uint32_t boneCount, unknown;
            std::memcpy(&boneCount, &data[offset], sizeof(uint32_t));
            std::memcpy(&unknown, &data[offset + 4], sizeof(uint32_t));
            offset += 8;

            // Validate bone count
            if (boneCount > 500)
            {
                return std::nullopt;  // Invalid bone count
            }

            // Parse bones using VLE decoder
            VLEDecoder decoder(data, dataSize, offset);
            std::vector<uint8_t> boneDepths;
            int errorsInRow = 0;

            for (uint32_t boneIdx = 0; boneIdx < boneCount; boneIdx++)
            {
                try
                {
                    if (decoder.RemainingBytes() < sizeof(BB9BoneAnimHeader))
                    {
                        break;
                    }

                    // Read bone header
                    BB9BoneAnimHeader boneHeader;
                    std::memcpy(&boneHeader, &data[decoder.GetOffset()], sizeof(BB9BoneAnimHeader));
                    decoder.SetOffset(decoder.GetOffset() + sizeof(BB9BoneAnimHeader));

                    // Validate key counts
                    if (boneHeader.posKeyCount > 10000 || boneHeader.rotKeyCount > 10000 ||
                        boneHeader.scaleKeyCount > 10000)
                    {
                        break;  // Invalid data
                    }

                    Animation::BoneTrack track;
                    track.boneIndex = boneIdx;

                    // Store base position with coordinate transform: (x, y, z) -> (x, -z, y)
                    // GW uses (left/right, front/back, down/up), GWMB uses (left/right, up/down, front/back)
                    // Scale will be applied after parsing if needed (when header scale is 0)
                    track.basePosition = {
                        boneHeader.baseX,
                        -boneHeader.baseZ,
                        boneHeader.baseY
                    };

                    uint8_t depth = boneHeader.GetHierarchyDepth();
                    boneDepths.push_back(depth);

                    // Track intermediate bones (Ghidra RE @ Model_UpdateSkeletonTransforms)
                    // Flag 0x10000000 means bone participates in hierarchy but produces no output matrix
                    constexpr uint32_t FLAG_INTERMEDIATE_BONE = 0x10000000;
                    bool isIntermediate = (boneHeader.boneFlags & FLAG_INTERMEDIATE_BONE) != 0;
                    clip.boneIsIntermediate.push_back(isIntermediate);

                    // Parse position keyframes
                    if (boneHeader.posKeyCount > 0)
                    {
                        auto posTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.posKeyCount);
                        auto positions = decoder.ReadFloat3s(boneHeader.posKeyCount);

                        track.positionKeys.reserve(boneHeader.posKeyCount);
                        for (size_t i = 0; i < boneHeader.posKeyCount; i++)
                        {
                            // Apply coordinate transform: (x, y, z) -> (x, -z, y)
                            // Scale will be applied after parsing if needed
                            XMFLOAT3 transformedPos = {
                                positions[i].x,
                                -positions[i].z,
                                positions[i].y
                            };
                            track.positionKeys.push_back({
                                static_cast<float>(posTimes[i]),
                                transformedPos
                            });
                        }
                    }

                    // Parse rotation keyframes
                    if (boneHeader.rotKeyCount > 0)
                    {
                        auto rotTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.rotKeyCount);
                        auto rotations = decoder.DecompressQuaternionKeys(boneHeader.rotKeyCount);

                        track.rotationKeys.reserve(boneHeader.rotKeyCount);
                        for (size_t i = 0; i < boneHeader.rotKeyCount; i++)
                        {
                            track.rotationKeys.push_back({
                                static_cast<float>(rotTimes[i]),
                                rotations[i]
                            });
                        }
                    }

                    // Parse scale keyframes
                    if (boneHeader.scaleKeyCount > 0)
                    {
                        auto scaleTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.scaleKeyCount);
                        auto scales = decoder.ReadFloat3s(boneHeader.scaleKeyCount);

                        track.scaleKeys.reserve(boneHeader.scaleKeyCount);
                        for (size_t i = 0; i < boneHeader.scaleKeyCount; i++)
                        {
                            track.scaleKeys.push_back({
                                static_cast<float>(scaleTimes[i]),
                                scales[i]
                            });
                        }
                    }

                    clip.boneTracks.push_back(std::move(track));
                    errorsInRow = 0;
                }
                catch (const std::exception&)
                {
                    errorsInRow++;
                    if (errorsInRow > 3)
                    {
                        break;  // Too many consecutive errors
                    }

                    // Add empty track placeholder
                    Animation::BoneTrack emptyTrack;
                    emptyTrack.boneIndex = boneIdx;
                    clip.boneTracks.push_back(std::move(emptyTrack));
                    boneDepths.push_back(0);
                    clip.boneIsIntermediate.push_back(false);  // Assume non-intermediate on error
                }
            }

            // Compute bone hierarchy from depth values
            bool usedSequentialMode = false;
            Animation::HierarchyMode hierarchyMode = Animation::HierarchyMode::TreeDepth;
            ComputeBoneParents(clip.boneParents, boneDepths, &usedSequentialMode, &hierarchyMode);
            clip.hierarchyMode = hierarchyMode;

            // Build output-to-animation bone mapping (for intermediate bone handling)
            // Based on Ghidra RE: bones with flag 0x10000000 don't produce output matrices
            clip.BuildOutputMapping();

            // Note: We do NOT apply geometry scale here during parsing.
            // The skeleton scale must match the mesh scale, which is computed
            // elsewhere based on the model's bounding box normalization.
            // The raw bone positions are stored as-is from the file.
        }

        // Compute time ranges
        clip.ComputeTimeRange();
        clip.ComputeSequenceTimeRanges();

        return clip;
    }

private:
    /**
     * @brief Parses FA1-specific keyframe format.
     *
     * This format is used by models like 0xBC68 (pig) where animation data
     * is organized as:
     *   1. FA1KeyframeHeader (16 bytes) - contains total rotation/position key counts
     *   2. Bone offset table (boneCount × 4 bytes) - bit offsets into VLE stream
     *   3. VLE-encoded keyframe stream
     *
     * The VLE stream contains interleaved keyframe data for all bones, indexed by
     * the bit offsets in the offset table.
     *
     * @param data Pointer to chunk data.
     * @param dataSize Size of the chunk data.
     * @param bindPoseOffset Offset to bind pose entries (for reading base positions).
     * @param keyframeOffset Current offset (after bind pose entries).
     * @param boneCount Number of bones.
     * @param clip Output animation clip.
     * @param boneDepths Output vector for hierarchy depths.
     * @return true if parsing succeeded, false on failure.
     */
    static bool ParseFA1KeyframeFormat(const uint8_t* data, size_t dataSize,
                                        size_t bindPoseOffset, size_t keyframeOffset,
                                        uint32_t boneCount, Animation::AnimationClip& clip,
                                        std::vector<uint8_t>& boneDepths)
    {
        if (keyframeOffset + sizeof(FA1KeyframeHeader) > dataSize)
        {
            return false;
        }

        // Read keyframe header
        FA1KeyframeHeader kfHeader;
        std::memcpy(&kfHeader, &data[keyframeOffset], sizeof(FA1KeyframeHeader));
        size_t offset = keyframeOffset + sizeof(FA1KeyframeHeader);

        // Read bone offset table (boneCount × 4 bytes)
        // Each entry is a bit offset into the VLE stream
        if (offset + boneCount * sizeof(uint32_t) > dataSize)
        {
            return false;
        }

        std::vector<uint32_t> boneOffsets(boneCount);
        for (uint32_t i = 0; i < boneCount; i++)
        {
            std::memcpy(&boneOffsets[i], &data[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }

        // Read bind pose entries to get base positions and hierarchy bytes
        // Each FA1BindPoseEntry is 16 bytes: float3 position + uint32_t parentInfo
        for (uint32_t i = 0; i < boneCount; i++)
        {
            size_t bpOffset = bindPoseOffset + i * sizeof(FA1BindPoseEntry);
            if (bpOffset + sizeof(FA1BindPoseEntry) > dataSize)
            {
                break;
            }

            FA1BindPoseEntry bindPose;
            std::memcpy(&bindPose, &data[bpOffset], sizeof(FA1BindPoseEntry));

            Animation::BoneTrack track;
            track.boneIndex = i;

            // Store base position with coordinate transform: (x, y, z) -> (x, -z, y)
            track.basePosition = {
                bindPose.posX,
                -bindPose.posZ,
                bindPose.posY
            };

            clip.boneTracks.push_back(std::move(track));

            // Extract hierarchy byte from parentInfo (low byte is hierarchy/pop count)
            uint8_t hierByte = static_cast<uint8_t>(bindPose.parentInfo & 0xFF);
            boneDepths.push_back(hierByte);

            // FA1 format: parentInfo bit 0x10000000 means "branch to root" (parent=0),
            // NOT "intermediate bone". FA1 bind pose doesn't have intermediate bone flags.
            // Intermediate bone detection for FA1 would need to come from elsewhere.
            // For now, assume all FA1 bones are non-intermediate (produce output matrices).
            clip.boneIsIntermediate.push_back(false);
        }

        // Decode FA1 VLE stream (bit-level offsets)
        // Observed on 0xBC68: offsets appear to be grouped in sets of 4 per animated bone:
        //   [posTimes][posValues][rotTimes][rotValues]
        // The data is bit-packed VLE; values are delta-encoded like BB9, but at bit offsets.
        //
        // Notes:
        // - Position values are stored as signed 16-bit deltas (VLE) and appear to scale ~1/1024.
        // - Rotation values are Euler deltas (VLE) using the same 16-bit angle mapping as BB9.
        // - Time streams sometimes decode poorly; if invalid, we fall back to implicit 0..N-1.

        struct BitVLEReader
        {
            const uint8_t* data;
            size_t dataSize;
            size_t bitOffset = 0;
            size_t bitEnd = 0;

            BitVLEReader(const uint8_t* inData, size_t inSize, size_t inBitOffset, size_t inBitEnd)
                : data(inData), dataSize(inSize), bitOffset(inBitOffset), bitEnd(inBitEnd)
            {
            }

            bool HasBits(size_t bits) const
            {
                return bitOffset + bits <= bitEnd;
            }

            uint8_t ReadByte()
            {
                if (!HasBits(8))
                {
                    // Clamp to end-of-stream to avoid exceptions on truncated bit ranges.
                    bitOffset = bitEnd;
                    return 0;
                }

                size_t byteIndex = bitOffset / 8;
                uint8_t shift = static_cast<uint8_t>(bitOffset % 8);
                uint8_t b0 = (byteIndex < dataSize) ? data[byteIndex] : 0;
                uint8_t value;

                if (shift == 0)
                {
                    value = b0;
                }
                else
                {
                    uint8_t b1 = (byteIndex + 1 < dataSize) ? data[byteIndex + 1] : 0;
                    value = static_cast<uint8_t>((b0 >> shift) | (b1 << (8 - shift)));
                }

                bitOffset += 8;
                return value;
            }

            uint32_t ReadVLEValue(bool* outSign = nullptr)
            {
                uint8_t b = ReadByte();
                uint32_t value = b & 0x3F;

                if (outSign)
                {
                    *outSign = (b & 0x40) != 0;
                }

                if (b & 0x80)
                {
                    b = ReadByte();
                    value |= (b & 0x7F) << 6;

                    if (b & 0x80)
                    {
                        b = ReadByte();
                        value |= (b & 0x7F) << 13;

                        if (b & 0x80)
                        {
                            b = ReadByte();
                            value |= (b & 0x7F) << 20;

                            if (b & 0x80)
                            {
                                b = ReadByte();
                                value |= static_cast<uint32_t>(b) << 27;
                            }
                        }
                    }
                }

                return value;
            }
        };

        auto DecodeTimes = [](BitVLEReader& reader, size_t maxCount) -> std::vector<uint32_t>
        {
            std::vector<uint32_t> times;
            times.reserve(maxCount);

            int32_t last1 = 0;
            int32_t last2 = 0;

            for (size_t i = 0; i < maxCount; i++)
            {
                if (!reader.HasBits(8))
                {
                    break;
                }

                bool signPositive = false;
                uint32_t raw = reader.ReadVLEValue(&signPositive);
                int32_t delta = signPositive ? static_cast<int32_t>(raw) : -static_cast<int32_t>(raw);
                int32_t value = (last1 * 2 - last2) + delta;
                times.push_back(static_cast<uint32_t>(value));
                last2 = last1;
                last1 = value;
            }

            return times;
        };

        auto DecodePositionValues = [](BitVLEReader& reader, size_t maxKeys, float scale) -> std::vector<XMFLOAT3>
        {
            std::vector<XMFLOAT3> values;
            values.reserve(maxKeys);

            int16_t prevX = 0, prevY = 0, prevZ = 0;

            for (size_t i = 0; i < maxKeys; i++)
            {
                if (!reader.HasBits(8 * 3))
                {
                    break;
                }

                bool sign = false;
                uint32_t raw = reader.ReadVLEValue(&sign);
                prevX = sign ? static_cast<int16_t>((prevX + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevX - raw) & 0xFFFF);

                raw = reader.ReadVLEValue(&sign);
                prevY = sign ? static_cast<int16_t>((prevY + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevY - raw) & 0xFFFF);

                raw = reader.ReadVLEValue(&sign);
                prevZ = sign ? static_cast<int16_t>((prevZ + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevZ - raw) & 0xFFFF);

                XMFLOAT3 pos = {
                    static_cast<float>(prevX) * scale,
                    static_cast<float>(prevY) * scale,
                    static_cast<float>(prevZ) * scale
                };

                // Coordinate transform: (x, y, z) -> (x, -z, y)
                values.push_back({pos.x, -pos.z, pos.y});
            }

            return values;
        };

        auto DecodeRotationValues = [](BitVLEReader& reader, size_t maxKeys) -> std::vector<XMFLOAT4>
        {
            std::vector<XMFLOAT4> rotations;
            rotations.reserve(maxKeys);

            constexpr float ANGLE_SCALE = (2.0f * 3.14159265358979323846f) / 65536.0f;
            constexpr float ANGLE_OFFSET = 3.14159265358979323846f;

            int16_t prevX = 0, prevY = 0, prevZ = 0;

            for (size_t i = 0; i < maxKeys; i++)
            {
                if (!reader.HasBits(8 * 3))
                {
                    break;
                }

                bool sign = false;
                uint32_t raw = reader.ReadVLEValue(&sign);
                prevX = sign ? static_cast<int16_t>((prevX + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevX - raw) & 0xFFFF);

                raw = reader.ReadVLEValue(&sign);
                prevY = sign ? static_cast<int16_t>((prevY + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevY - raw) & 0xFFFF);

                raw = reader.ReadVLEValue(&sign);
                prevZ = sign ? static_cast<int16_t>((prevZ + raw) & 0xFFFF)
                             : static_cast<int16_t>((prevZ - raw) & 0xFFFF);

                float rx_gw = -(prevX * ANGLE_SCALE - ANGLE_OFFSET);
                float ry_gw = -(prevY * ANGLE_SCALE - ANGLE_OFFSET);
                float rz_gw = -(prevZ * ANGLE_SCALE - ANGLE_OFFSET);

                XMFLOAT4 quat_gw = VLEDecoder::EulerToQuaternion(rx_gw, ry_gw, rz_gw);

                XMFLOAT4 quat;
                quat.x = quat_gw.x;
                quat.y = -quat_gw.z;
                quat.z = quat_gw.y;
                quat.w = quat_gw.w;

                if (!rotations.empty())
                {
                    const XMFLOAT4& prev = rotations.back();
                    float dot = quat.w * prev.w + quat.x * prev.x + quat.y * prev.y + quat.z * prev.z;
                    if (dot < 0.0f)
                    {
                        quat.w = -quat.w;
                        quat.x = -quat.x;
                        quat.y = -quat.y;
                        quat.z = -quat.z;
                    }
                }

                rotations.push_back(quat);
            }

            return rotations;
        };

        // Decode grouped offset layout: offsets are stored in 4 contiguous blocks
        // [posTimes][posValues][rotTimes][rotValues], with animBoneCount = boneCount / 4
        // (bind pose bones appear to be grouped in sets of 4 with near-identical positions).
        if (boneCount >= 4 && (boneCount % 4) == 0)
        {
            const uint32_t animBoneCount = boneCount / 4;
            const size_t streamStart = keyframeOffset + sizeof(FA1KeyframeHeader) + boneCount * sizeof(uint32_t);
            const size_t streamSize = (streamStart < dataSize) ? (dataSize - streamStart) : 0;
            const size_t streamBits = streamSize * 8;

            if (animBoneCount > 0 && boneOffsets.size() >= static_cast<size_t>(animBoneCount) * 4 && streamBits > 0)
            {
                const uint32_t* posTimesOffsets = boneOffsets.data();
                const uint32_t* posValuesOffsets = boneOffsets.data() + animBoneCount;
                const uint32_t* rotTimesOffsets = boneOffsets.data() + animBoneCount * 2;
                const uint32_t* rotValuesOffsets = boneOffsets.data() + animBoneCount * 3;

                auto IsValidTimes = [](const std::vector<uint32_t>& times, size_t keyCount) -> bool
                {
                    if (times.empty())
                    {
                        return false;
                    }

                    uint32_t prev = times[0];
                    for (size_t i = 1; i < times.size(); i++)
                    {
                        if (times[i] < prev)
                        {
                            return false;
                        }
                        prev = times[i];
                    }

                    const uint64_t softLimit = static_cast<uint64_t>(keyCount) * 10000ull;
                    if (softLimit > 0 && times.back() > softLimit)
                    {
                        return false;
                    }

                    return true;
                };

                auto IsValidRange = [streamBits](size_t start, size_t end) -> bool
                {
                    return start < end && end <= streamBits;
                };

                for (uint32_t animIdx = 0; animIdx < animBoneCount; animIdx++)
                {
                    size_t bitPosTimes = posTimesOffsets[animIdx];
                    size_t bitPosValues = posValuesOffsets[animIdx];
                    size_t bitRotTimes = rotTimesOffsets[animIdx];
                    size_t bitRotValues = rotValuesOffsets[animIdx];

                    size_t bitEndPosTimes = (animIdx + 1 < animBoneCount)
                                                ? static_cast<size_t>(posTimesOffsets[animIdx + 1])
                                                : static_cast<size_t>(posValuesOffsets[0]);
                    size_t bitEndPosValues = (animIdx + 1 < animBoneCount)
                                                ? static_cast<size_t>(posValuesOffsets[animIdx + 1])
                                                : static_cast<size_t>(rotTimesOffsets[0]);
                    size_t bitEndRotTimes = (animIdx + 1 < animBoneCount)
                                                ? static_cast<size_t>(rotTimesOffsets[animIdx + 1])
                                                : static_cast<size_t>(rotValuesOffsets[0]);
                    size_t bitEndRotValues = (animIdx + 1 < animBoneCount)
                                                ? static_cast<size_t>(rotValuesOffsets[animIdx + 1])
                                                : streamBits;

                    if (!IsValidRange(bitPosTimes, bitEndPosTimes) ||
                        !IsValidRange(bitPosValues, bitEndPosValues) ||
                        !IsValidRange(bitRotTimes, bitEndRotTimes) ||
                        !IsValidRange(bitRotValues, bitEndRotValues))
                    {
                        continue;
                    }

                    BitVLEReader posTimesReader(&data[streamStart], streamSize, bitPosTimes, bitEndPosTimes);
                    BitVLEReader posValuesReader(&data[streamStart], streamSize, bitPosValues, bitEndPosValues);
                    BitVLEReader rotTimesReader(&data[streamStart], streamSize, bitRotTimes, bitEndRotTimes);
                    BitVLEReader rotValuesReader(&data[streamStart], streamSize, bitRotValues, bitEndRotValues);

                    std::vector<uint32_t> posTimes = DecodeTimes(posTimesReader, kfHeader.positionKeyCount);
                    std::vector<XMFLOAT3> posValues = DecodePositionValues(posValuesReader, kfHeader.positionKeyCount, 1.0f / 1024.0f);
                    std::vector<uint32_t> rotTimes = DecodeTimes(rotTimesReader, kfHeader.rotationKeyCount);
                    std::vector<XMFLOAT4> rotValues = DecodeRotationValues(rotValuesReader, kfHeader.rotationKeyCount);

                    size_t posCount = posValues.size();
                    if (!posTimes.empty())
                    {
                        posCount = std::min(posCount, posTimes.size());
                    }

                    size_t rotCount = rotValues.size();
                    if (!rotTimes.empty())
                    {
                        rotCount = std::min(rotCount, rotTimes.size());
                    }

                    if (posCount == 0 && rotCount == 0)
                    {
                        continue;
                    }

                    bool usePosTimes = IsValidTimes(posTimes, posCount);
                    bool useRotTimes = IsValidTimes(rotTimes, rotCount);

                    std::vector<Animation::Keyframe<XMFLOAT3>> posKeys;
                    if (posCount > 0)
                    {
                        posKeys.reserve(posCount);
                        for (size_t i = 0; i < posCount; i++)
                        {
                            float t = usePosTimes ? static_cast<float>(posTimes[i]) : static_cast<float>(i);
                            posKeys.push_back({t, posValues[i]});
                        }
                    }

                    std::vector<Animation::Keyframe<XMFLOAT4>> rotKeys;
                    if (rotCount > 0)
                    {
                        rotKeys.reserve(rotCount);
                        for (size_t i = 0; i < rotCount; i++)
                        {
                            float t = useRotTimes ? static_cast<float>(rotTimes[i]) : static_cast<float>(i);
                            rotKeys.push_back({t, rotValues[i]});
                        }
                    }

                    // Apply this animation track to the 4-bone group
                    const uint32_t groupStart = animIdx * 4;
                    for (uint32_t groupOffset = 0; groupOffset < 4; groupOffset++)
                    {
                        uint32_t boneIndex = groupStart + groupOffset;
                        if (boneIndex >= clip.boneTracks.size())
                        {
                            break;
                        }

                        Animation::BoneTrack& track = clip.boneTracks[boneIndex];
                        if (!posKeys.empty())
                        {
                            track.positionKeys = posKeys;
                        }
                        if (!rotKeys.empty())
                        {
                            track.rotationKeys = rotKeys;
                        }
                    }
                }
            }
        }

        return true;
    }

    /**
     * @brief Parses BB9-style bone animation headers.
     *
     * This format is used by standalone BB9 animation files and some FA1 chunks.
     * Each bone has:
     *   1. BB9BoneAnimHeader (22 bytes) - base position, flags, key counts
     *   2. VLE-encoded position times + float3 positions
     *   3. VLE-encoded rotation times + VLE rotation deltas
     *   4. VLE-encoded scale times + float3 scales
     *
     * @param data Pointer to chunk data.
     * @param dataSize Size of the chunk data.
     * @param offset Current offset (after bind pose entries).
     * @param clip Output animation clip.
     * @param boneDepths Output vector for hierarchy depths.
     */
    static void ParseBB9StyleBoneAnimations(const uint8_t* data, size_t dataSize, size_t offset,
                                             Animation::AnimationClip& clip,
                                             std::vector<uint8_t>& boneDepths)
    {
        VLEDecoder decoder(data, dataSize, offset);
        int errorsInRow = 0;
        uint32_t boneIdx = 0;
        const uint32_t maxBones = 256;

        while (decoder.RemainingBytes() >= sizeof(BB9BoneAnimHeader) &&
               boneIdx < maxBones &&
               errorsInRow < 3)
        {
            try
            {
                // Read bone header
                BB9BoneAnimHeader boneHeader;
                std::memcpy(&boneHeader, &data[decoder.GetOffset()], sizeof(BB9BoneAnimHeader));

                // Validate header
                bool keyCountsValid = (boneHeader.posKeyCount <= 1000 &&
                                      boneHeader.rotKeyCount <= 1000 &&
                                      boneHeader.scaleKeyCount <= 1000);

                bool posValid = std::isfinite(boneHeader.baseX) &&
                               std::isfinite(boneHeader.baseY) &&
                               std::isfinite(boneHeader.baseZ) &&
                               std::abs(boneHeader.baseX) < 100000.0f &&
                               std::abs(boneHeader.baseY) < 100000.0f &&
                               std::abs(boneHeader.baseZ) < 100000.0f;

                if (!keyCountsValid || (!posValid && boneIdx > 0))
                {
                    break;
                }

                decoder.SetOffset(decoder.GetOffset() + sizeof(BB9BoneAnimHeader));

                Animation::BoneTrack track;
                track.boneIndex = boneIdx;

                // Store base position with coordinate transform: (x, y, z) -> (x, -z, y)
                track.basePosition = {
                    boneHeader.baseX,
                    -boneHeader.baseZ,
                    boneHeader.baseY
                };

                uint8_t depth = boneHeader.GetHierarchyDepth();
                boneDepths.push_back(depth);

                // Parse position keyframes
                if (boneHeader.posKeyCount > 0)
                {
                    auto posTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.posKeyCount);
                    auto positions = decoder.ReadFloat3s(boneHeader.posKeyCount);

                    track.positionKeys.reserve(boneHeader.posKeyCount);
                    for (size_t i = 0; i < boneHeader.posKeyCount; i++)
                    {
                        XMFLOAT3 transformedPos = {
                            positions[i].x,
                            -positions[i].z,
                            positions[i].y
                        };
                        track.positionKeys.push_back({
                            static_cast<float>(posTimes[i]),
                            transformedPos
                        });
                    }
                }

                // Parse rotation keyframes
                if (boneHeader.rotKeyCount > 0)
                {
                    auto rotTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.rotKeyCount);
                    auto rotations = decoder.DecompressQuaternionKeys(boneHeader.rotKeyCount);

                    track.rotationKeys.reserve(boneHeader.rotKeyCount);
                    for (size_t i = 0; i < boneHeader.rotKeyCount; i++)
                    {
                        track.rotationKeys.push_back({
                            static_cast<float>(rotTimes[i]),
                            rotations[i]
                        });
                    }
                }

                // Parse scale keyframes
                if (boneHeader.scaleKeyCount > 0)
                {
                    auto scaleTimes = decoder.ExpandUnsignedDeltaVLE(boneHeader.scaleKeyCount);
                    auto scales = decoder.ReadFloat3s(boneHeader.scaleKeyCount);

                    track.scaleKeys.reserve(boneHeader.scaleKeyCount);
                    for (size_t i = 0; i < boneHeader.scaleKeyCount; i++)
                    {
                        track.scaleKeys.push_back({
                            static_cast<float>(scaleTimes[i]),
                            scales[i]
                        });
                    }
                }

                clip.boneTracks.push_back(std::move(track));
                errorsInRow = 0;
                boneIdx++;
            }
            catch (const std::exception&)
            {
                errorsInRow++;
                if (errorsInRow >= 3)
                {
                    break;
                }

                // Add empty track placeholder
                Animation::BoneTrack emptyTrack;
                emptyTrack.boneIndex = boneIdx;
                clip.boneTracks.push_back(std::move(emptyTrack));
                boneDepths.push_back(0);
                boneIdx++;
            }
        }
    }

public:
    /**
     * @brief Parses an FA1 animation chunk from raw data.
     *
     * FA1 chunks have a different header structure (88 bytes) than BB9 (44 bytes).
     * This method correctly handles the FA1 format.
     *
     * @param data Pointer to chunk data (after chunk ID and size).
     * @param dataSize Size of the chunk data.
     * @return Parsed animation clip, or nullopt on failure.
     */
    static std::optional<Animation::AnimationClip> ParseFA1(const uint8_t* data, size_t dataSize)
    {
        if (dataSize < sizeof(FA1Header))
        {
            return std::nullopt;
        }

        // Parse FA1 header (88 bytes)
        FA1Header header;
        std::memcpy(&header, data, sizeof(FA1Header));

        Animation::AnimationClip clip;
        clip.modelHash0 = header.boundingBoxId;
        clip.modelHash1 = header.collisionMeshId;

        // Get geometry scale from FA1 header
        float headerScale = header.geometryScale;
        bool needsAutoScale = !(headerScale > 0.001f && headerScale < 100.0f);
        clip.geometryScale = needsAutoScale ? 1.0f : headerScale;

        size_t offset = sizeof(FA1Header);

        // Skip bounding cylinders (16 bytes each)
        offset += header.boundingCylinderCount * 16;

        // FA1 may have sequence data if HAS_ANIMATION_SEQUENCES flag is set
        // and sequenceCount0 > 0
        if (header.HasAnimationSequences() && header.sequenceCount0 > 0)
        {
            for (uint16_t i = 0; i < header.sequenceCount0; i++)
            {
                if (offset + sizeof(BB9SequenceEntry) > dataSize) break;

                BB9SequenceEntry entry;
                std::memcpy(&entry, &data[offset], sizeof(BB9SequenceEntry));
                offset += sizeof(BB9SequenceEntry);

                Animation::AnimationSequence seq;
                seq.hash = entry.animationId;
                seq.name = "seq_" + std::to_string(i);
                seq.frameCount = entry.frameCount;
                seq.sequenceIndex = entry.sequenceIndex;
                seq.bounds = {entry.boundX, entry.boundY, entry.boundZ};

                clip.sequences.push_back(seq);
            }
        }

        // Calculate cumulative frames
        uint32_t cumulativeFrames = 0;
        for (const auto& seq : clip.sequences)
        {
            cumulativeFrames += seq.frameCount;
        }
        clip.totalFrames = cumulativeFrames > 0 ? cumulativeFrames : 100;

        // Parse bone animation data
        // FA1 format has bind pose entries followed by animation keyframe data
        // Check for skeleton data using either HasSkeleton flag OR bindPoseBoneCount > 0
        bool hasSkeletonData = header.HasSkeleton() || (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256);
        if (hasSkeletonData && offset < dataSize)
        {
            // Save bind pose offset before skipping (needed for FA1 keyframe format parsing)
            size_t bindPoseOffset = offset;

            // Skip bind pose entries if present (bindPoseBoneCount × 16 bytes)
            // These contain position + parent info, parsed separately by ParseFA1BindPose
            if (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256)
            {
                offset += header.bindPoseBoneCount * sizeof(FA1BindPoseEntry);
            }

            // Detect animation format: FA1-specific (with FA1KeyframeHeader) or BB9-style (with BB9BoneAnimHeader)
            //
            // FA1-specific format (used by models like 0xBC68):
            //   1. FA1KeyframeHeader (16 bytes) - has zeros in reserved fields
            //   2. Bone offset table (boneCount × 4 bytes) - bit offsets into VLE stream
            //   3. VLE-encoded keyframe stream
            //
            // BB9-style format:
            //   For each bone:
            //     1. BB9BoneAnimHeader (22 bytes) - position + flags + key counts
            //     2. VLE-encoded keyframe data for that bone

            bool useFA1KeyframeFormat = false;
            if (offset + sizeof(FA1KeyframeHeader) <= dataSize)
            {
                FA1KeyframeHeader testHeader;
                std::memcpy(&testHeader, &data[offset], sizeof(FA1KeyframeHeader));
                useFA1KeyframeFormat = testHeader.IsValid();
            }

            std::vector<uint8_t> boneDepths;

            if (useFA1KeyframeFormat)
            {
                // Parse FA1-specific keyframe format
                // Pass both bindPoseOffset (for reading base positions) and offset (keyframe data start)
                if (!ParseFA1KeyframeFormat(data, dataSize, bindPoseOffset, offset, header.bindPoseBoneCount,
                                            clip, boneDepths))
                {
                    // Fall through to BB9-style parsing
                    useFA1KeyframeFormat = false;
                }
            }

            if (!useFA1KeyframeFormat)
            {
                // Parse BB9-style bone animation headers
                ParseBB9StyleBoneAnimations(data, dataSize, offset, clip, boneDepths);
            }

            // Compute bone hierarchy from depth values
            bool usedSequentialMode = false;
            Animation::HierarchyMode hierarchyMode = Animation::HierarchyMode::TreeDepth;
            ComputeBoneParents(clip.boneParents, boneDepths, &usedSequentialMode, &hierarchyMode);
            clip.hierarchyMode = hierarchyMode;

            // Build output-to-animation bone mapping (for intermediate bone handling)
            // FA1 format doesn't have intermediate bone flags in bind pose, so all bones produce output
            clip.BuildOutputMapping();
        }

        // Compute time ranges
        clip.ComputeTimeRange();
        clip.ComputeSequenceTimeRanges();

        return clip;
    }

    /**
     * @brief Creates a Skeleton from an AnimationClip.
     *
     * The skeleton is reconstructed from animation data, using base positions
     * and hierarchy depths to build the bone tree.
     *
     * @param clip The animation clip containing bone data.
     * @return Skeleton with bone hierarchy.
     */
    static Animation::Skeleton CreateSkeleton(const Animation::AnimationClip& clip)
    {
        Animation::Skeleton skeleton;
        skeleton.bones.reserve(clip.boneTracks.size());

        // Geometry scale is applied during parsing when needed (either from header or auto-computed)
        for (size_t i = 0; i < clip.boneTracks.size(); i++)
        {
            const auto& track = clip.boneTracks[i];

            Animation::Bone bone;
            bone.id = static_cast<uint32_t>(i);
            bone.bindPosition = track.basePosition;  // Already scaled during parsing
            bone.parentIndex = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;

            skeleton.bones.push_back(bone);
        }

        // Copy parent indices
        skeleton.boneParents = clip.boneParents;

        // Compute inverse bind matrices for skinning
        skeleton.ComputeInverseBindMatrices();
        skeleton.BuildBoneIdMap();

        return skeleton;
    }

private:
    /**
     * @brief Computes bone parent indices from hierarchy depth values.
     *
     * Different GW models use different encodings for the depth byte:
     *
     * 1. TREE_DEPTH mode: depth = absolute level in hierarchy tree
     *    Pattern: [0, 1, 2, 3, 2, 3, 1, 2, ...] - increases by 1 for children
     *    Used by most standard models (e.g., 0x1FBCD)
     *
     * 2. POP_COUNT mode: depth = number of levels to pop from matrix stack
     *    Pattern: [0, 0, 0, 0, 3, 0, 0, ...] - mostly 0s with occasional jumps
     *    Based on Ghidra RE of GrTrans_PushPopMatrix @ 0x0064ab40
     *    Used by some models (e.g., 0x14067)
     *
     * 3. WORLD_SPACE mode: No meaningful hierarchy (all zeros or invalid)
     *    Each bone is treated as independent with absolute transforms
     *
     * @param outParents Output vector of parent indices.
     * @param depths Vector of hierarchy values from bone flags.
     * @param outUsedSequentialMode Output flag: true if WORLD_SPACE mode was used.
     * @param outHierarchyMode Output: detected hierarchy encoding mode.
     */
    static void ComputeBoneParents(std::vector<int32_t>& outParents, const std::vector<uint8_t>& depths,
                                    bool* outUsedSequentialMode = nullptr,
                                    Animation::HierarchyMode* outHierarchyMode = nullptr)
    {
        outParents.resize(depths.size(), -1);
        if (outUsedSequentialMode) *outUsedSequentialMode = false;
        if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::TreeDepth;

        if (depths.empty()) return;

        // Analyze the depth pattern to detect encoding type
        int zeroCount = 0;
        int maxDepth = 0;
        int valuesAboveBoneCount = 0;

        for (size_t i = 0; i < depths.size(); i++)
        {
            uint8_t d = depths[i];
            if (d == 0) zeroCount++;
            if (d > maxDepth) maxDepth = d;
            if (d > static_cast<uint8_t>(i)) valuesAboveBoneCount++;
        }

        // Detection logic:
        // TREE_DEPTH: starts with 0,1 and values don't exceed bone index
        // POP_COUNT: mostly zeros OR has values that can't be tree depths
        // WORLD_SPACE: no meaningful hierarchy data

        bool startsWithZeroOne = (depths.size() >= 2 && depths[0] == 0 && depths[1] == 1);
        bool mostlyZeros = (zeroCount >= static_cast<int>(depths.size()) * 0.5);
        bool hasValuesExceedingIndex = (valuesAboveBoneCount > 0);

        bool looksLikeTreeDepths = startsWithZeroOne && !hasValuesExceedingIndex;
        bool noHierarchyData = (zeroCount >= static_cast<int>(depths.size()) * 0.95) ||
                                (maxDepth == 0 && depths.size() > 1);

        if (noHierarchyData)
        {
            // WORLD_SPACE MODE: No hierarchy data available
            // Treat all bones as independent with world-space transforms
            if (outUsedSequentialMode) *outUsedSequentialMode = true;
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::Sequential;

            for (size_t boneIdx = 0; boneIdx < depths.size(); boneIdx++)
            {
                outParents[boneIdx] = -1;
            }
        }
        else if (looksLikeTreeDepths)
        {
            // TREE_DEPTH MODE: depth = absolute level in hierarchy
            // Algorithm: track bones at each depth level, find parent at depth-1
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::TreeDepth;
            std::unordered_map<uint8_t, int32_t> depthToBone;

            for (size_t boneIdx = 0; boneIdx < depths.size(); boneIdx++)
            {
                uint8_t depth = depths[boneIdx];
                int32_t parent = -1;

                if (boneIdx == 0 || depth == 0)
                {
                    parent = -1;  // Root bone
                }
                else
                {
                    // Clear depth entries >= current depth (from other branches)
                    std::vector<uint8_t> depthsToClear;
                    for (const auto& pair : depthToBone)
                    {
                        if (pair.first >= depth)
                        {
                            depthsToClear.push_back(pair.first);
                        }
                    }
                    for (uint8_t d : depthsToClear)
                    {
                        depthToBone.erase(d);
                    }

                    // Find parent at depth-1
                    auto it = depthToBone.find(depth - 1);
                    if (it != depthToBone.end())
                    {
                        parent = it->second;
                    }
                    else
                    {
                        // Fallback: search for nearest ancestor
                        for (int searchDepth = static_cast<int>(depth) - 1; searchDepth >= 0; searchDepth--)
                        {
                            auto searchIt = depthToBone.find(static_cast<uint8_t>(searchDepth));
                            if (searchIt != depthToBone.end())
                            {
                                parent = searchIt->second;
                                break;
                            }
                        }
                    }
                }

                outParents[boneIdx] = parent;
                depthToBone[depth] = static_cast<int32_t>(boneIdx);
            }
        }
        else
        {
            // POP_COUNT MODE - True stack-based hierarchy computation
            //
            // Based on Ghidra RE of GrTrans_PushPopMatrix @ 0x0064ab40
            // The depth value is the number of levels to POP from the matrix stack
            // before pushing the current bone.
            //
            // Algorithm:
            // - Maintain a stack of bone indices
            // - For each bone:
            //   1. Pop depths[i] items from stack
            //   2. Parent = top of stack (or -1 if empty)
            //   3. Push current bone onto stack
            //
            // This creates hierarchies where:
            // - depth=0: chain from previous (push without pop)
            // - depth=N: go back N levels in the tree to find parent
            //
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::PopCount;

            std::vector<int32_t> stack;
            stack.reserve(depths.size());

            for (size_t boneIdx = 0; boneIdx < depths.size(); boneIdx++)
            {
                uint8_t popCount = depths[boneIdx];

                // Pop 'popCount' items from stack
                for (uint8_t p = 0; p < popCount && !stack.empty(); p++)
                {
                    stack.pop_back();
                }

                // Parent is top of stack, or -1 if stack is empty (root)
                int32_t parent = stack.empty() ? -1 : stack.back();
                outParents[boneIdx] = parent;

                // Push current bone onto stack
                stack.push_back(static_cast<int32_t>(boneIdx));
            }
        }
    }
};

/**
 * @brief Finds a chunk in FFNA file data.
 *
 * @param data File data starting from FFNA signature.
 * @param dataSize Total file size.
 * @param targetChunkId Chunk ID to find (e.g., 0xBB9, 0xFA1).
 * @param outOffset Receives the offset to chunk data (after ID and size).
 * @param outSize Receives the chunk size.
 * @return true if chunk was found.
 */
inline bool FindChunk(const uint8_t* data, size_t dataSize, uint32_t targetChunkId,
                      size_t& outOffset, size_t& outSize)
{
    // Start after FFNA signature (4 bytes) and type (1 byte)
    size_t offset = 5;

    while (offset + 8 <= dataSize)
    {
        uint32_t chunkId, chunkSize;
        std::memcpy(&chunkId, &data[offset], sizeof(uint32_t));
        std::memcpy(&chunkSize, &data[offset + 4], sizeof(uint32_t));

        if (chunkId == 0 || chunkSize == 0)
        {
            break;
        }

        if (chunkId == targetChunkId)
        {
            outOffset = offset + 8;  // Skip ID and size
            outSize = chunkSize;
            return true;
        }

        offset += 8 + chunkSize;
    }

    return false;
}

/**
 * @brief Parses animation from an FFNA file.
 *
 * Searches for BB9 or FA1 chunk and parses it.
 *
 * @param fileData Complete FFNA file data.
 * @param fileSize File size in bytes.
 * @return Parsed animation clip, or nullopt on failure.
 */
inline std::optional<Animation::AnimationClip> ParseAnimationFromFile(
    const uint8_t* fileData, size_t fileSize)
{
    // Verify FFNA signature
    if (fileSize < 5 || fileData[0] != 'f' || fileData[1] != 'f' ||
        fileData[2] != 'n' || fileData[3] != 'a')
    {
        return std::nullopt;
    }

    // Try BB9 first (type 2 "other" format) - uses 44-byte header
    size_t chunkOffset, chunkSize;
    if (FindChunk(fileData, fileSize, CHUNK_ID_BB9, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::Parse(&fileData[chunkOffset], chunkSize);
    }

    // Try FA1 (type 2 "standard" format) - uses 88-byte header
    if (FindChunk(fileData, fileSize, CHUNK_ID_FA1, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::ParseFA1(&fileData[chunkOffset], chunkSize);
    }

    return std::nullopt;
}

} // namespace GW::Parsers
