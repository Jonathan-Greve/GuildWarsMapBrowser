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
    float unknown_0x2C;               // 0x2C: Unknown
    uint32_t unknown_0x30;            // 0x30: Unknown
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

                    // Debug: Log bone flags to check for bone IDs
                    if (boneIdx < 10 || (boneHeader.boneFlags >> 8) != 0)
                    {
                        char debug[256];
                        sprintf_s(debug, "Bone %u: flags=0x%08X, depth=%u, upperBytes=0x%06X\n",
                            boneIdx, boneHeader.boneFlags, depth, boneHeader.boneFlags >> 8);
                        // Use inline logging since LogBB8Debug might not be available here
                        static std::ofstream anim_log("bb8_debug.log", std::ios::app);
                        if (anim_log.is_open()) anim_log << debug << std::flush;
                    }

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
                }
            }

            // Compute bone hierarchy from depth values
            bool usedSequentialMode = false;
            ComputeBoneParents(clip.boneParents, boneDepths, &usedSequentialMode);
            clip.useWorldSpaceRotations = usedSequentialMode;

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
        // FA1 format: similar to BB9 but without BoneTransformHeader prefix
        // Bone data consists of: header (22 bytes) + VLE keyframe times + keyframe values
        // We parse until we run out of valid data or hit an error
        if (header.HasSkeleton() && offset < dataSize)
        {
            VLEDecoder decoder(data, dataSize, offset);
            std::vector<uint8_t> boneDepths;
            int errorsInRow = 0;
            uint32_t boneIdx = 0;
            const uint32_t maxBones = 256;  // Safety limit

            // Debug: log FA1 header info
            static std::ofstream fa1_log("fa1_debug.log", std::ios::app);
            if (fa1_log.is_open())
            {
                fa1_log << "=== FA1 Parse: classFlags=0x" << std::hex << header.classFlags
                       << ", seqKeyframes=" << std::dec << header.sequenceKeyframeCount0
                       << ", offset=" << offset << " ===" << std::endl;
            }

            while (decoder.RemainingBytes() >= sizeof(BB9BoneAnimHeader) &&
                   boneIdx < maxBones &&
                   errorsInRow < 3)
            {
                try
                {
                    // Read bone header
                    BB9BoneAnimHeader boneHeader;
                    std::memcpy(&boneHeader, &data[decoder.GetOffset()], sizeof(BB9BoneAnimHeader));

                    // More conservative validation for FA1:
                    // - Key counts should be reasonable (not 0x8000 which indicates misalignment)
                    // - Position values should be reasonable floats
                    bool keyCountsValid = (boneHeader.posKeyCount <= 1000 &&
                                          boneHeader.rotKeyCount <= 1000 &&
                                          boneHeader.scaleKeyCount <= 1000);

                    // Check if position looks like a valid float (not NaN, not huge)
                    bool posValid = std::isfinite(boneHeader.baseX) &&
                                   std::isfinite(boneHeader.baseY) &&
                                   std::isfinite(boneHeader.baseZ) &&
                                   std::abs(boneHeader.baseX) < 100000.0f &&
                                   std::abs(boneHeader.baseY) < 100000.0f &&
                                   std::abs(boneHeader.baseZ) < 100000.0f;

                    if (!keyCountsValid || (!posValid && boneIdx > 0))
                    {
                        if (fa1_log.is_open())
                        {
                            fa1_log << "Bone " << boneIdx << ": Invalid header at offset "
                                   << decoder.GetOffset() << ", stopping. "
                                   << "pos=(" << boneHeader.baseX << "," << boneHeader.baseY << "," << boneHeader.baseZ << "), "
                                   << "keys=(" << boneHeader.posKeyCount << "," << boneHeader.rotKeyCount << "," << boneHeader.scaleKeyCount << ")"
                                   << std::endl;
                        }
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

                    if (fa1_log.is_open())
                    {
                        fa1_log << "Bone " << (boneIdx - 1) << ": parsed OK, pos=("
                               << boneHeader.baseX << "," << boneHeader.baseY << "," << boneHeader.baseZ << "), "
                               << "keys=(" << boneHeader.posKeyCount << "," << boneHeader.rotKeyCount << "," << boneHeader.scaleKeyCount << ")"
                               << std::endl;
                    }
                }
                catch (const std::exception& e)
                {
                    errorsInRow++;
                    if (fa1_log.is_open())
                    {
                        fa1_log << "Bone " << boneIdx << ": Exception - " << e.what() << std::endl;
                    }
                    if (errorsInRow >= 3)
                    {
                        break;
                    }

                    // Try to recover by adding an empty track
                    Animation::BoneTrack emptyTrack;
                    emptyTrack.boneIndex = boneIdx;
                    clip.boneTracks.push_back(std::move(emptyTrack));
                    boneDepths.push_back(0);
                    boneIdx++;
                }
            }

            if (fa1_log.is_open())
            {
                fa1_log << "FA1 Parse complete: " << clip.boneTracks.size() << " bones parsed" << std::endl;
                fa1_log << std::endl;
            }

            // Compute bone hierarchy from depth values
            bool usedSequentialMode = false;
            ComputeBoneParents(clip.boneParents, boneDepths, &usedSequentialMode);
            clip.useWorldSpaceRotations = usedSequentialMode;
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
     */
    static void ComputeBoneParents(std::vector<int32_t>& outParents, const std::vector<uint8_t>& depths,
                                    bool* outUsedSequentialMode = nullptr)
    {
        outParents.resize(depths.size(), -1);
        if (outUsedSequentialMode) *outUsedSequentialMode = false;

        // Debug logging for hierarchy analysis
        static std::ofstream hierarchy_log("hierarchy_debug.log", std::ios::app);

        if (depths.empty()) return;

        // Analyze the depth pattern to detect encoding type
        int zeroCount = 0;
        int maxDepth = 0;
        int increaseBy1Count = 0;  // depth[i] == depth[i-1] + 1 (tree depth pattern)
        int decreaseCount = 0;     // depth[i] < depth[i-1] (branching back)
        int startsWithZeroOne = 0; // First bone is 0, second is 1 (tree depth signature)

        for (size_t i = 0; i < depths.size(); i++)
        {
            uint8_t d = depths[i];
            if (d == 0) zeroCount++;
            if (d > maxDepth) maxDepth = d;

            if (i > 0)
            {
                if (d == depths[i-1] + 1) increaseBy1Count++;
                if (d < depths[i-1]) decreaseCount++;
            }
        }

        if (depths.size() >= 2 && depths[0] == 0 && depths[1] == 1)
        {
            startsWithZeroOne = 1;
        }

        // Detection logic:
        // - Tree depths: start with 0,1 and have many +1 transitions
        // - Pop counts: mostly 0s (child of previous), occasional larger values at branches
        // - World space: all/most values are 0 with no meaningful pattern

        bool looksLikeTreeDepths = (startsWithZeroOne == 1) &&
                                    (increaseBy1Count >= static_cast<int>(depths.size()) * 0.2);

        bool looksLikePopCounts = !looksLikeTreeDepths &&
                                   (zeroCount >= static_cast<int>(depths.size()) * 0.5) &&
                                   (maxDepth > 0);

        bool noHierarchyData = (zeroCount >= static_cast<int>(depths.size()) * 0.95) ||
                                (maxDepth == 0 && depths.size() > 1);

        const char* modeName = noHierarchyData ? "WORLD_SPACE" :
                               (looksLikeTreeDepths ? "TREE_DEPTH" : "POP_COUNT");

        if (hierarchy_log.is_open())
        {
            hierarchy_log << "=== ComputeBoneParents: " << depths.size() << " bones ===" << std::endl;
            hierarchy_log << "Analysis: zeroCount=" << zeroCount
                         << ", maxDepth=" << maxDepth
                         << ", +1transitions=" << increaseBy1Count
                         << ", decreases=" << decreaseCount
                         << ", starts01=" << startsWithZeroOne
                         << " -> mode=" << modeName << std::endl;
            hierarchy_log << "Depths: ";
            for (size_t i = 0; i < depths.size() && i < 50; i++)
            {
                hierarchy_log << (int)depths[i] << " ";
            }
            if (depths.size() > 50) hierarchy_log << "...";
            hierarchy_log << std::endl;
        }

        if (noHierarchyData)
        {
            // WORLD_SPACE MODE: No hierarchy data available
            // Treat all bones as independent with world-space transforms
            if (outUsedSequentialMode) *outUsedSequentialMode = true;

            for (size_t boneIdx = 0; boneIdx < depths.size(); boneIdx++)
            {
                outParents[boneIdx] = -1;
            }
        }
        else if (looksLikeTreeDepths)
        {
            // TREE_DEPTH MODE: depth = absolute level in hierarchy
            // Algorithm: track bones at each depth level, find parent at depth-1
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
            // POP_COUNT MODE: depth = number of levels to pop from matrix stack
            // Based on Ghidra RE of GrTrans_PushPopMatrix
            std::vector<int32_t> stack;

            for (size_t boneIdx = 0; boneIdx < depths.size(); boneIdx++)
            {
                uint8_t popCount = depths[boneIdx];

                // Pop 'popCount' entries from the stack
                for (uint8_t i = 0; i < popCount && !stack.empty(); i++)
                {
                    stack.pop_back();
                }

                // Parent is current top of stack, or -1 if empty (root)
                outParents[boneIdx] = stack.empty() ? -1 : stack.back();

                // Push current bone onto stack
                stack.push_back(static_cast<int32_t>(boneIdx));
            }
        }

        // Count and log disconnected bones
        if (hierarchy_log.is_open())
        {
            int disconnected = 0;
            for (size_t i = 1; i < outParents.size(); i++)
            {
                if (outParents[i] == -1) disconnected++;
            }
            hierarchy_log << "Result: " << disconnected << " disconnected bones (excl root)" << std::endl;
            hierarchy_log << "Hierarchy: ";
            for (size_t i = 0; i < outParents.size() && i < 50; i++)
            {
                hierarchy_log << i << "->" << outParents[i] << " ";
            }
            if (outParents.size() > 50) hierarchy_log << "...";
            hierarchy_log << std::endl << std::endl;
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
