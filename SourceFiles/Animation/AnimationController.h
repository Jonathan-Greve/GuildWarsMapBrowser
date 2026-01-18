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

namespace GW::Animation {

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
     * @brief Initializes the controller with a clip and optional skeleton.
     *
     * @param clip Animation clip to play.
     * @param skeleton Optional skeleton for skinning (can be nullptr).
     */
    void Initialize(std::shared_ptr<AnimationClip> clip, std::shared_ptr<Skeleton> skeleton = nullptr)
    {
        m_clip = clip;
        m_skeleton = skeleton;
        m_currentSequenceIndex = 0;
        m_currentTime = clip ? clip->minTime : 0.0f;
        m_state = PlaybackState::Stopped;

        if (clip)
        {
            m_boneMatrices.resize(clip->boneTracks.size());
            m_boneWorldPositions.resize(clip->boneTracks.size());
            m_boneWorldRotations.resize(clip->boneTracks.size());

            // Set initial time range
            if (!clip->sequences.empty())
            {
                const auto& seq = clip->sequences[0];
                m_sequenceStartTime = seq.startTime;
                m_sequenceEndTime = seq.endTime;
            }
            else
            {
                m_sequenceStartTime = clip->minTime;
                m_sequenceEndTime = clip->maxTime;
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
        m_currentTime = m_sequenceStartTime;
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

        // Advance time based on playback speed
        // GW animation time units are roughly 1000 = 1 second
        float timeAdvance = deltaTime * m_playbackSpeed;
        m_currentTime += timeAdvance;

        // Handle looping/cycling
        if (m_currentTime >= m_sequenceEndTime)
        {
            if (m_autoCycleSequences && m_clip->sequences.size() > 1)
            {
                // Auto-advance to next sequence
                NextSequence();
                NotifyCallback("sequence_loop");
            }
            else if (m_looping)
            {
                // Loop current sequence
                m_currentTime = m_sequenceStartTime;
                NotifyCallback("loop");
            }
            else
            {
                // Stop at end
                m_currentTime = m_sequenceEndTime;
                m_state = PlaybackState::Stopped;
                NotifyCallback("finished");
            }
        }

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
     * @brief Gets the current sequence index.
     */
    size_t GetCurrentSequenceIndex() const { return m_currentSequenceIndex; }

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
     * @brief Gets the skeleton.
     */
    std::shared_ptr<Skeleton> GetSkeleton() const { return m_skeleton; }

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

private:
    void EvaluateBoneMatrices()
    {
        if (!m_clip)
        {
            return;
        }

        // First, evaluate hierarchical transforms to get world positions and rotations
        // These are needed for bone visualization and debugging
        m_evaluator.EvaluateHierarchical(*m_clip, m_currentTime, m_boneWorldPositions, m_boneWorldRotations);

        // Then compute skinning matrices from the world transforms
        m_evaluator.ComputeSkinningFromHierarchy(*m_clip, m_currentTime, m_boneMatrices);
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
    std::shared_ptr<Skeleton> m_skeleton;
    AnimationEvaluator m_evaluator;

    PlaybackState m_state = PlaybackState::Stopped;
    float m_currentTime = 0.0f;
    float m_playbackSpeed = 100000.0f;  // Time units per second
    bool m_looping = true;
    bool m_autoCycleSequences = false;

    size_t m_currentSequenceIndex = 0;
    float m_sequenceStartTime = 0.0f;
    float m_sequenceEndTime = 0.0f;

    std::vector<XMFLOAT4X4> m_boneMatrices;
    std::vector<XMFLOAT3> m_boneWorldPositions;  // Actual world positions for visualization
    std::vector<XMFLOAT4> m_boneWorldRotations;  // World rotations for debugging
    AnimationCallback m_callback;
};

} // namespace GW::Animation
