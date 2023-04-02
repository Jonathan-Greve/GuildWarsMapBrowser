#include "pch.h"
#include "draw_texture_panel.h"
#include "draw_dat_browser.h"
#include <commdlg.h>

extern SelectedDatTexture selected_dat_texture;

bool SaveTextureToBMP(const char* filename, const DatTexture& texture);
std::string OpenFileDialog();

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
            std::string savePath = OpenFileDialog();
            if (! savePath.empty())
            {
                if (SaveTextureToBMP(savePath.c_str(), selected_dat_texture.dat_texture))
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

bool SaveTextureToBMP(const char* filename, const DatTexture& texture)
{
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    int width = texture.width;
    int height = texture.height;

    fileHeader.bfType = 0x4D42; // 'BM'
    fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (width * height * 4);
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = -height; // Negative value for top-down bitmap
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = 0;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    FILE* file;
    errno_t err = fopen_s(&file, filename, "wb");
    if (err != 0)
        return false;

    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, file);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(texture.rgba_data.data(), sizeof(RGBA), width * height, file);

    fclose(file);
    return true;
}

std::string OpenFileDialog()
{
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"bmp";

    if (GetSaveFileName(&ofn))
    {
        // Convert the wide string to a narrow string
        std::wstring wFileName(fileName);
        std::string sFileName(wFileName.begin(), wFileName.end());
        return sFileName;
    }

    return "";
}
