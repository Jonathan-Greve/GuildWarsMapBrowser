#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <string>

using namespace DirectX;

namespace GW::Animation {

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
    uint32_t sequenceIndex = 0;  // Index/grouping identifier
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

    std::vector<BoneTrack> boneTracks;           // Per-bone animation data
    std::vector<int32_t> boneParents;            // Bone hierarchy (parent indices)
    std::vector<AnimationSequence> sequences;    // Animation sequences

    /**
     * @brief Gets the number of animated bones.
     */
    size_t GetBoneCount() const { return boneTracks.size(); }

    /**
     * @brief Gets the number of sequences.
     */
    size_t GetSequenceCount() const { return sequences.size(); }

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
};

} // namespace GW::Animation
