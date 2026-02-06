#pragma once

#include <DirectXMath.h>
#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <string>
#include <format>
#include "GWAnimationHashes.h"

using namespace DirectX;

namespace GW::Animation {

/**
 * @brief Hierarchy encoding mode detected from animation data.
 *
 * Guild Wars uses different methods to encode bone hierarchy:
 * - TREE_DEPTH: Depth value = absolute level in tree (0=root, 1=child, etc.)
 * - POP_COUNT: Depth value = number of levels to pop from matrix stack
 * - SEQUENTIAL: No hierarchy data (world-space transforms)
 * - DIRECT_PARENT: FA1 format - low byte = (parent_index + 1), 0 = root
 */
enum class HierarchyMode
{
    TreeDepth,    // TREE_DEPTH: depth = absolute level in hierarchy
    PopCount,     // POP_COUNT: depth = levels to pop from stack
    Sequential,   // No hierarchy, world-space transforms
    DirectParent  // DIRECT_PARENT: FA1 format, value = parent + 1
};

/**
 * @brief A single keyframe with a time and value.
 * @tparam T The value type (XMFLOAT3 for position/scale, XMFLOAT4 for rotation quaternion).
 */
template<typename T>
struct Keyframe
{
    float time = 0.0f;  // Time in animation units (game's internal timing)
    T value;

    Keyframe() = default;
    Keyframe(float t, const T& v) : time(t), value(v) {}
};

/**
 * @brief Animation keyframes for a single bone.
 *
 * Contains position, rotation (quaternion), and scale keyframes.
 * Keyframes are stored sorted by time for efficient binary search interpolation.
 */
struct BoneTrack
{
    uint32_t boneIndex = 0;  // Index into skeleton bone array

    std::vector<Keyframe<XMFLOAT3>> positionKeys;   // Position keyframes
    std::vector<Keyframe<XMFLOAT4>> rotationKeys;   // Rotation quaternion keyframes (w,x,y,z)
    std::vector<Keyframe<XMFLOAT3>> scaleKeys;      // Scale keyframes

    // Bind pose position (absolute world coordinates from BB9)
    XMFLOAT3 basePosition = {0.0f, 0.0f, 0.0f};

    /**
     * @brief Checks if this track has any keyframe data.
     */
    bool HasKeyframes() const
    {
        return !positionKeys.empty() || !rotationKeys.empty() || !scaleKeys.empty();
    }

    /**
     * @brief Checks if this track has position animation.
     */
    bool HasPositionAnimation() const { return !positionKeys.empty(); }

    /**
     * @brief Checks if this track has rotation animation.
     */
    bool HasRotationAnimation() const { return !rotationKeys.empty(); }

    /**
     * @brief Checks if this track has scale animation.
     */
    bool HasScaleAnimation() const { return !scaleKeys.empty(); }

    /**
     * @brief Gets the time range of this track's keyframes.
     * @param outMinTime Minimum time value.
     * @param outMaxTime Maximum time value.
     */
    void GetTimeRange(float& outMinTime, float& outMaxTime) const
    {
        outMinTime = FLT_MAX;
        outMaxTime = 0.0f;

        if (!positionKeys.empty())
        {
            outMinTime = std::min(outMinTime, positionKeys.front().time);
            outMaxTime = std::max(outMaxTime, positionKeys.back().time);
        }
        if (!rotationKeys.empty())
        {
            outMinTime = std::min(outMinTime, rotationKeys.front().time);
            outMaxTime = std::max(outMaxTime, rotationKeys.back().time);
        }
        if (!scaleKeys.empty())
        {
            outMinTime = std::min(outMinTime, scaleKeys.front().time);
            outMaxTime = std::max(outMaxTime, scaleKeys.back().time);
        }

        if (outMinTime == FLT_MAX)
        {
            outMinTime = 0.0f;
        }
    }
};

/**
 * @brief Represents a single animation sequence within an animation clip.
 *
 * Guild Wars animations can contain multiple sequences (idle, walk, run, etc.)
 * Each sequence has its own frame count and time range.
 */
struct AnimationSequence
{
    uint32_t hash = 0;           // Animation hash identifier
    std::string name;            // Human-readable name (if available)
    float startTime = 0.0f;      // Animation start time
    float endTime = 0.0f;        // Animation end time
    uint32_t frameCount = 0;     // Number of frames in this sequence
    uint32_t sequenceIndex = 0;  // Index/grouping identifier (sequences with same value have compatible poses)
    XMFLOAT3 bounds = {0.0f, 0.0f, 0.0f};  // Bounding information

    /**
     * @brief Gets the duration of this sequence.
     */
    float GetDuration() const { return endTime - startTime; }

    /**
     * @brief Checks if this sequence is valid.
     */
    bool IsValid() const { return frameCount > 0 && endTime > startTime; }
};

/**
 * @brief Configuration for animation looping behavior.
 *
 * Many Guild Wars animations have an intro phase that plays once, followed by
 * a loop region that repeats. For example, the dance animation:
 * - Phase 1: Intro (bind pose → dance pose) - plays once
 * - Phases 2-5: Dance loop - repeats until stopped
 * - Exit: Play Phase 1 in reverse (or blend to bind pose)
 *
 * The loop pattern is: 1 → 2 → 3 → 4 → 5 → 2 → 3 → 4 → 5 → 2 → ...
 */
struct AnimationLoopConfig
{
    size_t introStartSequence = 0;    // First sequence of intro (usually 0)
    size_t introEndSequence = 0;      // Last sequence of intro (e.g., Phase 1 = index 0)
    size_t loopStartSequence = 1;     // First sequence of loop region (e.g., Phase 2 = index 1)
    size_t loopEndSequence = SIZE_MAX; // Last sequence of loop region (SIZE_MAX = last sequence)

    bool hasIntro = false;            // Animation has intro that plays once before looping
    bool canPlayIntroReverse = true;  // Intro can be played in reverse to exit animation

    /**
     * @brief Gets the actual loop end sequence index, clamped to valid range.
     */
    size_t GetLoopEndSequence(size_t sequenceCount) const
    {
        if (loopEndSequence == SIZE_MAX || loopEndSequence >= sequenceCount)
            return sequenceCount > 0 ? sequenceCount - 1 : 0;
        return loopEndSequence;
    }

    /**
     * @brief Checks if a sequence is part of the intro.
     */
    bool IsIntroSequence(size_t seqIndex) const
    {
        return hasIntro && seqIndex >= introStartSequence && seqIndex <= introEndSequence;
    }

    /**
     * @brief Checks if a sequence is part of the loop region.
     */
    bool IsLoopSequence(size_t seqIndex, size_t sequenceCount) const
    {
        return seqIndex >= loopStartSequence && seqIndex <= GetLoopEndSequence(sequenceCount);
    }
};

/**
 * @brief Represents a complete animation (may span multiple sequences/phases).
 *
 * A single animation file can contain multiple distinct animations (e.g., dance, laugh, cheer).
 * Each animation is identified by its animationId hash and can have multiple phases
 * (sequences with the same or related animationId).
 */
struct AnimationGroup
{
    uint32_t animationId = 0;               // Primary animation hash
    std::string displayName;                // "Animation 0x12345678" or mapped name
    float startTime = 0.0f;                 // Start of first phase
    float endTime = 0.0f;                   // End of last phase
    std::vector<size_t> sequenceIndices;    // Which sequences belong to this animation

    /**
     * @brief Gets the duration of this animation group.
     */
    float GetDuration() const { return endTime - startTime; }

    /**
     * @brief Gets the number of phases/sequences in this group.
     */
    size_t GetPhaseCount() const { return sequenceIndices.size(); }

    /**
     * @brief Checks if this animation group is valid.
     */
    bool IsValid() const { return !sequenceIndices.empty() && endTime > startTime; }
};

/**
 * @brief Animation segment entry (engine-normalized metadata container).
 *
 * Parsed from BB9/FA1 chunk. These define animation regions within phases:
 * - Loop boundaries (main animation segment vs intro)
 * - Sub-animation markers (/laugh, /cheer, strafe variants within a phase)
 *
 * For simple looping animations (like dance):
 * - The segment with the largest time range defines the loop region
 * - Everything before that segment's startTime is the intro
 *
 * For complex animations (like 0x3AAA with 110 segments):
 * - Each segment defines a distinct sub-animation within phases
 * - Segments can overlap or be sequential
 *
 * Sound timing comes from separate Type 8 files (BBC/FA6 references).
 */
#pragma pack(push, 1)
struct AnimationSegmentEntry
{
    uint32_t hash = 0;              // 0x00: Animation segment identifier
    uint32_t startTime = 0;         // 0x04: Start time in animation units (100000 = 1 sec)
    uint32_t endTime = 0;           // 0x08: End time in animation units
    // Packed from BB9/FA1 runtime fields:
    // - flags low/high bytes: phaseStartIndex/phaseEndIndex
    // - reserved[0..3]: loopStartOffset (u32)
    // - reserved[4..7]: transitionParam (f32)
    uint16_t flags = 0;
    uint8_t  reserved[8] = {};
    // Total: 22 bytes (0x16) for backward compatibility with existing code.

    /**
     * @brief Gets the start time in seconds.
     * @param timeScale Time scale (default 100000 = 1 second).
     */
    float GetStartTimeSeconds(float timeScale = 100000.0f) const
    {
        return static_cast<float>(startTime) / timeScale;
    }

    /**
     * @brief Gets the end time in seconds.
     * @param timeScale Time scale (default 100000 = 1 second).
     */
    float GetEndTimeSeconds(float timeScale = 100000.0f) const
    {
        return static_cast<float>(endTime) / timeScale;
    }

    /**
     * @brief Gets the duration in animation units.
     */
    uint32_t GetDuration() const
    {
        return endTime > startTime ? endTime - startTime : 0;
    }

    uint8_t GetPhaseStartIndex() const
    {
        return static_cast<uint8_t>(flags & 0xFF);
    }

    uint8_t GetPhaseEndIndex() const
    {
        return static_cast<uint8_t>((flags >> 8) & 0xFF);
    }

    uint32_t GetLoopStartOffset() const
    {
        uint32_t value = 0;
        std::memcpy(&value, &reserved[0], sizeof(uint32_t));
        return value;
    }

    float GetTransitionParam() const
    {
        float value = 0.0f;
        std::memcpy(&value, &reserved[4], sizeof(float));
        return value;
    }
};
#pragma pack(pop)

static_assert(sizeof(AnimationSegmentEntry) == 22, "AnimationSegmentEntry must be 22 bytes!");

/**
 * @brief Complete animation clip containing all bone tracks and sequences.
 *
 * An animation clip represents all animation data parsed from a BB9/FA1 chunk.
 * It contains per-bone keyframe data and sequence information.
 */
struct AnimationClip
{
    std::string name;                            // Clip name
    float duration = 0.0f;                       // Total duration
    float minTime = 0.0f;                        // Minimum keyframe time
    float maxTime = 0.0f;                        // Maximum keyframe time
    uint32_t totalFrames = 0;                    // Total frame count across all sequences

    uint32_t modelHash0 = 0;                     // Model signature part 1
    uint32_t modelHash1 = 0;                     // Model signature part 2

    float geometryScale = 1.0f;                  // Geometry scale factor from header (FA1 offset 0x20)

    HierarchyMode hierarchyMode = HierarchyMode::TreeDepth;  // Detected hierarchy encoding mode

    std::string sourceChunkType;                 // Source chunk type ("BB9" or "FA1")

    std::vector<BoneTrack> boneTracks;           // Per-bone animation data
    std::vector<int32_t> boneParents;            // Bone hierarchy (parent indices)
    std::vector<AnimationSequence> sequences;    // Animation sequences
    std::vector<AnimationGroup> animationGroups; // Grouped animations by animationId
    std::vector<AnimationSegmentEntry> animationSegments;  // Segment timing/hash entries (BB9/FA1)
    // For FA1 only: per-segment source selector from segmentType.
    // 0 = local clip, >0 = external referenced animation source index.
    std::vector<uint8_t> animationSegmentSourceTypes;

    AnimationLoopConfig loopConfig;                  // Loop configuration (intro/loop regions)

    // Intermediate bone tracking (from Ghidra RE @ Model_UpdateSkeletonTransforms)
    // Bones with flag 0x10000000 are "intermediate" - they participate in hierarchy
    // but don't produce output skinning matrices. Mesh vertices reference OUTPUT
    // indices which skip intermediate bones.
    std::vector<bool> boneIsIntermediate;        // True if bone has flag 0x10000000
    std::vector<uint32_t> outputToAnimBone;      // Maps output index -> animation bone index
    std::vector<int32_t> animBoneToOutput;       // Maps animation bone -> output index (-1 if intermediate)

    /**
     * @brief Gets the number of animated bones.
     */
    size_t GetBoneCount() const { return boneTracks.size(); }

    /**
     * @brief Gets the number of sequences.
     */
    size_t GetSequenceCount() const { return sequences.size(); }

    /**
     * @brief Gets FA1 segment source type for a segment index.
     *
     * Returns 0 for BB9 segments (local clip) and also for FA1 segments when source
     * metadata is unavailable.
     */
    uint8_t GetSegmentSourceType(size_t segmentIndex) const
    {
        if (segmentIndex < animationSegmentSourceTypes.size())
        {
            return animationSegmentSourceTypes[segmentIndex];
        }
        return 0;
    }

    /**
     * @brief Checks if the clip has valid animation data.
     */
    bool IsValid() const { return !boneTracks.empty() && maxTime > minTime; }

    /**
     * @brief Computes the total time range from all bone tracks.
     */
    void ComputeTimeRange()
    {
        minTime = FLT_MAX;
        maxTime = 0.0f;

        for (const auto& track : boneTracks)
        {
            if (!track.HasKeyframes())
            {
                continue;
            }

            float trackMin, trackMax;
            track.GetTimeRange(trackMin, trackMax);
            minTime = std::min(minTime, trackMin);
            maxTime = std::max(maxTime, trackMax);
        }

        if (minTime == FLT_MAX)
        {
            minTime = 0.0f;
        }

        duration = maxTime - minTime;
    }

    /**
     * @brief Computes time ranges for all sequences based on frame counts.
     *
     * Assumes frames are evenly distributed across the total time range.
     */
    void ComputeSequenceTimeRanges()
    {
        if (totalFrames <= 1 || sequences.empty())
        {
            return;
        }

        float timePerFrame = (maxTime - minTime) / static_cast<float>(totalFrames - 1);
        uint32_t cumulativeFrames = 0;

        for (auto& seq : sequences)
        {
            seq.startTime = minTime + cumulativeFrames * timePerFrame;
            cumulativeFrames += seq.frameCount;
            seq.endTime = minTime + cumulativeFrames * timePerFrame;
        }
    }

    /**
     * @brief Builds animation groups from sequences.
     *
     * Groups sequences by their animation hash (animationId). Each group represents
     * a distinct animation that may span multiple phases/sequences.
     */
    void BuildAnimationGroups()
    {
        animationGroups.clear();

        if (sequences.empty())
        {
            return;
        }

        // Group sequences by their hash (animationId)
        std::map<uint32_t, AnimationGroup> groupMap;

        for (size_t i = 0; i < sequences.size(); i++)
        {
            const auto& seq = sequences[i];
            auto& group = groupMap[seq.hash];

            if (group.sequenceIndices.empty())
            {
                // First sequence with this hash
                group.animationId = seq.hash;
                group.startTime = seq.startTime;
                group.endTime = seq.endTime;
                // Use hash lookup for known animation names
                group.displayName = GetAnimationCategorizedName(seq.hash);
            }
            else
            {
                // Extend time range
                group.startTime = std::min(group.startTime, seq.startTime);
                group.endTime = std::max(group.endTime, seq.endTime);
            }

            group.sequenceIndices.push_back(i);
        }

        // Move groups to vector (preserves insertion order due to std::map)
        for (auto& [id, group] : groupMap)
        {
            animationGroups.push_back(std::move(group));
        }
    }

    /**
     * @brief Gets the animation group at the specified index.
     * @param index Group index.
     * @return Pointer to group, or nullptr if out of range.
     */
    const AnimationGroup* GetAnimationGroup(size_t index) const
    {
        if (index < animationGroups.size())
        {
            return &animationGroups[index];
        }
        return nullptr;
    }

    /**
     * @brief Gets the number of animation groups.
     */
    size_t GetAnimationGroupCount() const
    {
        return animationGroups.size();
    }

    /**
     * @brief Finds the animation group containing the given time.
     * @param time Animation time to check.
     * @return Pointer to group, or nullptr if not found.
     */
    const AnimationGroup* GetAnimationGroupAtTime(float time) const
    {
        for (const auto& group : animationGroups)
        {
            if (time >= group.startTime && time <= group.endTime)
            {
                return &group;
            }
        }
        return nullptr;
    }

    /**
     * @brief Gets the index of the sequence containing the given time.
     * @param time Animation time to check.
     * @return Sequence index, or -1 if not found.
     */
    int GetSequenceIndexAtTime(float time) const
    {
        for (size_t i = 0; i < sequences.size(); i++)
        {
            if (time >= sequences[i].startTime && time <= sequences[i].endTime)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Gets the sequence that contains the given time.
     * @param time Animation time to check.
     * @return Pointer to sequence, or nullptr if not found.
     */
    const AnimationSequence* GetSequenceAtTime(float time) const
    {
        for (const auto& seq : sequences)
        {
            if (time >= seq.startTime && time <= seq.endTime)
            {
                return &seq;
            }
        }
        return nullptr;
    }

    /**
     * @brief Gets a sequence by index.
     * @param index Sequence index.
     * @return Pointer to sequence, or nullptr if out of range.
     */
    const AnimationSequence* GetSequence(size_t index) const
    {
        if (index < sequences.size())
        {
            return &sequences[index];
        }
        return nullptr;
    }

    /**
     * @brief Gets the number of bones with position keyframes.
     */
    size_t GetBonesWithPositionAnimation() const
    {
        size_t count = 0;
        for (const auto& track : boneTracks)
        {
            if (track.HasPositionAnimation()) count++;
        }
        return count;
    }

    /**
     * @brief Gets the number of bones with rotation keyframes.
     */
    size_t GetBonesWithRotationAnimation() const
    {
        size_t count = 0;
        for (const auto& track : boneTracks)
        {
            if (track.HasRotationAnimation()) count++;
        }
        return count;
    }

    /**
     * @brief Gets the number of bones with scale keyframes.
     */
    size_t GetBonesWithScaleAnimation() const
    {
        size_t count = 0;
        for (const auto& track : boneTracks)
        {
            if (track.HasScaleAnimation()) count++;
        }
        return count;
    }

    /**
     * @brief Builds the output-to-animation bone mapping.
     *
     * Based on Ghidra RE of Model_UpdateSkeletonTransforms @ 0x00754720:
     * Bones with flag 0x10000000 are intermediate - they participate in the
     * hierarchy calculation but don't produce output skinning matrices.
     * Mesh vertices reference OUTPUT indices, which skip intermediate bones.
     *
     * Call this after setting boneIsIntermediate for all bones.
     */
    void BuildOutputMapping()
    {
        outputToAnimBone.clear();
        animBoneToOutput.clear();
        animBoneToOutput.resize(boneTracks.size(), -1);

        // Ensure boneIsIntermediate has correct size
        if (boneIsIntermediate.size() != boneTracks.size())
        {
            boneIsIntermediate.resize(boneTracks.size(), false);
        }

        uint32_t outputIndex = 0;
        for (size_t i = 0; i < boneTracks.size(); i++)
        {
            if (!boneIsIntermediate[i])
            {
                outputToAnimBone.push_back(static_cast<uint32_t>(i));
                animBoneToOutput[i] = static_cast<int32_t>(outputIndex);
                outputIndex++;
            }
        }
    }

    /**
     * @brief Gets the number of output (non-intermediate) bones.
     */
    size_t GetOutputBoneCount() const
    {
        return outputToAnimBone.size();
    }

    /**
     * @brief Maps an output index to an animation bone index.
     * @param outputIdx Output bone index (what mesh vertices reference).
     * @return Animation bone index, or 0 if out of range.
     */
    uint32_t GetAnimBoneFromOutput(uint32_t outputIdx) const
    {
        if (outputIdx < outputToAnimBone.size())
        {
            return outputToAnimBone[outputIdx];
        }
        // Fallback: return identity mapping if no intermediate bones
        return outputIdx;
    }

    /**
     * @brief Maps an animation bone index to an output index.
     * @param animBoneIdx Animation bone index.
     * @return Output index, or -1 if bone is intermediate.
     */
    int32_t GetOutputFromAnimBone(uint32_t animBoneIdx) const
    {
        if (animBoneIdx < animBoneToOutput.size())
        {
            return animBoneToOutput[animBoneIdx];
        }
        // Fallback: return identity mapping if no intermediate bones
        return static_cast<int32_t>(animBoneIdx);
    }

    /**
     * @brief Detects loop configuration based on sequence analysis.
     *
     * Analyzes the sequenceIndex field of each sequence to determine:
     * 1. Which sequences form the intro (unique sequenceIndex, plays once)
     * 2. Which sequences form the loop region (matching sequenceIndex at boundaries)
     *
     * For example, in a dance animation:
     * - Phase 1 (intro): sequenceIndex=0 (bind pose → dance pose)
     * - Phases 2-5 (loop): sequenceIndex=1 (all share same pose compatibility)
     *
     * The loop region is detected when the last sequence's sequenceIndex matches
     * an earlier sequence, indicating they can transition smoothly.
     */
    void DetectLoopConfiguration()
    {
        loopConfig = AnimationLoopConfig();  // Reset to defaults

        if (sequences.size() < 2)
        {
            // Single sequence or empty - no intro/loop distinction
            return;
        }

        // Find the first sequence that has a matching sequenceIndex with the last sequence
        // This indicates a compatible pose for looping
        uint32_t lastSeqIndex = sequences.back().sequenceIndex;
        size_t loopStartIdx = SIZE_MAX;

        for (size_t i = 0; i < sequences.size() - 1; i++)
        {
            if (sequences[i].sequenceIndex == lastSeqIndex)
            {
                loopStartIdx = i;
                break;
            }
        }

        if (loopStartIdx == SIZE_MAX)
        {
            // No matching sequenceIndex found - might be a simple linear animation
            // Check if all sequences share the same sequenceIndex (no intro)
            bool allSame = true;
            uint32_t firstSeqIndex = sequences[0].sequenceIndex;
            for (size_t i = 1; i < sequences.size(); i++)
            {
                if (sequences[i].sequenceIndex != firstSeqIndex)
                {
                    allSame = false;
                    break;
                }
            }

            if (allSame)
            {
                // All sequences have same index - loop the whole thing
                loopConfig.hasIntro = false;
                loopConfig.loopStartSequence = 0;
                loopConfig.loopEndSequence = sequences.size() - 1;
            }
            return;
        }

        // We found a loop region
        if (loopStartIdx > 0)
        {
            // Sequences before loopStartIdx are the intro
            loopConfig.hasIntro = true;
            loopConfig.introStartSequence = 0;
            loopConfig.introEndSequence = loopStartIdx - 1;
            loopConfig.loopStartSequence = loopStartIdx;
            loopConfig.loopEndSequence = sequences.size() - 1;

            // Check if intro can be played in reverse
            // This is possible if the first sequence's start pose matches bind pose
            // For now, assume it can (this is common in GW animations)
            loopConfig.canPlayIntroReverse = true;
        }
        else
        {
            // First sequence already matches last - no intro, just loop everything
            loopConfig.hasIntro = false;
            loopConfig.loopStartSequence = 0;
            loopConfig.loopEndSequence = sequences.size() - 1;
        }
    }

    /**
     * @brief Gets the next sequence index for looping playback.
     *
     * Handles the loop pattern: intro sequences play once, then loop region repeats.
     * When loop region ends, jumps back to loopStartSequence.
     *
     * @param currentSeqIndex Current sequence index.
     * @param hasPlayedIntro Whether the intro has already played.
     * @return Next sequence index to play.
     */
    size_t GetNextLoopSequence(size_t currentSeqIndex, bool& hasPlayedIntro) const
    {
        if (sequences.empty())
            return 0;

        // If in intro and haven't finished it
        if (loopConfig.hasIntro && !hasPlayedIntro)
        {
            if (currentSeqIndex < loopConfig.introEndSequence)
            {
                // Continue through intro
                return currentSeqIndex + 1;
            }
            else if (currentSeqIndex == loopConfig.introEndSequence)
            {
                // Finished intro, start loop region
                hasPlayedIntro = true;
                return loopConfig.loopStartSequence;
            }
        }

        // In loop region
        size_t loopEnd = loopConfig.GetLoopEndSequence(sequences.size());
        if (currentSeqIndex >= loopEnd)
        {
            // End of loop region - wrap to loop start
            return loopConfig.loopStartSequence;
        }
        else if (currentSeqIndex >= loopConfig.loopStartSequence)
        {
            // Continue through loop region
            return currentSeqIndex + 1;
        }

        // Fallback: simple increment with wrap
        return (currentSeqIndex + 1) % sequences.size();
    }

    /**
     * @brief Gets the time range for the loop region.
     *
     * @param outStartTime Start time of loop region.
     * @param outEndTime End time of loop region.
     */
    void GetLoopTimeRange(float& outStartTime, float& outEndTime) const
    {
        if (sequences.empty())
        {
            outStartTime = minTime;
            outEndTime = maxTime;
            return;
        }

        size_t loopStart = loopConfig.loopStartSequence;
        size_t loopEnd = loopConfig.GetLoopEndSequence(sequences.size());

        if (loopStart < sequences.size())
        {
            outStartTime = sequences[loopStart].startTime;
        }
        else
        {
            outStartTime = minTime;
        }

        if (loopEnd < sequences.size())
        {
            outEndTime = sequences[loopEnd].endTime;
        }
        else
        {
            outEndTime = maxTime;
        }
    }

    /**
     * @brief Gets the time range for the intro region.
     *
     * @param outStartTime Start time of intro.
     * @param outEndTime End time of intro.
     * @return true if animation has an intro, false otherwise.
     */
    bool GetIntroTimeRange(float& outStartTime, float& outEndTime) const
    {
        if (!loopConfig.hasIntro || sequences.empty())
        {
            outStartTime = 0.0f;
            outEndTime = 0.0f;
            return false;
        }

        outStartTime = sequences[loopConfig.introStartSequence].startTime;
        if (loopConfig.introEndSequence < sequences.size())
        {
            outEndTime = sequences[loopConfig.introEndSequence].endTime;
        }
        else
        {
            outEndTime = sequences[0].endTime;
        }
        return true;
    }
};

} // namespace GW::Animation
