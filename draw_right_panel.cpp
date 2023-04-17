#include "pch.h"
#include "draw_right_panel.h"
#include "MapRenderer.h"

extern FileType selected_file_type;

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
    float window_height = 0;
    if (ImGui::Begin("Render settings", NULL, window_flags))
    {

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

            int terrain_pixel_shader_type = static_cast<int>(map_renderer->GetTerrainPixelShaderType());
            if (ImGui::Combo("Terrain pixel shader", &terrain_pixel_shader_type,
                             "Default\0Terrain Default\0Terrain Checkered\0Terrain textured\0"))
            {
                map_renderer->SetTerrainPixelShaderType(
                  static_cast<PixelShaderType>(terrain_pixel_shader_type));
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

            if (! lock_aspect_ratio && ImGui::SliderFloat("Frustum height", &frustum_height, 1, 300000))
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

        max_window_height = ImGui::GetIO().DisplaySize.y - window_height -
          (3 *
           GuiGlobalConstants::panel_padding); // Calculate max height based on app window size and padding

        // Set up the props visibility settings window
        ImGui::SetNextWindowPos(
          ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::right_panel_width -
                   GuiGlobalConstants::panel_padding,
                 GuiGlobalConstants::panel_padding + window_height + GuiGlobalConstants::panel_padding));
        ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(GuiGlobalConstants::right_panel_width, max_window_height));
        if (ImGui::Begin("Props Visibility", NULL, window_flags))
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
        }
        ImGui::End();
    }

    ImGui::PopStyleVar();
}
