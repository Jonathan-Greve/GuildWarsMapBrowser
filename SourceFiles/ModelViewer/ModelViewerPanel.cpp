#include "pch.h"
#include "ModelViewerPanel.h"
#include "ModelViewer.h"
#include "animation_state.h"
#include "GuiGlobalConstants.h"
#include "MapRenderer.h"
#include "DATManager.h"
#include "Animation/GWAnimationHashes.h"
#include "Parsers/FileReferenceParser.h"
#include "imgui.h"
#include <format>
#include <algorithm>
#include <filesystem>

namespace
{
int FindFA8AnimationSourceIndex(const std::vector<AnimationSource>& sources, uint8_t sourceType)
{
    if (sourceType == 0)
    {
        return -1;
    }

    uint8_t current = 1;
    for (size_t i = 0; i < sources.size(); i++)
    {
        if (sources[i].referenceChunkId == GW::Parsers::CHUNK_ID_FA8)
        {
            if (current == sourceType)
            {
                return static_cast<int>(i);
            }
            current++;
        }
    }
    return -1;
}

size_t FindBestSegmentByHash(const GW::Animation::AnimationClip& clip, uint32_t hash)
{
    const auto& segments = clip.animationSegments;
    if (segments.empty())
    {
        return static_cast<size_t>(-1);
    }

    const bool hasSourceTypes = (clip.animationSegmentSourceTypes.size() == segments.size());
    size_t bestLocal = static_cast<size_t>(-1);
    size_t bestAny = static_cast<size_t>(-1);
    uint32_t bestLocalDuration = 0;
    uint32_t bestAnyDuration = 0;

    for (size_t i = 0; i < segments.size(); i++)
    {
        const auto& seg = segments[i];
        if (seg.hash != hash)
        {
            continue;
        }

        const uint32_t duration = seg.GetDuration();
        const bool isLocal = !hasSourceTypes || clip.GetSegmentSourceType(i) == 0;
        if (isLocal && (bestLocal == static_cast<size_t>(-1) || duration > bestLocalDuration))
        {
            bestLocal = i;
            bestLocalDuration = duration;
        }
        if (bestAny == static_cast<size_t>(-1) || duration > bestAnyDuration)
        {
            bestAny = i;
            bestAnyDuration = duration;
        }
    }

    return (bestLocal != static_cast<size_t>(-1)) ? bestLocal : bestAny;
}

bool ResolveAndPlayExternalFA1Segment(
    uint8_t sourceType,
    uint32_t segmentHash,
    std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    const int sourceIndex = FindFA8AnimationSourceIndex(g_animationState.animationSources, sourceType);
    if (sourceIndex < 0)
    {
        return false;
    }

    LoadAnimationFromReference(sourceIndex, dat_managers);
    if (!g_animationState.clip || !g_animationState.controller)
    {
        return false;
    }

    auto& resolvedClip = *g_animationState.clip;
    const size_t resolvedSegment = FindBestSegmentByHash(resolvedClip, segmentHash);
    if (resolvedSegment == static_cast<size_t>(-1))
    {
        return false;
    }

    g_animationState.playbackMode = AnimationPlaybackMode::SegmentLoop;
    g_animationState.controller->SetPlaybackMode(GW::Animation::PlaybackMode::SegmentLoop);
    g_animationState.controller->SetSegment(resolvedSegment);
    return true;
}
} // namespace

void draw_model_viewer_panel(MapRenderer* mapRenderer, std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (!GuiGlobalConstants::is_model_viewer_panel_open)
        return;

    if (!g_modelViewerState.isActive)
    {
        ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Model Viewer", &GuiGlobalConstants::is_model_viewer_panel_open))
        {
            ImGui::TextWrapped("Model viewer is not active. Load a model from the DAT browser to activate.");
            if (ImGui::Button("Close"))
            {
                GuiGlobalConstants::is_model_viewer_panel_open = false;
            }
        }
        ImGui::End();
        return;
    }

    auto& state = g_modelViewerState;
    auto& options = state.options;
    auto& vis = g_animationState.visualization;

    // Sync bone info from animation state
    if (g_animationState.clip && g_animationState.clip->boneTracks.size() != state.bones.size())
    {
        state.animClip = g_animationState.clip;
        state.animController = g_animationState.controller;
        state.UpdateBoneInfo();
    }

    // Sync meshes if changed
    if (g_animationState.originalMeshes.size() != state.meshes.size())
    {
        state.meshes = g_animationState.originalMeshes;
        state.meshIds = g_animationState.meshIds;
        state.vertexBoneGroups = g_animationState.perVertexBoneGroups;
        state.ComputeBounds();
    }

    ImGui::SetNextWindowSize(ImVec2(340, 550), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Model Viewer", &GuiGlobalConstants::is_model_viewer_panel_open))
    {
        bool deferExternalSegmentResolve = false;
        uint8_t deferredExternalSourceType = 0;
        uint32_t deferredExternalSegmentHash = 0;

        GuiGlobalConstants::ClampWindowToScreen();

        // Model info header
        if (state.modelFileId != 0)
        {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Model: 0x%X", state.modelFileId);
            ImGui::SameLine();
            ImGui::TextDisabled("| %zu meshes | %zu bones", state.meshes.size(), state.bones.size());
        }

        ImGui::Separator();

        // ========== ANIMATION SECTION ==========
        if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (g_animationState.hasAnimation && g_animationState.controller)
            {
                auto& ctrl = *g_animationState.controller;
                auto& settings = g_animationState.playbackSettings;
                auto& clip = g_animationState.clip;

                // Current animation info
                ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[%s] 0x%X",
                    g_animationState.currentChunkType.empty() ? "?" : g_animationState.currentChunkType.c_str(),
                    g_animationState.currentFileId);
                if (clip)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("| %zu bones | %zu seq",
                        clip->boneTracks.size(),
                        clip->sequences.size());
                }
                ImGui::Spacing();

                // --- Animation Group Selector (primary control for selecting distinct animations) ---
                if (clip && !clip->animationGroups.empty())
                {
                    ImGui::Text("Animation:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                    int currentGroup = g_animationState.currentAnimationGroupIndex;
                    const auto& groups = clip->animationGroups;
                    std::string currentLabel = (currentGroup >= 0 && currentGroup < static_cast<int>(groups.size()))
                        ? groups[currentGroup].displayName : "None";

                    if (ImGui::BeginCombo("##AnimGroup", currentLabel.c_str()))
                    {
                        for (size_t i = 0; i < groups.size(); i++)
                        {
                            const auto& g = groups[i];
                            // Show duration in seconds (GW time units / 100000)
                            std::string label = std::format("{} ({:.2f}s, {} phases)",
                                g.displayName, g.GetDuration() / 100000.0f, g.GetPhaseCount());

                            bool isSelected = (static_cast<int>(i) == currentGroup);
                            if (ImGui::Selectable(label.c_str(), isSelected))
                            {
                                g_animationState.currentAnimationGroupIndex = static_cast<int>(i);
                                ctrl.SetAnimationGroup(i);
                            }
                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TextDisabled("%zu animations in file", groups.size());
                    ImGui::Spacing();
                }

                // --- Playback Mode Selection ---
                ImGui::Text("Mode:");
                ImGui::SameLine();

                auto mode = g_animationState.playbackMode;

                if (ImGui::RadioButton("Phase", mode == AnimationPlaybackMode::SinglePhase))
                {
                    g_animationState.playbackMode = AnimationPlaybackMode::SinglePhase;
                    ctrl.SetPlaybackMode(GW::Animation::PlaybackMode::SinglePhase);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Play only one sequence/phase at a time");
                }

                ImGui::SameLine();
                if (ImGui::RadioButton("All", mode == AnimationPlaybackMode::EntireFile))
                {
                    g_animationState.playbackMode = AnimationPlaybackMode::EntireFile;
                    ctrl.SetPlaybackMode(GW::Animation::PlaybackMode::EntireFile);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Play entire file from start to end");
                }

                ImGui::SameLine();
                if (ImGui::RadioButton("Segment", mode == AnimationPlaybackMode::SegmentLoop))
                {
                    g_animationState.playbackMode = AnimationPlaybackMode::SegmentLoop;
                    ctrl.SetPlaybackMode(GW::Animation::PlaybackMode::SegmentLoop);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Play and loop a single animation segment\n"
                                      "(sub-animation within phases like /laugh, /cheer, etc.)");
                }

                // Segment selector (shown only in SegmentLoop mode)
                if (mode == AnimationPlaybackMode::SegmentLoop && clip && !clip->animationSegments.empty())
                {
                    int currentSeg = static_cast<int>(ctrl.GetCurrentSegmentIndex());
                    const auto& segments = clip->animationSegments;
                    const bool isFA1 = (clip->sourceChunkType == "FA1");
                    const bool hasSourceTypes = (clip->animationSegmentSourceTypes.size() == segments.size());

                    static bool s_showExternalFA1Segments = false;

                    size_t localSegmentCount = 0;
                    size_t externalSegmentCount = 0;
                    std::vector<size_t> visibleIndices;
                    std::vector<size_t> selectableIndices;
                    visibleIndices.reserve(segments.size());
                    selectableIndices.reserve(segments.size());

                    for (size_t i = 0; i < segments.size(); i++)
                    {
                        uint8_t sourceType = clip->GetSegmentSourceType(i);
                        bool isExternal = isFA1 && hasSourceTypes && sourceType != 0;
                        if (isExternal) externalSegmentCount++;
                        else localSegmentCount++;

                        if (isExternal && !s_showExternalFA1Segments)
                        {
                            continue;
                        }

                        visibleIndices.push_back(i);
                        if (!isExternal)
                        {
                            selectableIndices.push_back(i);
                        }
                    }

                    if (isFA1 && hasSourceTypes)
                    {
                        ImGui::Checkbox("Show External FA1 Segments", &s_showExternalFA1Segments);
                        ImGui::SameLine();
                        ImGui::TextDisabled("%zu local | %zu external", localSegmentCount, externalSegmentCount);
                    }

                    if (!visibleIndices.empty())
                    {
                        auto isVisible = [&](size_t index) -> bool
                        {
                            return std::find(visibleIndices.begin(), visibleIndices.end(), index) != visibleIndices.end();
                        };

                        if (currentSeg < 0 || currentSeg >= static_cast<int>(segments.size()) ||
                            !isVisible(static_cast<size_t>(currentSeg)))
                        {
                            if (!selectableIndices.empty())
                            {
                                ctrl.SetSegment(selectableIndices[0]);
                                currentSeg = static_cast<int>(selectableIndices[0]);
                            }
                            else
                            {
                                currentSeg = static_cast<int>(visibleIndices[0]);
                            }
                        }

                        // Find longest visible segment for reference.
                        size_t longestSegIdx = visibleIndices[0];
                        uint32_t longestDuration = 0;
                        for (size_t idx : visibleIndices)
                        {
                            uint32_t dur = segments[idx].GetDuration();
                            if (dur > longestDuration)
                            {
                                longestDuration = dur;
                                longestSegIdx = idx;
                            }
                        }

                        size_t displayPos = 0;
                        for (size_t i = 0; i < visibleIndices.size(); i++)
                        {
                            if (static_cast<int>(visibleIndices[i]) == currentSeg)
                            {
                                displayPos = i;
                                break;
                            }
                        }

                        std::string segLabel = std::format("Segment {} / {}", displayPos + 1, visibleIndices.size());
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::BeginCombo("##Segment", segLabel.c_str()))
                        {
                            bool resolveExternal = false;
                            uint8_t resolveSourceType = 0;
                            uint32_t resolveHash = 0;

                            for (size_t displayIndex = 0; displayIndex < visibleIndices.size(); displayIndex++)
                            {
                                size_t i = visibleIndices[displayIndex];
                                bool isSelected = (static_cast<int>(i) == currentSeg);
                                const auto& seg = segments[i];
                                uint8_t sourceType = clip->GetSegmentSourceType(i);
                                bool isExternal = isFA1 && hasSourceTypes && sourceType != 0;

                                float durationSec = static_cast<float>(seg.GetDuration()) / 100000.0f;
                                float startSec = static_cast<float>(seg.startTime) / 100000.0f;
                                float endSec = static_cast<float>(seg.endTime) / 100000.0f;

                                std::string marker = (i == longestSegIdx) ? " [main]" : "";
                                std::string sourceLabel = isExternal
                                    ? std::format(" src:ref{}", sourceType)
                                    : " src:local";

                                std::string animName = GW::Animation::GetAnimationNameFromHash(seg.hash);
                                std::string hashStr = animName.empty() ? std::format("0x{:08X}", seg.hash) : animName;
                                std::string label = std::format("Seg {} ({}) {:.2f}s [{:.2f}-{:.2f}]{}{}##{}",
                                    displayIndex + 1, hashStr, durationSec, startSec, endSec, marker, sourceLabel, i);

                                if (ImGui::Selectable(label.c_str(), isSelected))
                                {
                                    if (isExternal)
                                    {
                                        resolveExternal = true;
                                        resolveSourceType = sourceType;
                                        resolveHash = seg.hash;
                                    }
                                    else
                                    {
                                        ctrl.SetSegment(i);
                                    }
                                }
                                if (isSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }

                                if (ImGui::IsItemHovered())
                                {
                                    std::string knownName = GW::Animation::GetAnimationCategorizedName(seg.hash);
                                    float loopOffsetSec = static_cast<float>(seg.GetLoopStartOffset()) / 100000.0f;
                                    ImGui::SetTooltip(
                                        "Name: %s\nHash: 0x%08X\nStart: %.3fs\nEnd: %.3fs\nDuration: %.3fs\nPhase Range: [%u, %u)\nLoop Offset: %.3fs\nTransition: %.3f\nSource: %s",
                                        knownName.c_str(), seg.hash, startSec, endSec, durationSec,
                                        static_cast<unsigned>(seg.GetPhaseStartIndex()),
                                        static_cast<unsigned>(seg.GetPhaseEndIndex()),
                                        loopOffsetSec, seg.GetTransitionParam(),
                                        isExternal ? "External FA1 reference (select to resolve and load source clip)"
                                                   : "Local clip");
                                }
                            }
                            ImGui::EndCombo();

                            if (resolveExternal)
                            {
                                deferExternalSegmentResolve = true;
                                deferredExternalSourceType = resolveSourceType;
                                deferredExternalSegmentHash = resolveHash;
                            }
                        }

                        // Show current segment info.
                        if (currentSeg >= 0 && currentSeg < static_cast<int>(segments.size()))
                        {
                            const auto& seg = segments[currentSeg];
                            uint8_t sourceType = clip->GetSegmentSourceType(static_cast<size_t>(currentSeg));
                            bool isExternal = isFA1 && hasSourceTypes && sourceType != 0;
                            float durationSec = static_cast<float>(seg.GetDuration()) / 100000.0f;
                            std::string segName = GW::Animation::GetAnimationDisplayName(seg.hash);
                            std::string sourceLabel = isExternal
                                ? std::format("[src ref{}]", sourceType)
                                : "[src local]";
                            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                                "%s | %.2fs", segName.c_str(), durationSec);
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", sourceLabel.c_str());
                            if (static_cast<size_t>(currentSeg) == longestSegIdx)
                            {
                                ImGui::SameLine();
                                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[main loop]");
                            }
                        }

                        // Segment navigation uses only selectable (local-playable) entries.
                        int currentSelectablePos = -1;
                        for (size_t i = 0; i < selectableIndices.size(); i++)
                        {
                            if (static_cast<int>(selectableIndices[i]) == currentSeg)
                            {
                                currentSelectablePos = static_cast<int>(i);
                                break;
                            }
                        }

                        bool canPrev = currentSelectablePos > 0;
                        bool canNext = currentSelectablePos >= 0 &&
                                       currentSelectablePos < static_cast<int>(selectableIndices.size()) - 1;

                        if (!canPrev) ImGui::BeginDisabled();
                        if (ImGui::Button("< Prev Segment", ImVec2(110, 0)) && canPrev)
                        {
                            ctrl.SetSegment(selectableIndices[static_cast<size_t>(currentSelectablePos - 1)]);
                        }
                        if (!canPrev) ImGui::EndDisabled();

                        ImGui::SameLine();

                        if (!canNext) ImGui::BeginDisabled();
                        if (ImGui::Button("Next Segment >", ImVec2(110, 0)) && canNext)
                        {
                            ctrl.SetSegment(selectableIndices[static_cast<size_t>(currentSelectablePos + 1)]);
                        }
                        if (!canNext) ImGui::EndDisabled();
                    }
                    else
                    {
                        ImGui::TextDisabled("No segments match the current FA1 segment filter.");
                    }
                }

                ImGui::Spacing();

                // Playback controls row
                bool isPlaying = ctrl.IsPlaying();
                if (ImGui::Button(isPlaying ? "Pause" : "Play", ImVec2(55, 0)))
                {
                    if (isPlaying) ctrl.Pause();
                    else ctrl.Play();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop", ImVec2(55, 0)))
                {
                    ctrl.Stop();
                }
                ImGui::SameLine();
                bool looping = ctrl.IsLooping();
                if (ImGui::Checkbox("Loop", &looping))
                {
                    ctrl.SetLooping(looping);
                    settings.looping = looping;
                }

                // Speed control
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
                if (ImGui::SliderFloat("##Speed", &settings.playbackSpeed, 0.1f, 4.0f, "Speed: %.1fx"))
                {
                    ctrl.SetPlaybackSpeed(settings.playbackSpeed * 100000.0f);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("1x"))
                {
                    settings.playbackSpeed = 1.0f;
                    ctrl.SetPlaybackSpeed(100000.0f);
                }

                // Timeline
                if (g_animationState.clip)
                {
                    float currentTime = ctrl.GetTime();
                    float startTime = ctrl.GetSequenceStartTime();
                    float endTime = ctrl.GetSequenceEndTime();
                    float duration = endTime - startTime;

                    if (duration > 0)
                    {
                        float progress = std::clamp((currentTime - startTime) / duration, 0.0f, 1.0f);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 45);
                        if (ImGui::SliderFloat("##Timeline", &progress, 0.0f, 1.0f, ""))
                        {
                            ctrl.SetTime(startTime + progress * duration);
                        }
                        ImGui::SameLine();
                        ImGui::Text("%5.1f%%", progress * 100.0f);
                    }
                }

                // Sequence/Phase selector (shown only in SinglePhase mode)
                if (mode == AnimationPlaybackMode::SinglePhase && clip && !clip->sequences.empty())
                {
                    int currentSeq = static_cast<int>(ctrl.GetCurrentSequenceIndex());
                    const auto& sequences = clip->sequences;
                    std::string seqLabel = std::format("Phase {} / {}", currentSeq + 1, sequences.size());

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##Sequence", seqLabel.c_str()))
                    {
                        for (size_t i = 0; i < sequences.size(); i++)
                        {
                            bool isSelected = (static_cast<int>(i) == currentSeq);
                            const auto& seq = sequences[i];
                            std::string seqName = GW::Animation::GetAnimationDisplayName(seq.hash);
                            std::string label = std::format("Phase {} ({}, {:.2f}s)",
                                i + 1, seqName, seq.GetDuration() / 100000.0f);
                            if (ImGui::Selectable(label.c_str(), isSelected))
                            {
                                ctrl.SetSequence(i);
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                // Current phase display (for all modes)
                if (clip && !clip->sequences.empty())
                {
                    int phaseIdx = static_cast<int>(ctrl.GetCurrentSequenceIndex());
                    if (phaseIdx >= 0 && phaseIdx < static_cast<int>(clip->sequences.size()))
                    {
                        const auto& seq = clip->sequences[phaseIdx];
                        std::string phaseName = GW::Animation::GetAnimationDisplayName(seq.hash);
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
                            "Phase %d/%zu: %s",
                            phaseIdx + 1,
                            clip->sequences.size(),
                            phaseName.c_str());
                    }
                }

                // ========== ANIMATION TIMELINE ==========
                if (clip && clip->IsValid())
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Timeline");

                    float startTime = ctrl.GetSequenceStartTime();
                    float endTime = ctrl.GetSequenceEndTime();
                    float currentTime = ctrl.GetTime();
                    float duration = endTime - startTime;

                    if (duration > 0)
                    {
                        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                        ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 60);
                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        // Background
                        drawList->AddRectFilled(canvasPos,
                            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(30, 30, 30, 255));

                        // Animation phases (colored bars)
                        float phaseHeight = 18.0f;
                        for (size_t i = 0; i < clip->sequences.size(); i++)
                        {
                            const auto& seq = clip->sequences[i];
                            if (seq.endTime < startTime || seq.startTime > endTime) continue;

                            float x1 = canvasPos.x + ((seq.startTime - startTime) / duration) * canvasSize.x;
                            float x2 = canvasPos.x + ((seq.endTime - startTime) / duration) * canvasSize.x;
                            float y1 = canvasPos.y + 3;
                            float y2 = y1 + phaseHeight;

                            // Color by phase index
                            ImU32 phaseColor = ImGui::ColorConvertFloat4ToU32(
                                ImVec4(0.3f + (i % 3) * 0.2f, 0.5f, 0.3f + (i % 2) * 0.3f, 0.8f));
                            drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), phaseColor);
                            drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(100, 100, 100, 255));
                        }

                        // Sound events (triangles/markers)
                        auto& soundMgr = g_animationState.soundManager;
                        if (soundMgr && soundMgr->HasSounds())
                        {
                            float soundY = canvasPos.y + 28;
                            for (const auto& event : soundMgr->GetSoundEvents())
                            {
                                float eventTime = event.timing;
                                if (eventTime < startTime || eventTime > endTime) continue;

                                float x = canvasPos.x + ((eventTime - startTime) / duration) * canvasSize.x;

                                // Sound marker (triangle pointing down)
                                drawList->AddTriangleFilled(
                                    ImVec2(x, soundY),
                                    ImVec2(x - 4, soundY + 8),
                                    ImVec2(x + 4, soundY + 8),
                                    IM_COL32(255, 200, 50, 255));

                                // Vertical line to timeline
                                drawList->AddLine(
                                    ImVec2(x, soundY + 8),
                                    ImVec2(x, canvasPos.y + canvasSize.y - 5),
                                    IM_COL32(255, 200, 50, 100));
                            }
                        }

                        // Current time indicator (vertical red line)
                        float timeX = canvasPos.x + ((currentTime - startTime) / duration) * canvasSize.x;
                        drawList->AddLine(
                            ImVec2(timeX, canvasPos.y),
                            ImVec2(timeX, canvasPos.y + canvasSize.y),
                            IM_COL32(255, 50, 50, 255), 2.0f);

                        // Time labels
                        drawList->AddText(ImVec2(canvasPos.x + 2, canvasPos.y + canvasSize.y - 13),
                            IM_COL32(200, 200, 200, 255),
                            std::format("{:.2f}s", startTime / 100000.0f).c_str());
                        drawList->AddText(ImVec2(canvasPos.x + canvasSize.x - 35, canvasPos.y + canvasSize.y - 13),
                            IM_COL32(200, 200, 200, 255),
                            std::format("{:.2f}s", endTime / 100000.0f).c_str());

                        ImGui::Dummy(canvasSize);  // Reserve space

                        // Legend
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "=");
                        ImGui::SameLine();
                        ImGui::TextDisabled("Phases");
                        if (soundMgr && soundMgr->HasSounds())
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "v");
                            ImGui::SameLine();
                            ImGui::TextDisabled("Sounds");
                        }
                    }
                }

                // ========== SOUND CONTROLS ==========
                if (g_animationState.soundManager)
                {
                    auto& soundMgr = *g_animationState.soundManager;

                    ImGui::Spacing();
                    ImGui::Separator();

                    bool enabled = soundMgr.IsEnabled();
                    if (ImGui::Checkbox("Play Sounds", &enabled))
                    {
                        soundMgr.SetEnabled(enabled);
                    }

                    if (enabled)
                    {
                        ImGui::SameLine();
                        float volume = soundMgr.GetVolume();
                        ImGui::SetNextItemWidth(80);
                        if (ImGui::SliderFloat("##SoundVol", &volume, 0.0f, 1.0f, "%.0f%%"))
                        {
                            soundMgr.SetVolume(volume);
                        }
                    }

                    // Show loaded sounds count
                    ImGui::TextDisabled("%zu sounds, %zu events",
                        soundMgr.GetSoundFileIds().size(),
                        soundMgr.GetSoundEvents().size());
                }

                ImGui::Spacing();
            }
            else if (g_animationState.hasModel)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No animation loaded");
                ImGui::Spacing();
            }

            // Animation Search subsection
            if (g_animationState.hasModel)
            {
                if (g_animationState.searchInProgress.load())
                {
                    float progress = g_animationState.totalFiles.load() > 0
                        ? static_cast<float>(g_animationState.filesProcessed.load()) / g_animationState.totalFiles.load()
                        : 0.0f;
                    ImGui::ProgressBar(progress, ImVec2(-1, 0),
                        std::format("Searching... {}/{}",
                            g_animationState.filesProcessed.load(),
                            g_animationState.totalFiles.load()).c_str());

                    if (ImGui::Button("Cancel Search", ImVec2(-1, 0)))
                    {
                        CancelAnimationSearch();
                    }
                }
                else
                {
                    if (ImGui::Button("Search Animations", ImVec2(-1, 0)))
                    {
                        if (g_animationState.modelHash0 != 0)
                        {
                            StartAnimationSearch(dat_managers);
                        }
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Search DAT files for animations\nmatching this model's hashes");
                    }
                }

                // Search results
                if (!g_animationState.searchResults.empty())
                {
                    ImGui::Spacing();
                    ImGui::Text("Found %zu animation(s):", g_animationState.searchResults.size());

                    if (ImGui::BeginChild("##AnimResults", ImVec2(-1, 80), true))
                    {
                        for (size_t i = 0; i < g_animationState.searchResults.size(); i++)
                        {
                            const auto& result = g_animationState.searchResults[i];
                            bool isSelected = (g_animationState.selectedResultIndex == static_cast<int>(i));

                            std::string label = std::format("[{}] 0x{:X} - {} seq, {} bones##{}",
                                result.chunkType, result.fileId, result.sequenceCount, result.boneCount, i);

                            if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                            {
                                g_animationState.selectedResultIndex = static_cast<int>(i);

                                // Double-click to load
                                if (ImGui::IsMouseDoubleClicked(0))
                                {
                                    LoadAnimationFromSearchResult(static_cast<int>(i), dat_managers);
                                }
                            }

                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("Type: %s\nFile: 0x%X\nSequences: %u\nBones: %u\n\nDouble-click to load\nRight-click for options",
                                    result.chunkType.c_str(), result.fileId, result.sequenceCount, result.boneCount);
                            }

                            // Context menu for each animation result item
                            std::string contextMenuId = std::format("AnimResultContext##{}", i);
                            if (ImGui::BeginPopupContextItem(contextMenuId.c_str()))
                            {
                                if (ImGui::MenuItem("Load Animation"))
                                {
                                    LoadAnimationFromSearchResult(static_cast<int>(i), dat_managers);
                                }

                                if (ImGui::MenuItem("Save Decompressed Data to File"))
                                {
                                    // Find the DAT manager for this result
                                    auto it = dat_managers.find(result.datAlias);
                                    if (it != dat_managers.end() && it->second)
                                    {
                                        std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", result.fileId), L"gwraw");
                                        if (!savePath.empty())
                                        {
                                            it->second->save_raw_decompressed_data_to_file(result.mftIndex, savePath);
                                        }
                                    }
                                }

                                ImGui::EndPopup();
                            }
                        }
                    }
                    ImGui::EndChild();

                    // Load button
                    bool hasSelection = g_animationState.selectedResultIndex >= 0 &&
                        g_animationState.selectedResultIndex < static_cast<int>(g_animationState.searchResults.size());

                    ImGui::BeginDisabled(!hasSelection);
                    if (ImGui::Button("Load Selected", ImVec2(-1, 0)))
                    {
                        LoadAnimationFromSearchResult(g_animationState.selectedResultIndex, dat_managers);
                    }
                    ImGui::EndDisabled();

                    // Extract all animations button
                    if (ImGui::Button("Extract All Animations", ImVec2(-1, 0)))
                    {
                        std::wstring saveDir = OpenDirectoryDialog();
                        if (!saveDir.empty())
                        {
                            for (const auto& result : g_animationState.searchResults)
                            {
                                auto it = dat_managers.find(result.datAlias);
                                if (it != dat_managers.end() && it->second)
                                {
                                    // Format: type_{chunkType}_fileId_{hash}_anim.gwraw
                                    std::wstring filename = std::format(L"type_{}_fileId_0x{:X}_anim.gwraw",
                                        result.chunkType == "FA1" ? L"FA1" : L"BB9",
                                        result.fileId);

                                    std::filesystem::path fullPath = std::filesystem::path(saveDir) / filename;
                                    it->second->save_raw_decompressed_data_to_file(result.mftIndex, fullPath.wstring());
                                }
                            }
                        }
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Save all found animations to a folder");
                    }
                }
            }

            // --- Animation File References (BBC/BBD chunks) ---
            if (g_animationState.hasScannedReferences && !g_animationState.animationSources.empty())
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f),
                    "Referenced Files (%zu):", g_animationState.animationSources.size());

                if (ImGui::BeginChild("##AnimRefs", ImVec2(-1, 100), true))
                {
                    for (size_t i = 0; i < g_animationState.animationSources.size(); i++)
                    {
                        const auto& source = g_animationState.animationSources[i];
                        bool isLoaded = source.isLoaded;
                        bool canLoad = source.mftIndex >= 0;

                        // Color coding: green if loaded, yellow if available, gray if not found
                        ImVec4 color = isLoaded ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                            : (canLoad ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                        std::string chunkLabel = source.chunkType;
                        if (source.referenceChunkId == GW::Parsers::CHUNK_ID_FA8 && source.referenceIndex > 0)
                        {
                            chunkLabel = std::format("{}#{}", source.chunkType, source.referenceIndex);
                        }

                        std::string label = std::format("[{}] 0x{:X}{}##ref{}",
                            chunkLabel,
                            source.fileId,
                            isLoaded ? " (loaded)" : (canLoad ? "" : " (not found)"),
                            i);

                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        if (ImGui::Selectable(label.c_str(), false, canLoad ? ImGuiSelectableFlags_AllowDoubleClick : ImGuiSelectableFlags_Disabled))
                        {
                            if (canLoad && ImGui::IsMouseDoubleClicked(0))
                            {
                                LoadAnimationFromReference(static_cast<int>(i), dat_managers);
                            }
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered() && canLoad)
                        {
                            ImGui::SetTooltip(
                                "File ID: 0x%X\nChunk: %s\nReference Index: %u\nMFT Index: %d\n\nDouble-click to load",
                                source.fileId,
                                source.chunkType.c_str(),
                                source.referenceIndex,
                                source.mftIndex);
                        }

                        // Context menu
                        std::string contextMenuId = std::format("AnimRefContext##{}", i);
                        if (ImGui::BeginPopupContextItem(contextMenuId.c_str()))
                        {
                            if (ImGui::MenuItem("Load Animation", nullptr, false, canLoad))
                            {
                                LoadAnimationFromReference(static_cast<int>(i), dat_managers);
                            }

                            if (ImGui::MenuItem("Save Decompressed Data", nullptr, false, canLoad))
                            {
                                auto it = dat_managers.find(source.datAlias);
                                if (it != dat_managers.end() && it->second)
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", source.fileId), L"gwraw");
                                    if (!savePath.empty())
                                    {
                                        it->second->save_raw_decompressed_data_to_file(source.mftIndex, savePath);
                                    }
                                }
                            }

                            ImGui::EndPopup();
                        }
                    }
                }
                ImGui::EndChild();

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Animation files referenced by this file.\n"
                                      "These share the same skeleton and contain\n"
                                      "additional animation sequences.\n\n"
                                      "Double-click to load.");
                }
            }

            // --- Sound Event Sources (Type 8 files from BBC refs) ---
            if (!g_animationState.soundEventSources.empty())
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                    "Sound Event Files (%zu):", g_animationState.soundEventSources.size());

                if (ImGui::BeginChild("##SoundSources", ImVec2(-1, 60), true))
                {
                    for (size_t i = 0; i < g_animationState.soundEventSources.size(); i++)
                    {
                        const auto& source = g_animationState.soundEventSources[i];
                        bool isLoaded = source.isLoaded;
                        bool canLoad = source.mftIndex >= 0;
                        bool isSelected = (g_animationState.currentSoundSourceIndex == static_cast<int>(i));

                        // Color coding: green if loaded, yellow if available, gray if not found
                        ImVec4 color = isLoaded ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                            : (canLoad ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                        std::string label = std::format("[T8] 0x{:X}{}##snd{}",
                            source.fileId,
                            isLoaded ? " (loaded)" : (canLoad ? "" : " (not found)"),
                            i);

                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        if (ImGui::Selectable(label.c_str(), isSelected, canLoad ? ImGuiSelectableFlags_AllowDoubleClick : ImGuiSelectableFlags_Disabled))
                        {
                            if (canLoad && ImGui::IsMouseDoubleClicked(0))
                            {
                                LoadSoundEventsFromReference(static_cast<int>(i), dat_managers);
                            }
                        }
                        ImGui::PopStyleColor();

                        if (ImGui::IsItemHovered() && canLoad)
                        {
                            ImGui::SetTooltip("Sound Event File\nFile ID: 0x%X\nMFT Index: %d\n\nDouble-click to load",
                                source.fileId, source.mftIndex);
                        }

                        // Context menu
                        std::string contextMenuId = std::format("SoundSrcContext##{}", i);
                        if (ImGui::BeginPopupContextItem(contextMenuId.c_str()))
                        {
                            if (ImGui::MenuItem("Load Sound Events", nullptr, false, canLoad))
                            {
                                LoadSoundEventsFromReference(static_cast<int>(i), dat_managers);
                            }

                            if (ImGui::MenuItem("Save Decompressed Data", nullptr, false, canLoad))
                            {
                                auto it = dat_managers.find(source.datAlias);
                                if (it != dat_managers.end() && it->second)
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", source.fileId), L"gwraw");
                                    if (!savePath.empty())
                                    {
                                        it->second->save_raw_decompressed_data_to_file(source.mftIndex, savePath);
                                    }
                                }
                            }

                            ImGui::EndPopup();
                        }
                    }
                }
                ImGui::EndChild();

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Type 8 sound event files.\n"
                                      "These contain timing data for sounds\n"
                                      "to play during animation.\n\n"
                                      "Double-click to load.");
                }
            }
        }

        // ========== VIEW OPTIONS SECTION ==========
        if (ImGui::CollapsingHeader("View Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Mesh options row
            if (ImGui::Checkbox("Mesh", &vis.showMesh))
            {
                options.showMesh = vis.showMesh;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Wireframe", &vis.wireframeMode))
            {
                options.showWireframe = vis.wireframeMode;
            }

            // Mesh alpha
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("##MeshAlpha", &vis.meshAlpha, 0.0f, 1.0f, "Mesh Alpha: %.2f");

            // Lock root position option
            if (g_animationState.hasAnimation && g_animationState.controller)
            {
                bool lockRoot = vis.lockRootPosition;
                if (ImGui::Checkbox("Lock Root Position", &lockRoot))
                {
                    vis.lockRootPosition = lockRoot;
                    g_animationState.controller->SetLockRootPosition(lockRoot);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Keep root bones at bind pose position.\n"
                                      "Useful for scene animations where root\n"
                                      "motion positions multiple characters.");
                }
            }

            ImGui::Spacing();

            // Debug: color by bone index
            ImGui::Checkbox("Color by Bone Index", &vis.colorByBoneIndex);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Color vertices by bone index.\nUseful for debugging bone assignments.");
            }

            if (vis.colorByBoneIndex)
            {
                ImGui::SameLine();
                if (ImGui::Checkbox("Raw", &vis.showRawBoneIndex))
                {
                    // No rebuild needed, just changes shader mode
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Raw: FA0 palette index (before remapping)\n"
                                      "Unchecked: Skeleton bone (after remapping)");
                }
            }

            // Bone options
            if (ImGui::Checkbox("Bones", &vis.showBones))
            {
                options.showBones = vis.showBones;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Labels", &options.showBoneLabels);

            if (vis.showBones)
            {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::SliderFloat("##BoneSize", &vis.jointRadius, 5.0f, 100.0f, "Bone Size: %.0f"))
                {
                    options.boneRadius = vis.jointRadius;
                }
            }

            ImGui::Spacing();

            // Background color
            float bgColor[3] = { options.backgroundColor.x, options.backgroundColor.y, options.backgroundColor.z };
            if (ImGui::ColorEdit3("Background", bgColor, ImGuiColorEditFlags_NoInputs))
            {
                options.backgroundColor = { bgColor[0], bgColor[1], bgColor[2], 1.0f };
            }
        }

        // ========== CAMERA SECTION ==========
        if (ImGui::CollapsingHeader("Camera"))
        {
            if (ImGui::Button("Fit to Model", ImVec2(100, 0)))
            {
                state.camera->FitToBounds(state.boundsMin, state.boundsMax);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(60, 0)))
            {
                state.camera->Reset();
                state.camera->FitToBounds(state.boundsMin, state.boundsMax);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Dist: %.0f", state.camera->GetDistance());
        }

        // ========== BONES SECTION ==========
        if (!state.bones.empty() && ImGui::CollapsingHeader("Bones"))
        {
            // Header row
            if (options.selectedBoneIndex >= 0)
            {
                if (ImGui::SmallButton("Clear"))
                {
                    state.SelectBone(-1);
                }
                ImGui::SameLine();
            }
            ImGui::TextDisabled("%zu bones total", state.bones.size());

            // Bone list
            if (ImGui::BeginChild("##BoneList", ImVec2(-1, 120), true))
            {
                for (size_t i = 0; i < state.bones.size(); i++)
                {
                    const auto& bone = state.bones[i];
                    bool isSelected = (static_cast<int>(i) == options.selectedBoneIndex);

                    std::string label = std::format("{}  (parent: {})", i, bone.parentIndex);

                    if (isSelected)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        state.SelectBone(static_cast<int>(i));
                    }

                    if (isSelected)
                        ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Bone %d\nParent: %d\nPosition: (%.1f, %.1f, %.1f)\nVertices: %d",
                            bone.index, bone.parentIndex,
                            bone.position.x, bone.position.y, bone.position.z,
                            bone.vertexCount);
                    }
                }
            }
            ImGui::EndChild();

            // Selected bone info
            if (options.selectedBoneIndex >= 0 && options.selectedBoneIndex < static_cast<int>(state.bones.size()))
            {
                const auto& bone = state.bones[options.selectedBoneIndex];
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Selected: Bone %d", bone.index);
                ImGui::Text("Parent: %d | Vertices: %d", bone.parentIndex, bone.vertexCount);
            }
        }

        // ========== SUBMESHES SECTION ==========
        if (g_animationState.submeshCount > 0 && ImGui::CollapsingHeader("Submeshes"))
        {
            // Ensure submesh visibility vector is properly sized
            if (vis.submeshVisibility.size() != g_animationState.submeshCount)
            {
                vis.ResetSubmeshVisibility(g_animationState.submeshCount);
            }

            // Header row with show/hide all buttons
            if (ImGui::SmallButton("Show All"))
            {
                for (size_t i = 0; i < vis.submeshVisibility.size(); i++)
                {
                    vis.submeshVisibility[i] = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Hide All"))
            {
                for (size_t i = 0; i < vis.submeshVisibility.size(); i++)
                {
                    vis.submeshVisibility[i] = false;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu submeshes", g_animationState.submeshCount);

            // Submesh list with checkboxes
            if (ImGui::BeginChild("##SubmeshList", ImVec2(-1, 120), true))
            {
                for (size_t i = 0; i < g_animationState.submeshCount; i++)
                {
                    bool visible = vis.IsSubmeshVisible(i);
                    std::string label = std::format("Submesh {}##sub{}", i, i);

                    if (ImGui::Checkbox(label.c_str(), &visible))
                    {
                        if (i < vis.submeshVisibility.size())
                        {
                            vis.submeshVisibility[i] = visible;
                        }
                    }

                    // Show vertex count on hover if mesh data available
                    if (ImGui::IsItemHovered() && i < g_animationState.originalMeshes.size())
                    {
                        const auto& mesh = g_animationState.originalMeshes[i];
                        ImGui::SetTooltip("Submesh %zu\nVertices: %zu\nIndices: %zu",
                            i, mesh.vertices.size(), mesh.indices.size());
                    }
                }
            }
            ImGui::EndChild();
        }

        if (deferExternalSegmentResolve)
        {
            ResolveAndPlayExternalFA1Segment(
                deferredExternalSourceType,
                deferredExternalSegmentHash,
                dat_managers);
        }

        // ========== EXIT BUTTON ==========
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Exit Model Viewer", ImVec2(-1, 0)))
        {
            DeactivateModelViewer(mapRenderer);
            GuiGlobalConstants::is_model_viewer_panel_open = false;
        }
    }
    ImGui::End();
}
