#pragma once

#include "../Parsers/SoundEventParser.h"
#include "../Animation/AnimationClip.h"
#include "../bass.h"
#include <vector>
#include <memory>
#include <map>
#include <cstdint>

// Forward declarations
class DATManager;

namespace GW::Audio {

/**
 * @brief Loaded sound data from DAT file.
 */
struct LoadedSound {
    uint32_t fileId = 0;
    std::vector<uint8_t> data;      // Raw MP3 data from DAT
    HSTREAM streamHandle = 0;        // BASS stream (created on demand)
    bool isLoaded = false;
};

/**
 * @brief Trigger state for a sound event.
 */
struct SoundTriggerState {
    bool hasTriggered = false;       // Has this event triggered in current loop?
    float lastTriggerTime = 0.0f;    // When it last triggered
};

/**
 * @brief Sound event with timing from Type 8 files.
 *
 * Combines timing from Type 8 sound event bytecode with loaded sound data.
 */
struct AnimSoundEvent {
    float timing = 0.0f;        // Trigger time in animation units
    uint32_t soundIndex = 0;    // Index into loaded sounds (from Type 8)
    uint32_t effectHash = 0;    // Effect/sound hash
    float volume = 1.0f;        // Volume/param (0.0-1.0)
};

/**
 * @brief Manages animation sound event playback using BASS audio library.
 *
 * Handles:
 * - Loading sound files from DAT based on Type 8 file references
 * - Creating BASS streams for playback
 * - Triggering sounds at specific animation times
 * - Tracking which sounds have played to avoid re-triggering in a loop
 */
class AnimationSoundManager {
public:
    AnimationSoundManager() = default;
    ~AnimationSoundManager();

    // Non-copyable
    AnimationSoundManager(const AnimationSoundManager&) = delete;
    AnimationSoundManager& operator=(const AnimationSoundManager&) = delete;

    /**
     * @brief Load sound file references from a Type 8 file.
     *
     * Parses the Type 8 file and loads referenced sound files from DAT.
     * Note: This only loads the sound files, not the timing. Call SetTimingFromClip()
     * to set up timing from AnimationClip's effectSoundEntries.
     *
     * @param fileData Type 8 file data.
     * @param fileSize File size in bytes.
     * @param dat_managers DAT managers for loading sound files.
     * @return true if parsing and loading succeeded.
     */
    bool LoadFromType8File(const uint8_t* fileData, size_t fileSize,
                          std::map<int, std::unique_ptr<DATManager>>& dat_managers);

    /**
     * @brief Set animation segment info from AnimationClip.
     *
     * Stores the animation segments for reference (used for timeline display).
     * Sound timing comes from Type 8 files, not from animation segments.
     *
     * @param clip AnimationClip containing animationSegments.
     */
    void SetTimingFromClip(const GW::Animation::AnimationClip& clip);

    /**
     * @brief Clear all loaded sounds and events.
     */
    void Clear();

    /**
     * @brief Update sound playback based on animation time.
     *
     * Call this each frame from animation controller update.
     *
     * @param currentTime Current animation time in GW units.
     * @param loopStartTime Start of current playback range.
     * @param loopEndTime End of current playback range.
     * @param isPlaying Whether animation is currently playing.
     */
    void Update(float currentTime, float loopStartTime, float loopEndTime, bool isPlaying);

    /**
     * @brief Reset trigger states.
     *
     * Call when animation loops or changes to allow sounds to trigger again.
     */
    void ResetTriggers();

    /**
     * @brief Enable or disable sound playback.
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    /**
     * @brief Check if sound playback is enabled.
     */
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Set volume level.
     * @param volume Volume from 0.0 (mute) to 1.0 (full).
     */
    void SetVolume(float volume);

    /**
     * @brief Get current volume level.
     */
    float GetVolume() const { return m_volume; }

    /**
     * @brief Access sound events for timeline visualization.
     */
    const std::vector<AnimSoundEvent>& GetSoundEvents() const { return m_soundEvents; }

    /**
     * @brief Access animation segments from animation clip (for timeline display).
     */
    const std::vector<GW::Animation::AnimationSegmentEntry>& GetAnimationSegments() const { return m_animationSegments; }

    /**
     * @brief Access sound file IDs for debugging.
     */
    const std::vector<uint32_t>& GetSoundFileIds() const { return m_soundFileIds; }

    /**
     * @brief Check if any sound data is loaded.
     */
    bool HasSounds() const { return !m_soundEvents.empty() || !m_loadedSounds.empty(); }

    /**
     * @brief Check if animation segments have been set from clip.
     */
    bool HasAnimationSegments() const { return !m_animationSegments.empty(); }

    /**
     * @brief Get the file ID of the currently loaded Type 8 file.
     */
    uint32_t GetLoadedFileId() const { return m_loadedFileId; }

private:
    /**
     * @brief Trigger a sound event by index.
     */
    void TriggerSound(size_t eventIndex);

    /**
     * @brief Create or get BASS stream for a sound.
     */
    HSTREAM CreateSoundStream(size_t soundIndex);

    /**
     * @brief Find a loaded sound by its file ID.
     * @param fileId The file ID to search for.
     * @return Index into m_loadedSounds, or -1 if not found.
     */
    int32_t FindSoundByFileId(uint32_t fileId) const;

    // Sound events with timing (from Type 8 sound event files)
    std::vector<AnimSoundEvent> m_soundEvents;

    // Animation segments from AnimationClip (for timeline display)
    std::vector<GW::Animation::AnimationSegmentEntry> m_animationSegments;

    // Sound file references and loaded data (from Type 8)
    std::vector<uint32_t> m_soundFileIds;
    std::vector<LoadedSound> m_loadedSounds;
    std::vector<SoundTriggerState> m_triggerStates;

    uint32_t m_loadedFileId = 0;     // File ID of loaded Type 8 file
    uint32_t m_maxTiming = 0;        // Max cumulative timing from Type 8 bytecode
    float m_animDuration = 0.0f;     // Animation duration for timing scale
    float m_lastTime = 0.0f;
    float m_volume = 1.0f;
    bool m_enabled = true;
    bool m_wasPlaying = false;

    /**
     * @brief Scale cumulative timing to animation time units.
     *
     * @param cumulativeTiming Raw cumulative timing value from bytecode.
     * @return Time in animation units.
     */
    float ScaleTimingToAnimUnits(uint32_t cumulativeTiming) const;
};

} // namespace GW::Audio
