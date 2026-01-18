#include "pch.h"
#include "draw_animation_panel.h"
#include <GuiGlobalConstants.h>
#include <DATManager.h>
#include "Parsers/BB9AnimationParser.h"
#include <format>
#include <thread>
#include <mutex>

// Global animation state
AnimationPanelState g_animationState;

// Local state for the panel
static float s_playbackSpeed = 1.0f;  // User-friendly speed (1.0 = normal)
static bool s_showBoneInfo = false;
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
            uint32_t savedModelFileId = g_animationState.currentFileId;
            bool savedHasModel = g_animationState.hasModel;

            g_animationState.Initialize(clip, skeleton, result.fileId);

            // Restore model info
            g_animationState.modelHash0 = savedHash0;
            g_animationState.modelHash1 = savedHash1;
            g_animationState.hasModel = savedHasModel;

            // Reset playback speed
            s_playbackSpeed = 1.0f;
            if (g_animationState.controller)
            {
                g_animationState.controller->SetPlaybackSpeed(100000.0f);
            }
        }

        delete[] fileData;
    }
    catch (...)
    {
        // Ignore errors
    }
}

void draw_animation_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (!GuiGlobalConstants::is_animation_panel_open) return;

    // Store pointer for search thread
    s_datManagersPtr = &dat_managers;

    if (ImGui::Begin("Animation Controller", &GuiGlobalConstants::is_animation_panel_open, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        GuiGlobalConstants::ClampWindowToScreen();

        if (!g_animationState.hasAnimation || !g_animationState.controller)
        {
            if (g_animationState.hasModel)
            {
                // Model loaded but no animation - show model hashes and search UI
                ImGui::Text("Model File ID: %u (0x%08X)", g_animationState.currentFileId, g_animationState.currentFileId);
                ImGui::Separator();

                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Model Hashes:");
                ImGui::Text("  Hash0: %u (0x%08X)", g_animationState.modelHash0, g_animationState.modelHash0);
                ImGui::Text("  Hash1: %u (0x%08X)", g_animationState.modelHash1, g_animationState.modelHash1);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Search button and progress
                bool canSearch = !g_animationState.searchInProgress.load() &&
                                 g_animationState.modelHash0 != 0 &&
                                 !dat_managers.empty();

                if (g_animationState.searchInProgress.load())
                {
                    // Show progress
                    int processed = g_animationState.filesProcessed.load();
                    int total = g_animationState.totalFiles.load();
                    float progress = (total > 0) ? static_cast<float>(processed) / total : 0.0f;

                    char progressText[64];
                    snprintf(progressText, sizeof(progressText), "Searching... %d/%d files", processed, total);
                    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), progressText);

                    if (ImGui::Button("Cancel Search"))
                    {
                        g_animationState.searchInProgress.store(false);
                    }
                }
                else
                {
                    ImGui::BeginDisabled(!canSearch);
                    if (ImGui::Button("Find Animations", ImVec2(-1.0f, 0.0f)))
                    {
                        g_animationState.searchInProgress.store(true);
                        g_animationState.filesProcessed.store(0);
                        g_animationState.totalFiles.store(0);

                        uint32_t hash0 = g_animationState.modelHash0;
                        uint32_t hash1 = g_animationState.modelHash1;

                        std::thread(SearchForAnimationsWorker, hash0, hash1).detach();
                    }
                    ImGui::EndDisabled();

                    if (!canSearch && g_animationState.modelHash0 == 0)
                    {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                            "Select a model file to search for animations.");
                    }
                }

                // Show search results
                {
                    std::lock_guard<std::mutex> lock(s_resultsMutex);

                    if (!g_animationState.searchResults.empty())
                    {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Text("Found %zu animation file(s):", g_animationState.searchResults.size());

                        if (ImGui::BeginChild("AnimationResults", ImVec2(0, 150), true))
                        {
                            for (size_t i = 0; i < g_animationState.searchResults.size(); ++i)
                            {
                                const auto& result = g_animationState.searchResults[i];

                                bool isSelected = (g_animationState.selectedResultIndex == static_cast<int>(i));

                                std::string label = std::format("0x{:08X} - {} ({} seq, {} bones)##{}",
                                    result.fileId,
                                    result.chunkType,
                                    result.sequenceCount,
                                    result.boneCount,
                                    i);

                                if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                                {
                                    g_animationState.selectedResultIndex = static_cast<int>(i);

                                    // Double-click to load
                                    if (ImGui::IsMouseDoubleClicked(0))
                                    {
                                        LoadAnimationFromResult(result, dat_managers);
                                    }
                                }

                                if (ImGui::IsItemHovered())
                                {
                                    ImGui::SetTooltip("Double-click to load\nDAT%d, MFT Index: %d",
                                        result.datAlias, result.mftIndex);
                                }
                            }
                        }
                        ImGui::EndChild();

                        // Load button for selected result
                        bool hasSelection = g_animationState.selectedResultIndex >= 0 &&
                            g_animationState.selectedResultIndex < static_cast<int>(g_animationState.searchResults.size());

                        ImGui::BeginDisabled(!hasSelection);
                        if (ImGui::Button("Load Selected Animation", ImVec2(-1.0f, 0.0f)))
                        {
                            const auto& result = g_animationState.searchResults[g_animationState.selectedResultIndex];
                            LoadAnimationFromResult(result, dat_managers);
                        }
                        ImGui::EndDisabled();
                    }
                    else if (!g_animationState.searchInProgress.load() &&
                             g_animationState.filesProcessed.load() > 0)
                    {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                            "No matching animations found.");
                        ImGui::TextWrapped("This model may not have animation data in the DAT, or the animation files may use different hashes.");
                    }
                }
            }
            else
            {
                ImGui::TextWrapped("No model or animation loaded.");
                ImGui::TextWrapped("Select a model (FFNA Type2) from the DAT browser.");
            }
        }
        else
        {
            auto& ctrl = *g_animationState.controller;
            const auto& clip = g_animationState.clip;

            // File info
            ImGui::Text("Animation File ID: %u (0x%08X)", g_animationState.currentFileId, g_animationState.currentFileId);

            if (g_animationState.hasModel)
            {
                ImGui::Text("Model Hashes: 0x%08X / 0x%08X",
                    g_animationState.modelHash0, g_animationState.modelHash1);
            }

            // Animation info
            if (clip)
            {
                ImGui::Text("Bones: %zu, Sequences: %zu",
                    clip->boneTracks.size(), clip->sequences.size());
            }

            ImGui::Separator();

            // Playback controls section
            ImGui::Text("Playback Controls");
            ImGui::Separator();

            // Center the playback buttons
            float buttonWidth = 60.0f;
            float totalWidth = buttonWidth * 4 + ImGui::GetStyle().ItemSpacing.x * 3;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) / 2);

            // Play button
            bool isPlaying = ctrl.IsPlaying();
            if (isPlaying)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            }
            if (ImGui::Button(isPlaying ? "Pause" : "Play", ImVec2(buttonWidth, 0)))
            {
                ctrl.TogglePlayPause();
            }
            if (isPlaying)
            {
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(buttonWidth, 0)))
            {
                ctrl.Stop();
            }

            ImGui::SameLine();
            if (ImGui::Button("|<", ImVec2(buttonWidth, 0)))
            {
                ctrl.PreviousSequence();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Previous Sequence");
            }

            ImGui::SameLine();
            if (ImGui::Button(">|", ImVec2(buttonWidth, 0)))
            {
                ctrl.NextSequence();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Next Sequence");
            }

            ImGui::Spacing();

            // Time slider
            ImGui::Text("Timeline");
            ImGui::Separator();

            float startTime = ctrl.GetSequenceStartTime();
            float endTime = ctrl.GetSequenceEndTime();
            float currentTime = ctrl.GetTime();
            float normalizedTime = ctrl.GetNormalizedTime();

            // Format time display
            auto formatTime = [](float t) -> std::string {
                if (t >= 1000.0f)
                {
                    return std::format("{:.1f}k", t / 1000.0f);
                }
                return std::format("{:.0f}", t);
            };

            std::string timeLabel = std::format("{} / {} ({:.0f}%%)",
                formatTime(currentTime), formatTime(endTime), normalizedTime * 100.0f);

            // Time scrubber
            float scrubTime = currentTime;
            if (ImGui::SliderFloat(timeLabel.c_str(), &scrubTime, startTime, endTime, ""))
            {
                ctrl.SetTime(scrubTime);
            }

            ImGui::Spacing();

            // Sequence selection
            ImGui::Text("Sequence");
            ImGui::Separator();

            if (clip && !clip->sequences.empty())
            {
                size_t currentSeq = ctrl.GetCurrentSequenceIndex();
                std::string seqName = ctrl.GetCurrentSequenceName();
                if (seqName.empty())
                {
                    seqName = std::format("Sequence {}", currentSeq);
                }

                std::string comboLabel = std::format("{} ({}/{})", seqName, currentSeq + 1, clip->sequences.size());

                if (ImGui::BeginCombo("##Sequence", comboLabel.c_str()))
                {
                    for (size_t i = 0; i < clip->sequences.size(); i++)
                    {
                        const auto& seq = clip->sequences[i];
                        std::string label = seq.name.empty()
                            ? std::format("Sequence {} (hash: 0x{:08X})", i, seq.hash)
                            : std::format("{} (0x{:08X})", seq.name, seq.hash);

                        bool isSelected = (i == currentSeq);
                        if (ImGui::Selectable(label.c_str(), isSelected))
                        {
                            ctrl.SetSequence(i, true);
                        }
                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }

                        // Show sequence timing in tooltip
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Start: %.0f\nEnd: %.0f\nFrames: %u",
                                seq.startTime, seq.endTime, seq.frameCount);
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                ImGui::TextDisabled("No sequences defined");
            }

            ImGui::Spacing();

            // Playback options
            ImGui::Text("Options");
            ImGui::Separator();

            // Playback speed
            if (ImGui::SliderFloat("Speed", &s_playbackSpeed, 0.1f, 4.0f, "%.1fx"))
            {
                // Convert user-friendly speed to internal time units per second
                // At 1.0x speed, we want roughly 100000 time units per second
                ctrl.SetPlaybackSpeed(s_playbackSpeed * 100000.0f);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Playback speed multiplier. 1.0 = normal speed.");
            }

            // Reset speed button
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset"))
            {
                s_playbackSpeed = 1.0f;
                ctrl.SetPlaybackSpeed(100000.0f);
            }

            // Looping option
            bool looping = ctrl.IsLooping();
            if (ImGui::Checkbox("Loop", &looping))
            {
                ctrl.SetLooping(looping);
            }

            ImGui::SameLine();

            // Auto-cycle option
            bool autoCycle = ctrl.IsAutoCyclingSequences();
            if (ImGui::Checkbox("Auto-cycle sequences", &autoCycle))
            {
                ctrl.SetAutoCycleSequences(autoCycle);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Automatically advance to the next sequence when current one ends.");
            }

            ImGui::Spacing();

            // Visualization options
            if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& vis = g_animationState.visualization;

                // Mesh visibility and rendering mode
                ImGui::Checkbox("Show Mesh", &vis.showMesh);

                ImGui::SameLine();
                ImGui::Checkbox("Wireframe", &vis.wireframeMode);

                // Alpha slider (only enabled when mesh is visible)
                ImGui::BeginDisabled(!vis.showMesh);
                ImGui::SliderFloat("Mesh Alpha", &vis.meshAlpha, 0.0f, 1.0f, "%.2f");
                ImGui::EndDisabled();

                ImGui::Spacing();

                // Debug options
                ImGui::Checkbox("Disable Skinning", &vis.disableSkinning);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Show mesh in bind pose (no animation deformation).\nUseful for debugging skinning issues.");
                }

                ImGui::Spacing();

                // Bone visualization
                ImGui::Checkbox("Show Bones", &vis.showBones);

                if (vis.showBones)
                {
                    ImGui::Indent();
                    ImGui::SliderFloat("Joint Radius", &vis.jointRadius, 10.0f, 200.0f, "%.0f");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Radius of spheres at bone joints (in GW units)");
                    }

                    // Bone color picker
                    float boneColorArr[4] = { vis.boneColor.x, vis.boneColor.y, vis.boneColor.z, vis.boneColor.w };
                    if (ImGui::ColorEdit4("Bone Color", boneColorArr, ImGuiColorEditFlags_NoInputs))
                    {
                        vis.boneColor = { boneColorArr[0], boneColorArr[1], boneColorArr[2], boneColorArr[3] };
                    }

                    ImGui::SameLine();
                    float jointColorArr[4] = { vis.jointColor.x, vis.jointColor.y, vis.jointColor.z, vis.jointColor.w };
                    if (ImGui::ColorEdit4("Joint Color", jointColorArr, ImGuiColorEditFlags_NoInputs))
                    {
                        vis.jointColor = { jointColorArr[0], jointColorArr[1], jointColorArr[2], jointColorArr[3] };
                    }
                    ImGui::Unindent();
                }

                ImGui::Spacing();

                // Submesh visibility
                if (g_animationState.submeshCount > 0)
                {
                    ImGui::Text("Submeshes (%zu):", g_animationState.submeshCount);

                    // Show All / Hide All buttons
                    if (ImGui::SmallButton("Show All"))
                    {
                        for (size_t i = 0; i < vis.submeshVisibility.size(); ++i)
                            vis.submeshVisibility[i] = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Hide All"))
                    {
                        for (size_t i = 0; i < vis.submeshVisibility.size(); ++i)
                            vis.submeshVisibility[i] = false;
                    }

                    // Individual submesh toggles
                    if (ImGui::BeginChild("SubmeshList", ImVec2(0, std::min(120.0f, g_animationState.submeshCount * 22.0f)), true))
                    {
                        for (size_t i = 0; i < g_animationState.submeshCount && i < vis.submeshVisibility.size(); ++i)
                        {
                            std::string label;
                            if (i < g_animationState.submeshNames.size() && !g_animationState.submeshNames[i].empty())
                            {
                                label = std::format("{}##sub{}", g_animationState.submeshNames[i], i);
                            }
                            else
                            {
                                label = std::format("Submesh {}##sub{}", i, i);
                            }
                            // std::vector<bool> uses bit packing, need temp variable
                            bool isVisible = vis.submeshVisibility[i];
                            if (ImGui::Checkbox(label.c_str(), &isVisible))
                            {
                                vis.submeshVisibility[i] = isVisible;
                            }
                        }
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::Spacing();

            // Button to search for more animations
            if (g_animationState.hasModel && g_animationState.modelHash0 != 0)
            {
                ImGui::Separator();
                if (ImGui::Button("Find Other Animations...", ImVec2(-1.0f, 0.0f)))
                {
                    // Clear current animation to show search UI
                    g_animationState.controller.reset();
                    g_animationState.clip.reset();
                    g_animationState.skeleton.reset();
                    g_animationState.hasAnimation = false;
                }
            }

            // Bone info (collapsible)
            if (ImGui::CollapsingHeader("Bone Information"))
            {
                if (clip)
                {
                    ImGui::Text("Total bones: %zu", clip->boneTracks.size());

                    // Show bone list in a scrollable region
                    if (ImGui::BeginChild("BoneList", ImVec2(0, 150), true))
                    {
                        for (size_t i = 0; i < clip->boneTracks.size(); i++)
                        {
                            const auto& track = clip->boneTracks[i];
                            std::string boneLabel = std::format("Bone {} (P:{} R:{} S:{})",
                                i,
                                track.positionKeys.size(),
                                track.rotationKeys.size(),
                                track.scaleKeys.size());

                            if (ImGui::Selectable(boneLabel.c_str(), false))
                            {
                                // Could expand to show detailed bone info
                            }

                            if (ImGui::IsItemHovered() && !track.positionKeys.empty())
                            {
                                const auto& firstPos = track.positionKeys[0];
                                int32_t parentIdx = (i < clip->boneParents.size()) ? clip->boneParents[i] : -1;
                                ImGui::SetTooltip("Parent: %d\nFirst keyframe pos: (%.2f, %.2f, %.2f)",
                                    parentIdx,
                                    firstPos.value.x, firstPos.value.y, firstPos.value.z);
                            }
                        }
                    }
                    ImGui::EndChild();
                }
            }
        }
    }
    ImGui::End();
}
