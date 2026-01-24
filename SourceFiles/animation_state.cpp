#include "pch.h"
#include "animation_state.h"
#include "ModelViewer/ModelViewer.h"
#include <GuiGlobalConstants.h>
#include <DATManager.h>
#include "Parsers/BB9AnimationParser.h"
#include <format>
#include <thread>
#include <mutex>

// Global animation state
AnimationPanelState g_animationState;

// Local state for animation search
static std::mutex s_resultsMutex;

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

        // Check for BB9 or FA1 chunk
        if (chunkId == GW::Parsers::CHUNK_ID_BB9 || chunkId == GW::Parsers::CHUNK_ID_FA1)
        {
            size_t chunkDataOffset = offset + 8;

            // Check if we have enough data for the header
            if (chunkDataOffset + sizeof(GW::Parsers::BB9Header) <= dataSize)
            {
                GW::Parsers::BB9Header header;
                std::memcpy(&header, &data[chunkDataOffset], sizeof(header));

                // Check if model hashes match
                if (header.modelHash0 == targetHash0 && header.modelHash1 == targetHash1)
                {
                    outResult.chunkType = (chunkId == GW::Parsers::CHUNK_ID_BB9) ? "BB9" : "FA1";

                    // Try to parse for more info
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
            if (mft[i].Hash == fileId)
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

