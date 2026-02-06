#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <cstdio>

namespace GW::Animation {

/**
 * @brief Animation identification system extracted from Gw.exe.
 *
 * Guild Wars uses multiple hash/ID systems for animations:
 *
 * 1. Animation State Indices (0-62): Internal indices for animation "types"
 *    Source: g_animSequenceFallbackTable at 0x00a510e0
 *
 * 2. Fallback Table Hashes: Universal type identifiers (e.g., 0x37306E37 = Cheer)
 *    These identify animation concepts across all models.
 *
 * 3. Segment Hashes (Search Keys): Computed from fallback hash + bone slot
 *    Formula from AvCharAnim_lookup_anim_index @ 0x007c5180:
 *      segment_hash = boneSlotChar + 0xDFFFF9F + (fallback_hash * 0x17 - adjustments)
 *    where adjustments depend on boneSlotChar comparisons with 'y','z','v','u','j'
 *
 * 4. Sequence IDs: Model-specific numeric IDs (e.g., 0x8EBF, 0x9A20)
 *    Found in Agent_GetAnimationSoundId and animation files.
 */

/**
 * @brief Bone slot characters from g_animBoneSlotPriorityTable @ 0x00a518c0
 * Used in segment hash computation.
 * Extended list includes all 12 known slots plus 'G' which appears to be used for some models.
 */
static const char g_boneSlotChars[] = {
    'u', 's', 'w', 'h', 'b', 't', 'p', 'r',  // Primary slots 0-7
    'c', 'd', 'y', 'a',                       // Additional slots 8-11
    'G'                                        // Special slot (0x47) observed in some models
};
static const size_t g_boneSlotCount = sizeof(g_boneSlotChars) / sizeof(g_boneSlotChars[0]);

/**
 * @brief Computes adjustments for segment hash based on bone slot character.
 * From AvCharAnim_lookup_anim_index decompilation.
 */
inline int32_t ComputeBoneSlotAdjustment(char c)
{
    int32_t adj = 0;
    if ('y' < c) adj += 1;      // (uint)('y' < cVar1)
    if (c == 'z') adj += 1;     // (uint)(cVar1 == 'z')
    if (c == 'v') adj += 4;     // ((cVar1 != 'v') - 1 & 4) = 4 if equal
    if ('u' < c) adj += 1;      // (uint)('u' < cVar1)
    if ('j' < c) adj += 1;      // (uint)('j' < cVar1)
    return adj;
}

/**
 * @brief Computes segment hash from fallback table hash and bone slot.
 * Reverse-engineered from AvCharAnim_lookup_anim_index @ 0x007c5180
 */
inline uint32_t ComputeSegmentHash(uint32_t fallbackHash, char boneSlotChar)
{
    int32_t adj = ComputeBoneSlotAdjustment(boneSlotChar);
    // segment_hash = char + (-0x20000061) + (fallback_hash * 0x17 - adj)
    // = char - 0x20000061 + fallback_hash * 0x17 - adj
    // = char + 0xDFFFF9F + fallback_hash * 0x17 - adj  (using unsigned wraparound)
    uint32_t result = (uint32_t)boneSlotChar + 0xDFFFF9Fu + (fallbackHash * 0x17u) - (uint32_t)adj;
    return result;
}

/**
 * @brief Reverses segment hash to get fallback table hash.
 * Returns 0 if the segment hash doesn't correspond to a valid fallback hash.
 */
inline uint32_t ReverseSegmentHash(uint32_t segmentHash, char boneSlotChar)
{
    int32_t adj = ComputeBoneSlotAdjustment(boneSlotChar);
    // Reverse: fallback_hash = (segment_hash - char - 0xDFFFF9F + adj) / 0x17
    uint32_t numerator = segmentHash - (uint32_t)boneSlotChar - 0xDFFFF9Fu + (uint32_t)adj;

    // Check if divisible by 0x17 (23)
    if (numerator % 0x17 != 0) {
        return 0;  // Not a valid segment hash for this bone slot
    }

    return numerator / 0x17;
}

// Note: Sound event IDs (from Agent_GetAnimationSoundId @ Gw.exe) are NOT used for animation naming.
// They map sequence animationIds to footstep sound types for audio playback only.
// The IDs (0x8EBF, 0x9A20, etc.) should NOT be displayed as animation names.

/**
 * @brief Animation state entry from the fallback table.
 */
struct AnimationStateEntry
{
    uint32_t primaryHash;      // Primary sequence hash (upper 24 bits significant)
    uint32_t fallbackHash;     // Fallback sequence hash (0xFFFFFFFF if none)
    uint32_t flags;            // Animation flags
    const char* name;          // Human-readable name
    const char* category;      // Animation category
};

/**
 * @brief Animation state table extracted from Gw.exe @ 0x00a510e0.
 *
 * The upper 24 bits of segment hashes in files match these type hashes.
 * E.g., segment 0x37306E83 matches Cheer (0x37306E37) via mask 0xFFFFFF00.
 */
static const AnimationStateEntry g_animationStateTable[] = {
    // Index 0x00-0x02: Idle states
    {0x33E48DF5, 0xFFFFFFFF, 0x00000000, "Idle/Stand", "Idle"},
    {0x33E48D3C, 0xFFFFFFFF, 0x00000000, "Idle Variant", "Idle"},
    {0x33E46F23, 0xFFFFFFFF, 0x00000000, "Idle Variant 2", "Idle"},

    // Index 0x03-0x0D: Combat states
    {0x3712868A, 0xFFFFFFFF, 0x00000001, "Combat Ready", "Combat"},
    {0x372F7BDC, 0xFFFFFFFF, 0x00000010, "Attack 1", "Combat"},
    {0x370D234D, 0x33E3FEA7, 0x00000010, "Attack 2", "Combat"},
    {0x33E46E31, 0xFFFFFFFF, 0x00000010, "Attack 3", "Combat"},
    {0x37128598, 0xFFFFFFFF, 0x00000011, "Skill Cast", "Combat"},
    {0x31B7057B, 0xFFFFFFFF, 0x00000018, "Knockdown", "Combat"},
    {0x372F7CCE, 0xFFFFFFFF, 0x00000018, "Get Up", "Combat"},
    {0x300025FB, 0xFFFFFFFF, 0x00000018, "Flinch/Hit", "Combat"},
    {0x370D243F, 0xFFFFFFFF, 0x00000018, "Block", "Combat"},
    {0xFFFFFFFD, 0xFFFFFFFF, 0x00000020, "Death", "Combat"},
    {0xFFFFFFFD, 0xFFFFFFFF, 0x00000020, "Dead/Corpse", "Combat"},

    // Index 0x0E-0x10: Basic emotes
    {0x3001CE8E, 0xFFFFFFFF, 0x00000030, "Emote 1", "Emote"},
    {0x30022D0D, 0xFFFFFFFF, 0x00000030, "Emote 2", "Emote"},
    {0x33BBD495, 0xFFFFFFFF, 0x00000030, "Emote 3", "Emote"},

    // Index 0x11-0x20: Movement states
    {0x365BE353, 0xFFFFFFFF, 0x00000040, "Slow Move", "Movement"},
    {0x337428C9, 0xFFFFFFFF, 0x00000040, "Walk Backwards", "Movement"},
    {0x3712C1C2, 0xFFFFFFFF, 0x00000040, "Strafe Right", "Movement"},
    {0x372C6088, 0xFFFFFFFF, 0x00000040, "Move Backwards", "Movement"},
    {0x365BD0E4, 0xFFFFFFFF, 0x00000040, "Walk Forward", "Movement"},
    {0x37136D6F, 0xFFFFFFFF, 0x00000040, "Walk Variant", "Movement"},
    {0x372D0C35, 0xFFFFFFFF, 0x00000040, "Walk Armed", "Movement"},
    {0x365BEA51, 0xFFFFFFFF, 0x00000040, "Run Forward", "Movement"},
    {0x304ED229, 0xFFFFFFFF, 0x00000040, "Run Variant", "Movement"},
    {0x304FEF53, 0xFFFFFFFF, 0x00000040, "Run Armed", "Movement"},
    {0x371386DC, 0xFFFFFFFF, 0x00000040, "Run Back", "Movement"},
    {0x372D25A2, 0xFFFFFFFF, 0x00000040, "Run Back Armed", "Movement"},
    {0x35A3BF31, 0x00000000, 0x00000040, "Swimming", "Movement"},
    {0x365BF223, 0x37316AC3, 0x00000040, "Combat Move", "Movement"},
    {0x36FF1A97, 0xFFFFFFFF, 0x00000040, "Turn Left", "Movement"},
    {0x304FEF10, 0xFFFFFFFF, 0x00000040, "Turn Right", "Movement"},

    // Index 0x21-0x22: Weapon actions
    {0x35B1C59C, 0xFFFFFFFF, 0x00000050, "Weapon Draw", "Action"},
    {0x33D7112B, 0x300F0B66, 0x00000050, "Weapon Sheathe", "Action"},

    // Index 0x23-0x28: Skill casting
    {0x3712868A, 0xFFFFFFFF, 0x00000110, "Skill Channel", "Combat"},
    {0x30321B82, 0xFFFFFFFF, 0x00000110, "Skill Channel 2", "Combat"},
    {0x3158BD67, 0xFFFFFFFF, 0x00000110, "Skill Finish", "Combat"},
    {0x36557AF2, 0xFFFFFFFF, 0x00000110, "Skill Cast 2", "Combat"},
    {0x3719E6E0, 0xFFFFFFFF, 0x00000110, "Spell Cast", "Combat"},
    {0x305EA15B, 0xFFFFFFFF, 0x00000110, "Spell Channel", "Combat"},

    // Index 0x29-0x34: Emotes and actions
    {0x337C96C1, 0xFFFFFFFF, 0x00000120, "Activate", "Action"},
    {0x3001DCFE, 0xFFFFFFFF, 0x00000120, "Activate 2", "Action"},
    {0x3011E7B1, 0xFFFFFFFF, 0x00000120, "Use Object", "Action"},
    {0x34D2901F, 0xFFFFFFFF, 0x00000120, "Dance", "Emote"},
    {0xFFFFFFFC, 0xFFFFFFFF, 0x00000120, "Dance Continue", "Emote"},
    {0x34249693, 0xFFFFFFFF, 0x00000120, "Sit", "Emote"},
    {0x35621277, 0xFFFFFFFF, 0x00000120, "Sit Continue", "Emote"},
    {0x30398267, 0xFFFFFFFF, 0x00000120, "Laugh", "Emote"},
    {0x31602449, 0xFFFFFFFF, 0x00000120, "Bow", "Emote"},
    {0x305009BE, 0xFFFFFFFF, 0x00000120, "Point", "Emote"},
    {0x3176ABA3, 0xFFFFFFFF, 0x00000120, "Wave", "Emote"},
    {0x37306E37, 0x30500FF1, 0x00000120, "Cheer", "Emote"},

    // Index 0x35-0x37: Instrument
    {0x338019B5, 0xFFFFFFFF, 0x00000180, "Play Instrument", "Emote"},
    {0x36738A6D, 0xFFFFFFFF, 0x00000180, "Instrument 2", "Emote"},
    {0x338019B5, 0x36D2D133, 0x00000200, "Instrument Loop", "Emote"},

    // Index 0x38-0x3E: Special
    {0x3001EFF5, 0xFFFFFFFF, 0x00000201, "Special 1", "Special"},
    {0x33E47BF0, 0xFFFFFFFF, 0x00000200, "Special 2", "Special"},
    {0x33D7112B, 0x300F0B66, 0x00000201, "Special 3", "Special"},
      {0xFFFFFFFE, 0xFFFFFFFF, 0x00000300, "Collect/Pickup", "Action"},
      {0xFFFFFFFE, 0xFFFFFFFF, 0x00000300, "Collect 2", "Action"},
      {0x30213BC4, 0xFFFFFFFF, 0x00000500, "Cinematic", "Special"},
      {0x30213BC4, 0xFFFFFFFF, 0x00000500, "Cinematic 2", "Special"},

      // Custom/derived hashes (from segment hash reverse)
      {0x018BFBaf, 0xFFFFFFFF, 0x00000120, "Warrior Dance (Male)", "Emote"},
  };

static const size_t g_animationStateCount = sizeof(g_animationStateTable) / sizeof(g_animationStateTable[0]);

/**
 * @brief Movement animation table indices by direction and movement type.
 */
struct MovementAnimationTable
{
    const char* name;
    uint8_t indices[8];
};

static const MovementAnimationTable g_movementTables[] = {
    {"Swimming",  {0x1D, 0x1D, 0x1D, 0x14, 0x12, 0x13, 0x1D, 0x1D}},
    {"Combat",    {0x1E, 0x1E, 0x1A, 0x14, 0x12, 0x13, 0x19, 0x1E}},
    {"Running",   {0x18, 0x1C, 0x1A, 0x14, 0x12, 0x13, 0x19, 0x1B}},
    {"Walking",   {0x15, 0x17, 0x1A, 0x14, 0x12, 0x13, 0x19, 0x16}},
    {"Slow",      {0x11, 0x11, 0x11, 0x14, 0x12, 0x13, 0x11, 0x11}},
};

static const size_t g_movementTableCount = sizeof(g_movementTables) / sizeof(g_movementTables[0]);

/**
 * @brief Lookup class for animation hashes with fuzzy matching support.
 */
class AnimationHashLookup
{
private:
    // Exact hash matches
    std::unordered_map<uint32_t, const AnimationStateEntry*> m_hashToEntry;
    std::unordered_map<uint32_t, size_t> m_hashToStateIndex;

    // Upper 24-bit matches (for segment hashes)
    std::unordered_map<uint32_t, const AnimationStateEntry*> m_maskedHashToEntry;

    // Lower 16-bit matches for computed segment hashes (bone slot 'u')
    std::unordered_map<uint16_t, const AnimationStateEntry*> m_lower16ToEntry;

    // Exact per-segment display name/category overrides
    std::unordered_map<uint32_t, const char*> m_exactNameOverrides;
    std::unordered_map<uint32_t, const char*> m_exactCategoryOverrides;

    bool m_initialized = false;

    void Initialize()
    {
        if (m_initialized) return;

        // User-provided explicit segment naming overrides.
        m_exactNameOverrides.emplace(0x8985FC26u, "Idle (RH open. LH closed)");
        m_exactNameOverrides.emplace(0x8985FC2Cu, "Idle (2H Carrying flag)");
        m_exactNameOverrides.emplace(0x8985FC36u, "Idle (Both hands closed)");
        m_exactNameOverrides.emplace(0x8985FC38u, "Idle (Both hands open)");
        m_exactNameOverrides.emplace(0x8935FC39u, "Idle (RH closed. LH open)");
        m_exactNameOverrides.emplace(0x339EC012u, "Opening mouth");
        m_exactNameOverrides.emplace(0x319BD0E7u, "1H melee attack (Stab, right to left swing)");
        m_exactNameOverrides.emplace(0x80318B57u, "1H spear ranged attack");
        m_exactNameOverrides.emplace(0x08318B6Du, "2H melee attack (right to left swing)");
        m_exactNameOverrides.emplace(0x80318B6Du, "2H melee attack (right to left, then left to right)");

        m_exactCategoryOverrides.emplace(0x8985FC26u, "Idle");
        m_exactCategoryOverrides.emplace(0x8985FC2Cu, "Idle");
        m_exactCategoryOverrides.emplace(0x8985FC36u, "Idle");
        m_exactCategoryOverrides.emplace(0x8985FC38u, "Idle");
        m_exactCategoryOverrides.emplace(0x8935FC39u, "Idle");
        m_exactCategoryOverrides.emplace(0x339EC012u, "Emote");
        m_exactCategoryOverrides.emplace(0x319BD0E7u, "Combat");
        m_exactCategoryOverrides.emplace(0x80318B57u, "Combat");
        m_exactCategoryOverrides.emplace(0x08318B6Du, "Combat");
        m_exactCategoryOverrides.emplace(0x80318B6Du, "Combat");

        // Build exact match table
        for (size_t i = 0; i < g_animationStateCount; i++)
        {
            const auto& entry = g_animationStateTable[i];

            // Exact match
            m_hashToEntry[entry.primaryHash] = &entry;
            m_hashToStateIndex[entry.primaryHash] = i;

            // Upper 24-bit masked match (for segment hash matching)
            uint32_t masked = entry.primaryHash & 0xFFFFFF00;
            if (m_maskedHashToEntry.find(masked) == m_maskedHashToEntry.end())
            {
                m_maskedHashToEntry[masked] = &entry;
            }

            // Also map fallback hash
            if (entry.fallbackHash != 0xFFFFFFFF && entry.fallbackHash != 0)
            {
                if (m_hashToEntry.find(entry.fallbackHash) == m_hashToEntry.end())
                {
                    m_hashToEntry[entry.fallbackHash] = &entry;
                    m_hashToStateIndex[entry.fallbackHash] = i;
                }
                uint32_t maskedFallback = entry.fallbackHash & 0xFFFFFF00;
                if (m_maskedHashToEntry.find(maskedFallback) == m_maskedHashToEntry.end())
                {
                    m_maskedHashToEntry[maskedFallback] = &entry;
                }
            }

            // Build lower 16-bit lookup from computed segment hashes
            // Try multiple bone slots to maximize coverage
            for (size_t slot = 0; slot < g_boneSlotCount; slot++)
            {
                uint32_t computedSeg = ComputeSegmentHash(entry.primaryHash, g_boneSlotChars[slot]);
                uint16_t lower16 = static_cast<uint16_t>(computedSeg & 0xFFFF);
                // Only add if not already present (first match wins)
                if (m_lower16ToEntry.find(lower16) == m_lower16ToEntry.end())
                {
                    m_lower16ToEntry[lower16] = &entry;
                }
            }
        }

        m_initialized = true;
    }

public:
    static AnimationHashLookup& Instance()
    {
        static AnimationHashLookup instance;
        return instance;
    }

    /**
     * @brief Looks up animation name by segment hash.
     *
     * Segment hashes are computed from fallback table hashes using a formula that
     * includes bone slot character and multiplier. The lower 16 bits tend to be
     * consistent across different models/bone slots.
     *
     * Priority order:
     * 1. Exact match against fallback table hashes
     * 2. Reverse segment hash computation for each bone slot
     * 3. Lower 16-bit match against precomputed segment hashes (fast hash table lookup)
     */
    std::string GetAnimationName(uint32_t hash)
    {
        Initialize();

        // Exact per-segment override.
        auto overrideIt = m_exactNameOverrides.find(hash);
        if (overrideIt != m_exactNameOverrides.end())
        {
            return overrideIt->second;
        }

        // Try exact match against fallback table first
        auto it = m_hashToEntry.find(hash);
        if (it != m_hashToEntry.end())
        {
            return it->second->name;
        }

        // Try reversing segment hash for each bone slot
        for (size_t slot = 0; slot < g_boneSlotCount; slot++)
        {
            uint32_t fallbackHash = ReverseSegmentHash(hash, g_boneSlotChars[slot]);
            if (fallbackHash != 0)
            {
                auto fallbackIt = m_hashToEntry.find(fallbackHash);
                if (fallbackIt != m_hashToEntry.end())
                {
                    return fallbackIt->second->name;
                }
            }
        }

        // Try matching lower 16 bits against precomputed segment hashes
        // This works because the lower bits are consistent across models
        uint16_t lower16 = static_cast<uint16_t>(hash & 0xFFFF);
        auto lower16It = m_lower16ToEntry.find(lower16);
        if (lower16It != m_lower16ToEntry.end())
        {
            return lower16It->second->name;
        }

        return "";
    }

    /**
     * @brief Looks up animation category by hash.
     */
    std::string GetAnimationCategory(uint32_t hash)
    {
        Initialize();

        // Exact per-segment override.
        auto overrideIt = m_exactCategoryOverrides.find(hash);
        if (overrideIt != m_exactCategoryOverrides.end())
        {
            return overrideIt->second;
        }

        // Exact match
        auto it = m_hashToEntry.find(hash);
        if (it != m_hashToEntry.end())
        {
            return it->second->category;
        }

        // Try reversing segment hash for each bone slot
        for (size_t slot = 0; slot < g_boneSlotCount; slot++)
        {
            uint32_t fallbackHash = ReverseSegmentHash(hash, g_boneSlotChars[slot]);
            if (fallbackHash != 0)
            {
                auto fallbackIt = m_hashToEntry.find(fallbackHash);
                if (fallbackIt != m_hashToEntry.end())
                {
                    return fallbackIt->second->category;
                }
            }
        }

        // Lower 16-bit match against precomputed segment hashes
        uint16_t lower16 = static_cast<uint16_t>(hash & 0xFFFF);
        auto lower16It = m_lower16ToEntry.find(lower16);
        if (lower16It != m_lower16ToEntry.end())
        {
            return lower16It->second->category;
        }

        // Upper 24-bit masked match (legacy fallback)
        uint32_t masked = hash & 0xFFFFFF00;
        auto maskedIt = m_maskedHashToEntry.find(masked);
        if (maskedIt != m_maskedHashToEntry.end())
        {
            return maskedIt->second->category;
        }

        return "";
    }

    /**
     * @brief Gets the animation state index for a hash.
     */
    int GetStateIndex(uint32_t hash)
    {
        Initialize();

        auto it = m_hashToStateIndex.find(hash);
        if (it != m_hashToStateIndex.end())
        {
            return static_cast<int>(it->second);
        }

        // Try masked match
        uint32_t masked = hash & 0xFFFFFF00;
        for (size_t i = 0; i < g_animationStateCount; i++)
        {
            if ((g_animationStateTable[i].primaryHash & 0xFFFFFF00) == masked)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    /**
     * @brief Gets a display name for an animation, with fallback to hex format.
     */
    std::string GetDisplayName(uint32_t hash)
    {
        std::string name = GetAnimationName(hash);
        if (!name.empty())
        {
            return name;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%08X", hash);
        return buf;
    }

    /**
     * @brief Gets a categorized display name for an animation.
     */
    std::string GetCategorizedDisplayName(uint32_t hash)
    {
        Initialize();

        std::string name = GetAnimationName(hash);
        std::string category = GetAnimationCategory(hash);

        if (!name.empty() && !category.empty())
        {
            return "[" + category + "] " + name;
        }
        else if (!name.empty())
        {
            return name;
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "0x%08X", hash);
        return buf;
    }

    /**
     * @brief Checks if a hash matches any known animation.
     */
    bool IsKnownAnimation(uint32_t hash)
    {
        Initialize();

        if (m_hashToEntry.find(hash) != m_hashToEntry.end())
            return true;

        // Check lower 16-bit match
        uint16_t lower16 = static_cast<uint16_t>(hash & 0xFFFF);
        if (m_lower16ToEntry.find(lower16) != m_lower16ToEntry.end())
            return true;

        uint32_t masked = hash & 0xFFFFFF00;
        return m_maskedHashToEntry.find(masked) != m_maskedHashToEntry.end();
    }

    /**
     * @brief Checks if a hash is a known movement animation.
     */
    bool IsMovementAnimation(uint32_t hash)
    {
        int idx = GetStateIndex(hash);
        return idx >= 0x11 && idx <= 0x20;
    }

    /**
     * @brief Checks if a hash is a known emote animation.
     */
    bool IsEmoteAnimation(uint32_t hash)
    {
        std::string cat = GetAnimationCategory(hash);
        return cat == "Emote";
    }

    /**
     * @brief Checks if a hash is a known combat animation.
     */
    bool IsCombatAnimation(uint32_t hash)
    {
        std::string cat = GetAnimationCategory(hash);
        return cat == "Combat";
    }
};

// Convenience functions
inline std::string GetAnimationNameFromHash(uint32_t hash)
{
    return AnimationHashLookup::Instance().GetAnimationName(hash);
}

inline std::string GetAnimationDisplayName(uint32_t hash)
{
    return AnimationHashLookup::Instance().GetDisplayName(hash);
}

inline std::string GetAnimationCategorizedName(uint32_t hash)
{
    return AnimationHashLookup::Instance().GetCategorizedDisplayName(hash);
}

inline bool IsKnownAnimationHash(uint32_t hash)
{
    return AnimationHashLookup::Instance().IsKnownAnimation(hash);
}

/**
 * @brief Debug function to dump computed segment hashes for all bone slots.
 *
 * Shows what segment hashes would be generated for each animation type and bone slot.
 */
inline void DebugDumpComputedSegmentHashes()
{
    char debugMsg[512];

    // Show for both 'u' (standard) and 'G' (observed in some models)
    const char testSlots[] = { 'u', 'G' };

    for (char slot : testSlots)
    {
        snprintf(debugMsg, sizeof(debugMsg), "[GWAnimHashes] Computed segment hashes for bone slot '%c' (0x%02X):\n", slot, (unsigned char)slot);
#ifdef _WIN32
        OutputDebugStringA(debugMsg);
#endif

        for (size_t i = 0; i < g_animationStateCount; i++)
        {
            const auto& entry = g_animationStateTable[i];
            uint32_t segHash = ComputeSegmentHash(entry.primaryHash, slot);
            snprintf(debugMsg, sizeof(debugMsg),
                "[GWAnimHashes]   fallback=0x%08X -> segment=0x%08X '%s'\n",
                entry.primaryHash, segHash, entry.name);
#ifdef _WIN32
            OutputDebugStringA(debugMsg);
#endif
        }
    }
}

/**
 * @brief Debug function to try reversing a segment hash.
 */
inline void DebugReverseSegmentHash(uint32_t segmentHash)
{
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "[GWAnimHashes] Trying to reverse segment hash 0x%08X:\n", segmentHash);
#ifdef _WIN32
    OutputDebugStringA(debugMsg);
#endif

    for (size_t slot = 0; slot < g_boneSlotCount; slot++)
    {
        char c = g_boneSlotChars[slot];
        uint32_t fallbackHash = ReverseSegmentHash(segmentHash, c);
        if (fallbackHash != 0)
        {
            // Check if this fallback hash is in our table
            std::string name = "";
            for (size_t i = 0; i < g_animationStateCount; i++)
            {
                if (g_animationStateTable[i].primaryHash == fallbackHash)
                {
                    name = g_animationStateTable[i].name;
                    break;
                }
            }
            snprintf(debugMsg, sizeof(debugMsg),
                "[GWAnimHashes]   slot '%c': fallback=0x%08X -> '%s'\n",
                c, fallbackHash, name.empty() ? "(not in table)" : name.c_str());
#ifdef _WIN32
            OutputDebugStringA(debugMsg);
#endif
        }
    }
}

/**
 * @brief Debug function to verify the hash lookup is working correctly.
 *
 * Call this to check if the lookup tables are properly initialized.
 * Prints results to OutputDebugString (visible in debugger output window).
 *
 * @return true if all test lookups succeed.
 */
inline bool DebugVerifyHashLookup()
{
    bool allPassed = true;

    struct TestCase {
        uint32_t hash;
        const char* expectedName;
        const char* description;
    };

    // Test exact matches from fallback table
    TestCase exactTests[] = {
        {0x37306E37, "Cheer", "exact match Cheer"},
        {0x33E48DF5, "Idle/Stand", "exact match Idle"},
        {0x34D2901F, "Dance", "exact match Dance"},
    };

    // Test actual segment hashes from user's animation files
    // These should match via lower 16-bit lookup against computed segment hashes
    struct SegmentTestCase {
        uint32_t hash;
        const char* expectedName;  // Empty string if no match expected
    };
    SegmentTestCase segmentTests[] = {
        {0x8985FC26, "Idle (RH open. LH closed)"},
        {0x8985FC2C, "Idle (2H Carrying flag)"},
        {0x8985FC36, "Idle (Both hands closed)"},
        {0x8985FC38, "Idle (Both hands open)"},
        {0x8935FC39, "Idle (RH closed. LH open)"},
        {0x339EC012, "Opening mouth"},
        {0x319BD0E7, "1H melee attack (Stab, right to left swing)"},
        {0x80318B57, "1H spear ranged attack"},
        {0x08318B6D, "2H melee attack (right to left swing)"},
        {0x80318B6D, "2H melee attack (right to left, then left to right)"},
        {0xC2420D5A, "Run Forward"},     // lower 16 = 0D5A matches computed segment
        {0xD0EB63A4, "Turn Left"},       // lower 16 = 63A4 matches computed segment
        {0x358F7A68, ""},                // lower 16 = 7A68 - no match expected
    };

    char debugMsg[256];

    // Test exact matches
    snprintf(debugMsg, sizeof(debugMsg), "[GWAnimHashes] Testing exact match lookups...\n");
#ifdef _WIN32
    OutputDebugStringA(debugMsg);
#endif

    for (const auto& test : exactTests)
    {
        std::string result = GetAnimationNameFromHash(test.hash);
        bool passed = (result == test.expectedName);
        allPassed &= passed;

        snprintf(debugMsg, sizeof(debugMsg),
            "[GWAnimHashes] 0x%08X (%s): expected '%s', got '%s' -> %s\n",
            test.hash, test.description, test.expectedName, result.c_str(), passed ? "PASS" : "FAIL");
#ifdef _WIN32
        OutputDebugStringA(debugMsg);
#endif
    }

    // Test real segment hash lookups via lower 16-bit matching
    snprintf(debugMsg, sizeof(debugMsg), "[GWAnimHashes] Testing segment hash lookups (lower 16-bit matching)...\n");
#ifdef _WIN32
    OutputDebugStringA(debugMsg);
#endif

    for (const auto& test : segmentTests)
    {
        std::string result = GetAnimationNameFromHash(test.hash);
        bool passed = (result == test.expectedName);
        // Only fail if we expected a match but didn't get it
        if (test.expectedName[0] != '\0')
        {
            allPassed &= passed;
        }

        snprintf(debugMsg, sizeof(debugMsg),
            "[GWAnimHashes] segment 0x%08X (lower16=0x%04X): expected '%s', got '%s' -> %s\n",
            test.hash, test.hash & 0xFFFF,
            test.expectedName[0] ? test.expectedName : "(no match)",
            result.empty() ? "(not found)" : result.c_str(),
            passed ? "PASS" : "FAIL");
#ifdef _WIN32
        OutputDebugStringA(debugMsg);
#endif
    }

    snprintf(debugMsg, sizeof(debugMsg),
        "[GWAnimHashes] Hash lookup verification complete.\n");
#ifdef _WIN32
    OutputDebugStringA(debugMsg);
#endif

    return allPassed;
}

} // namespace GW::Animation
