#include "pch.h"
#include "AnimationSoundManager.h"
#include "DATManager.h"
#include "FFNA_ModelFile_Other.h"  // For LogBB8Debug

// Extern BASS function pointers (from Main.cpp)
extern LPFNBASSSTREAMCREATEFILE lpfnBassStreamCreateFile;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop;
extern LPFNBASSSTREAMFREE lpfnBassStreamFree;
extern LPFNBASSCHANNELSETATTRIBUTE lpfnBassChannelSetAttribute;
extern bool is_bass_working;

namespace GW::Audio {

AnimationSoundManager::~AnimationSoundManager()
{
    Clear();
}

bool AnimationSoundManager::LoadFromType8File(
    const uint8_t* fileData, size_t fileSize,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    // Clear existing data but preserve animation segments if already set
    auto savedAnimSegments = std::move(m_animationSegments);
    Clear();
    m_animationSegments = std::move(savedAnimSegments);

    // Parse the Type 8 file for sound file references only
    GW::Parsers::AnimationSoundEventFile soundFile;
    if (!GW::Parsers::SoundEventParser::Parse(fileData, fileSize, soundFile))
    {
        return false;
    }

    m_soundFileIds = std::move(soundFile.soundFileIds);

    char msg[256];
    sprintf_s(msg, "  Loading %zu sound files from Type 8 references:\n", m_soundFileIds.size());
    LogBB8Debug(msg);

    // Load sound file data from DAT
    m_loadedSounds.resize(m_soundFileIds.size());
    for (size_t i = 0; i < m_soundFileIds.size(); i++)
    {
        uint32_t fileId = m_soundFileIds[i];
        m_loadedSounds[i].fileId = fileId;

        // Find file in DAT managers
        for (auto& [alias, manager] : dat_managers)
        {
            if (!manager) continue;

            int mftIndex = -1;
            const auto& mft = manager->get_MFT();
            for (size_t j = 0; j < mft.size(); j++)
            {
                if (static_cast<uint32_t>(mft[j].Hash) == fileId)
                {
                    mftIndex = static_cast<int>(j);
                    break;
                }
            }

            if (mftIndex >= 0)
            {
                // Load the raw data
                uint8_t* rawData = manager->read_file(mftIndex);
                if (rawData)
                {
                    size_t dataSize = mft[mftIndex].uncompressedSize;
                    m_loadedSounds[i].data.assign(rawData, rawData + dataSize);
                    m_loadedSounds[i].isLoaded = true;
                    sprintf_s(msg, "    Sound[%zu]: fileId=0x%X, size=%zu bytes\n",
                        i, fileId, dataSize);
                    LogBB8Debug(msg);
                    delete[] rawData;
                }
                break;
            }
        }

        if (!m_loadedSounds[i].isLoaded)
        {
            sprintf_s(msg, "    Sound[%zu]: fileId=0x%X NOT FOUND in DAT\n", i, fileId);
            LogBB8Debug(msg);
        }
    }

    // Store max timing for scaling
    m_maxTiming = soundFile.maxTiming;

    // Create sound events from Type 8 bytecode events (NOT from effect entries!)
    // Effect entries are animation segment markers, not sound triggers.
    // The actual sound timing comes from the Type 8 file's event bytecode.
    if (!soundFile.events.empty() && !m_loadedSounds.empty())
    {
        m_soundEvents.clear();
        char msg[256];
        sprintf_s(msg, "  Type 8 bytecode: %zu events parsed, maxTiming=%u\n",
            soundFile.events.size(), soundFile.maxTiming);
        LogBB8Debug(msg);

        for (const auto& srcEvent : soundFile.events)
        {
            // Validate sound index
            if (srcEvent.soundIndex >= m_loadedSounds.size())
            {
                sprintf_s(msg, "    Event %u: INVALID soundIndex=%u (only %zu sounds loaded)\n",
                    srcEvent.eventIndex, srcEvent.soundIndex, m_loadedSounds.size());
                LogBB8Debug(msg);
                continue;
            }

            AnimSoundEvent event;
            // Store raw cumulative timing - will be scaled to animation time in Update()
            event.timing = static_cast<float>(srcEvent.timing);
            event.effectHash = 0;  // No effect association
            event.soundIndex = srcEvent.soundIndex;
            // Convert param (0-255) to volume (0.0-1.0)
            event.volume = static_cast<float>(srcEvent.param) / 255.0f;
            if (event.volume < 0.1f) event.volume = 1.0f;  // Default if not set

            sprintf_s(msg, "    Event %zu: timing=%u (raw frames), soundIndex=%u, fileId=0x%X, volume=%.2f\n",
                m_soundEvents.size(), srcEvent.timing, srcEvent.soundIndex,
                m_loadedSounds[srcEvent.soundIndex].fileId, event.volume);
            LogBB8Debug(msg);

            m_soundEvents.push_back(event);
        }

        m_triggerStates.resize(m_soundEvents.size());
        ResetTriggers();
    }

    return !m_loadedSounds.empty();
}

void AnimationSoundManager::SetTimingFromClip(const GW::Animation::AnimationClip& clip)
{
    // Store animation segments for timeline display (shows loop regions and sub-animations)
    // NOTE: Animation segments define regions, NOT sound triggers!
    // Sound timing comes from Type 8 files loaded via LoadFromType8File().
    m_animationSegments = clip.animationSegments;

    // Store animation info for reference
    m_animDuration = clip.maxTime - clip.minTime;

    // Type 8 timing values are cumulative frame counts, NOT normalized positions.
    // Convert them to animation time units assuming 30 fps.
    // Frame count / 30 = seconds, then * 100000 for animation units.
    constexpr float FRAMES_PER_SECOND = 30.0f;
    constexpr float ANIM_UNITS_PER_SECOND = 100000.0f;
    constexpr float FRAME_TO_ANIM_UNITS = ANIM_UNITS_PER_SECOND / FRAMES_PER_SECOND;

    if (!m_soundEvents.empty())
    {
        for (auto& event : m_soundEvents)
        {
            // event.timing is cumulative frame count from Type 8 bytecode
            // Convert to animation time units: frames * (100000 / 30)
            event.timing = event.timing * FRAME_TO_ANIM_UNITS;
        }
    }
}

void AnimationSoundManager::Clear()
{
    // Free all BASS streams
    if (is_bass_working && lpfnBassStreamFree)
    {
        for (auto& sound : m_loadedSounds)
        {
            if (sound.streamHandle != 0)
            {
                lpfnBassStreamFree(sound.streamHandle);
                sound.streamHandle = 0;
            }
        }
    }

    m_soundEvents.clear();
    m_animationSegments.clear();
    m_soundFileIds.clear();
    m_loadedSounds.clear();
    m_triggerStates.clear();
    m_loadedFileId = 0;
    m_maxTiming = 0;
    m_animDuration = 0.0f;
    m_lastTime = 0.0f;
}

void AnimationSoundManager::Update(float currentTime, float loopStartTime,
                                   float loopEndTime, bool isPlaying)
{
    if (!m_enabled || !isPlaying || m_soundEvents.empty())
    {
        m_wasPlaying = isPlaying;
        m_lastTime = currentTime;
        return;
    }

    // Detect loop (time went backwards significantly)
    if (currentTime < m_lastTime - 1000.0f)
    {
        ResetTriggers();
    }

    // Check each event for triggering
    for (size_t i = 0; i < m_soundEvents.size(); i++)
    {
        const auto& event = m_soundEvents[i];

        if (i >= m_triggerStates.size()) break;
        auto& state = m_triggerStates[i];

        // Event timing is relative to animation (from Type 8 bytecode)
        float eventTime = event.timing;

        // Only trigger if:
        // 1. Not already triggered this loop
        // 2. Current time is at or past event time
        // 3. Event time is within the playback range
        if (!state.hasTriggered &&
            currentTime >= eventTime &&
            eventTime >= loopStartTime &&
            eventTime <= loopEndTime)
        {
            TriggerSound(i);
            state.hasTriggered = true;
            state.lastTriggerTime = currentTime;
        }
    }

    m_wasPlaying = isPlaying;
    m_lastTime = currentTime;
}

void AnimationSoundManager::ResetTriggers()
{
    for (auto& state : m_triggerStates)
    {
        state.hasTriggered = false;
    }
}

void AnimationSoundManager::TriggerSound(size_t eventIndex)
{
    if (!is_bass_working || eventIndex >= m_soundEvents.size())
        return;

    const auto& event = m_soundEvents[eventIndex];
    size_t soundIdx = event.soundIndex;

    if (soundIdx >= m_loadedSounds.size())
        return;

    auto& sound = m_loadedSounds[soundIdx];

    if (!sound.isLoaded || sound.data.empty())
        return;

    // Create stream if not already created
    HSTREAM stream = CreateSoundStream(soundIdx);
    if (stream == 0)
    {
        char msg[128];
        sprintf_s(msg, "  BASS stream creation failed for sound %zu (fileId=0x%X)\n",
            soundIdx, sound.fileId);
        LogBB8Debug(msg);
        return;
    }

    // Set volume
    float finalVolume = m_volume * event.volume;
    if (lpfnBassChannelSetAttribute)
    {
        lpfnBassChannelSetAttribute(stream, BASS_ATTRIB_VOL, finalVolume);
    }

    // Play from the start
    if (lpfnBassChannelPlay)
    {
        lpfnBassChannelPlay(stream, TRUE);
        // Log successful trigger (only on first trigger to avoid spam)
        static size_t s_lastLoggedEvent = SIZE_MAX;
        if (s_lastLoggedEvent != eventIndex)
        {
            char msg[128];
            sprintf_s(msg, "  Playing sound: event %zu, soundIdx=%zu, fileId=0x%X, vol=%.2f, timing=%.1f\n",
                eventIndex, soundIdx, sound.fileId, finalVolume, event.timing);
            LogBB8Debug(msg);
            s_lastLoggedEvent = eventIndex;
        }
    }
}

HSTREAM AnimationSoundManager::CreateSoundStream(size_t soundIndex)
{
    if (soundIndex >= m_loadedSounds.size())
        return 0;

    auto& sound = m_loadedSounds[soundIndex];

    // If stream already exists, return it
    if (sound.streamHandle != 0)
        return sound.streamHandle;

    if (!lpfnBassStreamCreateFile || sound.data.empty())
        return 0;

    // Create stream from memory
    sound.streamHandle = lpfnBassStreamCreateFile(
        TRUE,                           // mem
        sound.data.data(),              // file
        0,                              // offset
        sound.data.size(),              // length
        BASS_STREAM_PRESCAN             // flags
    );

    return sound.streamHandle;
}

void AnimationSoundManager::SetVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

int32_t AnimationSoundManager::FindSoundByFileId(uint32_t fileId) const
{
    for (size_t i = 0; i < m_loadedSounds.size(); i++)
    {
        if (m_loadedSounds[i].fileId == fileId)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

float AnimationSoundManager::ScaleTimingToAnimUnits(uint32_t cumulativeTiming) const
{
    // Type 8 timing is cumulative frame count at 30 fps
    // Convert to animation units: frames / 30 * 100000
    constexpr float FRAMES_PER_SECOND = 30.0f;
    constexpr float ANIM_UNITS_PER_SECOND = 100000.0f;
    return static_cast<float>(cumulativeTiming) * (ANIM_UNITS_PER_SECOND / FRAMES_PER_SECOND);
}

} // namespace GW::Audio
