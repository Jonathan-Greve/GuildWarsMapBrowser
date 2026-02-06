#include "pch.h"
#include "animation_state.h"
#include "ModelViewer/ModelViewer.h"
#include <GuiGlobalConstants.h>
#include <DATManager.h>
#include "Parsers/BB9AnimationParser.h"
#include "Parsers/FileReferenceParser.h"
#include "Animation/GWAnimationHashes.h"  // For animation hash lookup
#include "FFNA_ModelFile_Other.h"  // For LogBB8Debug
#include <condition_variable>
#include <format>
#include <mutex>
#include <optional>
#include <thread>

// Global animation state
AnimationPanelState g_animationState;

/**
 * @brief Logs detailed sequence information for debugging animation structure.
 */
static void LogSequenceData(const GW::Animation::AnimationClip& clip, uint32_t fileId)
{
    char msg[512];
    sprintf_s(msg, "\n=== Animation Sequence Data for File 0x%X ===\n", fileId);
    LogBB8Debug(msg);

    // One-time verification of hash lookup tables
    static bool s_hashLookupVerified = false;
    if (!s_hashLookupVerified)
    {
        s_hashLookupVerified = true;
        LogBB8Debug("Verifying animation hash lookup tables...\n");
        GW::Animation::DebugDumpComputedSegmentHashes();
        GW::Animation::DebugVerifyHashLookup();
        LogBB8Debug("Hash lookup verification complete.\n");
    }

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

        // Look up the animation name from hash
        std::string animName = GW::Animation::GetAnimationNameFromHash(seq.hash);
        std::string nameStr = animName.empty() ? "(unknown)" : animName;

        sprintf_s(msg, "  [%zu] hash=0x%08X -> '%s' seqIdx=%u frames=%u time=[%.1f - %.1f] (%.2fs)\n",
            i,
            seq.hash,
            nameStr.c_str(),
            seq.sequenceIndex,
            seq.frameCount,
            seq.startTime,
            seq.endTime,
            durationSec);
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

    // Log animation segments with hash lookup
    if (!clip.animationSegments.empty())
    {
        sprintf_s(msg, "\nAnimation Segments: %zu total\n", clip.animationSegments.size());
        LogBB8Debug(msg);

        for (size_t i = 0; i < clip.animationSegments.size(); i++)
        {
            const auto& seg = clip.animationSegments[i];
            float durationSec = static_cast<float>(seg.GetDuration()) / 100000.0f;
            uint8_t sourceType = clip.GetSegmentSourceType(i);
            const char* sourceLabel = (sourceType == 0) ? "local" : "external";

            // Look up the animation name from segment hash
            std::string segName = GW::Animation::GetAnimationNameFromHash(seg.hash);
            std::string nameStr = segName.empty() ? "(unknown)" : segName;
            uint32_t maskedHash = seg.hash & 0xFFFFFF00;

            sprintf_s(msg, "  [%zu] hash=0x%08X (masked=0x%08X) -> '%s' time=[%u - %u] (%.2fs) flags=0x%04X src=%s(%u)\n",
                i,
                seg.hash,
                maskedHash,
                nameStr.c_str(),
                seg.startTime,
                seg.endTime,
                durationSec,
                seg.flags,
                sourceLabel,
                static_cast<unsigned>(sourceType));
            LogBB8Debug(msg);
        }
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

                    // FA8/BBD are animation source tables and preserve source ordering used by FA1 segmentType.
                    // FA6/BBC usually point to Type 8 sound event files.
                    const bool isAnimationReferenceChunk =
                        (chunkId == GW::Parsers::CHUNK_ID_FA8 || chunkId == GW::Parsers::CHUNK_ID_BBD);
                    const bool isSoundEventFile = (ffnaType == 8);

                    if (isAnimationReferenceChunk || !isSoundEventFile)
                    {
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
                        source.referenceChunkId = chunkId;
                        source.referenceIndex = i + 1;
                        source.isLoaded = false;
                        source.mftIndex = foundMftIndex;
                        source.datAlias = foundDatAlias;
                        g_animationState.animationSources.push_back(source);
                    }
                    else
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

// Pointer to DAT managers for use in animation search/load helpers.
static std::map<int, std::unique_ptr<DATManager>>* s_datManagersPtr = nullptr;

namespace
{
enum class AnimationSearchMode
{
    ManualAllResults,
    AutoFirstMatch
};

struct AnimationSearchRequest
{
    uint32_t targetHash0 = 0;
    uint32_t targetHash1 = 0;
    uint32_t modelFileId = 0;
    AnimationSearchMode mode = AnimationSearchMode::ManualAllResults;
};

struct CompletedAnimationSearch
{
    AnimationSearchRequest request;
    std::vector<AnimationSearchResult> results;
};

std::mutex s_searchControlMutex;
std::condition_variable s_searchRequestCv;
std::optional<AnimationSearchRequest> s_pendingSearchRequest;
std::optional<CompletedAnimationSearch> s_completedSearch;
std::atomic<bool> s_abortActiveSearch{ false };
std::atomic<bool> s_searchWorkerStarted{ false };

bool HasPendingSearchRequest()
{
    std::lock_guard<std::mutex> lock(s_searchControlMutex);
    return s_pendingSearchRequest.has_value();
}

void PublishCompletedSearch(const AnimationSearchRequest& request, std::vector<AnimationSearchResult>&& results)
{
    std::lock_guard<std::mutex> lock(s_searchControlMutex);
    s_completedSearch = CompletedAnimationSearch{ request, std::move(results) };
}
} // namespace

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
        if (chunkDataOffset + chunkSize > dataSize)
            break;

        // Check for BB9 chunk
        if (chunkId == GW::Parsers::CHUNK_ID_BB9)
        {
            if (chunkDataOffset + sizeof(GW::Parsers::BB9Header) <= dataSize)
            {
                GW::Parsers::BB9Header header;
                std::memcpy(&header, &data[chunkDataOffset], sizeof(header));

                if (header.modelHash0 == targetHash0 && header.modelHash1 == targetHash1)
                {
                    auto clipOpt = GW::Parsers::BB9AnimationParser::Parse(
                        &data[chunkDataOffset], chunkSize);
                    if (clipOpt && clipOpt->IsValid())
                    {
                        outResult.chunkType = "BB9";
                        outResult.sequenceCount = static_cast<uint32_t>(clipOpt->sequences.size());
                        outResult.boneCount = static_cast<uint32_t>(clipOpt->boneTracks.size());
                        return true;
                    }
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
                    auto clipOpt = GW::Parsers::BB9AnimationParser::ParseFA1(
                        &data[chunkDataOffset], chunkSize);
                    if (clipOpt && clipOpt->IsValid())
                    {
                        outResult.chunkType = "FA1";
                        outResult.sequenceCount = static_cast<uint32_t>(clipOpt->sequences.size());
                        outResult.boneCount = static_cast<uint32_t>(clipOpt->boneTracks.size());
                        return true;
                    }
                }
            }
        }

        offset += 8 + chunkSize;
    }

    return false;
}

/**
 * @brief Executes one animation search request.
 *
 * Runs on the background worker thread and aborts early when a newer request
 * has been queued or cancellation was requested.
 */
static void RunAnimationSearchRequest(const AnimationSearchRequest& request)
{
    if (!s_datManagersPtr)
    {
        return;
    }

    auto& dat_managers = *s_datManagersPtr;
    g_animationState.filesProcessed.store(0);

    // Count total files for progress reporting.
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
        PublishCompletedSearch(request, {});
        return;
    }

    std::vector<AnimationSearchResult> localResults;
    if (request.mode == AnimationSearchMode::AutoFirstMatch)
    {
        localResults.reserve(1);
    }

    // Search each DAT.
    for (const auto& pair : dat_managers)
    {
        if (s_abortActiveSearch.load() || HasPendingSearchRequest())
        {
            return;
        }

        DATManager* manager = pair.second.get();
        int datAlias = pair.first;

        if (!manager)
        {
            continue;
        }

        const auto& mft = manager->get_MFT();
        for (size_t i = 0; i < mft.size(); ++i)
        {
            if (s_abortActiveSearch.load() || HasPendingSearchRequest())
            {
                return;
            }

            const auto& entry = mft[i];

            // Skip files that cannot contain model animation chunks.
            if (entry.uncompressedSize < 57 || entry.type != FFNA_Type2)
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
                    request.targetHash0,
                    request.targetHash1,
                    result);

                if (found)
                {
                    result.fileId = entry.Hash;
                    result.mftIndex = static_cast<int>(i);
                    result.datAlias = datAlias;
                    localResults.push_back(result);

                    if (request.mode == AnimationSearchMode::AutoFirstMatch)
                    {
                        delete[] fileData;
                        g_animationState.filesProcessed.fetch_add(1);
                        if (s_abortActiveSearch.load() || HasPendingSearchRequest())
                        {
                            return;
                        }
                        PublishCompletedSearch(request, std::move(localResults));
                        return;
                    }
                }

                delete[] fileData;
            }
            catch (...)
            {
                // Ignore errors for individual files.
            }

            g_animationState.filesProcessed.fetch_add(1);
        }
    }

    if (s_abortActiveSearch.load() || HasPendingSearchRequest())
    {
        return;
    }
    PublishCompletedSearch(request, std::move(localResults));
}

/**
 * @brief Background worker that processes queued animation searches.
 *
 * Only one scan runs at a time. New requests replace old ones, and the active
 * scan exits quickly so the latest model request takes priority.
 */
static void AnimationSearchWorkerLoop()
{
    for (;;)
    {
        AnimationSearchRequest request;
        {
            std::unique_lock<std::mutex> lock(s_searchControlMutex);
            s_searchRequestCv.wait(lock, [] { return s_pendingSearchRequest.has_value(); });
            request = *s_pendingSearchRequest;
            s_pendingSearchRequest.reset();
        }

        s_abortActiveSearch.store(false);
        g_animationState.searchInProgress.store(true);
        RunAnimationSearchRequest(request);

        bool hasPendingRequest = false;
        {
            std::lock_guard<std::mutex> lock(s_searchControlMutex);
            hasPendingRequest = s_pendingSearchRequest.has_value();
        }

        if (!hasPendingRequest)
        {
            g_animationState.searchInProgress.store(false);
        }
    }
}

static void EnsureAnimationSearchWorkerStarted()
{
    if (s_searchWorkerStarted.exchange(true))
    {
        return;
    }

    std::thread(AnimationSearchWorkerLoop).detach();
}

static void QueueAnimationSearchRequest(const AnimationSearchRequest& request, bool clearCurrentResults)
{
    if (clearCurrentResults)
    {
        g_animationState.searchResults.clear();
        g_animationState.selectedResultIndex = -1;
    }

    g_animationState.filesProcessed.store(0);
    g_animationState.totalFiles.store(0);
    g_animationState.searchInProgress.store(true);

    EnsureAnimationSearchWorkerStarted();
    {
        std::lock_guard<std::mutex> lock(s_searchControlMutex);
        s_pendingSearchRequest = request;
        s_completedSearch.reset();
        // Signal the current scan to stop; the worker will pick up the latest request.
        s_abortActiveSearch.store(true);
    }
    s_searchRequestCv.notify_one();
}

static bool TryConsumeCompletedSearch(CompletedAnimationSearch& outCompleted)
{
    std::lock_guard<std::mutex> lock(s_searchControlMutex);
    if (!s_completedSearch.has_value())
    {
        return false;
    }

    outCompleted = std::move(*s_completedSearch);
    s_completedSearch.reset();
    return true;
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

            // Reset animation group selection (playback mode is set by Initialize).
            g_animationState.currentAnimationGroupIndex = 0;

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
                    if (clipOpt && clipOpt->IsValid())
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

                        // Reset animation group selection (playback mode is set by Initialize).
                        g_animationState.currentAnimationGroupIndex = 0;

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
                    if (clipOpt && !clipOpt->IsValid())
                    {
                        char msg[192];
                        sprintf_s(msg,
                            "AutoLoad: file 0x%X has no playable animation keyframes (chunk=%s), continuing search\n",
                            fileId,
                            clipOpt->sourceChunkType.empty() ? "?" : clipOpt->sourceChunkType.c_str());
                        LogBB8Debug(msg);
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
    // Store DAT managers pointer for background search and deferred loading.
    s_datManagersPtr = &dat_managers;

    // Skip if no model loaded or animation already loaded.
    if (!g_animationState.hasModel || g_animationState.hasAnimation)
    {
        return;
    }

    // First, try to load animation from the same file as the model.
    if (TryLoadAnimationFromSameFile(g_animationState.currentFileId, dat_managers))
    {
        return;
    }

    // Queue cancellable background discovery and auto-load the first match.
    AnimationSearchRequest request;
    request.targetHash0 = g_animationState.modelHash0;
    request.targetHash1 = g_animationState.modelHash1;
    request.modelFileId = g_animationState.currentFileId;
    request.mode = AnimationSearchMode::AutoFirstMatch;
    QueueAnimationSearchRequest(request, true);
}

void StartAnimationSearch(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (!g_animationState.hasModel)
    {
        return;
    }

    s_datManagersPtr = &dat_managers;
    AnimationSearchRequest request;
    request.targetHash0 = g_animationState.modelHash0;
    request.targetHash1 = g_animationState.modelHash1;
    request.modelFileId = g_animationState.currentFileId;
    request.mode = AnimationSearchMode::ManualAllResults;
    QueueAnimationSearchRequest(request, true);
}

void CancelAnimationSearch()
{
    s_abortActiveSearch.store(true);
    {
        std::lock_guard<std::mutex> lock(s_searchControlMutex);
        s_pendingSearchRequest.reset();
        s_completedSearch.reset();
    }

    g_animationState.searchInProgress.store(false);
    g_animationState.filesProcessed.store(0);
    g_animationState.totalFiles.store(0);
}

void PumpAnimationSearchResults(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    CompletedAnimationSearch completed;
    if (!TryConsumeCompletedSearch(completed))
    {
        return;
    }

    const bool modelMatchesRequest =
        g_animationState.hasModel &&
        g_animationState.currentFileId == completed.request.modelFileId &&
        g_animationState.modelHash0 == completed.request.targetHash0 &&
        g_animationState.modelHash1 == completed.request.targetHash1;

    if (!modelMatchesRequest)
    {
        return;
    }

    g_animationState.searchResults = std::move(completed.results);
    g_animationState.selectedResultIndex = g_animationState.searchResults.empty() ? -1 : 0;

    if (completed.request.mode == AnimationSearchMode::AutoFirstMatch &&
        !g_animationState.searchResults.empty() &&
        !g_animationState.hasAnimation)
    {
        LoadAnimationFromResult(g_animationState.searchResults.front(), dat_managers);
    }
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

            // Reset animation group selection (playback mode is set by Initialize).
            g_animationState.currentAnimationGroupIndex = 0;

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

