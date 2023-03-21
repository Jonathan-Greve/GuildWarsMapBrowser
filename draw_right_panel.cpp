#include "pch.h"
#include "draw_right_panel.h"
#include "MapRenderer.h"

void draw_right_panel(MapRenderer* map_renderer)
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    // Set up the right panel
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
                                     GuiGlobalConstants::panel_padding,
                                   GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));

    ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    ImGui::Begin("Render settings", NULL, window_flags);

    static float water_level = 0.0f;

    auto terrain = map_renderer->GetTerrain();
    if (terrain)
    {
        float min_level = terrain->m_bounds.map_min_y;
        float max_level = terrain->m_bounds.map_max_y;

        // Create the slider for changing the water level with text input enabled
        if (ImGui::SliderFloat("Water level", &water_level, min_level, max_level, "%.2f", 0))
        {
            water_level = ImClamp(water_level, min_level, max_level);
            map_renderer->UpdateTerrainWaterLevel(water_level);
        }

        int rasterizerState = static_cast<int>(map_renderer->GetCurrentRasterizerState());
        if (ImGui::Combo("Rasterizer State", &rasterizerState,
                         "Solid\0Solid_NoCull\0Wireframe\0Wireframe_NoCull\0"))
        {
            map_renderer->SwitchRasterizerState(static_cast<RasterizerStateType>(rasterizerState));
        }
    }

    float window_height = ImGui::GetWindowSize().y;
    ImGui::End();

    // Set up the second right panel
    ImGui::SetNextWindowPos(
      ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
               GuiGlobalConstants::panel_padding,
             GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
    ImGui::Begin("Second panel", NULL, window_flags);

    // Add your content for the second panel here

    ImGui::End();
    ImGui::PopStyleVar();
}
