#include "pch.h"
#include "draw_gui_for_open_dat_file.h"

void draw_gui_for_open_dat_file()
{
    constexpr auto window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;
    constexpr auto window_name = "Select your Gw.dat file";
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin(window_name, nullptr, window_flags);

    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    ImVec2 window_pos = (screen_size - window_size) * 0.5f;

    ImGui::SetWindowPos(window_pos);

    // Get the size of the button
    ImVec2 button_size = ImVec2(200, 40);

    // Calculate the position of the button
    float x = (window_size.x - button_size.x) / 2.0f;
    float y = (window_size.y - button_size.y) / 2.0f;

    // Set the position of the button
    ImGui::SetCursorPos(ImVec2(x, y));

    // Set the background color for a button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Select a \"Gw.dat\" File", button_size))
    {
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".dat", ".");
    }
    // Restore the original style
    ImGui::PopStyleColor(3);

    // Display File Dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse,
                                             ImVec2(500, 400)))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

            std::wstring wstr(filePathName.begin(), filePathName.end());
            gw_dat_path = wstr;
            gw_dat_path_set = true;
        }

        // close
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::End();
}
