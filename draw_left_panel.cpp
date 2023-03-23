#include "pch.h"
#include "draw_left_panel.h"
#include "FFNA_ModelFile.h"
#include "draw_props_info_panel.h"
#include "draw_props_filenames_panel.h"
#include "draw_chunk_20000003.h"

extern FileType selected_file_type;
extern FFNA_ModelFile selected_ffna_model_file;
extern FFNA_MapFile selected_ffna_map_file;

void draw_left_panel(MapRenderer* map_renderer)
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    // set up the left panel
    ImGui::SetNextWindowPos(ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width, 0));
    ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    ImGui::Begin("File info", NULL, window_flags);

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
        draw_props_info_panel(selected_ffna_map_file.props_info_chunk);
        draw_props_filenames_panel(selected_ffna_map_file.prop_filenames_chunk);
        //draw_chunk_20000003(selected_ffna_map_file.chunk5);
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

    ImGui::End();
    ImGui::PopStyleVar();
}
