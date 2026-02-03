#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace GW::Parsers {

// Chunk IDs for Type 8 Animation Sound Event files
constexpr uint32_t CHUNK_ID_SOUND_REFS = 0x00000001;    // Sound file references (MP3s)
constexpr uint32_t CHUNK_ID_SOUND_EVENTS = 0x00000002;  // Sound event bytecode

// Bytecode opcodes for sound events
namespace SoundOpcode {
    constexpr uint8_t NOP = 0x00;
    constexpr uint8_t MARK1 = 0x01;
    constexpr uint8_t PUSH = 0x02;        // PUSH + u32 value
    constexpr uint8_t MARK3 = 0x03;
    constexpr uint8_t OP_04 = 0x04;
    constexpr uint8_t OP_05 = 0x05;
    constexpr uint8_t TRIGGER = 0x06;     // TRIGGER (followed by TIMING)
    constexpr uint8_t TIMING = 0x07;      // TIMING + u8 byte (cumulative, wraps at 256)
    constexpr uint8_t OP_08 = 0x08;
    constexpr uint8_t PARAM = 0x09;       // PARAM + u8 value (volume 0-255)
    constexpr uint8_t END_LF = 0x0A;
    constexpr uint8_t OP_0B = 0x0B;
    constexpr uint8_t SETUP_C = 0x0C;
    constexpr uint8_t END_CR = 0x0D;
    constexpr uint8_t SETUP_F = 0x0F;
    constexpr uint8_t SETUP_10 = 0x10;
    constexpr uint8_t SETUP_11 = 0x11;
    constexpr uint8_t OP_17 = 0x17;
    constexpr uint8_t HEADER_SEP = 0x18;
    constexpr uint8_t EMIT = 0x1A;        // EMIT event (uses accumulated state)
    constexpr uint8_t TIMING_SET = 0x1C;  // TIMING_SET (followed by TIMING opcode)
    constexpr uint8_t ALT_FORMAT = 0x2F;  // Alternative timing format
}

/**
 * @brief Sound file reference entry (6 bytes).
 *
 * References a sound file (typically MP3) that can be triggered.
 * Decoded file ID = (id0 - 0xFF00FF) + (id1 * 0xFF00)
 */
#pragma pack(push, 1)
struct SoundFileRef
{
    uint16_t id0;
    uint16_t id1;
    uint16_t flags;

    uint32_t DecodeFileId() const
    {
        int32_t result = static_cast<int32_t>(id0) - 0xFF00FF;
        result += static_cast<int32_t>(id1) * 0xFF00;
        return static_cast<uint32_t>(result);
    }
};
#pragma pack(pop)

static_assert(sizeof(SoundFileRef) == 6, "SoundFileRef must be 6 bytes!");

/**
 * @brief Decoded sound event information.
 *
 * Represents when and how to trigger a sound during animation playback.
 * Timing is stored as cumulative frame count (with 256 wrap-around detection).
 */
struct SoundEvent
{
    uint32_t timing = 0;       // Cumulative timing value (frame count with wrap handling)
    uint32_t soundIndex = 0;   // Index into sound file references (chunk 0x01)
    uint32_t param = 0;        // Volume/distance parameter (0-255)
    uint32_t eventIndex = 0;   // Event number (sequential)

    /**
     * @brief Convert timing to animation time units.
     *
     * The timing value is a cumulative frame count. To convert to animation
     * time units, multiply by a scale factor derived from the animation duration.
     *
     * @param maxTiming Maximum cumulative timing value from all events.
     * @param animDuration Animation duration in time units (100000 = 1 second).
     * @return Time in animation units.
     */
    float GetAnimationTime(uint32_t maxTiming, float animDuration) const
    {
        if (maxTiming == 0) return 0.0f;
        return (static_cast<float>(timing) / static_cast<float>(maxTiming)) * animDuration;
    }

    /**
     * @brief Convert timing to seconds assuming 30 fps frame rate.
     */
    float GetTimeSeconds30fps() const
    {
        return static_cast<float>(timing) / 30.0f;
    }
};

/**
 * @brief Parsed Animation Sound Event file (Type 8).
 *
 * Contains sound file references and timing events for animation playback.
 */
struct AnimationSoundEventFile
{
    std::vector<uint32_t> soundFileIds;  // Decoded sound file IDs
    std::vector<SoundEvent> events;       // Decoded sound events
    uint32_t maxTiming = 0;               // Maximum cumulative timing for scaling

    bool IsValid() const
    {
        return !soundFileIds.empty() || !events.empty();
    }

    void Clear()
    {
        soundFileIds.clear();
        events.clear();
        maxTiming = 0;
    }

    /**
     * @brief Get animation time for an event, scaled to animation duration.
     *
     * @param eventIndex Index of the event.
     * @param animDuration Total animation duration in time units.
     * @return Time in animation units, or 0 if invalid.
     */
    float GetEventAnimationTime(size_t eventIndex, float animDuration) const
    {
        if (eventIndex >= events.size() || maxTiming == 0) return 0.0f;
        return events[eventIndex].GetAnimationTime(maxTiming, animDuration);
    }
};

/**
 * @brief Parser for FFNA Type 8 Animation Sound Event files.
 *
 * These files contain:
 * - Chunk 0x01: Sound file references (which MP3 files to use)
 * - Chunk 0x02: Bytecode defining when to trigger sounds during animation
 */
class SoundEventParser
{
public:
    /**
     * @brief Parses a complete Type 8 file.
     *
     * @param fileData Complete FFNA file data.
     * @param fileSize File size in bytes.
     * @param outFile Output parsed sound event file.
     * @return true if parsing succeeded.
     */
    static bool Parse(const uint8_t* fileData, size_t fileSize, AnimationSoundEventFile& outFile)
    {
        outFile.Clear();

        // Verify FFNA signature
        if (fileSize < 5 || fileData[0] != 'f' || fileData[1] != 'f' ||
            fileData[2] != 'n' || fileData[3] != 'a')
        {
            return false;
        }

        // Get FFNA type (should be 8)
        uint8_t ffnaType = fileData[4];
        if (ffnaType != 8)
        {
            return false;  // Not a Type 8 file
        }

        // Parse chunks
        size_t offset = 5;
        while (offset + 8 <= fileSize)
        {
            uint32_t chunkId, chunkSize;
            std::memcpy(&chunkId, &fileData[offset], sizeof(uint32_t));
            std::memcpy(&chunkSize, &fileData[offset + 4], sizeof(uint32_t));

            if (chunkId == 0 || chunkSize == 0 || offset + 8 + chunkSize > fileSize)
            {
                break;
            }

            const uint8_t* chunkData = &fileData[offset + 8];

            if (chunkId == CHUNK_ID_SOUND_REFS)
            {
                ParseSoundRefs(chunkData, chunkSize, outFile.soundFileIds);
            }
            else if (chunkId == CHUNK_ID_SOUND_EVENTS)
            {
                ParseSoundEvents(chunkData, chunkSize, outFile.events, outFile.maxTiming);
            }

            offset += 8 + chunkSize;
        }

        return outFile.IsValid();
    }

private:
    /**
     * @brief Parses sound file references from chunk 0x01.
     *
     * Format: No count field, number of entries = chunkSize / 6
     */
    static bool ParseSoundRefs(const uint8_t* data, size_t dataSize, std::vector<uint32_t>& outFileIds)
    {
        size_t numEntries = dataSize / sizeof(SoundFileRef);

        for (size_t i = 0; i < numEntries; i++)
        {
            size_t offset = i * sizeof(SoundFileRef);
            if (offset + sizeof(SoundFileRef) > dataSize)
            {
                break;
            }

            SoundFileRef ref;
            std::memcpy(&ref, &data[offset], sizeof(SoundFileRef));

            uint32_t fileId = ref.DecodeFileId();
            outFileIds.push_back(fileId);
        }

        return !outFileIds.empty();
    }

    /**
     * @brief Parses sound event bytecode from chunk 0x02.
     *
     * Two bytecode formats exist:
     *
     * Format 1 (Simple files - no EMIT opcode):
     *   PUSH <event_index>
     *   TRIGGER (0x06)
     *   TIMING <byte>
     *   NOP
     *   PUSH <sound_index>
     *   MARK1 MARK1 MARK3
     *   PARAM <volume>
     *
     * Format 2 (Complex files - uses EMIT opcode):
     *   TIMING_SET (0x1C) or ALT_FORMAT (0x2F)
     *   TIMING <byte>
     *   ... various PUSH values ...
     *   PUSH <sound_index>
     *   EMIT (0x1A)
     *   PARAM <volume>
     *
     * @param data Bytecode data from chunk 0x02.
     * @param dataSize Size of bytecode data.
     * @param outEvents Output vector of parsed events.
     * @param outMaxTiming Output maximum cumulative timing value.
     * @return true if any events were parsed.
     */
    static bool ParseSoundEvents(const uint8_t* data, size_t dataSize,
                                 std::vector<SoundEvent>& outEvents,
                                 uint32_t& outMaxTiming)
    {
        outMaxTiming = 0;

        // Cumulative timing tracking
        uint32_t cumulativeTiming = 0;
        uint8_t prevTimingByte = 0;
        uint32_t currentTiming = 0;

        // Stack for PUSH values
        std::vector<uint32_t> stack;

        // Current event being built (for Format 1)
        uint32_t eventCounter = 0;
        uint32_t pendingParam = 255;  // Default volume
        bool afterTrigger = false;     // Tracks if we're building an event after TRIGGER

        // First pass: detect if this uses EMIT or TRIGGER-based events
        bool hasEmit = false;
        for (size_t i = 0; i < dataSize; i++)
        {
            if (data[i] == SoundOpcode::EMIT)
            {
                hasEmit = true;
                break;
            }
        }

        size_t pos = 0;
        while (pos < dataSize)
        {
            uint8_t opcode = data[pos];
            pos++;

            switch (opcode)
            {
            case SoundOpcode::PUSH:
                // PUSH takes a 4-byte value
                if (pos + 4 <= dataSize)
                {
                    uint32_t value;
                    std::memcpy(&value, &data[pos], sizeof(uint32_t));
                    pos += 4;
                    stack.push_back(value);
                }
                break;

            case SoundOpcode::TIMING:
                // TIMING takes a 1-byte value - cumulative with wrap detection
                if (pos < dataSize)
                {
                    uint8_t timingByte = data[pos];
                    pos++;

                    // Detect wrap-around: if current byte < previous, we wrapped
                    if (timingByte < prevTimingByte)
                    {
                        cumulativeTiming += 256;
                    }
                    currentTiming = cumulativeTiming + timingByte;
                    prevTimingByte = timingByte;

                    if (currentTiming > outMaxTiming)
                    {
                        outMaxTiming = currentTiming;
                    }
                }
                break;

            case SoundOpcode::PARAM:
                // PARAM takes a 1-byte value (volume 0-255)
                if (pos < dataSize)
                {
                    pendingParam = data[pos];
                    pos++;

                    // For Format 1 (TRIGGER-based): PARAM completes the event
                    if (!hasEmit && afterTrigger && stack.size() >= 1)
                    {
                        // The last small PUSH value after TIMING is the sound index
                        uint32_t soundIndex = 0;
                        bool foundSoundIndex = false;
                        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
                        {
                            if (*it < 100)
                            {
                                soundIndex = *it;
                                foundSoundIndex = true;
                                break;
                            }
                        }

                        if (foundSoundIndex)
                        {
                            SoundEvent event;
                            event.timing = currentTiming;
                            event.soundIndex = soundIndex;
                            event.param = pendingParam;
                            event.eventIndex = eventCounter++;
                            outEvents.push_back(event);
                        }

                        afterTrigger = false;
                        stack.clear();
                        pendingParam = 255;
                    }
                }
                break;

            case SoundOpcode::EMIT:
                // EMIT creates a sound event (Format 2)
                // Look for sound index in the stack (small values 0-99)
                {
                    uint32_t soundIndex = 0;
                    bool foundSoundIndex = false;

                    // Search stack from end for a small value (sound index)
                    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
                    {
                        if (*it < 100)  // Sound indices are small
                        {
                            soundIndex = *it;
                            foundSoundIndex = true;
                            break;
                        }
                    }

                    if (foundSoundIndex)
                    {
                        SoundEvent event;
                        event.timing = currentTiming;
                        event.soundIndex = soundIndex;
                        event.param = pendingParam;
                        event.eventIndex = eventCounter++;
                        outEvents.push_back(event);
                    }

                    stack.clear();
                    pendingParam = 255;  // Reset to default
                }
                break;

            case SoundOpcode::TRIGGER:
                // TRIGGER marks the start of an event sequence
                afterTrigger = true;
                stack.clear();
                break;

            case SoundOpcode::TIMING_SET:
            case SoundOpcode::ALT_FORMAT:
                // These precede TIMING opcode - no action needed
                break;

            case SoundOpcode::HEADER_SEP:
                // Section separator - reset state
                stack.clear();
                afterTrigger = false;
                break;

            case SoundOpcode::NOP:
            case SoundOpcode::MARK1:
            case SoundOpcode::MARK3:
            case SoundOpcode::OP_04:
            case SoundOpcode::OP_05:
            case SoundOpcode::OP_08:
            case SoundOpcode::OP_0B:
            case SoundOpcode::SETUP_C:
            case SoundOpcode::SETUP_F:
            case SoundOpcode::SETUP_10:
            case SoundOpcode::SETUP_11:
            case SoundOpcode::OP_17:
                // Single-byte opcodes, no additional data
                break;

            case SoundOpcode::END_CR:
            case SoundOpcode::END_LF:
                // End of bytecode section
                break;

            default:
                // Unknown opcode, skip
                break;
            }
        }

        return !outEvents.empty();
    }

    /**
     * @brief Legacy wrapper for ParseSoundEvents without max timing output.
     */
    static bool ParseSoundEvents(const uint8_t* data, size_t dataSize, std::vector<SoundEvent>& outEvents)
    {
        uint32_t maxTiming;
        return ParseSoundEvents(data, dataSize, outEvents, maxTiming);
    }
};

} // namespace GW::Parsers
