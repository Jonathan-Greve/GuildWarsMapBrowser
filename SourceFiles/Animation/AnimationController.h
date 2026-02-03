#pragma once

#include "AnimationClip.h"
#include "AnimationEvaluator.h"
#include "Skeleton.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

using namespace DirectX;

// Forward declaration - full definition in animation_state.h
enum class AnimationPlaybackMode;

namespace GW::Animation {

/**
 * @brief Playback mode for animation controller (local copy for header isolation).
 */
enum class PlaybackMode
{
    FullAnimation,  // Play selected animation group (all its phases)
    SinglePhase,    // Play only one sequence/phase
    EntireFile,     // Play entire file start to end
    SmartLoop,      // Play intro once, then loop the loop region (1 → 2 → 3 → 4 → 5 → 2 → ...)
    SegmentLoop     // Play and loop a single animation segment (sub-animation within phases)
};

/**
 * @brief Controls animation playback for a model.
 *
 * Handles:
 * - Play, pause, stop control
 * - Time advancement and looping
 * - Sequence selection and cycling
 * - Bone matrix computation for GPU skinning
 */
class AnimationController
{
public:
    /**
     * @brief Playback state enumeration.
     */
    enum class PlaybackState
    {
        Stopped,
        Playing,
        Paused
    };

    /**
     * @brief Callback for animation events (sequence change, loop point, etc.).
     */
    using AnimationCallback = std::function<void(const AnimationController&, const std::string&)>;

    AnimationController() = default;

    /**
     * @brief Initializes the controller with a clip.
     *
     * @param clip Animation clip to play.
     */
    void Initialize(std::shared_ptr<AnimationClip> clip)
    {
        m_clip = clip;
        m_currentSequenceIndex = 0;
        m_currentGroupIndex = 0;
        m_currentTime = clip ? clip->minTime : 0.0f;
        m_state = PlaybackState::Stopped;
        m_playbackMode = PlaybackMode::FullAnimation;
        m_hasPlayedIntro = false;
        m_isPlayingReverse = false;

        if (clip)
        {
            m_boneMatrices.resize(clip->boneTracks.size());
            m_boneWorldPositions.resize(clip->boneTracks.size());
            m_boneWorldRotations.resize(clip->boneTracks.size());

            // Detect loop configuration for smart looping
            clip->DetectLoopConfiguration();

            // Set initial time range based on animation groups if available
            if (!clip->animationGroups.empty())
            {
                const auto& group = clip->animationGroups[0];
                m_groupStartTime = group.startTime;
                m_groupEndTime = group.endTime;
                m_sequenceStartTime = group.startTime;
                m_sequenceEndTime = group.endTime;
                m_currentTime = group.startTime;

                // Set current sequence to first in group
                if (!group.sequenceIndices.empty())
                {
                    m_currentSequenceIndex = group.sequenceIndices[0];
                }
            }
            else if (!clip->sequences.empty())
            {
                const auto& seq = clip->sequences[0];
                m_sequenceStartTime = seq.startTime;
                m_sequenceEndTime = seq.endTime;
                m_groupStartTime = clip->minTime;
                m_groupEndTime = clip->maxTime;
            }
            else
            {
                m_sequenceStartTime = clip->minTime;
                m_sequenceEndTime = clip->maxTime;
                m_groupStartTime = clip->minTime;
                m_groupEndTime = clip->maxTime;
            }

            // Evaluate initial bone transforms so visualization works immediately
            EvaluateBoneMatrices();
        }
    }

    /**
     * @brief Starts or resumes playback.
     */
    void Play()
    {
        if (m_clip)
        {
            m_state = PlaybackState::Playing;
            NotifyCallback("play");
        }
    }

    /**
     * @brief Pauses playback.
     */
    void Pause()
    {
        if (m_state == PlaybackState::Playing)
        {
            m_state = PlaybackState::Paused;
            NotifyCallback("pause");
        }
    }

    /**
     * @brief Stops playback and resets to beginning.
     */
    void Stop()
    {
        m_state = PlaybackState::Stopped;
        m_isPlayingReverse = false;

        // In SmartLoop mode, reset to beginning including intro
        if (m_playbackMode == PlaybackMode::SmartLoop && m_clip)
        {
            m_hasPlayedIntro = false;
            if (m_clip->loopConfig.hasIntro)
            {
                float introStart, introEnd;
                if (m_clip->GetIntroTimeRange(introStart, introEnd))
                {
                    m_sequenceStartTime = introStart;
                    m_sequenceEndTime = introEnd;
                    m_currentTime = introStart;
                }
            }
            else
            {
                m_currentTime = m_sequenceStartTime;
            }
        }
        else
        {
            m_currentTime = m_sequenceStartTime;
        }
        NotifyCallback("stop");
    }

    /**
     * @brief Toggles between play and pause.
     */
    void TogglePlayPause()
    {
        if (m_state == PlaybackState::Playing)
        {
            Pause();
        }
        else
        {
            Play();
        }
    }

    /**
     * @brief Selects an animation sequence by index.
     *
     * @param index Sequence index.
     * @param resetTime If true, resets time to sequence start.
     */
    void SetSequence(size_t index, bool resetTime = true)
    {
        if (!m_clip || index >= m_clip->sequences.size())
        {
            return;
        }

        m_currentSequenceIndex = index;
        const auto& seq = m_clip->sequences[index];
        m_sequenceStartTime = seq.startTime;
        m_sequenceEndTime = seq.endTime;

        if (resetTime)
        {
            m_currentTime = m_sequenceStartTime;
        }

        NotifyCallback("sequence_changed");
    }

    /**
     * @brief Advances to the next sequence.
     */
    void NextSequence()
    {
        if (m_clip && !m_clip->sequences.empty())
        {
            size_t nextIndex = (m_currentSequenceIndex + 1) % m_clip->sequences.size();
            SetSequence(nextIndex);
        }
    }

    /**
     * @brief Goes to the previous sequence.
     */
    void PreviousSequence()
    {
        if (m_clip && !m_clip->sequences.empty())
        {
            size_t prevIndex = (m_currentSequenceIndex > 0)
                ? m_currentSequenceIndex - 1
                : m_clip->sequences.size() - 1;
            SetSequence(prevIndex);
        }
    }

    /**
     * @brief Updates animation state.
     *
     * @param deltaTime Real time elapsed since last update (seconds).
     */
    void Update(float deltaTime)
    {
        if (!m_clip || m_state != PlaybackState::Playing)
        {
            return;
        }

        // Advance time based on playback speed and direction
        float timeAdvance = deltaTime * m_playbackSpeed;
        if (m_isPlayingReverse)
        {
            m_currentTime -= timeAdvance;
        }
        else
        {
            m_currentTime += timeAdvance;
        }

        // Handle looping/cycling based on playback mode
        if (m_playbackMode == PlaybackMode::SmartLoop)
        {
            // Smart loop mode: intro plays once, then loop region repeats
            HandleSmartLoopUpdate();
        }
        else if (m_playbackMode == PlaybackMode::SegmentLoop)
        {
            // Segment loop mode: loop within the segment's time range
            if (m_currentTime >= m_sequenceEndTime)
            {
                if (m_looping)
                {
                    m_currentTime = m_sequenceStartTime;
                    NotifyCallback("segment_loop");
                }
                else
                {
                    m_currentTime = m_sequenceEndTime;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("segment_finished");
                }
            }
        }
        else if (m_currentTime >= m_sequenceEndTime)
        {
            if (m_playbackMode == PlaybackMode::SinglePhase)
            {
                // Single phase mode: cycle through sequences or loop current
                if (m_autoCycleSequences && m_clip->sequences.size() > 1)
                {
                    NextSequence();
                    NotifyCallback("sequence_loop");
                }
                else if (m_looping)
                {
                    m_currentTime = m_sequenceStartTime;
                    NotifyCallback("loop");
                }
                else
                {
                    m_currentTime = m_sequenceEndTime;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("finished");
                }
            }
            else if (m_playbackMode == PlaybackMode::FullAnimation)
            {
                // Full animation mode: loop within group bounds
                if (m_looping)
                {
                    m_currentTime = m_groupStartTime;
                    NotifyCallback("loop");
                }
                else
                {
                    m_currentTime = m_groupEndTime;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("finished");
                }
            }
            else // EntireFile
            {
                // Entire file mode: loop from start
                if (m_looping)
                {
                    m_currentTime = m_clip->minTime;
                    NotifyCallback("loop");
                }
                else
                {
                    m_currentTime = m_clip->maxTime;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("finished");
                }
            }
        }
        else if (m_isPlayingReverse && m_currentTime <= m_sequenceStartTime)
        {
            // Finished reverse playback
            m_currentTime = m_sequenceStartTime;
            m_isPlayingReverse = false;
            m_state = PlaybackState::Stopped;
            NotifyCallback("finished_reverse");
        }

        // Track current phase for UI display
        UpdateCurrentPhaseFromTime();

        // Evaluate animation
        EvaluateBoneMatrices();
    }

    /**
     * @brief Sets the current time directly (for scrubbing).
     */
    void SetTime(float time)
    {
        m_currentTime = std::clamp(time, m_sequenceStartTime, m_sequenceEndTime);
        EvaluateBoneMatrices();
    }

    /**
     * @brief Gets the current animation time.
     */
    float GetTime() const { return m_currentTime; }

    /**
     * @brief Gets the current time as a normalized value [0, 1].
     */
    float GetNormalizedTime() const
    {
        float range = m_sequenceEndTime - m_sequenceStartTime;
        if (range > 0.0f)
        {
            return (m_currentTime - m_sequenceStartTime) / range;
        }
        return 0.0f;
    }

    /**
     * @brief Gets the playback state.
     */
    PlaybackState GetState() const { return m_state; }

    /**
     * @brief Checks if currently playing.
     */
    bool IsPlaying() const { return m_state == PlaybackState::Playing; }

    /**
     * @brief Sets playback speed (time units per second).
     *
     * Default is 100000 (roughly 100 seconds = 1 full animation range).
     */
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float GetPlaybackSpeed() const { return m_playbackSpeed; }

    /**
     * @brief Sets whether to loop the current sequence.
     */
    void SetLooping(bool loop) { m_looping = loop; }
    bool IsLooping() const { return m_looping; }

    /**
     * @brief Sets whether to auto-cycle through sequences.
     */
    void SetAutoCycleSequences(bool autoCycle) { m_autoCycleSequences = autoCycle; }
    bool IsAutoCyclingSequences() const { return m_autoCycleSequences; }

    /**
     * @brief Sets whether to lock root bone positions to bind pose.
     *
     * When enabled, root bones (bones with no parent) will not have position
     * animation applied - they stay at their bind pose position. This is useful
     * for multi-character scene animations where root motion positions actors.
     */
    void SetLockRootPosition(bool lock) { m_lockRootPosition = lock; }
    bool IsRootPositionLocked() const { return m_lockRootPosition; }

    /**
     * @brief Gets the current sequence index.
     */
    size_t GetCurrentSequenceIndex() const { return m_currentSequenceIndex; }

    /**
     * @brief Gets the current animation group index.
     */
    size_t GetCurrentAnimationGroupIndex() const { return m_currentGroupIndex; }

    /**
     * @brief Sets the playback mode.
     */
    void SetPlaybackMode(PlaybackMode mode)
    {
        m_playbackMode = mode;
        UpdateTimeRange();
    }

    /**
     * @brief Gets the current playback mode.
     */
    PlaybackMode GetPlaybackMode() const { return m_playbackMode; }

    /**
     * @brief Sets the current animation group.
     *
     * When in FullAnimation mode, this sets the time range to cover
     * all sequences in the group.
     */
    void SetAnimationGroup(size_t groupIndex)
    {
        if (!m_clip || groupIndex >= m_clip->animationGroups.size())
        {
            return;
        }

        m_currentGroupIndex = groupIndex;
        const auto& group = m_clip->animationGroups[groupIndex];
        m_groupStartTime = group.startTime;
        m_groupEndTime = group.endTime;

        // Update time range if in FullAnimation mode
        if (m_playbackMode == PlaybackMode::FullAnimation)
        {
            m_sequenceStartTime = m_groupStartTime;
            m_sequenceEndTime = m_groupEndTime;
            m_currentTime = m_groupStartTime;

            // Update current sequence index to first sequence in group
            if (!group.sequenceIndices.empty())
            {
                m_currentSequenceIndex = group.sequenceIndices[0];
            }
        }

        EvaluateBoneMatrices();
    }

    /**
     * @brief Sets the current animation segment for SegmentLoop mode.
     *
     * Animation segments define sub-animations within phases (e.g., /laugh, /cheer,
     * strafe variants). Each segment has its own start/end time range.
     *
     * @param segmentIndex Index into animationSegments vector.
     */
    void SetSegment(size_t segmentIndex)
    {
        if (!m_clip || segmentIndex >= m_clip->animationSegments.size())
        {
            return;
        }

        m_currentSegmentIndex = segmentIndex;
        const auto& seg = m_clip->animationSegments[segmentIndex];

        // Set time range to segment boundaries
        m_sequenceStartTime = static_cast<float>(seg.startTime);
        m_sequenceEndTime = static_cast<float>(seg.endTime);
        m_currentTime = m_sequenceStartTime;

        // Update current sequence index based on segment time
        UpdateCurrentPhaseFromTime();

        EvaluateBoneMatrices();
        NotifyCallback("segment_changed");
    }

    /**
     * @brief Gets the current segment index.
     */
    size_t GetCurrentSegmentIndex() const { return m_currentSegmentIndex; }

    /**
     * @brief Updates the time range based on current playback mode.
     */
    void UpdateTimeRange()
    {
        if (!m_clip)
        {
            return;
        }

        switch (m_playbackMode)
        {
            case PlaybackMode::EntireFile:
                m_sequenceStartTime = m_clip->minTime;
                m_sequenceEndTime = m_clip->maxTime;
                break;

            case PlaybackMode::FullAnimation:
                if (m_currentGroupIndex < m_clip->animationGroups.size())
                {
                    const auto& group = m_clip->animationGroups[m_currentGroupIndex];
                    m_sequenceStartTime = group.startTime;
                    m_sequenceEndTime = group.endTime;
                    m_groupStartTime = group.startTime;
                    m_groupEndTime = group.endTime;
                }
                break;

            case PlaybackMode::SinglePhase:
                // Keep existing sequence bounds
                if (m_currentSequenceIndex < m_clip->sequences.size())
                {
                    const auto& seq = m_clip->sequences[m_currentSequenceIndex];
                    m_sequenceStartTime = seq.startTime;
                    m_sequenceEndTime = seq.endTime;
                }
                break;

            case PlaybackMode::SmartLoop:
                // Reset intro state when switching to smart loop
                m_hasPlayedIntro = false;
                m_isPlayingReverse = false;

                // If has intro, start from intro; otherwise start from loop region
                if (m_clip->loopConfig.hasIntro)
                {
                    float introStart, introEnd;
                    if (m_clip->GetIntroTimeRange(introStart, introEnd))
                    {
                        m_sequenceStartTime = introStart;
                        m_sequenceEndTime = introEnd;
                    }
                }
                else
                {
                    // No intro, just use loop region
                    float loopStart, loopEnd;
                    m_clip->GetLoopTimeRange(loopStart, loopEnd);
                    m_sequenceStartTime = loopStart;
                    m_sequenceEndTime = loopEnd;
                }
                break;

            case PlaybackMode::SegmentLoop:
                // Use current segment's time range
                if (m_currentSegmentIndex < m_clip->animationSegments.size())
                {
                    const auto& seg = m_clip->animationSegments[m_currentSegmentIndex];
                    m_sequenceStartTime = static_cast<float>(seg.startTime);
                    m_sequenceEndTime = static_cast<float>(seg.endTime);
                }
                else if (!m_clip->animationSegments.empty())
                {
                    // Default to first segment
                    m_currentSegmentIndex = 0;
                    const auto& seg = m_clip->animationSegments[0];
                    m_sequenceStartTime = static_cast<float>(seg.startTime);
                    m_sequenceEndTime = static_cast<float>(seg.endTime);
                }
                break;
        }

        // Clamp current time to new range
        m_currentTime = std::clamp(m_currentTime, m_sequenceStartTime, m_sequenceEndTime);
        EvaluateBoneMatrices();
    }

    /**
     * @brief Updates the current sequence index based on current time.
     *
     * Called during playback to track which phase/sequence we're in.
     */
    void UpdateCurrentPhaseFromTime()
    {
        if (!m_clip)
        {
            return;
        }

        int seqIdx = m_clip->GetSequenceIndexAtTime(m_currentTime);
        if (seqIdx >= 0)
        {
            m_currentSequenceIndex = static_cast<size_t>(seqIdx);
        }
    }

    /**
     * @brief Gets the group start time.
     */
    float GetGroupStartTime() const { return m_groupStartTime; }

    /**
     * @brief Gets the group end time.
     */
    float GetGroupEndTime() const { return m_groupEndTime; }

    /**
     * @brief Gets the current sequence name.
     */
    std::string GetCurrentSequenceName() const
    {
        if (m_clip && m_currentSequenceIndex < m_clip->sequences.size())
        {
            return m_clip->sequences[m_currentSequenceIndex].name;
        }
        return "";
    }

    /**
     * @brief Gets the bone matrices for GPU upload.
     */
    const std::vector<XMFLOAT4X4>& GetBoneMatrices() const { return m_boneMatrices; }

    /**
     * @brief Gets the number of bones.
     */
    size_t GetBoneCount() const { return m_boneMatrices.size(); }

    /**
     * @brief Gets the animation clip.
     */
    std::shared_ptr<AnimationClip> GetClip() const { return m_clip; }

    /**
     * @brief Sets a callback for animation events.
     */
    void SetCallback(AnimationCallback callback) { m_callback = callback; }

    /**
     * @brief Gets the sequence start time.
     */
    float GetSequenceStartTime() const { return m_sequenceStartTime; }

    /**
     * @brief Gets the sequence end time.
     */
    float GetSequenceEndTime() const { return m_sequenceEndTime; }

    /**
     * @brief Gets the world position of each bone for debug visualization.
     *
     * Returns the actual bone world positions, not extracted from skinning matrices.
     * These positions are computed during hierarchical evaluation.
     */
    const std::vector<XMFLOAT3>& GetBoneWorldPositions() const
    {
        return m_boneWorldPositions;
    }

    /**
     * @brief Gets the world rotation of each bone.
     */
    const std::vector<XMFLOAT4>& GetBoneWorldRotations() const
    {
        return m_boneWorldRotations;
    }

    /**
     * @brief Gets the bone parent indices.
     */
    const std::vector<int32_t>& GetBoneParents() const
    {
        if (m_clip)
        {
            return m_clip->boneParents;
        }
        static std::vector<int32_t> empty;
        return empty;
    }

    /**
     * @brief Checks if the animation has an intro phase.
     */
    bool HasIntro() const
    {
        return m_clip && m_clip->loopConfig.hasIntro;
    }

    /**
     * @brief Gets whether the intro has been played.
     */
    bool HasPlayedIntro() const { return m_hasPlayedIntro; }

    /**
     * @brief Resets the intro state (call when restarting animation).
     */
    void ResetIntro()
    {
        m_hasPlayedIntro = false;
        m_isPlayingReverse = false;
    }

    /**
     * @brief Starts playing the intro in reverse (for exiting animation).
     *
     * Only works if the animation has an intro that can be reversed.
     */
    void PlayIntroReverse()
    {
        if (!m_clip || !m_clip->loopConfig.hasIntro || !m_clip->loopConfig.canPlayIntroReverse)
        {
            return;
        }

        // Get intro time range
        float introStart, introEnd;
        if (m_clip->GetIntroTimeRange(introStart, introEnd))
        {
            m_sequenceStartTime = introStart;
            m_sequenceEndTime = introEnd;
            m_currentTime = introEnd;  // Start at end of intro
            m_isPlayingReverse = true;
            m_state = PlaybackState::Playing;
            NotifyCallback("intro_reverse_start");
        }
    }

    /**
     * @brief Gets the loop configuration from the clip.
     */
    const AnimationLoopConfig* GetLoopConfig() const
    {
        return m_clip ? &m_clip->loopConfig : nullptr;
    }

private:
    /**
     * @brief Handles smart loop playback logic.
     *
     * Smart loop plays intro once, then loops the loop region indefinitely.
     * Pattern: 1 → 2 → 3 → 4 → 5 → 2 → 3 → 4 → 5 → 2 → ...
     */
    void HandleSmartLoopUpdate()
    {
        if (!m_clip)
        {
            return;
        }

        const auto& config = m_clip->loopConfig;

        // Handle reverse playback (exiting animation)
        if (m_isPlayingReverse)
        {
            float introStart, introEnd;
            if (m_clip->GetIntroTimeRange(introStart, introEnd))
            {
                if (m_currentTime <= introStart)
                {
                    m_currentTime = introStart;
                    m_isPlayingReverse = false;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("intro_reverse_finished");
                }
            }
            return;
        }

        // Get loop region bounds
        float loopStart, loopEnd;
        m_clip->GetLoopTimeRange(loopStart, loopEnd);

        // If we haven't played the intro yet
        if (config.hasIntro && !m_hasPlayedIntro)
        {
            float introStart, introEnd;
            if (m_clip->GetIntroTimeRange(introStart, introEnd))
            {
                if (m_currentTime >= introEnd)
                {
                    // Intro finished, transition to loop region
                    m_hasPlayedIntro = true;
                    m_currentTime = loopStart;
                    m_sequenceStartTime = loopStart;
                    m_sequenceEndTime = loopEnd;
                    NotifyCallback("intro_finished");
                }
            }
        }
        else
        {
            // In loop region
            if (m_currentTime >= loopEnd)
            {
                if (m_looping)
                {
                    // Loop back to loop region start (not intro)
                    m_currentTime = loopStart;
                    NotifyCallback("loop");
                }
                else
                {
                    m_currentTime = loopEnd;
                    m_state = PlaybackState::Stopped;
                    NotifyCallback("finished");
                }
            }
        }
    }

    void EvaluateBoneMatrices()
    {
        if (!m_clip)
        {
            return;
        }

        // Evaluate hierarchical transforms to get world positions and rotations
        // These are needed for bone visualization and skinning
        // Pass lockRootPosition flag to keep roots at bind pose when enabled
        m_evaluator.EvaluateHierarchical(*m_clip, m_currentTime, m_boneWorldPositions, m_boneWorldRotations, nullptr, m_lockRootPosition);

        // Compute skinning matrices using animation bind positions
        // GW's algorithm: T(basePos + delta) * R(localRot) * T(-basePos)
        m_evaluator.ComputeSkinningFromHierarchy(*m_clip, m_currentTime, m_boneMatrices, m_lockRootPosition);
    }

    void NotifyCallback(const std::string& event)
    {
        if (m_callback)
        {
            m_callback(*this, event);
        }
    }

private:
    std::shared_ptr<AnimationClip> m_clip;
    AnimationEvaluator m_evaluator;

    PlaybackState m_state = PlaybackState::Stopped;
    float m_currentTime = 0.0f;
    float m_playbackSpeed = 100000.0f;  // Time units per second
    bool m_looping = true;
    bool m_autoCycleSequences = true;
    bool m_lockRootPosition = false;

    // Smart loop state
    bool m_hasPlayedIntro = false;      // Whether intro has played in current playback
    bool m_isPlayingReverse = false;    // Currently playing intro in reverse (exiting)

    size_t m_currentSequenceIndex = 0;
    size_t m_currentSegmentIndex = 0;  // Current animation segment (for SegmentLoop mode)
    float m_sequenceStartTime = 0.0f;
    float m_sequenceEndTime = 0.0f;

    // Animation group playback support
    PlaybackMode m_playbackMode = PlaybackMode::FullAnimation;
    size_t m_currentGroupIndex = 0;
    float m_groupStartTime = 0.0f;
    float m_groupEndTime = 0.0f;

    std::vector<XMFLOAT4X4> m_boneMatrices;
    std::vector<XMFLOAT3> m_boneWorldPositions;  // Actual world positions for visualization
    std::vector<XMFLOAT4> m_boneWorldRotations;  // World rotations for debugging
    AnimationCallback m_callback;
};

} // namespace GW::Animation
