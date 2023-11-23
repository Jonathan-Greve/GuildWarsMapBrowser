#include "pch.h"
#include "draw_dat_compare_panel.h"
#include <filesystem>

std::vector<std::wstring> file_paths;
std::map<int, DATManager*> dat_managers; // Map to hold pointers to DATManagers with integer aliases
std::map<std::wstring, int> filepath_to_alias; // Map to track filepath and its alias


void draw_dat_compare_panel(DATManager& existing_dat_manager)
{
    static const std::wstring& existing_dat_filepath = existing_dat_manager.get_filepath();
    static bool isExistingFilePathAdded = false;

    if (!isExistingFilePathAdded) {
        file_paths.push_back(existing_dat_filepath);
        filepath_to_alias[existing_dat_filepath] = 0; // Alias 0 for existing DAT file
        dat_managers[0] = &existing_dat_manager; // Use the reference for the existing dat_manager
        isExistingFilePathAdded = true;
    }

    ImGui::Begin("Compare DAT files");

    // File selection button
    if (ImGui::Button("Select File")) {
        std::ifstream inFile("dat_browser_last_filepath.txt");
        std::string initial_filepath;

        if (inFile) {
            std::getline(inFile, initial_filepath);
            inFile.close();
        }

        if (!std::filesystem::exists(initial_filepath) || !std::filesystem::is_directory(initial_filepath)) {
            initial_filepath = ".";
        }

        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".dat", initial_filepath + "\\.");
    }

    // Display File Dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

            std::ofstream outFile("dat_browser_last_filepath.txt", std::ios::trunc);
            outFile << filePath;
            outFile.close();

            std::wstring wstr(filePathName.begin(), filePathName.end());

            if (std::find(file_paths.begin(), file_paths.end(), wstr) == file_paths.end()) {
                file_paths.push_back(wstr);
                filepath_to_alias[wstr] = filepath_to_alias.size();
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // List selected files
    for (size_t i = 0; i < file_paths.size(); ++i) {
        int alias = filepath_to_alias[file_paths[i]];
        ImGui::Text("DAT %d: %ls", alias, file_paths[i].c_str());

        std::string btn_label = "Remove ##" + std::to_string(i);
        if (file_paths[i] != existing_dat_filepath) {
            ImGui::SameLine();
            if (ImGui::Button(btn_label.c_str())) {

                int alias_to_remove = filepath_to_alias[file_paths[i]];

                // Remove DATManager if it exists
                if (dat_managers.find(alias_to_remove) != dat_managers.end()) {
                    dat_managers.erase(alias_to_remove);
                }

                // Erase file from vectors and map
                filepath_to_alias.erase(file_paths[i]);
                file_paths.erase(file_paths.begin() + i);

                // Reorder the aliases in the map
                for (auto& pair : filepath_to_alias) {
                    if (pair.second > alias_to_remove) {
                        pair.second -= 1;
                    }
                }

                // Update dat_managers map to reflect new aliases
                std::map<int, DATManager*> updated_dat_managers;
                for (const auto& pair : dat_managers) {
                    if (pair.first > alias_to_remove) {
                        updated_dat_managers[pair.first - 1] = pair.second;
                    }
                    else if (pair.first < alias_to_remove) {
                        updated_dat_managers[pair.first] = pair.second;
                    }
                }
                dat_managers = std::move(updated_dat_managers);

                --i; // Adjust loop counter as the list size has changed
            }
        }
    }

    if (ImGui::Button("Start Analyzing")) {
        for (const auto& filepath : file_paths) {
            if (filepath != existing_dat_filepath && dat_managers.find(filepath_to_alias[filepath]) == dat_managers.end()) {
                DATManager* new_dat_manager = new DATManager();
                if (new_dat_manager->Init(filepath)) {
                    dat_managers[filepath_to_alias[filepath]] = new_dat_manager;
                }
                else {
                    delete new_dat_manager;
                }
            }
        }
    }

    ImGui::End();
}