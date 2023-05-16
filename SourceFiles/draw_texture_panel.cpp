#include "pch.h"
#include "draw_texture_panel.h"
#include "draw_dat_browser.h"
#include <commdlg.h>

extern SelectedDatTexture selected_dat_texture;

bool SaveTextureToPng(ID3D11ShaderResourceView* texture, std::wstring& filename,
                      TextureManager* texture_manager);
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
        ImGui::Begin("Texture Panel");

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
        if (ImGui::Button("Export Texture"))
        {
            std::wstring savePath = OpenPngFileDialog();
            if (! savePath.empty())
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

        ImGui::End();
    }
}

bool SaveTextureToPng(ID3D11ShaderResourceView* texture, std::wstring& filename,
                      TextureManager* texture_manager)
{
    HRESULT hr = texture_manager->SaveTextureToFile(texture, filename.c_str());
    if (FAILED(hr))
    {
        // Handle the error
        return false;
    }

    return true;
}

std::wstring OpenPngFileDialog()
{
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"png";

    if (GetSaveFileName(&ofn))
    {
        std::wstring wFileName(fileName);
        return wFileName;
    }

    return L"";
}
