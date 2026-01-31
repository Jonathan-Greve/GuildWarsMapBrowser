#pragma once

#include "VLEDecoder.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Skeleton.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <optional>
#include <fstream>
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
 * Parent encoding (discovered via analysis of 0x3AAA):
 * - Low byte contains (parent_index + 1) where:
 *   - 0 = root bone (no parent)
 *   - 1 = parent is bone 0
 *   - 2 = parent is bone 1
 *   - N = parent is bone (N-1)
 * - High bits (0x10000000 etc) are format flags, not parent info
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
     * The low byte contains (parent_index + 1), so:
     * - lowByte 0 = root (no parent) = return -1
     * - lowByte 1 = parent is bone 0 = return 0
     * - lowByte N = parent is bone (N-1) = return N-1
     *
     * @param boneIndex The index of this bone (unused, kept for compatibility).
     * @return Parent bone index, or -1 for root.
     */
    int32_t GetParentIndex(size_t boneIndex) const
    {
        // First bone is always root regardless of parentInfo
        if (boneIndex == 0)
        {
            return -1;
        }

        // Low byte contains (parent_index + 1)
        // 0 = root, 1 = parent is bone 0, 2 = parent is bone 1, etc.
        uint8_t lowByte = static_cast<uint8_t>(parentInfo & 0xFF);

        if (lowByte == 0)
        {
            // This bone branches back to root
            return -1;
        }
        else
        {
            // Parent is bone (lowByte - 1)
            return static_cast<int32_t>(lowByte - 1);
        }
    }
};
#pragma pack(pop)

static_assert(sizeof(FA1BindPoseEntry) == 16, "FA1BindPoseEntry must be 16 bytes!");

/**
 * @brief FA1 keyframe header structure (16 bytes).
 *
 * Found in FA1 chunks after bind pose entries. The FA1 format stores animation
 * data in a random-access layout optimized for runtime:
 *
 *   1. This header (16 bytes) - total keyframe counts
 *   2. Four offset arrays (4 × boneCount × 4 bytes):
 *      - posTimesOffsets[boneCount]   - BIT offsets to position time data
 *      - posValuesOffsets[boneCount]  - BIT offsets to position value data
 *      - rotTimesOffsets[boneCount]   - BIT offsets to rotation time data
 *      - rotValuesOffsets[boneCount]  - BIT offsets to rotation value data
 *   3. Combined VLE stream containing all keyframe data
 *
 * The BIT offsets allow random access to any bone's animation data without
 * sequential decoding. This is in contrast to BB9 which stores per-bone
 * headers inline with data (sequential access required).
 */
#pragma pack(push, 1)
struct FA1KeyframeHeader
{
    uint16_t reserved0;         // 0x00: Always 0
    uint16_t reserved1;         // 0x02: Always 0
    uint16_t reserved2;         // 0x04: Always 0
    uint16_t positionKeyCount;  // 0x06: Total position keyframes across all bones
    uint16_t rotationKeyCount;  // 0x08: Total rotation keyframes across all bones
    uint16_t reserved3;         // 0x0A: Always 0
    uint16_t timeScaleLow;      // 0x0C: Time scale/duration (low 16 bits). NOT a keyframe count!
    uint16_t timeScaleHigh;     // 0x0E: Time scale/duration (high 16 bits). Usually 0.
    // Total: 16 bytes

    /**
     * @brief Gets the time scale value for normalizing keyframe times.
     *
     * This 32-bit value at offset 0x0C appears to be related to animation duration
     * or time scale. Examples: 3333 for 0x3AAA, 30000 for 0x47D9.
     * Use this to normalize decoded time values if needed.
     */
    uint32_t GetTimeScale() const
    {
        return static_cast<uint32_t>(timeScaleLow) |
               (static_cast<uint32_t>(timeScaleHigh) << 16);
    }

    /**
     * @brief Checks if this looks like a valid FA1 keyframe header.
     *
     * Valid headers have zeros in the reserved fields and reasonable key counts.
     * Note: timeScaleLow (offset 0x0C) is NOT a keyframe count - it's a time scale
     * or duration value (e.g., 3333 for 0x3AAA, 30000 for 0x47D9). Do NOT validate it.
     */
    bool IsValid() const
    {
        // Check reserved fields are zero and key counts are reasonable
        // timeScaleLow is NOT validated - it's a time scale value, not a keyframe count
        return reserved0 == 0 && reserved1 == 0 && reserved2 == 0 &&
               reserved3 == 0 && timeScaleHigh == 0 &&
               positionKeyCount < 50000 && rotationKeyCount < 50000 &&
               (positionKeyCount > 0 || rotationKeyCount > 0);
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

        // Calculate offset to bind pose data: header + bounding cylinders + sequence entries
        // FA1 format: Header(88) + BoundingCylinders(N*16) + SequenceEntries(M*24) + BindPose
        size_t bindPoseOffset = sizeof(FA1Header) + (header.boundingCylinderCount * 16);

        // Skip sequence entries (transformDataSize * 24 bytes each)
        if (header.transformDataSize > 0 && header.transformDataSize < 256)
        {
            bindPoseOffset += header.transformDataSize * sizeof(BB9SequenceEntry);
        }

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
        clip.sourceChunkType = "BB9";
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

        // Debug output for BB9 - same anomaly detection as FA1
        {
            std::ofstream debugLog("bb9_parse_debug.log", std::ios::app);
            if (debugLog.is_open())
            {
                debugLog << "\n========================================" << std::endl;
                debugLog << "=== BB9 Parse Complete ===" << std::endl;
                debugLog << "Bones: " << clip.boneTracks.size() << std::endl;
                debugLog << "Sequences: " << clip.sequences.size() << std::endl;
                debugLog << "TimeRange: " << clip.minTime << " - " << clip.maxTime << std::endl;

                // Log ALL sequences with their time ranges
                debugLog << "\n=== SEQUENCE DETAILS ===" << std::endl;
                for (size_t i = 0; i < clip.sequences.size(); i++)
                {
                    const auto& seq = clip.sequences[i];
                    debugLog << "Sequence " << i << ": hash=0x" << std::hex << seq.hash << std::dec
                             << " frames=" << seq.frameCount
                             << " time=[" << seq.startTime << " - " << seq.endTime << "]"
                             << " duration=" << (seq.endTime - seq.startTime) << std::endl;
                }

                // DETAILED ANALYSIS: Find keyframes with large position jumps
                debugLog << "\n=== ANOMALY DETECTION: Large position jumps ===" << std::endl;
                for (size_t boneIdx = 0; boneIdx < clip.boneTracks.size(); boneIdx++)
                {
                    const auto& track = clip.boneTracks[boneIdx];
                    for (size_t k = 1; k < track.positionKeys.size(); k++)
                    {
                        const auto& prev = track.positionKeys[k-1];
                        const auto& curr = track.positionKeys[k];
                        float dx = curr.value.x - prev.value.x;
                        float dy = curr.value.y - prev.value.y;
                        float dz = curr.value.z - prev.value.z;
                        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist > 50.0f)
                        {
                            debugLog << "  LARGE JUMP Bone " << boneIdx << " key " << (k-1) << "->" << k
                                     << ": t=" << prev.time << "->" << curr.time
                                     << " dist=" << dist
                                     << " pos=(" << prev.value.x << "," << prev.value.y << "," << prev.value.z << ")"
                                     << "->(" << curr.value.x << "," << curr.value.y << "," << curr.value.z << ")"
                                     << std::endl;
                        }
                    }
                }

                // DETAILED ANALYSIS: Find keyframes with large rotation changes
                debugLog << "\n=== ANOMALY DETECTION: Large rotation changes ===" << std::endl;
                for (size_t boneIdx = 0; boneIdx < clip.boneTracks.size(); boneIdx++)
                {
                    const auto& track = clip.boneTracks[boneIdx];
                    for (size_t k = 1; k < track.rotationKeys.size(); k++)
                    {
                        const auto& prev = track.rotationKeys[k-1];
                        const auto& curr = track.rotationKeys[k];
                        float dot = prev.value.x * curr.value.x + prev.value.y * curr.value.y +
                                    prev.value.z * curr.value.z + prev.value.w * curr.value.w;
                        float angle = std::acos(std::min(1.0f, std::abs(dot))) * 2.0f * 57.2958f;
                        if (angle > 90.0f)
                        {
                            debugLog << "  LARGE ROT Bone " << boneIdx << " key " << (k-1) << "->" << k
                                     << ": t=" << prev.time << "->" << curr.time
                                     << " angle=" << angle << " deg"
                                     << " quat=(" << prev.value.x << "," << prev.value.y << "," << prev.value.z << "," << prev.value.w << ")"
                                     << "->(" << curr.value.x << "," << curr.value.y << "," << curr.value.z << "," << curr.value.w << ")"
                                     << std::endl;
                        }
                    }
                }
                debugLog << std::endl;
            }
        }

        return clip;
    }

private:
    /**
     * @brief Bit-stream reader for FA1 VLE data.
     *
     * FA1 uses BIT offsets (not byte-aligned) into the VLE stream, so we need
     * a reader that can extract bytes at arbitrary bit positions.
     */
    struct FA1BitStreamReader
    {
        const uint8_t* data;
        size_t dataSize;
        size_t bitOffset;

        FA1BitStreamReader(const uint8_t* inData, size_t inSize, size_t startBitOffset = 0)
            : data(inData), dataSize(inSize), bitOffset(startBitOffset)
        {
        }

        void SetBitOffset(size_t offset) { bitOffset = offset; }
        size_t GetBitOffset() const { return bitOffset; }

        bool HasBits(size_t numBits) const
        {
            return (bitOffset + numBits) <= (dataSize * 8);
        }

        /**
         * @brief Reads a byte from the current bit position (may be non-aligned).
         */
        uint8_t ReadByte()
        {
            if (!HasBits(8))
            {
                return 0;
            }

            size_t byteIndex = bitOffset / 8;
            uint8_t bitShift = static_cast<uint8_t>(bitOffset % 8);

            uint8_t b0 = (byteIndex < dataSize) ? data[byteIndex] : 0;
            uint8_t result;

            if (bitShift == 0)
            {
                result = b0;
            }
            else
            {
                uint8_t b1 = (byteIndex + 1 < dataSize) ? data[byteIndex + 1] : 0;
                result = static_cast<uint8_t>((b0 >> bitShift) | (b1 << (8 - bitShift)));
            }

            bitOffset += 8;
            return result;
        }

        /**
         * @brief Reads a VLE-encoded value (1-5 bytes, same encoding as BB9).
         *
         * VLE format:
         * - Byte 0: bits 0-5 = value, bit 6 = sign, bit 7 = continuation
         * - Bytes 1-4: bits 0-6 = value, bit 7 = continuation
         */
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

    /**
     * @brief Parses FA1-specific keyframe format.
     *
     * FA1 uses RAW data (not VLE encoded like BB9):
     *
     * Per-bone format:
     *   [6-byte header: posCount|rotCount|scaleCount as uint16]
     *   [posCount × uint32 timestamps]
     *   [posCount × float3 positions]
     *   [rotCount × uint32 timestamps]
     *   [rotCount × float4 quaternions (XYZW)]
     *   [scaleCount × uint32 timestamps]
     *   [scaleCount × float3 scales]
     *
     * Bones are stored sequentially, one after another.
     *
     * @param data Pointer to chunk data.
     * @param dataSize Size of the chunk data.
     * @param bindPoseOffset Offset to bind pose entries (for reading base positions).
     * @param keyframeOffset Current offset (start of per-bone animation data).
     * @param boneCount Number of bones.
     * @param clip Output animation clip.
     * @param boneDepths Output vector for hierarchy depths.
     * @return true if parsing succeeded, false on failure.
     */
    static bool ParseFA1KeyframeFormat(const uint8_t* data, size_t dataSize,
                                        size_t bindPoseOffset, size_t animOffset,
                                        uint32_t boneCount, Animation::AnimationClip& clip,
                                        std::vector<uint8_t>& boneDepths)
    {
        // FA1 uses RAW data format (not VLE like BB9):
        // Per-bone: [6-byte header: posCount|rotCount|scaleCount]
        //           [posCount × uint32 timestamps]
        //           [posCount × float3 positions]
        //           [rotCount × uint32 timestamps]
        //           [rotCount × float4 quaternions XYZW]
        //           [scaleCount × uint32 timestamps]
        //           [scaleCount × float3 scales]

        // First read bind pose entries to build bone tracks and hierarchy
        clip.boneParents.clear();
        clip.boneParents.reserve(boneCount);
        clip.boneTracks.clear();
        clip.boneTracks.reserve(boneCount);
        boneDepths.clear();
        boneDepths.reserve(boneCount);
        clip.boneIsIntermediate.clear();

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

            // FA1 stores explicit parent indices: low byte = (parent + 1), 0 = root
            int32_t parentIdx = bindPose.GetParentIndex(i);
            clip.boneParents.push_back(parentIdx);

            // Store hierarchy byte for compatibility
            uint8_t hierByte = static_cast<uint8_t>(bindPose.parentInfo & 0xFF);
            boneDepths.push_back(hierByte);

            clip.boneIsIntermediate.push_back(false);
        }

        // Debug logging
        {
            std::ofstream debugLog("fa1_parse_debug.log", std::ios::app);
            if (debugLog.is_open())
            {
                debugLog << "\n========================================" << std::endl;
                debugLog << "=== FA1 RAW Keyframe Parsing ===" << std::endl;
                debugLog << "Format: Per-bone 6-byte header + RAW uint32 times + RAW float data" << std::endl;
                debugLog << "BoneCount: " << boneCount << std::endl;
                debugLog << "BindPoseOffset: 0x" << std::hex << bindPoseOffset << std::dec << std::endl;
                debugLog << "AnimOffset: 0x" << std::hex << animOffset << std::dec << std::endl;
            }
        }

        // Helper to transform quaternion from GW to GWMB coordinates
        auto processQuaternion = [](float qx, float qy, float qz, float qw,
                                    const std::vector<XMFLOAT4>& prevQuats) -> XMFLOAT4 {
            // FA1 quaternion to BB9/GWMB coordinate transform
            // Comparing BB9 vs FA1 for Bone 31:
            //   BB9: (0.00609624, 0.0828345, 0.799916, 0.594337)
            //   Raw FA1: qx=-0.00606, qy=-0.7999, qz=0.08287, qw=0.5943
            // So: GWMB_x = -FA1_x, GWMB_y = FA1_z, GWMB_z = -FA1_y, GWMB_w = FA1_w
            XMFLOAT4 quat;
            quat.x = -qx;
            quat.y = qz;   // NOT negated - raw FA1 qz matches BB9 y
            quat.z = -qy;
            quat.w = qw;

            // Normalize
            float len = std::sqrt(quat.x*quat.x + quat.y*quat.y + quat.z*quat.z + quat.w*quat.w);
            if (len > 0.0001f)
            {
                quat.x /= len;
                quat.y /= len;
                quat.z /= len;
                quat.w /= len;
            }

            // Ensure quaternion continuity
            if (!prevQuats.empty())
            {
                const XMFLOAT4& prev = prevQuats.back();
                float dot = quat.w * prev.w + quat.x * prev.x + quat.y * prev.y + quat.z * prev.z;
                if (dot < 0.0f)
                {
                    quat.w = -quat.w;
                    quat.x = -quat.x;
                    quat.y = -quat.y;
                    quat.z = -quat.z;
                }
            }

            return quat;
        };

        // Parse per-bone animation data
        size_t offset = animOffset;
        bool success = true;

        for (uint32_t boneIdx = 0; boneIdx < boneCount && success; boneIdx++)
        {
            // Read 6-byte per-bone header
            if (offset + 6 > dataSize)
            {
                success = false;
                break;
            }

            uint16_t posCount, rotCount, scaleCount;
            std::memcpy(&posCount, &data[offset + 0], sizeof(uint16_t));
            std::memcpy(&rotCount, &data[offset + 2], sizeof(uint16_t));
            std::memcpy(&scaleCount, &data[offset + 4], sizeof(uint16_t));

            // Validate counts
            if (posCount > 10000 || rotCount > 10000 || scaleCount > 10000)
            {
                success = false;
                break;
            }

            // Calculate offsets for each section
            size_t posTimesOff = offset + 6;
            size_t posValsOff = posTimesOff + posCount * sizeof(uint32_t);
            size_t rotTimesOff = posValsOff + posCount * sizeof(float) * 3;
            size_t rotValsOff = rotTimesOff + rotCount * sizeof(uint32_t);
            size_t scaleTimesOff = rotValsOff + rotCount * sizeof(float) * 4;
            size_t scaleValsOff = scaleTimesOff + scaleCount * sizeof(uint32_t);
            size_t nextOffset = scaleValsOff + scaleCount * sizeof(float) * 3;

            if (nextOffset > dataSize)
            {
                success = false;
                break;
            }

            Animation::BoneTrack& track = clip.boneTracks[boneIdx];
            track.positionKeys.clear();
            track.rotationKeys.clear();
            track.scaleKeys.clear();

            // Read position keyframes (raw uint32 timestamps + raw float3 values)
            if (posCount > 0)
            {
                track.positionKeys.reserve(posCount);
                for (uint32_t k = 0; k < posCount; k++)
                {
                    uint32_t ts;
                    float px, py, pz;
                    std::memcpy(&ts, &data[posTimesOff + k * sizeof(uint32_t)], sizeof(uint32_t));
                    std::memcpy(&px, &data[posValsOff + k * 12 + 0], sizeof(float));
                    std::memcpy(&py, &data[posValsOff + k * 12 + 4], sizeof(float));
                    std::memcpy(&pz, &data[posValsOff + k * 12 + 8], sizeof(float));

                    // FA1 stores positions as (x, z, -y) relative to BB9's (x, y, z)
                    // Convert to BB9 terms first: BB9_x=px, BB9_y=-pz, BB9_z=py
                    // Then apply GWMB transform (x, -z, y): (px, -py, -pz)
                    XMFLOAT3 pos = { px, -py, -pz };
                    track.positionKeys.push_back({ static_cast<float>(ts), pos });
                }
            }

            // Read rotation keyframes (raw uint32 timestamps + raw float4 quaternions XYZW)
            if (rotCount > 0)
            {
                track.rotationKeys.reserve(rotCount);
                std::vector<XMFLOAT4> prevQuats;
                prevQuats.reserve(rotCount);

                for (uint32_t k = 0; k < rotCount; k++)
                {
                    uint32_t ts;
                    float qx, qy, qz, qw;
                    std::memcpy(&ts, &data[rotTimesOff + k * sizeof(uint32_t)], sizeof(uint32_t));
                    std::memcpy(&qx, &data[rotValsOff + k * 16 + 0], sizeof(float));
                    std::memcpy(&qy, &data[rotValsOff + k * 16 + 4], sizeof(float));
                    std::memcpy(&qz, &data[rotValsOff + k * 16 + 8], sizeof(float));
                    std::memcpy(&qw, &data[rotValsOff + k * 16 + 12], sizeof(float));

                    XMFLOAT4 quat = processQuaternion(qx, qy, qz, qw, prevQuats);
                    prevQuats.push_back(quat);
                    track.rotationKeys.push_back({ static_cast<float>(ts), quat });
                }
            }

            // Read scale keyframes (raw uint32 timestamps + raw float3 values)
            if (scaleCount > 0)
            {
                track.scaleKeys.reserve(scaleCount);
                for (uint32_t k = 0; k < scaleCount; k++)
                {
                    uint32_t ts;
                    float sx, sy, sz;
                    std::memcpy(&ts, &data[scaleTimesOff + k * sizeof(uint32_t)], sizeof(uint32_t));
                    std::memcpy(&sx, &data[scaleValsOff + k * 12 + 0], sizeof(float));
                    std::memcpy(&sy, &data[scaleValsOff + k * 12 + 4], sizeof(float));
                    std::memcpy(&sz, &data[scaleValsOff + k * 12 + 8], sizeof(float));

                    XMFLOAT3 scale = { sx, sy, sz };
                    track.scaleKeys.push_back({ static_cast<float>(ts), scale });
                }
            }

            // If no keyframes, add identity
            if (track.positionKeys.empty())
            {
                track.positionKeys.push_back({0.0f, {0.0f, 0.0f, 0.0f}});
            }
            if (track.rotationKeys.empty())
            {
                track.rotationKeys.push_back({0.0f, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)});
            }

            offset = nextOffset;
        }

        // Debug logging
        {
            std::ofstream debugLog("fa1_parse_debug.log", std::ios::app);
            if (debugLog.is_open())
            {
                debugLog << "Parse success: " << (success ? "YES" : "PARTIAL") << std::endl;
                if (!clip.boneTracks.empty())
                {
                    debugLog << "Parsed " << clip.boneTracks.size() << " bones" << std::endl;
                    debugLog << "Sample bone 0: posKeys=" << clip.boneTracks[0].positionKeys.size()
                             << ", rotKeys=" << clip.boneTracks[0].rotationKeys.size() << std::endl;
                    if (!clip.boneTracks[0].positionKeys.empty())
                    {
                        const auto& key = clip.boneTracks[0].positionKeys[0];
                        debugLog << "  First pos key: t=" << key.time
                                 << " pos=(" << key.value.x << ", " << key.value.y << ", " << key.value.z << ")" << std::endl;
                    }
                    if (!clip.boneTracks[0].rotationKeys.empty())
                    {
                        const auto& key = clip.boneTracks[0].rotationKeys[0];
                        debugLog << "  First rot key: t=" << key.time
                                 << " quat=(" << key.value.x << ", " << key.value.y << ", "
                                 << key.value.z << ", " << key.value.w << ")" << std::endl;
                    }
                }
            }
        }

        // Return true if we have any bone data (even partial parse is usable)
        return !clip.boneTracks.empty();
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
        clip.sourceChunkType = "FA1";
        clip.modelHash0 = header.boundingBoxId;
        clip.modelHash1 = header.collisionMeshId;

        // Get geometry scale from FA1 header
        float headerScale = header.geometryScale;
        bool needsAutoScale = !(headerScale > 0.001f && headerScale < 100.0f);
        clip.geometryScale = needsAutoScale ? 1.0f : headerScale;

        size_t offset = sizeof(FA1Header);

        // Skip bounding cylinders (16 bytes each)
        offset += header.boundingCylinderCount * 16;

        // FA1 may have sequence data - check transformDataSize, NOT classFlags!
        // Some FA1 files (e.g., 0x47D9) have sequences but NO HAS_ANIMATION_SEQUENCES flag
        // IMPORTANT: Use transformDataSize for sequence count, NOT sequenceCount0!
        // transformDataSize = actual number of 24-byte sequence entries
        // sequenceCount0 is something else (possibly total keyframe count or unused)
        if (header.transformDataSize > 0 && header.transformDataSize < 256)
        {
            for (uint32_t i = 0; i < header.transformDataSize; i++)
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
        // FA1 format: bind pose entries (16 bytes each) followed by per-bone RAW animation data
        // Per-bone: [6-byte header: posCount|rotCount|scaleCount] + raw timestamps + raw values
        bool hasSkeletonData = header.HasSkeleton() || (header.bindPoseBoneCount > 0 && header.bindPoseBoneCount < 256);
        if (hasSkeletonData && offset < dataSize && header.bindPoseBoneCount > 0)
        {
            // Bind pose entries start here
            size_t bindPoseOffset = offset;

            // Keyframe data starts after bind pose entries
            size_t keyframeOffset = bindPoseOffset + header.bindPoseBoneCount * sizeof(FA1BindPoseEntry);

            if (keyframeOffset < dataSize)
            {
                std::vector<uint8_t> boneDepths;

                // Parse FA1 keyframe format (raw data, not VLE encoded)
                if (ParseFA1KeyframeFormat(data, dataSize, bindPoseOffset, keyframeOffset,
                                           header.bindPoseBoneCount, clip, boneDepths))
                {
                    // FA1 format uses DIRECT parent indexing: parent = (lowByte - 1)
                    clip.hierarchyMode = Animation::HierarchyMode::DirectParent;
                }
                else
                {
                    // Parsing failed - return nullopt
                    return std::nullopt;
                }

                // Build output-to-animation bone mapping (for intermediate bone handling)
                clip.BuildOutputMapping();
            }
        }

        // Compute time ranges
        clip.ComputeTimeRange();
        clip.ComputeSequenceTimeRanges();

        // Debug output to verify FA1 parsing
        {
            std::ofstream debugLog("fa1_parse_debug.log", std::ios::app);
            if (debugLog.is_open())
            {
                debugLog << "=== FA1 ParseFA1 Complete (RAW data format) ===" << std::endl;
                debugLog << "HierarchyMode: DirectParent" << std::endl;
                debugLog << "Bones: " << clip.boneTracks.size() << std::endl;
                debugLog << "Sequences: " << clip.sequences.size() << std::endl;
                debugLog << "TimeRange: " << clip.minTime << " - " << clip.maxTime << std::endl;
                debugLog << "Duration (raw units): " << clip.duration << std::endl;
                debugLog << "Duration (approx seconds): " << (clip.duration / 33330.0f) << std::endl;
                debugLog << "TotalFrames: " << clip.totalFrames << std::endl;

                // Log ALL sequences with their time ranges
                debugLog << "\n=== SEQUENCE DETAILS ===" << std::endl;
                for (size_t i = 0; i < clip.sequences.size(); i++)
                {
                    const auto& seq = clip.sequences[i];
                    debugLog << "Sequence " << i << ": hash=0x" << std::hex << seq.hash << std::dec
                             << " frames=" << seq.frameCount
                             << " time=[" << seq.startTime << " - " << seq.endTime << "]"
                             << " duration=" << (seq.endTime - seq.startTime) << std::endl;
                }

                // Log first 10 bone parents
                debugLog << "\nParent indices (first 10): ";
                for (size_t i = 0; i < std::min(clip.boneParents.size(), size_t(10)); i++)
                {
                    debugLog << clip.boneParents[i] << " ";
                }
                debugLog << std::endl;

                // DETAILED ANALYSIS: Find keyframes with large position jumps (potential head flying away)
                debugLog << "\n=== ANOMALY DETECTION: Large position jumps ===" << std::endl;
                for (size_t boneIdx = 0; boneIdx < clip.boneTracks.size(); boneIdx++)
                {
                    const auto& track = clip.boneTracks[boneIdx];
                    for (size_t k = 1; k < track.positionKeys.size(); k++)
                    {
                        const auto& prev = track.positionKeys[k-1];
                        const auto& curr = track.positionKeys[k];
                        float dx = curr.value.x - prev.value.x;
                        float dy = curr.value.y - prev.value.y;
                        float dz = curr.value.z - prev.value.z;
                        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                        // Flag jumps > 50 units (likely anomalous for character animation)
                        if (dist > 50.0f)
                        {
                            debugLog << "  LARGE JUMP Bone " << boneIdx << " key " << (k-1) << "->" << k
                                     << ": t=" << prev.time << "->" << curr.time
                                     << " dist=" << dist
                                     << " pos=(" << prev.value.x << "," << prev.value.y << "," << prev.value.z << ")"
                                     << "->(" << curr.value.x << "," << curr.value.y << "," << curr.value.z << ")"
                                     << std::endl;
                        }
                    }
                }

                // DETAILED ANALYSIS: Find keyframes with large rotation changes
                debugLog << "\n=== ANOMALY DETECTION: Large rotation changes ===" << std::endl;
                for (size_t boneIdx = 0; boneIdx < clip.boneTracks.size(); boneIdx++)
                {
                    const auto& track = clip.boneTracks[boneIdx];
                    for (size_t k = 1; k < track.rotationKeys.size(); k++)
                    {
                        const auto& prev = track.rotationKeys[k-1];
                        const auto& curr = track.rotationKeys[k];
                        // Dot product of quaternions - close to 1 means similar, close to 0 means 90 deg, negative means >90 deg
                        float dot = prev.value.x * curr.value.x + prev.value.y * curr.value.y +
                                    prev.value.z * curr.value.z + prev.value.w * curr.value.w;
                        // Angle in degrees (approximate)
                        float angle = std::acos(std::min(1.0f, std::abs(dot))) * 2.0f * 57.2958f;
                        // Flag rotations > 90 degrees between consecutive keys
                        if (angle > 90.0f)
                        {
                            debugLog << "  LARGE ROT Bone " << boneIdx << " key " << (k-1) << "->" << k
                                     << ": t=" << prev.time << "->" << curr.time
                                     << " angle=" << angle << " deg"
                                     << " quat=(" << prev.value.x << "," << prev.value.y << "," << prev.value.z << "," << prev.value.w << ")"
                                     << "->(" << curr.value.x << "," << curr.value.y << "," << curr.value.z << "," << curr.value.w << ")"
                                     << std::endl;
                        }
                    }
                }

                // Log first 5 bone positions
                debugLog << "\nBone positions (first 5):" << std::endl;
                for (size_t i = 0; i < std::min(clip.boneTracks.size(), size_t(5)); i++)
                {
                    const auto& pos = clip.boneTracks[i].basePosition;
                    debugLog << "  Bone " << i << ": (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                }

                // Log keyframe counts and first few time values for first 3 bones
                debugLog << "Keyframe details (first 3 bones):" << std::endl;
                for (size_t i = 0; i < std::min(clip.boneTracks.size(), size_t(3)); i++)
                {
                    const auto& track = clip.boneTracks[i];
                    debugLog << "  Bone " << i << ": pos=" << track.positionKeys.size()
                             << ", rot=" << track.rotationKeys.size();

                    // Log first 5 position times
                    if (!track.positionKeys.empty())
                    {
                        debugLog << "  posTimes=[";
                        for (size_t j = 0; j < std::min(track.positionKeys.size(), size_t(5)); j++)
                        {
                            if (j > 0) debugLog << ", ";
                            debugLog << track.positionKeys[j].time;
                        }
                        if (track.positionKeys.size() > 5) debugLog << ", ...";
                        debugLog << "]";
                    }
                    debugLog << std::endl;
                }

                debugLog << std::endl;
            }
        }

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
