#include "pch.h"
#include "ModelViewerPanel.h"
#include "ModelViewer.h"
#include "animation_state.h"
#include "GuiGlobalConstants.h"
#include "MapRenderer.h"
#include "DATManager.h"
#include "imgui.h"
#include <format>
#include <algorithm>

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

                // Sequence selector
                if (g_animationState.clip && !g_animationState.clip->sequences.empty())
                {
                    int currentSeq = ctrl.GetCurrentSequenceIndex();
                    const auto& sequences = g_animationState.clip->sequences;
                    std::string seqLabel = std::format("Seq {} / {}", currentSeq + 1, sequences.size());

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##Sequence", seqLabel.c_str()))
                    {
                        for (size_t i = 0; i < sequences.size(); i++)
                        {
                            bool isSelected = (static_cast<int>(i) == currentSeq);
                            std::string label = std::format("Sequence {}", i + 1);
                            if (ImGui::Selectable(label.c_str(), isSelected))
                            {
                                ctrl.SetSequence(static_cast<int>(i));
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
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

                            std::string label = std::format("0x{:X} - {} seq, {} bones##{}",
                                result.fileId, result.sequenceCount, result.boneCount, i);

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
                                ImGui::SetTooltip("File: 0x%X\nSequences: %u\nBones: %u\n\nDouble-click to load",
                                    result.fileId, result.sequenceCount, result.boneCount);
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
