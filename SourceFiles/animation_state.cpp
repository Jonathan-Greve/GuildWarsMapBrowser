#include "pch.h"
#include "animation_state.h"
#include "ModelViewer/ModelViewer.h"
#include <GuiGlobalConstants.h>
#include <DATManager.h>
#include "Parsers/BB9AnimationParser.h"
#include "Parsers/FileReferenceParser.h"
#include "FFNA_ModelFile_Other.h"  // For LogBB8Debug
#include <format>
#include <thread>
#include <mutex>

// Global animation state
AnimationPanelState g_animationState;

// Local state for animation search
static std::mutex s_resultsMutex;

/**
 * @brief Logs detailed sequence information for debugging animation structure.
 */
static void LogSequenceData(const GW::Animation::AnimationClip& clip, uint32_t fileId)
{
    char msg[512];
    sprintf_s(msg, "\n=== Animation Sequence Data for File 0x%X ===\n", fileId);
    LogBB8Debug(msg);

    sprintf_s(msg, "Clip: minTime=%.1f, maxTime=%.1f, totalFrames=%u, source=%s\n",
        clip.minTime, clip.maxTime, clip.totalFrames, clip.sourceChunkType.c_str());
    LogBB8Debug(msg);

    sprintf_s(msg, "Sequences: %zu total\n", clip.sequences.size());
    LogBB8Debug(msg);

    for (size_t i = 0; i < clip.sequences.size(); i++)
    {
        const auto& seq = clip.sequences[i];
        float durationMs = seq.endTime - seq.startTime;
        float durationSec = durationMs / 100000.0f;

        sprintf_s(msg, "  [%zu] hash=0x%08X seqIdx=%u frames=%u time=[%.1f - %.1f] (%.2fs) bounds=(%.1f, %.1f, %.1f)\n",
            i,
            seq.hash,
            seq.sequenceIndex,
            seq.frameCount,
            seq.startTime,
            seq.endTime,
            durationSec,
            seq.bounds.x, seq.bounds.y, seq.bounds.z);
        LogBB8Debug(msg);
    }

    // Log animation groups
    sprintf_s(msg, "\nAnimation Groups: %zu total\n", clip.animationGroups.size());
    LogBB8Debug(msg);

    for (size_t i = 0; i < clip.animationGroups.size(); i++)
    {
        const auto& group = clip.animationGroups[i];
        float durationSec = group.GetDuration() / 100000.0f;

        sprintf_s(msg, "  [%zu] %s: time=[%.1f - %.1f] (%.2fs) phases=%zu\n",
            i,
            group.displayName.c_str(),
            group.startTime,
            group.endTime,
            durationSec,
            group.GetPhaseCount());
        LogBB8Debug(msg);

        // List which sequences are in this group
        std::string seqList = "       seqIndices: ";
        for (size_t si : group.sequenceIndices)
        {
            seqList += std::to_string(si) + " ";
        }
        seqList += "\n";
        LogBB8Debug(seqList.c_str());
    }

    LogBB8Debug("=== End Sequence Data ===\n\n");
}

/**
 * @brief Scans for animation file references (BBC/BBD chunks) in a file.
 *
 * BBC and BBD chunks contain 6-byte encoded file references pointing to
 * additional animation files that share the same skeleton.
 *
 * @param fileData File data to scan.
 * @param fileSize Size of file data.
 * @param dat_managers DAT managers for looking up file locations.
 */
static void ScanForAnimationReferences(
    const uint8_t* fileData,
    size_t fileSize,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    g_animationState.animationSources.clear();
    g_animationState.soundEventSources.clear();
    g_animationState.currentSoundSourceIndex = -1;
    g_animationState.hasScannedReferences = true;

    // Verify FFNA signature
    if (fileSize < 5 || fileData[0] != 'f' || fileData[1] != 'f' ||
        fileData[2] != 'n' || fileData[3] != 'a')
    {
        return;
    }

    char msg[256];
    sprintf_s(msg, "\n=== Scanning for Animation File References ===\n");
    LogBB8Debug(msg);

    // Scan all chunks for BBC and BBD
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

        // Check for BBC, BBD, FA6, or FA8 chunks (file references)
        // BB9 format: BBC = Type 8 sound events, BBD = additional animations
        // FA1 format: FA6 = Type 8 sound events, FA8 = additional animations
        if (chunkId == GW::Parsers::CHUNK_ID_BBC || chunkId == GW::Parsers::CHUNK_ID_BBD ||
            chunkId == GW::Parsers::CHUNK_ID_FA6 || chunkId == GW::Parsers::CHUNK_ID_FA8)
        {
            sprintf_s(msg, "Found chunk 0x%X at offset %zu, size %u\n",
                chunkId, offset, chunkSize);
            LogBB8Debug(msg);

            // Parse the file references
            // BBC/BBD format: u32 unknown, u32 count, then 6-byte entries
            // FA6/FA8 format: u32 count, then 6-byte entries (no unknown field)
            if (chunkSize >= 4)
            {
                uint32_t count;
                size_t entryOffset;

                if (chunkId == GW::Parsers::CHUNK_ID_FA6 || chunkId == GW::Parsers::CHUNK_ID_FA8)
                {
                    // FA6/FA8 format: just count (4 bytes) then entries
                    std::memcpy(&count, &chunkData[0], sizeof(uint32_t));
                    entryOffset = 4;
                    sprintf_s(msg, "  FA%c Header: count=%u\n",
                        chunkId == GW::Parsers::CHUNK_ID_FA6 ? '6' : '8', count);
                }
                else
                {
                    // BBC/BBD format: unknown (4 bytes) + count (4 bytes) then entries
                    if (chunkSize < 8)
                    {
                        offset += 8 + chunkSize;
                        continue;
                    }
                    uint32_t unknown;
                    std::memcpy(&unknown, &chunkData[0], sizeof(uint32_t));
                    std::memcpy(&count, &chunkData[4], sizeof(uint32_t));
                    entryOffset = 8;
                    sprintf_s(msg, "  BB Header: unknown=%u, count=%u\n", unknown, count);
                }
                LogBB8Debug(msg);

                // Validate count
                size_t maxEntries = (chunkSize - entryOffset) / 6;
                if (count > maxEntries)
                {
                    sprintf_s(msg, "  Warning: count %u exceeds max entries %zu\n",
                        count, maxEntries);
                    LogBB8Debug(msg);
                    count = static_cast<uint32_t>(maxEntries);
                }

                // Parse each 6-byte entry
                for (uint32_t i = 0; i < count; i++)
                {
                    if (entryOffset + 6 > chunkSize)
                        break;

                    uint16_t id0, id1, flags;
                    std::memcpy(&id0, &chunkData[entryOffset], sizeof(uint16_t));
                    std::memcpy(&id1, &chunkData[entryOffset + 2], sizeof(uint16_t));
                    std::memcpy(&flags, &chunkData[entryOffset + 4], sizeof(uint16_t));

                    // Decode file ID: (id0 - 0xff00ff) + (id1 * 0xff00)
                    int32_t fileIdSigned = static_cast<int32_t>(id0) - 0xFF00FF;
                    fileIdSigned += static_cast<int32_t>(id1) * 0xFF00;
                    uint32_t fileId = static_cast<uint32_t>(fileIdSigned);

                    sprintf_s(msg, "  [%u] id0=%u, id1=%u, flags=%u -> fileId=0x%X\n",
                        i, id0, id1, flags, fileId);
                    LogBB8Debug(msg);

                    // Try to find the file in DAT managers and check its type
                    int foundMftIndex = -1;
                    int foundDatAlias = 0;
                    uint8_t ffnaType = 0;

                    for (const auto& [alias, manager] : dat_managers)
                    {
                        if (manager)
                        {
                            const auto& mft = manager->get_MFT();
                            for (size_t j = 0; j < mft.size(); j++)
                            {
                                // Compare as unsigned to handle large file IDs correctly
                                if (static_cast<uint32_t>(mft[j].Hash) == fileId)
                                {
                                    foundMftIndex = static_cast<int>(j);
                                    foundDatAlias = alias;

                                    // Read the file to check its FFNA type
                                    if (mft[j].uncompressedSize >= 5)
                                    {
                                        uint8_t* tempData = manager->read_file(static_cast<int>(j));
                                        if (tempData)
                                        {
                                            if (tempData[0] == 'f' && tempData[1] == 'f' &&
                                                tempData[2] == 'n' && tempData[3] == 'a')
                                            {
                                                ffnaType = tempData[4];
                                            }
                                            delete[] tempData;
                                        }
                                    }
                                    break;
                                }
                            }
                            if (foundMftIndex >= 0)
                                break;
                        }
                    }

                    // Type 8 = Sound Event files, others = Animation files
                    // BBC/FA6 chunks primarily contain Type 8 sound event references
                    // BBD/FA8 chunks primarily contain additional animation references
                    if (ffnaType == 8)
                    {
                        SoundEventSource soundSource;
                        soundSource.fileId = fileId;
                        soundSource.mftIndex = foundMftIndex;
                        soundSource.datAlias = foundDatAlias;
                        soundSource.isLoaded = false;
                        g_animationState.soundEventSources.push_back(soundSource);

                        sprintf_s(msg, "    -> Type 8 (Sound Event) file\n");
                        LogBB8Debug(msg);
                    }
                    else
                    {
                        // Create AnimationSource entry
                        AnimationSource source;
                        source.fileId = fileId;
                        if (chunkId == GW::Parsers::CHUNK_ID_FA8)
                            source.chunkType = "FA8";
                        else if (chunkId == GW::Parsers::CHUNK_ID_FA6)
                            source.chunkType = "FA6";
                        else if (chunkId == GW::Parsers::CHUNK_ID_BBC)
                            source.chunkType = "BBC";
                        else
                            source.chunkType = "BBD";
                        source.isLoaded = false;
                        source.mftIndex = foundMftIndex;
                        source.datAlias = foundDatAlias;

                        g_animationState.animationSources.push_back(source);
                    }

                    entryOffset += 6;
                }
            }
        }

        offset += 8 + chunkSize;
    }

    sprintf_s(msg, "Found %zu animation file references, %zu sound event files\n",
        g_animationState.animationSources.size(),
        g_animationState.soundEventSources.size());
    LogBB8Debug(msg);
    LogBB8Debug("=== End Animation References ===\n\n");
}

// Pointer to DAT managers for use in search thread
static std::map<int, std::unique_ptr<DATManager>>* s_datManagersPtr = nullptr;

/**
 * @brief Searches a file for BB9 or FA1 animation chunks with matching model hashes.
 */
static bool CheckFileForMatchingAnimation(
    const uint8_t* data,
    size_t dataSize,
    uint32_t targetHash0,
    uint32_t targetHash1,
    AnimationSearchResult& outResult)
{
    // Need at least FFNA header (5 bytes) + chunk header (8 bytes) + BB9/FA1 header
    if (dataSize < 5 + 8 + 44)
        return false;

    // Verify FFNA signature
    if (data[0] != 'f' || data[1] != 'f' || data[2] != 'n' || data[3] != 'a')
        return false;

    // Start after FFNA signature (4 bytes) and type (1 byte)
    size_t offset = 5;

    while (offset + 8 <= dataSize)
    {
        uint32_t chunkId, chunkSize;
        std::memcpy(&chunkId, &data[offset], sizeof(uint32_t));
        std::memcpy(&chunkSize, &data[offset + 4], sizeof(uint32_t));

        if (chunkId == 0 || chunkSize == 0)
            break;

        size_t chunkDataOffset = offset + 8;

        // Check for BB9 chunk
        if (chunkId == GW::Parsers::CHUNK_ID_BB9)
        {
            if (chunkDataOffset + sizeof(GW::Parsers::BB9Header) <= dataSize)
            {
                GW::Parsers::BB9Header header;
                std::memcpy(&header, &data[chunkDataOffset], sizeof(header));

                if (header.modelHash0 == targetHash0 && header.modelHash1 == targetHash1)
                {
                    outResult.chunkType = "BB9";

                    auto clipOpt = GW::Parsers::BB9AnimationParser::Parse(
                        &data[chunkDataOffset], chunkSize);
                    if (clipOpt)
                    {
                        outResult.sequenceCount = static_cast<uint32_t>(clipOpt->sequences.size());
                        outResult.boneCount = static_cast<uint32_t>(clipOpt->boneTracks.size());
                    }

                    return true;
                }
            }
        }
        // Check for FA1 chunk - uses different header structure!
        // FA1Header has boundingBoxId/collisionMeshId at 0x0C/0x10 which serve as model hashes
        else if (chunkId == GW::Parsers::CHUNK_ID_FA1)
        {
            if (chunkDataOffset + sizeof(GW::Parsers::FA1Header) <= dataSize)
            {
                GW::Parsers::FA1Header header;
                std::memcpy(&header, &data[chunkDataOffset], sizeof(header));

                // FA1 uses boundingBoxId/collisionMeshId as model hash equivalents
                if (header.boundingBoxId == targetHash0 && header.collisionMeshId == targetHash1)
                {
                    outResult.chunkType = "FA1";

                    // Use transformDataSize for sequence count, NOT sequenceCount0!
                    // transformDataSize = actual number of 24-byte sequence entries
                    outResult.sequenceCount = header.transformDataSize;
                    outResult.boneCount = header.bindPoseBoneCount;

                    return true;
                }
            }
        }

        offset += 8 + chunkSize;
    }

    return false;
}

/**
 * @brief Worker function to search DAT files for matching animations.
 */
static void SearchForAnimationsWorker(uint32_t targetHash0, uint32_t targetHash1)
{
    if (!s_datManagersPtr)
    {
        g_animationState.searchInProgress.store(false);
        return;
    }

    auto& dat_managers = *s_datManagersPtr;

    // Clear previous results
    {
        std::lock_guard<std::mutex> lock(s_resultsMutex);
        g_animationState.searchResults.clear();
    }

    g_animationState.filesProcessed.store(0);

    // Count total files
    int totalFiles = 0;
    for (const auto& pair : dat_managers)
    {
        if (pair.second)
        {
            totalFiles += static_cast<int>(pair.second->get_MFT().size());
        }
    }
    g_animationState.totalFiles.store(totalFiles);

    if (totalFiles == 0)
    {
        g_animationState.searchInProgress.store(false);
        return;
    }

    // Search each DAT
    for (const auto& pair : dat_managers)
    {
        if (!g_animationState.searchInProgress.load())
            break;

        DATManager* manager = pair.second.get();
        int datAlias = pair.first;

        if (!manager)
            continue;

        const auto& mft = manager->get_MFT();

        for (size_t i = 0; i < mft.size(); ++i)
        {
            if (!g_animationState.searchInProgress.load())
                break;

            const auto& entry = mft[i];

            // Skip small files that can't contain animation data
            // FFNA header (5) + chunk header (8) + BB9 header (44) = 57 bytes minimum
            if (entry.uncompressedSize < 57)
            {
                g_animationState.filesProcessed.fetch_add(1);
                continue;
            }

            // Only check FFNA Type2 files (models that might have animation)
            if (entry.type != FFNA_Type2)
            {
                g_animationState.filesProcessed.fetch_add(1);
                continue;
            }

            try
            {
                uint8_t* fileData = manager->read_file(static_cast<int>(i));
                if (!fileData)
                {
                    g_animationState.filesProcessed.fetch_add(1);
                    continue;
                }

                AnimationSearchResult result;
                bool found = CheckFileForMatchingAnimation(
                    fileData,
                    entry.uncompressedSize,
                    targetHash0,
                    targetHash1,
                    result);

                if (found)
                {
                    result.fileId = entry.Hash;
                    result.mftIndex = static_cast<int>(i);
                    result.datAlias = datAlias;

                    std::lock_guard<std::mutex> lock(s_resultsMutex);
                    g_animationState.searchResults.push_back(result);
                }

                delete[] fileData;
            }
            catch (...)
            {
                // Ignore errors for individual files
            }

            g_animationState.filesProcessed.fetch_add(1);
        }
    }

    g_animationState.searchInProgress.store(false);
}

/**
 * @brief Loads an animation from a search result.
 */
static void LoadAnimationFromResult(
    const AnimationSearchResult& result,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    auto it = dat_managers.find(result.datAlias);
    if (it == dat_managers.end() || !it->second)
        return;

    DATManager* manager = it->second.get();

    try
    {
        uint8_t* fileData = manager->read_file(result.mftIndex);
        if (!fileData)
            return;

        const auto& mft = manager->get_MFT();
        size_t fileSize = mft[result.mftIndex].uncompressedSize;

        auto clipOpt = GW::Parsers::ParseAnimationFromFile(fileData, fileSize);
        if (clipOpt)
        {
            auto clip = std::make_shared<GW::Animation::AnimationClip>(std::move(*clipOpt));

            // Build animation groups for the new playback system
            clip->BuildAnimationGroups();

            // Log sequence data for debugging
            LogSequenceData(*clip, result.fileId);

            // Scan for animation file references (BBC/BBD chunks)
            ScanForAnimationReferences(fileData, fileSize, dat_managers);

            auto skeleton = std::make_shared<GW::Animation::Skeleton>(
                GW::Parsers::BB9AnimationParser::CreateSkeleton(*clip));

            // Keep the model hashes from the original model
            uint32_t savedHash0 = g_animationState.modelHash0;
            uint32_t savedHash1 = g_animationState.modelHash1;
            bool savedHasModel = g_animationState.hasModel;

            // Initialize applies persistent playback settings automatically
            g_animationState.Initialize(clip, skeleton, result.fileId);

            // Restore model info
            g_animationState.modelHash0 = savedHash0;
            g_animationState.modelHash1 = savedHash1;
            g_animationState.hasModel = savedHasModel;
            g_animationState.currentChunkType = result.chunkType;

            // Reset animation group selection
            g_animationState.currentAnimationGroupIndex = 0;
            g_animationState.playbackMode = AnimationPlaybackMode::FullAnimation;

            // Update model viewer state with animation file info for saving
            g_modelViewerState.animFileId = result.fileId;
            g_modelViewerState.animMftIndex = result.mftIndex;
            g_modelViewerState.animDatManager = manager;
            g_modelViewerState.animClip = clip;
            g_modelViewerState.animController = g_animationState.controller;

            // Load all sound event sources if not already loaded
            LoadAllSoundEventSources(dat_managers);

            // Set animation segments for timeline display
            if (g_animationState.soundManager && !clip->animationSegments.empty())
            {
                g_animationState.soundManager->SetTimingFromClip(*clip);
            }
        }

        delete[] fileData;
    }
    catch (...)
    {
        // Ignore errors
    }
}

// NOTE: draw_animation_panel() removed - use Model Viewer panel instead (ModelViewerPanel.cpp)

/**
 * @brief Tries to load animation from the same file as the model.
 */
static bool TryLoadAnimationFromSameFile(
    uint32_t fileId,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    for (const auto& pair : dat_managers)
    {
        DATManager* manager = pair.second.get();
        if (!manager)
            continue;

        const auto& mft = manager->get_MFT();
        for (size_t i = 0; i < mft.size(); ++i)
        {
            // Compare as unsigned to handle large file IDs correctly
            if (static_cast<uint32_t>(mft[i].Hash) == fileId)
            {
                try
                {
                    uint8_t* fileData = manager->read_file(static_cast<int>(i));
                    if (!fileData)
                        continue;

                    auto clipOpt = GW::Parsers::ParseAnimationFromFile(fileData, mft[i].uncompressedSize);
                    if (clipOpt && !clipOpt->boneTracks.empty())
                    {
                        auto clip = std::make_shared<GW::Animation::AnimationClip>(std::move(*clipOpt));

                        // Build animation groups for the new playback system
                        clip->BuildAnimationGroups();

                        // Log sequence data for debugging
                        LogSequenceData(*clip, fileId);

                        // Scan for animation file references (BBC/BBD chunks)
                        ScanForAnimationReferences(fileData, mft[i].uncompressedSize, dat_managers);

                        auto skeleton = std::make_shared<GW::Animation::Skeleton>(
                            GW::Parsers::BB9AnimationParser::CreateSkeleton(*clip));

                        // Save model info
                        uint32_t savedHash0 = g_animationState.modelHash0;
                        uint32_t savedHash1 = g_animationState.modelHash1;
                        bool savedHasModel = g_animationState.hasModel;

                        g_animationState.Initialize(clip, skeleton, fileId);

                        // Restore model info
                        g_animationState.modelHash0 = savedHash0;
                        g_animationState.modelHash1 = savedHash1;
                        g_animationState.hasModel = savedHasModel;
                        g_animationState.currentChunkType = clip->sourceChunkType;

                        // Reset animation group selection
                        g_animationState.currentAnimationGroupIndex = 0;
                        g_animationState.playbackMode = AnimationPlaybackMode::FullAnimation;

                        // Update model viewer state with animation file info for saving
                        g_modelViewerState.animFileId = fileId;
                        g_modelViewerState.animMftIndex = static_cast<int>(i);
                        g_modelViewerState.animDatManager = manager;
                        g_modelViewerState.animClip = clip;
                        g_modelViewerState.animController = g_animationState.controller;

                        // Load all sound event sources if not already loaded
                        LoadAllSoundEventSources(dat_managers);

                        // Set animation segments for timeline display
                        if (g_animationState.soundManager && !clip->animationSegments.empty())
                        {
                            g_animationState.soundManager->SetTimingFromClip(*clip);
                        }

                        delete[] fileData;
                        return true;
                    }
                    delete[] fileData;
                }
                catch (...)
                {
                    // Ignore errors
                }
            }
        }
    }
    return false;
}

/**
 * @brief Synchronously searches for matching animations and returns results.
 */
static std::vector<AnimationSearchResult> SearchForAnimationsSync(
    uint32_t targetHash0,
    uint32_t targetHash1,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers,
    int maxResults = 10)
{
    std::vector<AnimationSearchResult> results;

    for (const auto& pair : dat_managers)
    {
        DATManager* manager = pair.second.get();
        int datAlias = pair.first;

        if (!manager)
            continue;

        const auto& mft = manager->get_MFT();

        for (size_t i = 0; i < mft.size(); ++i)
        {
            const auto& entry = mft[i];

            if (entry.uncompressedSize < 57 || entry.type != FFNA_Type2)
                continue;

            try
            {
                uint8_t* fileData = manager->read_file(static_cast<int>(i));
                if (!fileData)
                    continue;

                AnimationSearchResult result;
                bool found = CheckFileForMatchingAnimation(
                    fileData, entry.uncompressedSize,
                    targetHash0, targetHash1, result);

                if (found)
                {
                    result.fileId = entry.Hash;
                    result.mftIndex = static_cast<int>(i);
                    result.datAlias = datAlias;
                    results.push_back(result);

                    if (results.size() >= static_cast<size_t>(maxResults))
                    {
                        delete[] fileData;
                        return results;
                    }
                }

                delete[] fileData;
            }
            catch (...)
            {
                // Ignore errors
            }
        }
    }

    return results;
}

void SetAnimationDATManagers(std::map<int, std::unique_ptr<DATManager>>* dat_managers)
{
    s_datManagersPtr = dat_managers;
}

void AutoLoadAnimationFromStoredManagers()
{
    if (s_datManagersPtr)
    {
        AutoLoadAnimation(*s_datManagersPtr);
    }
}

void AutoLoadAnimation(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    // Store DAT managers pointer for potential future use
    s_datManagersPtr = &dat_managers;

    // Skip if no model loaded or animation already loaded
    if (!g_animationState.hasModel || g_animationState.hasAnimation)
        return;

    // First, try to load animation from the same file as the model
    if (TryLoadAnimationFromSameFile(g_animationState.currentFileId, dat_managers))
        return;

    // Search for animations matching the model hashes
    auto results = SearchForAnimationsSync(
        g_animationState.modelHash0,
        g_animationState.modelHash1,
        dat_managers,
        1);  // Just need the first match

    if (!results.empty())
    {
        LoadAnimationFromResult(results[0], dat_managers);
    }
}

void StartAnimationSearch(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (g_animationState.searchInProgress.load())
        return;

    s_datManagersPtr = &dat_managers;
    g_animationState.searchInProgress.store(true);
    g_animationState.filesProcessed.store(0);
    g_animationState.totalFiles.store(0);
    g_animationState.searchResults.clear();
    g_animationState.selectedResultIndex = -1;

    uint32_t hash0 = g_animationState.modelHash0;
    uint32_t hash1 = g_animationState.modelHash1;

    std::thread(SearchForAnimationsWorker, hash0, hash1).detach();
}

void LoadAnimationFromSearchResult(int resultIndex, std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (resultIndex < 0 || resultIndex >= static_cast<int>(g_animationState.searchResults.size()))
        return;

    const auto& result = g_animationState.searchResults[resultIndex];
    LoadAnimationFromResult(result, dat_managers);
}

void LoadAnimationFromReference(int refIndex, std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (refIndex < 0 || refIndex >= static_cast<int>(g_animationState.animationSources.size()))
        return;

    auto& source = g_animationState.animationSources[refIndex];

    // If not found in DAT, can't load
    if (source.mftIndex < 0)
    {
        char msg[128];
        sprintf_s(msg, "Animation file 0x%X not found in DAT files\n", source.fileId);
        LogBB8Debug(msg);
        return;
    }

    auto it = dat_managers.find(source.datAlias);
    if (it == dat_managers.end() || !it->second)
        return;

    DATManager* manager = it->second.get();

    try
    {
        uint8_t* fileData = manager->read_file(source.mftIndex);
        if (!fileData)
            return;

        const auto& mft = manager->get_MFT();
        size_t fileSize = mft[source.mftIndex].uncompressedSize;

        auto clipOpt = GW::Parsers::ParseAnimationFromFile(fileData, fileSize);
        if (clipOpt)
        {
            auto clip = std::make_shared<GW::Animation::AnimationClip>(std::move(*clipOpt));

            // Build animation groups for the new playback system
            clip->BuildAnimationGroups();

            // Log sequence data for debugging
            LogSequenceData(*clip, source.fileId);

            // Scan for animation file references in the new file
            ScanForAnimationReferences(fileData, fileSize, dat_managers);

            auto skeleton = std::make_shared<GW::Animation::Skeleton>(
                GW::Parsers::BB9AnimationParser::CreateSkeleton(*clip));

            // Keep the model hashes from the original model
            uint32_t savedHash0 = g_animationState.modelHash0;
            uint32_t savedHash1 = g_animationState.modelHash1;
            bool savedHasModel = g_animationState.hasModel;

            // Initialize applies persistent playback settings automatically
            g_animationState.Initialize(clip, skeleton, source.fileId);

            // Restore model info
            g_animationState.modelHash0 = savedHash0;
            g_animationState.modelHash1 = savedHash1;
            g_animationState.hasModel = savedHasModel;
            g_animationState.currentChunkType = clip->sourceChunkType;

            // Reset animation group selection
            g_animationState.currentAnimationGroupIndex = 0;
            g_animationState.playbackMode = AnimationPlaybackMode::FullAnimation;

            // Update model viewer state with animation file info for saving
            g_modelViewerState.animFileId = source.fileId;
            g_modelViewerState.animMftIndex = source.mftIndex;
            g_modelViewerState.animDatManager = manager;
            g_modelViewerState.animClip = clip;
            g_modelViewerState.animController = g_animationState.controller;

            // Mark the source as loaded
            source.clip = clip;
            source.isLoaded = true;

            // Load all sound event sources if not already loaded
            LoadAllSoundEventSources(dat_managers);

            // Set animation segments for timeline display
            if (g_animationState.soundManager && !clip->animationSegments.empty())
            {
                g_animationState.soundManager->SetTimingFromClip(*clip);
                char msg[128];
                sprintf_s(msg, "Set animation segments from clip: %zu segments\n",
                    clip->animationSegments.size());
                LogBB8Debug(msg);
            }
        }

        delete[] fileData;
    }
    catch (...)
    {
        // Ignore errors
    }
}

void LoadSoundEventsFromReference(int refIndex, std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (refIndex < 0 || refIndex >= static_cast<int>(g_animationState.soundEventSources.size()))
        return;

    auto& source = g_animationState.soundEventSources[refIndex];

    // If not found in DAT, can't load
    if (source.mftIndex < 0)
    {
        char msg[128];
        sprintf_s(msg, "Sound event file 0x%X not found in DAT files\n", source.fileId);
        LogBB8Debug(msg);
        return;
    }

    auto it = dat_managers.find(source.datAlias);
    if (it == dat_managers.end() || !it->second)
        return;

    DATManager* manager = it->second.get();

    try
    {
        uint8_t* fileData = manager->read_file(source.mftIndex);
        if (!fileData)
            return;

        const auto& mft = manager->get_MFT();
        size_t fileSize = mft[source.mftIndex].uncompressedSize;

        // Create sound manager if it doesn't exist
        if (!g_animationState.soundManager)
        {
            g_animationState.soundManager = std::make_shared<GW::Audio::AnimationSoundManager>();
        }

        // Load the Type 8 file (loads sound files, timing comes from animation clip)
        if (g_animationState.soundManager->LoadFromType8File(fileData, fileSize, dat_managers))
        {
            source.isLoaded = true;
            g_animationState.currentSoundSourceIndex = refIndex;

            char msg[128];
            sprintf_s(msg, "Loaded sound files from 0x%X: %zu events, %zu sounds\n",
                source.fileId,
                g_animationState.soundManager->GetSoundEvents().size(),
                g_animationState.soundManager->GetSoundFileIds().size());
            LogBB8Debug(msg);
        }

        delete[] fileData;
    }
    catch (...)
    {
        // Ignore errors
    }
}

void LoadAllSoundEventSources(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    // Load all Type 8 sound event sources that haven't been loaded yet
    for (size_t i = 0; i < g_animationState.soundEventSources.size(); i++)
    {
        auto& source = g_animationState.soundEventSources[i];
        if (!source.isLoaded && source.mftIndex >= 0)
        {
            LoadSoundEventsFromReference(static_cast<int>(i), dat_managers);
        }
    }
}

void UpdateAnimationSounds()
{
    if (!g_animationState.soundManager || !g_animationState.controller)
        return;

    auto& ctrl = *g_animationState.controller;
    g_animationState.soundManager->Update(
        ctrl.GetTime(),
        ctrl.GetSequenceStartTime(),
        ctrl.GetSequenceEndTime(),
        ctrl.IsPlaying()
    );
}

