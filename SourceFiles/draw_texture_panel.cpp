#include "pch.h"
#include "draw_texture_panel.h"
#include "draw_dat_browser.h"
#include <commdlg.h>
#include <GuiGlobalConstants.h>

extern SelectedDatTexture selected_dat_texture;
extern bool using_other_model_format;
extern FFNA_ModelFile_Other selected_ffna_model_file_other;
extern FileType selected_file_type;

std::vector<InlineTextureDisplay> inline_texture_displays;
std::vector<ModelTextureDisplay> model_texture_displays;

std::wstring OpenPngFileDialog();

void draw_texture_panel(MapRenderer* map_renderer)
{
    if (!GuiGlobalConstants::is_texture_panel_open) return;

    ID3D11ShaderResourceView* texture =
        map_renderer->GetTextureManager()->GetTexture(selected_dat_texture.texture_id);

    const float min_width = 256;
    const float min_height = 100;
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_width, min_height), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("Texture Panel", &GuiGlobalConstants::is_texture_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        GuiGlobalConstants::ClampWindowToScreen();

        if (!texture) {
            ImGui::TextWrapped("No texture loaded.");
            ImGui::TextWrapped("Select a texture file (ATEX, ATTX, DDS) or load a map to view textures here.");
        }
        else {
            // Display texture information
            ImGui::Text("Selected Texture ID: %d", selected_dat_texture.texture_id);
            ImGui::Text("Resolution: %d x %d", selected_dat_texture.dat_texture.width,
                selected_dat_texture.dat_texture.height);

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

        // Display model textures (from texture_filenames_chunk) for "other" model format
        if (using_other_model_format && selected_file_type == FFNA_Type2 && !model_texture_displays.empty())
        {
            ImGui::Separator();
            ImGui::Text("Model Textures (%zu found):", model_texture_displays.size());
            ImGui::Separator();

            float thumbnail_size = 128.0f;
            float window_width = ImGui::GetWindowWidth();
            int columns = std::max(1, static_cast<int>((window_width - 20) / (thumbnail_size + 10)));

            int col = 0;
            for (size_t i = 0; i < model_texture_displays.size(); i++)
            {
                const auto& tex_display = model_texture_displays[i];
                if (tex_display.texture_id < 0) continue;

                ID3D11ShaderResourceView* tex =
                    map_renderer->GetTextureManager()->GetTexture(tex_display.texture_id);
                if (!tex) continue;

                if (col > 0)
                {
                    ImGui::SameLine();
                }

                ImGui::BeginGroup();

                float scale = thumbnail_size / std::max(tex_display.width, tex_display.height);
                ImVec2 scaled_size(tex_display.width * scale, tex_display.height * scale);

                ImGui::Image((ImTextureID)tex, scaled_size);
                ImGui::Text("#%d: 0x%X", tex_display.index, tex_display.file_hash);
                ImGui::Text("%dx%d", tex_display.width, tex_display.height);

                ImGui::EndGroup();

                col++;
                if (col >= columns)
                {
                    col = 0;
                }
            }
        }

        // Display inline ATEX textures (inventory icons) for "other" model format
        if (using_other_model_format && selected_file_type == FFNA_Type2 &&
            selected_ffna_model_file_other.has_inline_textures && !inline_texture_displays.empty())
        {
            ImGui::Separator();
            ImGui::Text("Inventory Icon (%zu found):", inline_texture_displays.size());
            ImGui::Separator();

            float thumbnail_size = 64.0f;  // Smaller size for inventory icons
            float window_width = ImGui::GetWindowWidth();
            int columns = std::max(1, static_cast<int>((window_width - 20) / (thumbnail_size + 10)));

            int col = 0;
            for (size_t i = 0; i < inline_texture_displays.size(); i++)
            {
                const auto& tex_display = inline_texture_displays[i];
                if (tex_display.texture_id < 0) continue;

                ID3D11ShaderResourceView* tex =
                    map_renderer->GetTextureManager()->GetTexture(tex_display.texture_id);
                if (!tex) continue;

                if (col > 0)
                {
                    ImGui::SameLine();
                }

                ImGui::BeginGroup();

                float scale = thumbnail_size / std::max(tex_display.width, tex_display.height);
                ImVec2 scaled_size(tex_display.width * scale, tex_display.height * scale);

                ImGui::Image((ImTextureID)tex, scaled_size);
                ImGui::Text("#%d: %s", tex_display.index, tex_display.format.c_str());
                ImGui::Text("%dx%d", tex_display.width, tex_display.height);

                ImGui::EndGroup();

                col++;
                if (col >= columns)
                {
                    col = 0;
                }
            }
        }
    }
    ImGui::End();
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