#include "pch.h"
#include "draw_dat_load_progress_bar.h"
#include "GuiGlobalConstants.h"

void draw_dat_load_progress_bar(int num_files_read, int total_num_files)
{
    ImVec2 progress_bar_window_size =
      ImVec2(ImGui::GetIO().DisplaySize.x -
               (GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2) -
               (GuiGlobalConstants::right_panel_width + GuiGlobalConstants::panel_padding * 2),
             30);
    ImVec2 progress_bar_window_pos = ImVec2(
      GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2,
      ImGui::GetIO().DisplaySize.y - progress_bar_window_size.y - GuiGlobalConstants::panel_padding - 2);
    ImGui::SetNextWindowPos(progress_bar_window_pos);
    ImGui::SetNextWindowSize(progress_bar_window_size);
    ImGui::Begin("Progress Bar", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float progress = static_cast<float>(num_files_read) / static_cast<float>(total_num_files);
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x * progress, ImGui::GetContentRegionAvail().y);
    ImVec2 pos = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 clip_min = draw_list->GetClipRectMin();
    ImVec2 clip_max = draw_list->GetClipRectMax();
    draw_list->PushClipRect(clip_min, ImVec2(clip_min.x + size.x, clip_max.y), true);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImColor(255, 255, 255, 255));
    draw_list->PopClipRect();

    ImGui::SetCursorPosX(progress_bar_window_size.x / 2);
    const auto progress_str =
      std::format("{:.1f}%% ({}/{})", progress * 100, num_files_read, total_num_files);
    // Create a yellow color
    ImVec4 yellowColor = ImVec4(0.9f, 0.4f, 0.0f, 1.0f); // RGBA values: (1.0, 1.0, 0.0, 1.0)

    // Set the text color to yellow
    ImGui::PushStyleColor(ImGuiCol_Text, yellowColor);
    ImGui::Text(progress_str.c_str());
    ImGui::PopStyleColor();

    ImGui::End();
}
