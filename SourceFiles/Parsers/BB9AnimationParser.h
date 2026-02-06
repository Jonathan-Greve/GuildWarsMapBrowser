#pragma once

#include "VLEDecoder.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Skeleton.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <optional>
#include <unordered_map>
#include <DirectXMath.h>

using namespace DirectX;

namespace GW::Parsers {

// Animation chunk IDs
// BB9/BB8/FA1/FA0 hierarchy mode is chosen deterministically from hierarchy-byte
// stream validity (no offset scanning / no header-flag guesswork).
constexpr uint32_t CHUNK_ID_BB9 = 0x00000BB9;  // Animation chunk (44-byte header)
constexpr uint32_t CHUNK_ID_BB8 = 0x00000BB8;  // Animation chunk variant (44-byte header)
constexpr uint32_t CHUNK_ID_FA1 = 0x00000FA1;  // Animation chunk (88-byte header)
constexpr uint32_t CHUNK_ID_FA0 = 0x00000FA0;  // Animation chunk variant (88-byte header)
// Note: FA6 (0x00000FA6) is a FILE REFERENCE chunk (equivalent of BBC in FA format)
// It contains additional filename references (including Type 8 sound event files).
// See FileReferenceParser.h and animation_state.cpp for FA6 file reference handling.
// DO NOT use FA6 for animation parsing - it's not an animation chunk!

// BB9 Header flags
constexpr uint32_t BB9_FLAG_HAS_SEQUENCES = 0x0008;         // Animation sequences present
constexpr uint32_t BB9_FLAG_HAS_BONE_TRANSFORMS = 0x0010;   // Bone transform data present
constexpr uint32_t BB9_FLAG_HAS_PHASE_TIMING = 0x0100;      // Phase timing data present (between bone transforms and segments)

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
    uint32_t animationSegmentCount;  // 0x18: Number of animation segment entries (22 bytes each)
    uint32_t reserved[4];             // 0x1C-0x2B: Reserved/unknown fields (4 x 4 = 16 bytes)
    // Total: 7 fields (28) + reserved (16) = 44 (0x2C) bytes

    bool HasSequences() const { return (flags & BB9_FLAG_HAS_SEQUENCES) != 0; }
    bool HasBoneTransforms() const { return (flags & BB9_FLAG_HAS_BONE_TRANSFORMS) != 0; }
    bool HasPhaseTiming() const { return (flags & BB9_FLAG_HAS_PHASE_TIMING) != 0; }
    bool HasAnimationSegments() const { return animationSegmentCount > 0 && animationSegmentCount < 500; }
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
    uint16_t animationSegmentCount;   // 0x18: Number of animation segment entries (23 bytes each) - FA1 uses padding
    uint16_t reserved_0x1A;           // 0x1A: Reserved/unused
    uint32_t unknown_0x1C;            // 0x1C: Unknown
    float geometryScale;              // 0x20: Skeleton/geometry scale factor. If <=0, computed from bounding data
    uint32_t hierarchyFlags;          // 0x24: Unknown/reserved in FA1 files (not a reliable hierarchy-mode flag)
    float unknown_0x28;               // 0x28: Unknown (often 1.0)
    uint32_t bindPoseBoneCount;       // 0x2C: Number of bones in bind pose section (discovered via RE)
    uint32_t unknown_0x30;            // 0x30: Unknown (often 3)
    uint32_t transformDataSize;       // 0x34: Transform data size
    uint32_t submeshCount;            // 0x38: Submesh count
    // 0x3C: low 16 bits = phase/event count used by FA1 tail block.
    // Layout after section 0x38 is: u32 phaseTimes[count] + u8 phaseFlags[count].
    uint32_t unknown_0x3C;
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
    bool HasAnimationSegments() const { return animationSegmentCount > 0 && animationSegmentCount < 500; }
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
 * @brief BB9 animation segment entry structure (22 bytes = 0x16).
 *
 * Runtime mapping in Gw.exe (MdlLoad.cpp):
 * - hash (u32): animation segment identifier
 * - startTime/endTime (u32/u32): playback window
 * - phaseStartIndex/phaseEndIndex (u8/u8): index range into the phase timing table
 * - loopStartOffset (u32): offset from startTime used for loop wrap
 * - transitionParam (f32): unknown runtime parameter copied into segment state
 */
#pragma pack(push, 1)
struct BB9AnimationSegmentEntry
{
    uint32_t hash;              // 0x00: Animation segment identifier
    uint32_t startTime;         // 0x04: Start time in animation units (100000 = 1 sec)
    uint32_t endTime;           // 0x08: End time in animation units
    uint8_t phaseStartIndex;    // 0x0C: Start phase index
    uint8_t phaseEndIndex;      // 0x0D: End phase index (exclusive)
    uint32_t loopStartOffset;   // 0x0E: Loop start offset from startTime
    float transitionParam;      // 0x12: Unknown runtime value
    // Total: 22 bytes (0x16)
};
#pragma pack(pop)

static_assert(sizeof(BB9AnimationSegmentEntry) == 22, "BB9AnimationSegmentEntry must be 22 bytes!");

/**
 * @brief FA1 animation segment entry structure (23 bytes).
 *
 * Same logical fields as BB9, plus one leading byte.
 */
#pragma pack(push, 1)
struct FA1AnimationSegmentEntry
{
    uint8_t segmentType;        // 0x00: Source selector (0=local clip, >0=external referenced source)
    uint32_t hash;              // 0x01: Animation segment identifier
    uint32_t startTime;         // 0x05: Start time in animation units (100000 = 1 sec)
    uint32_t endTime;           // 0x09: End time in animation units
    uint8_t phaseStartIndex;    // 0x0D: Start phase index
    uint8_t phaseEndIndex;      // 0x0E: End phase index (exclusive)
    uint32_t loopStartOffset;   // 0x0F: Loop start offset from startTime
    float transitionParam;      // 0x13: Unknown runtime value
    // Total: 23 bytes (0x17)
};
#pragma pack(pop)

static_assert(sizeof(FA1AnimationSegmentEntry) == 23, "FA1AnimationSegmentEntry must be 23 bytes!");

/**
 * @brief FA1 bind pose entry structure (16 bytes per bone).
 *
 * Found in FA1 chunks after the header + bounding cylinders.
 * Contains bind pose position and hierarchy information.
 *
 * Hierarchy encoding (low byte of parentInfo):
 * - External FA1 animation files commonly use direct-parent encoding (0=root, n>0 => parent=n-1)
 * - Some embedded/model FA1 variants use pop-count encoding (levels to pop from matrix stack)
 *
 * The parser selects FA1 mode deterministically from stream validity
 * (no offset scanning and no unreliable bitflag dependency).
 *
 * High bits (0x10000000) indicate intermediate bones that don't produce
 * output skinning matrices.
 */
#pragma pack(push, 1)
struct FA1BindPoseEntry
{
    float posX;             // 0x00: Bind pose X position
    float posY;             // 0x04: Bind pose Y position
    float posZ;             // 0x08: Bind pose Z position
    uint32_t parentInfo;    // 0x0C: Hierarchy byte (low) + flags (high)
    // Total: 16 bytes

    /**
     * @brief Gets the raw hierarchy byte from the parentInfo field.
     * @return Low byte containing hierarchy encoding payload.
     */
    uint8_t GetHierarchyByte() const
    {
        return static_cast<uint8_t>(parentInfo & 0xFF);
    }

    /**
     * @brief Checks if this bone is an intermediate bone.
     * Intermediate bones participate in hierarchy but don't produce output matrices.
     * @return true if intermediate flag (0x10000000) is set.
     */
    bool IsIntermediate() const
    {
        return (parentInfo & 0x10000000) != 0;
    }
};
#pragma pack(pop)

static_assert(sizeof(FA1BindPoseEntry) == 16, "FA1BindPoseEntry must be 16 bytes!");

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
    enum class FA1HierarchyMode
    {
        DirectParent,
        PopCount,
        Sequential
    };

    /**
     * @brief Converts FA1 direct-parent bytes into parent indices.
     *
     * Encoding: 0 = root, n>0 => parent = n-1.
     * Invalid/forward parent references are clamped to root (-1).
     */
    static void ComputeDirectParents(std::vector<int32_t>& outParents,
                                     const std::vector<uint8_t>& hierarchyBytes)
    {
        outParents.assign(hierarchyBytes.size(), -1);
        const int32_t count = static_cast<int32_t>(hierarchyBytes.size());
        for (int32_t i = 0; i < count; i++)
        {
            uint8_t value = hierarchyBytes[static_cast<size_t>(i)];
            if (value == 0)
            {
                outParents[static_cast<size_t>(i)] = -1;
                continue;
            }

            int32_t parent = static_cast<int32_t>(value) - 1;
            if (parent >= 0 && parent < i)
            {
                outParents[static_cast<size_t>(i)] = parent;
            }
            else
            {
                outParents[static_cast<size_t>(i)] = -1;
            }
        }
    }

    /**
     * @brief Converts pop-count bytes into parent indices (matrix-stack semantics).
     */
    static void ComputePopCountParents(std::vector<int32_t>& outParents,
                                       const std::vector<uint8_t>& hierarchyBytes)
    {
        outParents.assign(hierarchyBytes.size(), -1);
        std::vector<int32_t> stack;
        stack.reserve(hierarchyBytes.size());

        for (size_t i = 0; i < hierarchyBytes.size(); i++)
        {
            uint8_t popCount = hierarchyBytes[i];
            for (uint8_t p = 0; p < popCount && !stack.empty(); p++)
            {
                stack.pop_back();
            }

            outParents[i] = stack.empty() ? -1 : stack.back();
            stack.push_back(static_cast<int32_t>(i));
        }
    }

    /**
     * @brief Creates a sequential/world-space parent array (all roots).
     */
    static void ComputeSequentialParents(std::vector<int32_t>& outParents, size_t count)
    {
        outParents.assign(count, -1);
    }

    /**
     * @brief Deterministic FA1 hierarchy-mode selection from hierarchy-byte stream.
     *
     * Rules:
     * - All-zero stream => Sequential
     * - If only one interpretation is valid, use it.
     * - If both DirectParent and PopCount are valid, choose the one with less
     *   root-fragmentation (fewer root bones). This matches ambiguous files
     *   where direct-parent interpretation explodes into many disconnected roots.
     */
    static FA1HierarchyMode DetectFA1HierarchyMode(const std::vector<uint8_t>& hierarchyBytes)
    {
        if (hierarchyBytes.empty())
        {
            return FA1HierarchyMode::Sequential;
        }

        // True world-space case: no hierarchy payload.
        bool allZero = true;
        for (uint8_t v : hierarchyBytes)
        {
            if (v != 0)
            {
                allZero = false;
                break;
            }
        }
        if (allZero)
        {
            return FA1HierarchyMode::Sequential;
        }

        // Validate direct-parent interpretation.
        bool directValid = true;
        for (size_t i = 0; i < hierarchyBytes.size(); i++)
        {
            uint8_t value = hierarchyBytes[i];
            if (value == 0)
            {
                continue;
            }
            int32_t parent = static_cast<int32_t>(value) - 1;
            if (parent < 0 || parent >= static_cast<int32_t>(i))
            {
                directValid = false;
                break;
            }
        }

        // Validate pop-count interpretation using runtime stack constraints.
        bool popValid = true;
        int32_t stackDepth = 0;
        for (uint8_t popCount : hierarchyBytes)
        {
            stackDepth += 1; // push current matrix index first
            if (static_cast<int32_t>(popCount) > stackDepth)
            {
                popValid = false;
                break;
            }
            stackDepth -= static_cast<int32_t>(popCount);
        }

        if (directValid && popValid)
        {
            size_t directRoots = 0;
            for (uint8_t value : hierarchyBytes)
            {
                if (value == 0)
                {
                    directRoots++;
                }
            }

            size_t popRoots = 0;
            std::vector<int32_t> stack;
            stack.reserve(hierarchyBytes.size());
            for (size_t i = 0; i < hierarchyBytes.size(); i++)
            {
                uint8_t popCount = hierarchyBytes[i];
                for (uint8_t p = 0; p < popCount && !stack.empty(); p++)
                {
                    stack.pop_back();
                }
                if (stack.empty())
                {
                    popRoots++;
                }
                stack.push_back(static_cast<int32_t>(i));
            }

            const size_t boneCount = hierarchyBytes.size();
            const bool directRootHeavy = (directRoots * 4) > boneCount; // > 25% roots
            const bool popRootHeavy = (popRoots * 4) > boneCount;       // > 25% roots

            if (directRootHeavy != popRootHeavy)
            {
                return directRootHeavy ? FA1HierarchyMode::PopCount : FA1HierarchyMode::DirectParent;
            }

            if (popRoots < directRoots)
            {
                return FA1HierarchyMode::PopCount;
            }

            return FA1HierarchyMode::DirectParent;
        }

        if (directValid)
        {
            return FA1HierarchyMode::DirectParent;
        }
        if (popValid)
        {
            return FA1HierarchyMode::PopCount;
        }

        // Final fallback for malformed streams.
        return FA1HierarchyMode::DirectParent;
    }

    /**
     * @brief Parses FA1 bind pose entries and extracts parent indices and positions.
     *
     * This extracts bind pose data and computes parents using deterministic
     * FA1 mode selection from hierarchy-byte validity.
     *
     * @param data Pointer to FA1 chunk data (starting at header).
     * @param dataSize Size of the chunk data.
     * @param outParentIndices Output vector for computed parent indices (-1 for root).
     * @param outBindPositions Output vector for bind positions.
     * @return Number of bones parsed, or 0 on failure.
     */
    static size_t ParseFA1BindPose(const uint8_t* data, size_t dataSize,
                                    std::vector<int32_t>& outParentIndices,
                                    std::vector<XMFLOAT3>& outBindPositions)
    {
        if (dataSize < sizeof(FA1Header))
        {
            return 0;
        }

        FA1Header header;
        std::memcpy(&header, data, sizeof(FA1Header));

        // Engine-accurate FA1 layout:
        // header -> bounding cylinders -> sequence entries -> bind pose.
        size_t offset = sizeof(FA1Header);  // 88 bytes
        offset += static_cast<size_t>(header.boundingCylinderCount) * 16;
        offset += static_cast<size_t>(header.transformDataSize) * sizeof(BB9SequenceEntry);
        uint32_t boneCount = header.bindPoseBoneCount;

        // Validate bone count
        if (boneCount == 0 || boneCount > 256)
        {
            return 0;
        }

        // Bone hierarchy data starts at current offset
        size_t bindPoseOffset = offset;

        // Check if there's enough data for all bind pose entries
        size_t bindPoseSize = boneCount * sizeof(FA1BindPoseEntry);
        if (bindPoseOffset + bindPoseSize > dataSize)
        {
            return 0;
        }

        std::vector<uint8_t> hierarchyBytes;
        hierarchyBytes.reserve(boneCount);
        outBindPositions.clear();
        outBindPositions.reserve(boneCount);

        // Parse each bind pose entry
        for (size_t i = 0; i < boneCount; i++)
        {
            FA1BindPoseEntry entry;
            std::memcpy(&entry, &data[bindPoseOffset + i * sizeof(FA1BindPoseEntry)], sizeof(FA1BindPoseEntry));

            hierarchyBytes.push_back(entry.GetHierarchyByte());

            // Transform coordinates: (x, y, z) -> (x, -z, y) for DirectX
            outBindPositions.push_back({
                entry.posX,
                -entry.posZ,
                entry.posY
            });
        }

        FA1HierarchyMode mode = DetectFA1HierarchyMode(hierarchyBytes);
        if (mode == FA1HierarchyMode::DirectParent)
        {
            ComputeDirectParents(outParentIndices, hierarchyBytes);
        }
        else if (mode == FA1HierarchyMode::PopCount)
        {
            ComputePopCountParents(outParentIndices, hierarchyBytes);
        }
        else
        {
            ComputeSequentialParents(outParentIndices, hierarchyBytes.size());
        }

        return boneCount;
    }

    /**
     * @brief Parses a BB9/BB8 animation chunk from raw data.
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
        // For BB9: this is reserved[1] (at offset 0x20), which may contain scale data
        // GW stores this at model+0x100 and uses it to scale bone positions
        // If the value is 0 or negative, GW computes scale from bounding data
        float headerScale;
        std::memcpy(&headerScale, &header.reserved[1], sizeof(float));
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
                            // BB9 position keyframes: coordinate transform (x,y,z) -> (x,z,-y)
                            // BB9 stores position deltas differently than FA1
                            XMFLOAT3 transformedPos = {
                                positions[i].x,
                                positions[i].y,
                                positions[i].z
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

            // Compute bone hierarchy from depth values using heuristic detection
            bool usedSequentialMode = false;
            Animation::HierarchyMode hierarchyMode = Animation::HierarchyMode::TreeDepth;
            ComputeBoneParents(clip.boneParents, boneDepths, &usedSequentialMode, &hierarchyMode, -1);
            clip.hierarchyMode = hierarchyMode;

            // Build output-to-animation bone mapping (for intermediate bone handling)
            // Based on Ghidra RE: bones with flag 0x10000000 don't produce output matrices
            clip.BuildOutputMapping();

            // Parse phase timing data if present (flag 0x0100)
            // Structure: count (u32) + timeValues (u32[count]) + phaseFlags (u8[count])
            // This section appears between bone transforms and animation segments.
            // Time values are phase boundaries in animation units (100000 = 1 second).
            // Phase flags indicate phase types or transition modes.
            size_t segmentOffset = decoder.GetOffset();

            if (header.HasPhaseTiming())
            {
                if (segmentOffset + 4 <= dataSize)
                {
                    uint32_t phaseTimingCount;
                    std::memcpy(&phaseTimingCount, &data[segmentOffset], sizeof(uint32_t));
                    segmentOffset += 4;

                    // Validate count (reasonable limit)
                    if (phaseTimingCount > 0 && phaseTimingCount < 256)
                    {
                        // Skip phase time values (u32 each)
                        size_t timeValuesSize = phaseTimingCount * sizeof(uint32_t);
                        if (segmentOffset + timeValuesSize <= dataSize)
                        {
                            // Could store these values if needed for UI/debugging:
                            // clip.phaseTimeValues.resize(phaseTimingCount);
                            // std::memcpy(clip.phaseTimeValues.data(), &data[segmentOffset], timeValuesSize);
                            segmentOffset += timeValuesSize;
                        }

                        // Skip phase flags (u8 each)
                        size_t phaseFlagsSize = phaseTimingCount * sizeof(uint8_t);
                        if (segmentOffset + phaseFlagsSize <= dataSize)
                        {
                            // Could store these values if needed:
                            // clip.phaseFlags.resize(phaseTimingCount);
                            // std::memcpy(clip.phaseFlags.data(), &data[segmentOffset], phaseFlagsSize);
                            segmentOffset += phaseFlagsSize;
                        }
                    }
                }
            }

            // Parse animation segment entries if present.
            // BB9 entry layout: hash/start/end + phase range + loop offset + transition param.
            if (header.HasAnimationSegments())
            {
                size_t entriesSize = header.animationSegmentCount * sizeof(BB9AnimationSegmentEntry);

                if (segmentOffset + entriesSize <= dataSize)
                {
                    clip.animationSegments.reserve(header.animationSegmentCount);
                    clip.animationSegmentSourceTypes.reserve(header.animationSegmentCount);
                    for (uint32_t i = 0; i < header.animationSegmentCount; i++)
                    {
                        BB9AnimationSegmentEntry rawEntry;
                        std::memcpy(&rawEntry, &data[segmentOffset], sizeof(BB9AnimationSegmentEntry));
                        segmentOffset += sizeof(BB9AnimationSegmentEntry);

                        Animation::AnimationSegmentEntry entry{};
                        entry.hash = rawEntry.hash;
                        entry.startTime = rawEntry.startTime;
                        entry.endTime = rawEntry.endTime;
                        entry.flags = static_cast<uint16_t>(
                            static_cast<uint16_t>(rawEntry.phaseStartIndex) |
                            (static_cast<uint16_t>(rawEntry.phaseEndIndex) << 8));
                        std::memcpy(&entry.reserved[0], &rawEntry.loopStartOffset, sizeof(uint32_t));
                        std::memcpy(&entry.reserved[4], &rawEntry.transitionParam, sizeof(float));

                        // Only add entries with valid timing (reasonable time values)
                        if (entry.startTime < 360000000 && entry.endTime < 360000000 &&
                            entry.startTime <= entry.endTime)
                        {
                            clip.animationSegments.push_back(entry);
                            clip.animationSegmentSourceTypes.push_back(0);
                        }
                    }
                }
            }

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
     * @param animOffset Current offset (start of per-bone animation data).
     * @param boneCount Number of bones.
     * @param clip Output animation clip.
     * @param boneDepths Output vector for hierarchy depths.
     * @param outEndOffset Output: offset where keyframe data ends (start of structured tail parsing).
     * @return true if parsing succeeded, false on failure.
     */
    static bool ParseFA1KeyframeFormat(const uint8_t* data, size_t dataSize,
                                        size_t bindPoseOffset, size_t animOffset,
                                        uint32_t boneCount, Animation::AnimationClip& clip,
                                        std::vector<uint8_t>& boneDepths,
                                        size_t& outEndOffset)
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

            // Store base position with coordinate transform
            // Original transform (x, -z, y) works for tree-based skeletons
            // Stack-based skeletons also use this for bind pose
            track.basePosition = {
                bindPose.posX,
                -bindPose.posZ,
                bindPose.posY
            };

            clip.boneTracks.push_back(std::move(track));

            // Raw hierarchy payload byte for deterministic FA1 mode selection.
            uint8_t hierByte = static_cast<uint8_t>(bindPose.parentInfo & 0xFF);
            boneDepths.push_back(hierByte);

            // Check for intermediate bone flag (same as BB9: 0x10000000)
            // Intermediate bones participate in hierarchy but don't produce output matrices
            constexpr uint32_t FLAG_INTERMEDIATE_BONE = 0x10000000;
            bool isIntermediate = (bindPose.parentInfo & FLAG_INTERMEDIATE_BONE) != 0;
            clip.boneIsIntermediate.push_back(isIntermediate);
        }

        // Deterministic FA1 mode selection from byte-stream validity.
        FA1HierarchyMode fa1Mode = DetectFA1HierarchyMode(boneDepths);
        if (fa1Mode == FA1HierarchyMode::DirectParent)
        {
            ComputeDirectParents(clip.boneParents, boneDepths);
            clip.hierarchyMode = Animation::HierarchyMode::DirectParent;
        }
        else if (fa1Mode == FA1HierarchyMode::PopCount)
        {
            ComputePopCountParents(clip.boneParents, boneDepths);
            clip.hierarchyMode = Animation::HierarchyMode::PopCount;
        }
        else
        {
            ComputeSequentialParents(clip.boneParents, boneDepths.size());
            clip.hierarchyMode = Animation::HierarchyMode::Sequential;
        }

        // Helper to transform quaternion from GW to GWMB coordinates
        auto processQuaternion = [](float qx, float qy, float qz, float qw,
                                    const std::vector<XMFLOAT4>& prevQuats) -> XMFLOAT4 {
            XMFLOAT4 quat;

            // Coordinate transform from GW space to GWMB space + conjugate
            // FA1 stores quaternions with opposite rotation direction vs BB9
            // BB9 negates Euler angles before conversion; FA1 needs explicit conjugate
            // Conjugate: negate x,y,z then coordinate transform (x,-z,y)
            // Result: (-qx, qz, -qy, qw)
            quat.x = -qx;
            quat.y = qz;
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

                    // Coordinate transform: GW (x,y,z) -> GWMB (x,-z,y)
                    // Must match bind position transform for correct animation
                    XMFLOAT3 pos = { px, -pz, py };
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

        // Return the offset where keyframes end (start of structured FA1 tail parsing)
        outEndOffset = offset;
        return !clip.boneTracks.empty();
    }

    /**
     * @brief Skips the FA1 variable-sized block controlled by header offset 0x38.
     *
     * Matches Gw.exe path:
     * Model_ParseArrayWithCallback(..., entrySize=0x0C, callback=Buffer_CalcWeightOffset)
     * where each entry is: 12 fixed bytes + (u32_at_offset_4 * 4) trailing bytes.
     */
    static bool SkipFA1Section0x38Block(const uint8_t* data, size_t dataSize,
                                        const FA1Header& header, size_t& cursor)
    {
        for (uint32_t i = 0; i < header.submeshCount; i++)
        {
            if (cursor + 12 > dataSize)
            {
                return false;
            }

            uint32_t extraWords = 0;
            std::memcpy(&extraWords, &data[cursor + 4], sizeof(uint32_t));
            if (extraWords > 0x100000)
            {
                return false;
            }

            size_t entrySize = 12 + static_cast<size_t>(extraWords) * sizeof(uint32_t);
            if (cursor + entrySize > dataSize)
            {
                return false;
            }
            cursor += entrySize;
        }
        return true;
    }

    /**
     * @brief Structured FA1 segment location from engine counters (no scanning).
     *
     * Gw.exe parsing path:
     *   [keyframe data ends at keyframeEndOffset]
     *   [section 0x38 variable block: header.submeshCount entries]
     *   [section 0x3C block: u32[count] + u8[count] (count = header.unknown_0x3C & 0xFFFF)]
     *   [segments: header.animationSegmentCount * 23]
     */
    static bool FindFA1SegmentBlockStructured(const uint8_t* data, size_t dataSize,
                                              const FA1Header& header, size_t keyframeEndOffset,
                                              size_t& outOffset)
    {
        size_t cursor = keyframeEndOffset;

        if (!SkipFA1Section0x38Block(data, dataSize, header, cursor))
        {
            return false;
        }

        uint32_t count3C = static_cast<uint32_t>(header.unknown_0x3C & 0xFFFF);
        size_t timingSize = static_cast<size_t>(count3C) * 5;
        if (cursor + timingSize > dataSize)
        {
            return false;
        }
        cursor += timingSize;

        size_t entriesSize = static_cast<size_t>(header.animationSegmentCount) * sizeof(FA1AnimationSegmentEntry);
        if (cursor + entriesSize > dataSize)
        {
            return false;
        }

        // Validate entries using required fields only.
        // Entry layout: type(1), hash(4), start(4), end(4), phaseStart(1), phaseEnd(1), ...
        constexpr uint32_t kMaxTime = 360000000;
        uint32_t phaseCount = static_cast<uint32_t>(header.unknown_0x3C & 0xFFFF);
        uint32_t nonZero = 0;
        uint32_t nonZeroHashes = 0;
        for (uint32_t i = 0; i < header.animationSegmentCount; i++)
        {
            size_t entryBase = cursor + i * sizeof(FA1AnimationSegmentEntry);
            if (entryBase + 15 > dataSize)
            {
                return false;
            }

            uint32_t hash = 0;
            uint32_t startTime = 0;
            uint32_t endTime = 0;
            std::memcpy(&hash, &data[entryBase + 1], sizeof(uint32_t));
            std::memcpy(&startTime, &data[entryBase + 5], sizeof(uint32_t));
            std::memcpy(&endTime, &data[entryBase + 9], sizeof(uint32_t));
            uint8_t phaseStart = data[entryBase + 13];
            uint8_t phaseEnd = data[entryBase + 14];

            if (startTime > endTime || startTime >= kMaxTime || endTime >= kMaxTime)
            {
                return false;
            }
            if (phaseStart > phaseEnd || phaseEnd > phaseCount)
            {
                return false;
            }
            if (endTime > startTime)
            {
                nonZero++;
            }
            if (hash != 0)
            {
                nonZeroHashes++;
            }
        }
        // Metadata-only clips can have zero-duration ranges but still carry valid segment hashes.
        if (nonZero == 0 && nonZeroHashes == 0)
        {
            return false;
        }
        outOffset = cursor;
        return true;
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

        // Engine-accurate layout:
        // header -> bounding cylinders -> sequence entries -> bind pose -> keyframe data -> tail sections.
        size_t offset = sizeof(FA1Header);  // 88 bytes
        offset += static_cast<size_t>(header.boundingCylinderCount) * 16;
        uint32_t actualBoneCount = header.bindPoseBoneCount;

        // Validate bone count
        if (actualBoneCount == 0 || actualBoneCount > 256)
        {
            return std::nullopt;
        }

        // FA1 sequence entries follow the bounding cylinder section.
        // FA1 may have sequence data - check transformDataSize
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
        // FA1 format: bone hierarchy data followed by per-bone keyframe animation data
        bool hasSkeletonData = actualBoneCount > 0;
        if (hasSkeletonData && offset < dataSize)
        {
            // Bone hierarchy data starts here
            size_t bindPoseOffset = offset;

            // Bind pose uses 16-byte FA1 entries in engine layout.
            size_t boneDataSize = static_cast<size_t>(actualBoneCount) * sizeof(FA1BindPoseEntry);
            size_t keyframeOffset = bindPoseOffset + boneDataSize;

            if (keyframeOffset < dataSize)
            {
                std::vector<uint8_t> boneDepths;

                // Parse FA1 keyframe format (raw data, not VLE encoded)
                size_t keyframeEndOffset = 0;
                if (!ParseFA1KeyframeFormat(data, dataSize, bindPoseOffset, keyframeOffset,
                                            actualBoneCount, clip, boneDepths, keyframeEndOffset))
                {
                    return std::nullopt;
                }

                // Parse FA1 animation segments deterministically from engine counters.
                if (header.HasAnimationSegments())
                {
                    size_t segmentOffset = 0;
                    size_t segmentCount = header.animationSegmentCount;
                    if (FindFA1SegmentBlockStructured(data, dataSize, header, keyframeEndOffset, segmentOffset))
                    {
                        constexpr uint32_t kMaxTime = 360000000;
                        clip.animationSegments.reserve(segmentCount);
                        clip.animationSegmentSourceTypes.reserve(segmentCount);
                        for (size_t i = 0; i < segmentCount; i++)
                        {
                            size_t entryBase = segmentOffset + i * sizeof(FA1AnimationSegmentEntry);
                            if (entryBase + 15 > dataSize)
                            {
                                break;
                            }

                            FA1AnimationSegmentEntry rawEntry{};
                            rawEntry.segmentType = data[entryBase + 0];
                            std::memcpy(&rawEntry.hash, &data[entryBase + 1], sizeof(uint32_t));
                            std::memcpy(&rawEntry.startTime, &data[entryBase + 5], sizeof(uint32_t));
                            std::memcpy(&rawEntry.endTime, &data[entryBase + 9], sizeof(uint32_t));
                            rawEntry.phaseStartIndex = data[entryBase + 13];
                            rawEntry.phaseEndIndex = data[entryBase + 14];
                            if (entryBase + 19 <= dataSize)
                            {
                                std::memcpy(&rawEntry.loopStartOffset, &data[entryBase + 15], sizeof(uint32_t));
                            }
                            if (entryBase + sizeof(FA1AnimationSegmentEntry) <= dataSize)
                            {
                                std::memcpy(&rawEntry.transitionParam, &data[entryBase + 19], sizeof(float));
                            }

                            if (rawEntry.startTime < kMaxTime && rawEntry.endTime < kMaxTime &&
                                rawEntry.startTime <= rawEntry.endTime)
                            {
                                Animation::AnimationSegmentEntry entry{};
                                entry.hash = rawEntry.hash;
                                entry.startTime = rawEntry.startTime;
                                entry.endTime = rawEntry.endTime;
                                entry.flags = static_cast<uint16_t>(
                                    static_cast<uint16_t>(rawEntry.phaseStartIndex) |
                                    (static_cast<uint16_t>(rawEntry.phaseEndIndex) << 8));
                                std::memcpy(&entry.reserved[0], &rawEntry.loopStartOffset, sizeof(uint32_t));
                                std::memcpy(&entry.reserved[4], &rawEntry.transitionParam, sizeof(float));
                                clip.animationSegments.push_back(entry);
                                clip.animationSegmentSourceTypes.push_back(rawEntry.segmentType);
                            }
                        }
                    }
                }

                // Build output-to-animation bone mapping (for intermediate bone handling)
                clip.BuildOutputMapping();
            }
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
     * @brief Computes parent indices from hierarchy bytes.
     *
     * Deterministic mode selection uses stream validity:
     * - DirectParent: value = parent_index + 1, 0 = root
     * - PopCount: value = stack-pop count before processing current bone
     * - TreeDepth: value = absolute depth in hierarchy tree
     * - Sequential: all-zero stream
     *
     * @param outParents Output vector of parent indices.
     * @param depths Raw hierarchy bytes from bone flags.
     * @param outUsedSequentialMode Output flag: true if sequential mode was used.
     * @param outHierarchyMode Output: selected hierarchy encoding mode.
     * @param forceTreeDepth Legacy override:
     *   - 1: prefer TreeDepth (fallback DirectParent)
     *   - 0: prefer PopCount (fallback DirectParent)
     *   - -1: deterministic auto-detect
     */
    static void ComputeBoneParents(std::vector<int32_t>& outParents, const std::vector<uint8_t>& depths,
                                    bool* outUsedSequentialMode = nullptr,
                                    Animation::HierarchyMode* outHierarchyMode = nullptr,
                                    int forceTreeDepth = -1)
    {
        outParents.assign(depths.size(), -1);
        if (outUsedSequentialMode) *outUsedSequentialMode = false;
        if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::TreeDepth;
        if (depths.empty()) return;

        auto isAllZero = [&depths]() -> bool {
            for (uint8_t d : depths)
            {
                if (d != 0) return false;
            }
            return true;
        };

        auto validateDirectParent = [&depths]() -> bool {
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t value = depths[i];
                if (value == 0) continue;
                int32_t parent = static_cast<int32_t>(value) - 1;
                if (parent < 0 || parent >= static_cast<int32_t>(i))
                {
                    return false;
                }
            }
            return true;
        };

        auto validatePopCount = [&depths]() -> bool {
            int32_t stackDepth = 0;
            for (uint8_t popCount : depths)
            {
                stackDepth += 1;
                if (static_cast<int32_t>(popCount) > stackDepth)
                {
                    return false;
                }
                stackDepth -= static_cast<int32_t>(popCount);
            }
            return true;
        };

        auto validateTreeDepth = [&depths]() -> bool {
            std::vector<int32_t> depthToBone(256, -1);
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t depth = depths[i];
                if (depth == 0)
                {
                    depthToBone[0] = static_cast<int32_t>(i);
                }
                else
                {
                    if (depthToBone[depth - 1] < 0)
                    {
                        return false;
                    }
                    depthToBone[depth] = static_cast<int32_t>(i);
                }

                for (size_t d = static_cast<size_t>(depth) + 1; d < depthToBone.size(); d++)
                {
                    depthToBone[d] = -1;
                }
            }
            return true;
        };

        auto computeDirectParent = [&outParents, &depths]() {
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t value = depths[i];
                if (value == 0)
                {
                    outParents[i] = -1;
                    continue;
                }
                int32_t parent = static_cast<int32_t>(value) - 1;
                outParents[i] = (parent >= 0 && parent < static_cast<int32_t>(i)) ? parent : -1;
            }
        };

        auto computePopCount = [&outParents, &depths]() {
            std::vector<int32_t> stack;
            stack.reserve(depths.size());
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t popCount = depths[i];
                for (uint8_t p = 0; p < popCount && !stack.empty(); p++)
                {
                    stack.pop_back();
                }
                outParents[i] = stack.empty() ? -1 : stack.back();
                stack.push_back(static_cast<int32_t>(i));
            }
        };

        auto computeTreeDepth = [&outParents, &depths]() {
            std::vector<int32_t> depthToBone(256, -1);
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t depth = depths[i];
                outParents[i] = (depth == 0) ? -1 : depthToBone[depth - 1];
                depthToBone[depth] = static_cast<int32_t>(i);
                for (size_t d = static_cast<size_t>(depth) + 1; d < depthToBone.size(); d++)
                {
                    depthToBone[d] = -1;
                }
            }
        };

        if (isAllZero())
        {
            if (outUsedSequentialMode) *outUsedSequentialMode = true;
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::Sequential;
            return;
        }

        const bool directValid = validateDirectParent();
        const bool popValid = validatePopCount();
        const bool treeValid = validateTreeDepth();

        if (forceTreeDepth == 1)
        {
            if (treeValid)
            {
                computeTreeDepth();
                if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::TreeDepth;
                return;
            }

            computeDirectParent();
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::DirectParent;
            return;
        }

        if (forceTreeDepth == 0)
        {
            if (popValid)
            {
                computePopCount();
                if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::PopCount;
                return;
            }

            computeDirectParent();
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::DirectParent;
            return;
        }

        // Ambiguous direct-vs-pop streams are resolved by root-fragmentation:
        // pick the interpretation with fewer roots / less disconnected hierarchy.
        if (directValid && popValid)
        {
            std::vector<int32_t> directParents(depths.size(), -1);
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t value = depths[i];
                if (value == 0)
                {
                    directParents[i] = -1;
                    continue;
                }
                int32_t parent = static_cast<int32_t>(value) - 1;
                directParents[i] = (parent >= 0 && parent < static_cast<int32_t>(i)) ? parent : -1;
            }

            std::vector<int32_t> popParents(depths.size(), -1);
            std::vector<int32_t> stack;
            stack.reserve(depths.size());
            for (size_t i = 0; i < depths.size(); i++)
            {
                uint8_t popCount = depths[i];
                for (uint8_t p = 0; p < popCount && !stack.empty(); p++)
                {
                    stack.pop_back();
                }
                popParents[i] = stack.empty() ? -1 : stack.back();
                stack.push_back(static_cast<int32_t>(i));
            }

            auto countRoots = [](const std::vector<int32_t>& parents) -> size_t {
                size_t roots = 0;
                for (int32_t parent : parents)
                {
                    if (parent < 0)
                    {
                        roots++;
                    }
                }
                return roots;
            };

            const size_t directRoots = countRoots(directParents);
            const size_t popRoots = countRoots(popParents);
            const size_t boneCount = depths.size();
            const bool directRootHeavy = (directRoots * 4) > boneCount; // > 25% roots
            const bool popRootHeavy = (popRoots * 4) > boneCount;       // > 25% roots

            const bool preferPop = (directRootHeavy != popRootHeavy)
                ? directRootHeavy
                : (popRoots < directRoots);

            if (preferPop)
            {
                outParents = std::move(popParents);
                if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::PopCount;
                return;
            }

            outParents = std::move(directParents);
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::DirectParent;
            return;
        }

        // Deterministic auto-detection with priority order when unambiguous:
        // DirectParent > PopCount > TreeDepth.
        if (directValid)
        {
            computeDirectParent();
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::DirectParent;
            return;
        }

        if (popValid)
        {
            computePopCount();
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::PopCount;
            return;
        }

        if (treeValid)
        {
            computeTreeDepth();
            if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::TreeDepth;
            return;
        }

        // Final fallback for malformed streams.
        computeDirectParent();
        if (outHierarchyMode) *outHierarchyMode = Animation::HierarchyMode::DirectParent;
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

    // Try BB9 first - uses 44-byte header
    size_t chunkOffset, chunkSize;
    if (FindChunk(fileData, fileSize, CHUNK_ID_BB9, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::Parse(&fileData[chunkOffset], chunkSize);
    }

    // Try BB8 - uses 44-byte header
    if (FindChunk(fileData, fileSize, CHUNK_ID_BB8, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::Parse(&fileData[chunkOffset], chunkSize);
    }

    // Try FA1 - uses 88-byte header
    if (FindChunk(fileData, fileSize, CHUNK_ID_FA1, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::ParseFA1(&fileData[chunkOffset], chunkSize);
    }

    // Note: FA6 is NOT an animation chunk - it's a file reference chunk (BBC equivalent)
    // Do not try to parse FA6 as animation data.

    // Try FA0 - uses 88-byte header
    if (FindChunk(fileData, fileSize, CHUNK_ID_FA0, chunkOffset, chunkSize))
    {
        return BB9AnimationParser::ParseFA1(&fileData[chunkOffset], chunkSize);
    }

    return std::nullopt;
}

} // namespace GW::Parsers

