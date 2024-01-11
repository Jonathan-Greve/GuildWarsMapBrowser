#include "pch.h"
#include "draw_extract_panel.h"
#include "GuiGlobalConstants.h"

void draw_extract_panel(ExtractPanelInfo& extract_panel_info)
{
    if (GuiGlobalConstants::is_extract_panel_open) {
        if (ImGui::Begin("Extract Panel", NULL, ImGuiWindowFlags_NoFocusOnAppearing)) {

            if (ImGui::CollapsingHeader("Extract map render to image file")) {

                // Input for pixels per tile in the x-direction
                ImGui::InputInt("Pixels per Tile X", &extract_panel_info.pixels_per_tile_x, 1, 5, ImGuiInputTextFlags_CharsDecimal);
                extract_panel_info.pixels_per_tile_x = (extract_panel_info.pixels_per_tile_x > 128) ? 128 : (extract_panel_info.pixels_per_tile_x < 1) ? 1 : extract_panel_info.pixels_per_tile_x; // Clamping value between 1 and 128

                // Input for pixels per tile in the y-direction
                ImGui::InputInt("Pixels per Tile Y", &extract_panel_info.pixels_per_tile_y, 1, 5, ImGuiInputTextFlags_CharsDecimal);
                extract_panel_info.pixels_per_tile_y = (extract_panel_info.pixels_per_tile_y > 128) ? 128 : (extract_panel_info.pixels_per_tile_y < 1) ? 1 : extract_panel_info.pixels_per_tile_y; // Clamping value between 1 and 128

                // Buttons for extraction
                if (ImGui::Button("Extract as DDS")) {
                    std::wstring saveDir = OpenDirectoryDialog();
                    if (!saveDir.empty())
                    {
                        extract_panel_info.save_directory = saveDir;
                    }
                    extract_panel_info.pixels_per_tile_changed = true;
                }

                ImGui::SameLine();

                if (ImGui::Button("Extract as PNG")) {
                    std::wstring saveDir = OpenDirectoryDialog();
                    if (!saveDir.empty())
                    {
                        extract_panel_info.save_directory = saveDir;
                    }
                    extract_panel_info.pixels_per_tile_changed = true;
                }
            }

            if (ImGui::CollapsingHeader("Extract decompressed files")) {
            }

        }
        ImGui::End();
    }
}

