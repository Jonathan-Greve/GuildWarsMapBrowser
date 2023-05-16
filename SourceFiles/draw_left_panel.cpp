#include "pch.h"
#include "draw_left_panel.h"
#include "FFNA_ModelFile.h"
#include "draw_props_info_panel.h"
#include "draw_props_filenames_panel.h"
#include "draw_chunk_20000003.h"
#include "draw_terrain_chunk.h"
#include "draw_map_info_chunk.h"
#include "draw_chunk_20000000.h"
#include "draw_prop_model_info.h"
#include "draw_dat_browser.h"

extern FileType selected_file_type;
extern FFNA_ModelFile selected_ffna_model_file;
extern FFNA_MapFile selected_ffna_map_file;
extern std::vector<FileData> selected_map_files;

void draw_left_panel(MapRenderer* map_renderer)
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    // set up the left panel
    ImGui::SetNextWindowPos(ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width, 0));
    ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    float window_height = 0;
    if (ImGui::Begin("File info", NULL, window_flags))
    {

        switch (selected_file_type)
        {
        case NONE:
            break;
        case AMAT:
            break;
        case AMP:
            break;
        case ATEXDXT1:
            break;
        case ATEXDXT2:
            break;
        case ATEXDXT3:
            break;
        case ATEXDXT4:
            break;
        case ATEXDXT5:
            break;
        case ATEXDXTN:
            break;
        case ATEXDXTA:
            break;
        case ATEXDXTL:
            break;
        case ATTXDXT1:
            break;
        case ATTXDXT3:
            break;
        case ATTXDXT5:
            break;
        case ATTXDXTN:
            break;
        case ATTXDXTA:
            break;
        case ATTXDXTL:
            break;
        case DDS:
            break;
        case FFNA_Type2:
            break;
        case FFNA_Type3:
            draw_chunk_20000000(selected_ffna_map_file.chunk1);
            draw_map_info_chunk(selected_ffna_map_file.map_info_chunk);
            draw_props_info_panel(selected_ffna_map_file.props_info_chunk);
            draw_props_filenames_panel(selected_ffna_map_file.prop_filenames_chunk);
            //draw_chunk_20000003(selected_ffna_map_file.chunk5);
            draw_terrain_chunk(selected_ffna_map_file.terrain_chunk);
            break;
        case FFNA_Unknown:
            break;
        case MFTBASE:
            break;
        case NOTREAD:
            break;
        case SOUND:
            break;
        case TEXT:
            break;
        case UNKNOWN:
            break;
        default:
            break;
        }
        window_height = ImGui::GetWindowSize().y;
    }
    ImGui::End();

    float max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
      (3 * GuiGlobalConstants::panel_padding); // Calculate max height based on app window size and padding

    switch (selected_file_type)
    {
    case NONE:
        break;
    case AMAT:
        break;
    case AMP:
        break;
    case ATEXDXT1:
        break;
    case ATEXDXT2:
        break;
    case ATEXDXT3:
        break;
    case ATEXDXT4:
        break;
    case ATEXDXT5:
        break;
    case ATEXDXTN:
        break;
    case ATEXDXTA:
        break;
    case ATEXDXTL:
        break;
    case ATTXDXT1:
        break;
    case ATTXDXT3:
        break;
    case ATTXDXT5:
        break;
    case ATTXDXTN:
        break;
    case ATTXDXTA:
        break;
    case ATTXDXTL:
        break;
    case DDS:
        break;
    case FFNA_Type2:
        break;
    case FFNA_Type3:
        ImGui::SetNextWindowPos(
          ImVec2(GuiGlobalConstants::panel_padding,
                 GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
        ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(GuiGlobalConstants::left_panel_width, max_window_height));
        if (selected_map_files.size() > 0)
        {
            if (ImGui::Begin("Prop models", NULL, window_flags))
            {

                int i = 0;
                int k = 0;
                for (const auto& file_data : selected_map_files)
                {
                    i++;
                    if (auto ffna_model_file_ptr = std::get_if<FFNA_ModelFile>(&file_data))
                    {
                        k++;
                        // Use *ffna_model_file_ptr to access the FFNA_ModelFile data
                        auto id = std::format("Filename index:{}", i);
                        if (ImGui::TreeNode(id.c_str()))
                        {
                            draw_prop_model_info(*ffna_model_file_ptr);
                            ImGui::TreePop();
                        }
                    }
                }
            }
            ImGui::End();
        }
        break;
    case FFNA_Unknown:
        break;
    case MFTBASE:
        break;
    case NOTREAD:
        break;
    case SOUND:
        break;
    case TEXT:
        break;
    case UNKNOWN:
        break;
    default:
        break;
    }

    ImGui::PopStyleVar();
}
