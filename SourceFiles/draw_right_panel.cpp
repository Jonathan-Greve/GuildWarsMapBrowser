#include "pch.h"
#include "draw_right_panel.h"
#include "MapRenderer.h"
#include "FFNA_MapFile.h"

extern FileType selected_file_type;
extern FFNA_MapFile selected_ffna_map_file;

void draw_right_panel(MapRenderer* map_renderer, int& FPS_target, DX::StepTimer& timer, bool& msaa_changed, int& msaa_level_index, const std::vector<std::pair<int, int>>& msaa_levels)
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

    // Set up the right panel
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
        GuiGlobalConstants::panel_padding,
        GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    float window_height = 0;
    if (ImGui::Begin("Render settings", NULL, window_flags))
    {
        static float terrain_tex_pad_x = 0.03f;
        static float terrain_tex_pad_y = 0.03f;

        ImGui::Text(std::format("Current FPS: {}", timer.GetFramesPerSecond()).c_str());

        if (ImGui::InputInt("Max FPS", &FPS_target)) {
            if (FPS_target < 1) {
                FPS_target = 1;
            }
        }

        if (msaa_levels.size() > 0)
        {
            std::vector<std::string> msaaOptions;
            for (const auto& level : msaa_levels) {
                msaaOptions.push_back(std::to_string(level.first) + "x MSAA");
            }

            // Convert vector of strings to vector of char pointers
            std::vector<const char*> msaaCharPtrs;
            for (const auto& str : msaaOptions) {
                msaaCharPtrs.push_back(str.c_str());
            }

            // Create a combo box with MSAA options
            if (ImGui::Combo("MSAA Level", &msaa_level_index, msaaCharPtrs.data(), msaaCharPtrs.size()))
            {
                msaa_changed = true; // Set flag to indicate MSAA level change
            }
        }

        int lod_quality = static_cast<uint8_t>(map_renderer->GetLODQuality());
        if (ImGui::Combo("LOD Quality", &lod_quality,
            "High\0Medium\0Low\0"))
        {
            map_renderer->SetLODQuality(static_cast<LODQuality>(lod_quality));
        }

        auto terrain = map_renderer->GetTerrain();
        if (terrain)
        {
            float min_level = terrain->m_bounds.map_min_y;
            float max_level = terrain->m_bounds.map_max_y;

            float water_level = map_renderer->GetWaterLevel();

            // Create the slider for changing the water level with text input enabled
            if (ImGui::SliderFloat("Water level", &water_level, min_level, max_level, "%.2f", 0))
            {
                water_level = ImClamp(water_level, min_level, max_level);
                map_renderer->UpdateTerrainWaterLevel(water_level);
            }

            bool should_render_sky = map_renderer->GetShouldRenderSky();
            if (should_render_sky) {
                float sky_height = map_renderer->GetSkyHeight();
                if (ImGui::SliderFloat("Sky height", &sky_height, -40000.0f, 40000.0f, "%.2f", 0))
                {
                    water_level = std::clamp(sky_height, -40000.0f, 40000.0f);
                    map_renderer->SetSkyHeight(sky_height);
                }
            }

            if (ImGui::SliderFloat("Terrain tex pad x", &terrain_tex_pad_x, 0, 0.5, "%.2f", 0))
            {
                terrain_tex_pad_x = ImClamp(terrain_tex_pad_x, 0.0f, 0.5f);
                map_renderer->UpdateTerrainTexturePadding(terrain_tex_pad_x, terrain_tex_pad_y);
            }

            if (ImGui::SliderFloat("Terrain tex pad y", &terrain_tex_pad_y, 0, 0.5, "%.2f", 0))
            {
                terrain_tex_pad_y = ImClamp(terrain_tex_pad_y, 0.0f, 0.5f);
                map_renderer->UpdateTerrainTexturePadding(terrain_tex_pad_x, terrain_tex_pad_y);
            }

            // Terrain pixel shader selection
            int terrain_shader_idx = (map_renderer->GetTerrainPixelShaderType() == PixelShaderType::TerrainTileChecker) ? 1 : 0;
            if (ImGui::Combo("Terrain shader", &terrain_shader_idx, "Textured\0Tile Checker\0"))
            {
                PixelShaderType new_shader = (terrain_shader_idx == 1) ? PixelShaderType::TerrainTileChecker : PixelShaderType::TerrainRev;
                map_renderer->SetTerrainPixelShaderType(new_shader);
            }

            if (ImGui::Checkbox("Show sky", &should_render_sky)) {
                map_renderer->SetShouldRenderSky(should_render_sky);
            }

            bool should_render_fog = map_renderer->GetShouldRenderFog();
            if (ImGui::Checkbox("Show fog", &should_render_fog)) {
                map_renderer->SetShouldRenderFog(should_render_fog);
            }

            bool should_render_shadows = map_renderer->GetShouldRenderShadows();
            if (ImGui::Checkbox("Show shadows", &should_render_shadows)) {
                map_renderer->SetShouldRenderShadows(should_render_shadows);
            }

            bool should_render_shadows_on_props = map_renderer->GetShouldRenderShadowsForModels();
            if (ImGui::Checkbox("Show shadows on props", &should_render_shadows_on_props)) {
                map_renderer->SetShouldRenderShadowsForModels(should_render_shadows_on_props);
            }

            bool should_render_water_reflection = map_renderer->GetShouldRenderWaterReflection();
            if (ImGui::Checkbox("Show water reflection", &should_render_water_reflection)) {
                map_renderer->SetShouldRenderWaterReflection(should_render_water_reflection);
            }

            bool should_render_shore = map_renderer->GetShouldRenderShoreWaves();
            if (ImGui::Checkbox("Show shore waves", &should_render_shore)) {
                map_renderer->SetShouldRenderShoreWaves(should_render_shore);
            }

            bool should_render_pathfinding = map_renderer->GetShouldRenderPathfinding();
            if (ImGui::Checkbox("Show pathfinding", &should_render_pathfinding)) {
                map_renderer->SetShouldRenderPathfinding(should_render_pathfinding);
            }

            bool should_use_picking_shader_for_models = map_renderer->GetShouldUsePickingShaderForModels();
            if (ImGui::Checkbox("Show model picking colors", &should_use_picking_shader_for_models)) {
                map_renderer->SetShouldUsePickingShaderForModels(should_use_picking_shader_for_models);
            }
        }

        window_height = ImGui::GetWindowSize().y;
    }
    ImGui::End();

    float max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
        (3 * GuiGlobalConstants::panel_padding); // Calculate max height based on app window size and padding

    // Set up the second right panel
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
            GuiGlobalConstants::panel_padding,
            GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
        ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));

    if (ImGui::Begin("Lighting", NULL, window_flags))
    {
        DirectionalLight directional_light = map_renderer->GetDirectionalLight();

        if (ImGui::SliderFloat3("Light Direction", &directional_light.direction.x, -1.0f, 1.0f, "%.2f", 0))
        {
            map_renderer->SetDirectionalLight(directional_light);
        }

        if (ImGui::ColorEdit3("Ambient Color", &directional_light.ambient.x))
        {
            map_renderer->SetDirectionalLight(directional_light);
        }

        if (ImGui::ColorEdit3("Diffuse Color", &directional_light.diffuse.x))
        {
            map_renderer->SetDirectionalLight(directional_light);
        }

        if (ImGui::ColorEdit3("Specular Color", &directional_light.specular.x))
        {
            map_renderer->SetDirectionalLight(directional_light);
        }

        window_height += ImGui::GetWindowSize().y;
    }
    ImGui::End();

    max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
        (3 * GuiGlobalConstants::panel_padding); // Calculate max height based on app window size and padding

    // Set up the props visibility settings window
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
            GuiGlobalConstants::panel_padding,
            GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
        ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));

    if (ImGui::Begin("Camera and movement", NULL, window_flags))
    {
        bool camera_projection_settings_changed = false;

        Camera* camera = map_renderer->GetCamera();
        float position[3];
        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(position), camera->GetPosition());
        ImGui::SliderFloat3("##position", position, -100000, 100000);
        camera->SetPosition(position[0], position[1], position[2]);
        ImGui::SliderFloat("Walk speed", &camera->m_walk_speed, 0.0f, 1000.0f);
        ImGui::SliderFloat("Strafe speed", &camera->m_strafe_speed, 0.0f, 1000.0f);

        float yaw = camera->GetYaw() * 180 / XM_PI;
        float pitch = camera->GetPitch() * 180 / XM_PI;

        if (ImGui::SliderFloat("Pitch", &pitch, -90.0f, 90.0f))
        {
            camera_projection_settings_changed = true;
        }
        if (ImGui::SliderFloat("Yaw", &yaw, -179.999f, 180.0f))
        {
            camera_projection_settings_changed = true;
        }

        auto camera_type = camera->GetCameraType();
        float fovY = camera->GetFovY() * 180 / XM_PI;
        float aspect_ratio = camera->GetAspectRatio();
        float near_z = camera->GetNearZ();
        float far_z = camera->GetFarZ();
        float frustum_width = camera->GetViewWidth();
        float frustum_height = camera->GetViewHeight();

        if (camera_type == CameraType::Perspective)
        {
            if (ImGui::SliderFloat("Vertical FoV", &fovY, 1, 179))
            {
                camera_projection_settings_changed = true;
            }
        }

        if (camera_type == CameraType::Orthographic)
        {
            static bool lock_aspect_ratio = true;
            ImGui::Checkbox("Lock aspect ratio", &lock_aspect_ratio);

            if (ImGui::SliderFloat("Frustum width", &frustum_width, 1, 300000))
            {
                camera_projection_settings_changed = true;
                if (lock_aspect_ratio)
                {
                    frustum_height = frustum_width / aspect_ratio;
                }
            }

            if (!lock_aspect_ratio && ImGui::SliderFloat("Frustum height", &frustum_height, 1, 300000))
            {
                camera_projection_settings_changed = true;
            }
        }

        if (ImGui::SliderFloat("Near frustum z-plane", &near_z, 1, 200000))
        {
            camera_projection_settings_changed = true;
        }

        if (ImGui::SliderFloat("Far frustum z-plane", &far_z, near_z + 1, near_z + 200000))
        {
            camera_projection_settings_changed = true;
        }

        if (camera_projection_settings_changed)
        {
            if (camera_type == CameraType::Perspective)
            {
                camera->SetFrustumAsPerspective(fovY * XM_PI / 180, aspect_ratio, near_z, far_z);
            }
            else
            {
                camera->SetFrustumAsOrthographic(frustum_width, frustum_height, near_z, far_z);
            }
            camera->SetOrientation(pitch * XM_PI / 180, yaw * XM_PI / 180);
        }

        if (camera_type == CameraType::Perspective)
        {
            if (ImGui::Button("Change to orthographic", ImVec2(-FLT_MIN, 0.0f)))
            {
                auto pos = camera->GetPosition();
                camera->SetFrustumAsOrthographic(frustum_width, frustum_width / aspect_ratio, near_z, far_z);
                camera->SetOrientation(-90.0f * XM_PI / 180, 0 * XM_PI / 180);
            }
        }
        else
        {
            if (ImGui::Button("Change to perspective", ImVec2(-FLT_MIN, 0.0f)))
            {
                camera->SetFrustumAsPerspective(fovY * XM_PI / 180, aspect_ratio, near_z, far_z);
                camera->SetOrientation(pitch * XM_PI / 180, yaw * XM_PI / 180);
            }
        }

        window_height += ImGui::GetWindowSize().y;
    }
    ImGui::End();

    if (selected_file_type == FFNA_Type3)
    {
        static bool is_props_visibility_window_open = false;
        static bool is_shore_visibility_window_open = false;
        static bool is_pathfinding_visibility_window_open = false;

        float num_visibility_windows_open = 0;
        if (is_props_visibility_window_open) {
            num_visibility_windows_open++;
        }

        if (is_shore_visibility_window_open) {
            num_visibility_windows_open++;
        }

        if (is_pathfinding_visibility_window_open) {
            num_visibility_windows_open++;
        }

        float max_height_factor = 1;
        if (num_visibility_windows_open > 0) {
            max_height_factor /= num_visibility_windows_open;
        }

        max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
            (3 *
                GuiGlobalConstants::panel_padding) * max_height_factor; // Calculate max height based on app window size and padding

        // Set up the props visibility settings window
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
                GuiGlobalConstants::panel_padding,
                GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
        ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
            ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));
        if (ImGui::Begin("Props Visibility", &is_props_visibility_window_open, window_flags))
        {
            auto& propsMeshIds = map_renderer->GetPropsMeshIds();

            // Set all and Clear all buttons
            if (ImGui::Button("Set all"))
            {
                for (const auto& [prop_id, mesh_ids] : propsMeshIds)
                {
                    for (int mesh_id : mesh_ids)
                    {
                        map_renderer->SetMeshShouldRender(mesh_id, true);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear all"))
            {
                for (const auto& [prop_id, mesh_ids] : propsMeshIds)
                {
                    for (int mesh_id : mesh_ids)
                    {
                        map_renderer->SetMeshShouldRender(mesh_id, false);
                    }
                }
            }

            // Props visibility checkboxes
            for (const auto& [prop_id, mesh_ids] : propsMeshIds)
            {
                std::string label = std::format("Prop index: {}", prop_id);
                if (ImGui::TreeNode(label.c_str()))
                {
                    // Individual Set and Clear buttons for each prop
                    std::string set_label = std::format("Set##{}", prop_id);
                    std::string clear_label = std::format("Clear##{}", prop_id);

                    if (ImGui::Button(set_label.c_str()))
                    {
                        for (int mesh_id : mesh_ids)
                        {
                            map_renderer->SetMeshShouldRender(mesh_id, true);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(clear_label.c_str()))
                    {
                        for (int mesh_id : mesh_ids)
                        {
                            map_renderer->SetMeshShouldRender(mesh_id, false);
                        }
                    }

                    for (uint32_t i = 0; i < mesh_ids.size(); i++)
                    {
                        auto mesh_id = mesh_ids[i];

                        bool should_render = map_renderer->GetMeshShouldRender(mesh_id);

                        auto sub_label = std::format("Mesh id: {}", mesh_id);
                        if (ImGui::Checkbox(sub_label.c_str(), &should_render))
                        {
                            map_renderer->SetMeshShouldRender(mesh_id, should_render);
                        }
                    }

                    ImGui::TreePop();
                }
            }
            window_height += ImGui::GetWindowSize().y;
        }
        ImGui::End();

        max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
            (3 * GuiGlobalConstants::panel_padding) * max_height_factor;


        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
            GuiGlobalConstants::panel_padding,
            GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
        ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));
        if (ImGui::Begin("Shore Visibility", NULL, window_flags))
        {
            auto& shoreMeshIds = map_renderer->GetShoreMeshIds();

            // Set all and Clear all buttons
            if (ImGui::Button("Set all"))
            {
                for (int mesh_id : shoreMeshIds)
                {
                    map_renderer->SetShoreMeshIdShouldRender(mesh_id, true);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear all"))
            {
                for (int mesh_id : shoreMeshIds)
                {
                    map_renderer->SetShoreMeshIdShouldRender(mesh_id, false);
                }
            }

            // Shore visibility checkboxes
            for (uint32_t i = 0; i < shoreMeshIds.size(); i++)
            {
                auto mesh_id = shoreMeshIds[i];

                bool should_render = map_renderer->GetShoreMeshIdShouldRender(mesh_id);

                auto label = std::format("Mesh id: {}", mesh_id);
                if (ImGui::Checkbox(label.c_str(), &should_render))
                {
                    map_renderer->SetShoreMeshIdShouldRender(mesh_id, should_render);
                }
            }
            window_height += ImGui::GetWindowSize().y;
        }
        ImGui::End();

        // Pathfinding Visibility window
        max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
            (3 * GuiGlobalConstants::panel_padding) * max_height_factor;

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
            GuiGlobalConstants::panel_padding,
            GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
        ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));
        if (ImGui::Begin("Pathfinding Visibility", &is_pathfinding_visibility_window_open, window_flags))
        {
            auto& pathfindingMeshIds = map_renderer->GetPathfindingMeshIds();

            // Global toggle for showing pathfinding
            bool should_render_pathfinding = map_renderer->GetShouldRenderPathfinding();
            if (ImGui::Checkbox("Show Pathfinding", &should_render_pathfinding))
            {
                map_renderer->SetShouldRenderPathfinding(should_render_pathfinding);
            }

            // Height offset slider
            static float height_offset = 50.0f;
            if (ImGui::SliderFloat("Height Offset", &height_offset, -500.0f, 2000.0f, "%.0f"))
            {
                if (selected_ffna_map_file.pathfinding_chunk.valid && map_renderer->GetTerrain())
                {
                    map_renderer->UpdatePathfindingMeshHeights(
                        height_offset,
                        map_renderer->GetTerrain(),
                        selected_ffna_map_file.pathfinding_chunk.all_trapezoids);
                }
            }

            auto& planeMeshIds = map_renderer->GetPathfindingPlaneMeshIds();

            if (pathfindingMeshIds.size() > 0)
            {
                ImGui::Text("Planes: %zu, Trapezoids: %zu", planeMeshIds.size(), pathfindingMeshIds.size());

                // Set all and Clear all buttons for all planes
                if (ImGui::Button("Set all"))
                {
                    for (int mesh_id : pathfindingMeshIds)
                    {
                        map_renderer->SetPathfindingMeshIdShouldRender(mesh_id, true);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear all"))
                {
                    for (int mesh_id : pathfindingMeshIds)
                    {
                        map_renderer->SetPathfindingMeshIdShouldRender(mesh_id, false);
                    }
                }

                // Planes with individual trapezoids
                for (size_t plane_idx = 0; plane_idx < planeMeshIds.size(); plane_idx++)
                {
                    const auto& plane_mesh_ids = planeMeshIds[plane_idx];
                    auto plane_label = std::format("Plane {} ({} traps)", plane_idx, plane_mesh_ids.size());

                    if (ImGui::TreeNode(plane_label.c_str()))
                    {
                        // Set/Clear buttons for this plane
                        std::string set_label = std::format("Set##plane{}", plane_idx);
                        std::string clear_label = std::format("Clear##plane{}", plane_idx);

                        if (ImGui::Button(set_label.c_str()))
                        {
                            for (int mesh_id : plane_mesh_ids)
                            {
                                map_renderer->SetPathfindingMeshIdShouldRender(mesh_id, true);
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(clear_label.c_str()))
                        {
                            for (int mesh_id : plane_mesh_ids)
                            {
                                map_renderer->SetPathfindingMeshIdShouldRender(mesh_id, false);
                            }
                        }

                        // Individual trapezoid checkboxes
                        for (size_t trap_idx = 0; trap_idx < plane_mesh_ids.size(); trap_idx++)
                        {
                            int mesh_id = plane_mesh_ids[trap_idx];
                            bool should_render = map_renderer->GetPathfindingMeshIdShouldRender(mesh_id);

                            auto label = std::format("Trap {}##plane{}trap{}", trap_idx, plane_idx, trap_idx);
                            if (ImGui::Checkbox(label.c_str(), &should_render))
                            {
                                map_renderer->SetPathfindingMeshIdShouldRender(mesh_id, should_render);
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            }
            else
            {
                ImGui::Text("No pathfinding data");
            }
        }
        ImGui::End();
    }

    ImGui::PopStyleVar();
}
