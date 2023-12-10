#include "pch.h"
#include "draw_file_info_editor_panel.h"
#include "rapidcsv.h"
#include <commdlg.h> 
#include <filesystem>
#include <ranges>

extern FileType selected_file_type;
extern uint32_t selected_item_hash;
uint32_t prev_selected_item_hash = -1;

const std::string last_csv_filename {"custom_file_info_last_filepath.txt"};

std::filesystem::path csv_filepath;
std::filesystem::path last_csv_filepath;

bool verify_temp_file(const std::string& temp_filepath,
    const std::vector<std::vector<std::string>>& expected_data,
    const std::string& updated_item_hash) {
    std::ifstream temp_file(temp_filepath);
    std::string line;
    size_t rowIdx = 0;

    while (std::getline(temp_file, line)) {
        std::stringstream linestream(line);
        std::string cell;
        std::vector<std::string> row;

        while (std::getline(linestream, cell, ',')) {
            row.push_back(cell);
        }

        if (row.size() != expected_data[rowIdx].size()) {
            return false; // Mismatch in number of columns
        }

        for (size_t colIdx = 0; colIdx < row.size(); ++colIdx) {
            // Skip comparison for the updated row
            if (expected_data[rowIdx][0] == updated_item_hash) {
                continue;
            }

            if (row[colIdx] != expected_data[rowIdx][colIdx]) {
                return false; // Mismatch in data
            }
        }

        rowIdx++;
        if (rowIdx >= expected_data.size()) {
            break; // All rows checked
        }
    }

    return rowIdx == expected_data.size(); // Ensure all rows were checked
}

void save_csv(const std::string& filepath,
    const std::vector<std::vector<std::string>>& data,
    const std::string& updated_item_hash) {
    std::string temp_filepath = filepath + ".tmp";
    std::string backup_filepath = filepath + ".bak";

    // Create a backup of the original file
    std::filesystem::copy_file(filepath, backup_filepath, std::filesystem::copy_options::overwrite_existing);

    // Write to temporary file
    std::ofstream temp_file(temp_filepath);
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            temp_file << row[i];
            if (i < row.size() - 1) temp_file << ",";
        }
        temp_file << "\n";
    }
    temp_file.close();

    // Verify the integrity of the temporary file
    if (!verify_temp_file(temp_filepath, data, updated_item_hash)) {
        std::filesystem::remove(temp_filepath);
        return;
    }

    // Replace the original file with the temporary file
    std::filesystem::rename(temp_filepath, filepath);

    // Remove the backup file after successful save
    std::filesystem::remove(backup_filepath);
}

bool is_type_texture(FileType type) {
    switch (type)
    {
    case ATEXDXT1:
    case ATEXDXT2:
    case ATEXDXT3:
    case ATEXDXT4:
    case ATEXDXT5:
    case ATEXDXTN:
    case ATEXDXTA:
    case ATEXDXTL:
    case ATTXDXT1:
    case ATTXDXT3:
    case ATTXDXT5:
    case ATTXDXTN:
    case ATTXDXTA:
    case ATTXDXTL:
    case DDS:
        return true;
    default:
        break;
    }

    return false;
}

std::wstring open_file_dialog() {
    OPENFILENAME ofn;
    WCHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL; // Replace with your window handle
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        return ofn.lpstrFile;
    }

    return L"";
}

std::vector<std::vector<std::string>> load_csv(const std::string& filepath) {
    rapidcsv::Document doc(filepath, rapidcsv::LabelParams(-1, -1));
    std::vector<std::vector<std::string>> data;
    size_t rowCount = doc.GetRowCount();
    size_t colCount = doc.GetColumnCount();

    for (size_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        std::vector<std::string> row;
        for (size_t colIdx = 0; colIdx < colCount; ++colIdx) {
            row.push_back(doc.GetCell<std::string>(colIdx, rowIdx));
        }
        data.push_back(row);
    }
    return data;
}

void draw_file_info_editor_panel(std::vector<std::vector<std::string>>& csv_data) {
    static std::set<ModelTypes> selected_model_types;

    std::string selected_item_hash_hex = std::format("0x{:08x}", selected_item_hash);

    ImGui::Begin("Custom File Info");

    // Load csv file if not already loaded
    if (csv_filepath.empty()) {
        const auto filepath = load_last_filepath(last_csv_filename);
        if (filepath.has_value()) {
            csv_filepath = filepath.value();
        }
    }

    // Load csv file if not already loaded
    if (csv_data.empty()) {
        if (csv_filepath.empty()) {
            if (ImGui::Button("Open CSV File")) {
                csv_filepath = open_file_dialog();
                const auto filepath = save_last_filepath(csv_filepath, last_csv_filename);
            }
        }

        if (!csv_filepath.empty()) {
            csv_data = load_csv(csv_filepath.string());
        }
    }

    // Button to change the CSV file
    if (!csv_filepath.empty()) {
        if (ImGui::Button("Change CSV File")) {
            csv_filepath = open_file_dialog();
            if (!csv_filepath.empty()) {
                csv_data = load_csv(csv_filepath.string());
                const auto filepath = save_last_filepath(csv_filepath, last_csv_filename);
            }
        }
        ImGui::Separator();
    }

    if (selected_item_hash == -1) {
    }
    else {
        static int found_row_index = -1;
        static bool edit_mode = false;

        bool selected_item_hash_changed = prev_selected_item_hash != selected_item_hash;
        if (selected_item_hash_changed) {
            selected_model_types.clear();

            edit_mode = false;
            found_row_index = -1;
            for (int i = 1; i < csv_data.size(); i++) {
                const auto& row = csv_data[i];

                uint32_t row_hash_int = -1; // max value (no such file_id in the dat, use as non-existance)

                // check if the file_id in the csv start with 0x or 0X
                if (row[0][0] == '0' && std::tolower(row[0][1]) == 'x') {
                    row_hash_int = std::stoi(row[0], 0, 16);
                }
                else {
                    row_hash_int = std::stoi(row[0]);
                }

                if (row_hash_int == selected_item_hash) {
                    found_row_index = i;
                    break;
                }
            }
        }

        // If no matching row is found, create a new one but we don't add it to the csv until the user press 'save'
        static std::vector<std::string> row_backup;
        std::vector<std::string> row(9);
        if (found_row_index == -1 && selected_item_hash != -1) {
            row[0] = selected_item_hash_hex;
            row[8] = std::to_string(static_cast<int>(selected_file_type));
            row_backup = row;
        }

        static std::string name_buf;
        static std::string gwwiki_buf;
        static std::string map_id_buf;
        static bool is_explorable = false;
        static bool is_outpost = false;
        static bool is_pvp = false;
        static std::string model_type;

        if (found_row_index >= 0 && found_row_index < csv_data.size() && !edit_mode) {
            row = csv_data[found_row_index];
            name_buf = row[1];
            gwwiki_buf = row[2];
            map_id_buf = row[3];
            is_explorable = row[4] == "yes";
            is_outpost = row[5] == "yes";
            is_pvp = row[6] == "yes";
            model_type = row[7];

            // Clear the current selection set
            selected_model_types.clear();

            // Split the model_type string by semicolons and convert each part to the enum value
            for (auto token : std::views::split(model_type, ';')) {
                std::string_view token_sv(&*token.begin(), std::ranges::distance(token));
                ModelTypes type = StringToModelType(std::string(token_sv)); // Convert token to string and then to ModelTypes enum
                if (type != ModelTypes::unknown) { // Check for a valid type
                    selected_model_types.insert(type);
                }
            }
        }

        // Display the row's data
        ImGui::Text("File ID: %s", row[0].c_str());


        if (edit_mode) {
            ImGui::InputText("Name", &name_buf);
            ImGui::InputText("URL", &gwwiki_buf);

            if (selected_file_type == FFNA_Type2) {
                static ModelTypes current_selection = ModelTypes::unknown;

                // Dropdown for selecting model types
                if (ImGui::BeginCombo("##ModelTypes", "Select Model Type")) {
                    // Loop through all model types
                    for (int i = 0; i <= static_cast<int>(ModelTypes::unknown); ++i) {
                        ModelTypes type = static_cast<ModelTypes>(i);
                        if (!selected_model_types.contains(type)) {
                            if (ImGui::Selectable(ModelTypeToString(type).c_str(), current_selection == type)) {
                                if (type != ModelTypes::unknown) {
                                    selected_model_types.insert(type);
                                }
                                current_selection = ModelTypes::unknown;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                // Display selected model types and allow for their removal
                for (auto it = selected_model_types.begin(); it != selected_model_types.end();) {
                    std::string label = ModelTypeToString(*it) + "##Selected";
                    if (ImGui::Button(("- " + label).c_str())) {
                        it = selected_model_types.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                // Convert selected model types to a semicolon-separated string
                model_type.clear();
                for (const auto& type : selected_model_types) {
                    if (!model_type.empty()) model_type += ";";
                    model_type += ModelTypeToString(type);
                }
            }

            if (ImGui::Button("Save")) {
                edit_mode = false;
                if (found_row_index == -1) {
                    csv_data.push_back(row);
                    found_row_index = csv_data.size() - 1;
                }

                // column 0 (file_id) and column 8 (file type) already set automatically

                csv_data[found_row_index][1] = name_buf;
                csv_data[found_row_index][2] = gwwiki_buf;

                if (selected_file_type == FFNA_Type2) {
                    csv_data[found_row_index][7] = model_type;
                }
                else if (selected_file_type == FFNA_Type3) {
                    csv_data[found_row_index][3] = map_id_buf;
                    csv_data[found_row_index][4] = is_explorable ? "yes" : "no";
                    csv_data[found_row_index][5] = is_outpost ? "yes" : "no";
                    csv_data[found_row_index][6] = is_pvp ? "yes" : "no";
                }

                save_csv(csv_filepath.string(), csv_data, selected_item_hash_hex);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                edit_mode = false;
                row = row_backup;
            }
        }
        else {
            if (row[1] != "") {
                ImGui::Text("Name: \"%s\"", row[1].c_str());
            }
            else {
                ImGui::Text("Name: N/A");
            }

            if (row[2] != "") {
                ImGui::Text("GW Wiki URL: \"%s\"", row[2].c_str());
            }
            else {
                ImGui::Text("GW Wiki URL: N/A");
            }

            if (selected_file_type == FFNA_Type2) {
                row[7].empty() ? ImGui::Text("Model Type: N/A") : ImGui::Text("Model Type: %s", row[7].c_str());
            }

            else if (selected_file_type == FFNA_Type3) {
                const auto& map_ids_string = row[3];
                ImGui::Text("Map ids: ");

                std::stringstream ss(map_ids_string);
                std::string map_id;
                while (std::getline(ss, map_id, ';')) {
                    if (!map_id.empty()) {
                        ImGui::SameLine();
                        ImGui::Text("%s, ", map_id.c_str());
                    }
                }

                bool is_explorable = row[4] == "yes";
                bool is_outpost = row[5] == "yes";
                bool is_pvp = row[6] == "yes";

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::Checkbox("Is explorable", &is_explorable);
                ImGui::Checkbox("Is outpost", &is_outpost);
                ImGui::Checkbox("Is PvP", &is_pvp);
                ImGui::PopItemFlag();
            }
        }

        if (!edit_mode && ImGui::Button("Edit")) {
            edit_mode = true;

            if (found_row_index != -1) {
                row_backup = csv_data[found_row_index];
            }
        }

        prev_selected_item_hash = selected_item_hash;
    }

    ImGui::End();
}
