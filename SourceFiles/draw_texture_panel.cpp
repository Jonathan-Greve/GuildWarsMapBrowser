#include "pch.h"
#include "draw_texture_panel.h"
#include "draw_dat_browser.h"
#include <commdlg.h>
#include <GuiGlobalConstants.h>

extern SelectedDatTexture selected_dat_texture;

std::wstring OpenPngFileDialog();

void draw_texture_panel(MapRenderer* map_renderer)
{
    ID3D11ShaderResourceView* texture =
        map_renderer->GetTextureManager()->GetTexture(selected_dat_texture.texture_id);

    if (texture)
    {
        const float min_width = 256;
        const float min_height = 256 + 60;
        ImGui::SetNextWindowSizeConstraints(ImVec2(min_width, min_height), ImVec2(FLT_MAX, FLT_MAX));
        if (GuiGlobalConstants::is_texture_panel_open) {
            if (ImGui::Begin("Texture Panel", &GuiGlobalConstants::is_texture_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {

                // Display texture information
                ImGui::Text("Selected Texture ID: %d", selected_dat_texture.texture_id);
                ImGui::Text("Resolution: %d x %d", selected_dat_texture.dat_texture.width,
                    selected_dat_texture.dat_texture.height);

                // Add any other relevant information about the texture here

                // Calculate the scaling factor to fit the texture within the window
                ImVec2 window_size = ImGui::GetWindowSize();
                float scale_x = (window_size.x - 50) / selected_dat_texture.dat_texture.width;
                float scale_y = (window_size.y - 80) / selected_dat_texture.dat_texture.height;
                float scale = (scale_x < scale_y) ? scale_x : scale_y;

                // Display the texture with the scaling factor applied
                ImVec2 scaled_size(selected_dat_texture.dat_texture.width * scale,
                    selected_dat_texture.dat_texture.height * scale);
                ImGui::Image((ImTextureID)texture, scaled_size);

                // Export texture functionality
                if (ImGui::Button("Export Texture as PNG"))
                {
                    std::wstring savePath = OpenFileDialog(std::format(L"texture_{}", selected_dat_texture.file_id), L"png");
                    if (!savePath.empty())
                    {
                        if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
                        {
                            ImGui::Text("Texture exported to: %s", savePath.c_str());
                        }
                        else
                        {
                            ImGui::Text("Failed to export texture.");
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Export Texture as DDS (BC1)")) {
                    const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(selected_dat_texture.file_id);
                    if (texture_data.has_value()) {
                        std::wstring savePath = OpenFileDialog(std::format(L"texture_{}", selected_dat_texture.file_id), L"dds");

                        const auto compression_format = CompressionFormat::BC1;
                        TexPanelExportDDS(texture_data, savePath, compression_format);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Export Texture as DDS (BC3)")) {
                    const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(selected_dat_texture.file_id);
                    if (texture_data.has_value()) {
                        std::wstring savePath = OpenFileDialog(std::format(L"texture_{}", selected_dat_texture.file_id), L"dds");

                        const auto compression_format = CompressionFormat::BC3;
                        TexPanelExportDDS(texture_data, savePath, compression_format);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Export Texture as DDS (BC5)")) {
                    const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(selected_dat_texture.file_id);
                    if (texture_data.has_value()) {
                        std::wstring savePath = OpenFileDialog(std::format(L"texture_{}", selected_dat_texture.file_id), L"dds");

                        const auto compression_format = CompressionFormat::BC5;
                        TexPanelExportDDS(texture_data, savePath, compression_format);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Export Texture as DDS (No compression)")) {
                    const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(selected_dat_texture.file_id);
                    if (texture_data.has_value()) {
                        std::wstring savePath = OpenFileDialog(std::format(L"texture_{}", selected_dat_texture.file_id), L"dds");

                        const auto compression_format = CompressionFormat::None;
                        TexPanelExportDDS(texture_data, savePath, compression_format);
                    }
                }
            }

            ImGui::End();
        }
    }
}

void TexPanelExportDDS(const std::optional<TextureData>& texture_data, std::wstring& savePath, const CompressionFormat compression_format)
{
    if (SaveTextureToDDS(texture_data.value(), savePath, compression_format))
    {
        // Success
    }
    else
    {
        // Error handling }
    }
}