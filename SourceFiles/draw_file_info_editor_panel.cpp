#include "pch.h"
#include "draw_file_info_editor_panel.h"
#include "rapidcsv.h"
#include <commdlg.h> 
#include <filesystem>
#include <ranges>

extern FileType selected_file_type;
extern uint32_t selected_item_hash;
extern uint32_t selected_item_murmurhash3;

uint32_t prev_selected_item_hash = -1;

const std::string last_csv_filename{ "custom_file_info_last_filepath.txt" };

std::filesystem::path csv_filepath;
std::filesystem::path last_csv_filepath;

const char multival_separator = '|'; // Used for multivalue columns like names, map ids and model types

std::vector<std::string> split(const std::string& str, const char delimiter) {
    std::vector<std::string> result;

    auto split_view = std::views::split(str, delimiter);
    for (const auto& part : split_view) {
        result.emplace_back(part.begin(), part.end());
    }

    return result;
}

// Callback function to filter out non-digit characters
static int FilterDigits(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (!isdigit(data->EventChar)) {
            return 1; // Filter this character (non digit character so don't keep it)
        }
    }
    return 0; // Keep this character (i.e. keep it if it's a digit)
}

static int FilterVerticalBarAway(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (data->EventChar == '|') {
            return 1; // Throw char away
        }
    }
    return 0; // Keep char (i.e. all other chars than '|')
}

bool is_decimal_number(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

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
        bool insideQuotes = false;

        while (linestream.good()) {
            char c = linestream.get();
            if (c == '\"') {
                if (insideQuotes && linestream.peek() == '\"') {
                    cell += '\"';
                    linestream.get();
                }
                else {
                    insideQuotes = !insideQuotes;
                }
            }
            else if (c == ',' && !insideQuotes) {
                row.push_back(cell);
                cell.clear();
            }
            else if (c != EOF) {
                cell += c;
            }
        }
        if (!cell.empty()) {
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
            std::string cell = row[i];
            // Escape quotes by replacing them with double quotes
            size_t pos = 0;
            while ((pos = cell.find("\"", pos)) != std::string::npos) {
                cell.replace(pos, 1, "\"\"");
                pos += 2; // Move past the new double quotes
            }
            // Enclose in quotes if the cell contains a comma, quote, or newline
            if (cell.find(',') != std::string::npos || cell.find('\"') != std::string::npos || cell.find('\n') != std::string::npos) {
                cell = "\"" + cell + "\"";
            }
            temp_file << cell;
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

bool create_empty_csv(const std::filesystem::path& csv_filepath) {
    if (std::filesystem::exists(csv_filepath)) {
        return false;
    }

    // Define the header
    const std::string header = "file_id,name,gww_url,map_ids,is_explorable,is_outpost,is_pvp,model_type,file_type";

    // Create and write the header to the CSV file
    try {
        std::ofstream file(csv_filepath);
        file << header << std::endl;
        file.close();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
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

std::wstring open_file_dialog(bool saveAs = false) {
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
    ofn.Flags = OFN_PATHMUSTEXIST;

    if (saveAs) {
        ofn.Flags |= OFN_OVERWRITEPROMPT; // Add overwrite prompt for saving files
        if (GetSaveFileName(&ofn) == TRUE) {
            std::wstring filePath = ofn.lpstrFile;

            // Check if the file extension is .csv, if not, append it
            if (filePath.size() < 4 || filePath.compare(filePath.size() - 4, 4, L".csv") != 0) {
                filePath += L".csv";
            }

            return filePath;
        }
    }
    else {
        ofn.Flags |= OFN_FILEMUSTEXIST; // Ensure file must exist for opening files
        if (GetOpenFileName(&ofn) == TRUE) {
            return ofn.lpstrFile;
        }
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

void edit_model(std::set<ModelTypes>& selected_model_types, ModelTypes& current_selection, std::string& model_type)
{
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
        if (!model_type.empty()) model_type += multival_separator;
        model_type += ModelTypeToString(type);
    }
}

void display_semicolon_separated_string(const std::string& separated_string) {
    const auto items = split(separated_string, multival_separator);

    for (int i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        if (!item.empty()) {
            if (i > 0) {
                ImGui::SameLine();
                ImGui::Text(", ");
                ImGui::SameLine();
            }
            ImGui::Text("%s", item.c_str());
        }
    }
}


void edit_name(std::string& curr_name_input_buf, std::set<std::string>& prev_added_names, std::string& prev_name_input, std::set<std::string>& selected_names, std::string& name_buf) {
    ImGui::InputText("Name", &curr_name_input_buf, ImGuiInputTextFlags_CallbackCharFilter, FilterVerticalBarAway);
    if (!curr_name_input_buf.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Add another")) {
            prev_added_names.insert(curr_name_input_buf);
            curr_name_input_buf.clear();
            prev_name_input.clear();
        }
    }

    if (curr_name_input_buf != prev_name_input) {
        if (!prev_name_input.empty() && !prev_added_names.contains(prev_name_input)) {
            selected_names.erase(prev_name_input);
        }

        if (!curr_name_input_buf.empty()) {
            selected_names.insert(curr_name_input_buf);
        }
    }

    for (auto it = selected_names.begin(); it != selected_names.end();) {
        std::string curr_map_id = *it;
        std::string label = curr_map_id + "##Selected";
        if (ImGui::Button(("- " + label).c_str())) {
            it = selected_names.erase(it);
            prev_added_names.erase(curr_map_id);
        }
        else {
            ++it;
        }
    }

    // Convert selected model types to a semicolon-separated string
    name_buf.clear();
    for (const auto& name : selected_names) {
        if (!name_buf.empty()) name_buf += multival_separator;
        name_buf += name;
    }

    prev_name_input = curr_name_input_buf;
}

void edit_map(std::string& curr_map_id_input_buf, std::set<std::string>& prev_added_map_ids, std::string& prev_map_id_input, std::set<int>& selected_map_ids, std::string& map_id_buf, bool& is_explorable, bool& is_outpost, bool& is_pvp)
{
    ImGui::InputText("Map id", &curr_map_id_input_buf, ImGuiInputTextFlags_CallbackCharFilter, FilterDigits);
    if (!curr_map_id_input_buf.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Add another")) {
            prev_added_map_ids.insert(curr_map_id_input_buf);
            curr_map_id_input_buf.clear();
            prev_map_id_input.clear();
        }
    }

    if (curr_map_id_input_buf != prev_map_id_input) {
        if (!prev_map_id_input.empty() && !prev_added_map_ids.contains(prev_map_id_input) && is_decimal_number(prev_map_id_input)) {
            selected_map_ids.erase(std::stoi(prev_map_id_input));
        }

        if (!curr_map_id_input_buf.empty() && is_decimal_number(curr_map_id_input_buf)) {
            selected_map_ids.insert(std::stoi(curr_map_id_input_buf));
        }
    }

    for (auto it = selected_map_ids.begin(); it != selected_map_ids.end();) {
        std::string curr_map_id = std::to_string(*it);
        std::string label = curr_map_id + "##Selected";
        if (ImGui::Button(("- " + label).c_str())) {
            it = selected_map_ids.erase(it);
            prev_added_map_ids.erase(curr_map_id);
        }
        else {
            ++it;
        }
    }

    // Convert selected model types to a semicolon-separated string
    map_id_buf.clear();
    for (const auto& map_id : selected_map_ids) {
        if (!map_id_buf.empty()) map_id_buf += multival_separator;
        map_id_buf += std::to_string(map_id);
    }

    ImGui::Checkbox("Is Explorable", &is_explorable);
    ImGui::Checkbox("Is Outpost", &is_outpost);
    ImGui::Checkbox("Is PvP", &is_pvp);

    prev_map_id_input = curr_map_id_input_buf;
}

void save(bool& edit_mode, int& found_row_index, std::vector<std::vector<std::string>>& csv_data, std::vector<std::string>& row, std::string& name_buf, std::string& gwwiki_buf, std::string& model_type, std::string& map_id_buf, bool is_explorable, bool is_outpost, bool is_pvp, std::string& selected_item_hash_hex, bool& csv_changed)
{
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
    csv_changed = true;
}

bool draw_file_info_editor_panel(std::vector<std::vector<std::string>>& csv_data) {
    static std::set<ModelTypes> selected_model_types;
    static std::set<int> selected_map_ids;
    static std::set<std::string> selected_names;

    // Used for map ids input
    static std::string curr_map_id_input_buf;
    static std::string prev_map_id_input;
    static std::set<std::string> prev_added_map_ids; // when "Add another" is pressed we save the map_id value here. This is to make sure we don't delete them when changing the new map_id and it overlaps the previous one but then changes.

    // Used for names input
    static std::string curr_name_input_buf;
    static std::string prev_name_input;
    static std::set<std::string> prev_added_names;

    static std::unordered_map<uint32_t, int> item_hash_to_row_index;

    bool csv_changed = false;

    const uint32_t item_hash = selected_item_hash > 0 ? selected_item_hash : selected_item_murmurhash3;

    std::string selected_item_hash_hex = std::format("0x{:08x}", item_hash);

    ImGui::Begin("Custom File Info");

    if (csv_filepath.empty()) {
        ImGui::Text("Loaded file: None");
    }
    else {
        ImGui::Text("Loaded file: \"%s\"", csv_filepath.string().c_str());
    }

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
            csv_changed = true;
        }
    }

    // Button to change the CSV file
    if (!csv_filepath.empty()) {
        if (ImGui::Button("Change CSV File")) {
            csv_filepath = open_file_dialog();
            if (!csv_filepath.empty()) {
                csv_data = load_csv(csv_filepath.string());
                const auto filepath = save_last_filepath(csv_filepath, last_csv_filename);
                csv_changed = true;
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Create new empty csv file")) {
        csv_filepath = open_file_dialog(true);
        if (!csv_filepath.empty()) {
            if (create_empty_csv(csv_filepath.string())) {
                csv_data = load_csv(csv_filepath.string());
                save_last_filepath(csv_filepath, last_csv_filename);
                csv_changed = true;
            }
        }
    }

    static std::set<uint32_t> duplicate_hashes_in_csv;
    if (csv_changed) {
        for (int i = 1; i < csv_data.size(); i++) {
            const auto& row = csv_data[i];

            uint32_t row_hash_int = -1; // max value (no such file_id or murmurhash3 in the dat, use -1 as non-existance)

            // check if the file_id in the csv start with 0x or 0X
            if (row[0][0] == '0' && std::tolower(row[0][1]) == 'x') {
                row_hash_int = std::stoul(row[0], 0, 16);
            }
            else {
                row_hash_int = std::stoul(row[0]);
            }

            if (item_hash_to_row_index.contains(row_hash_int)) {
                duplicate_hashes_in_csv.insert(row_hash_int);
            }
            else{
                item_hash_to_row_index.try_emplace(row_hash_int, i);
            }
        }
    }

    ImGui::Separator();

    if (item_hash == -1 || csv_data.empty()) {
    }
    else {
        static int found_row_index = -1;
        static bool edit_mode = false;

        static std::string name_buf;
        static std::string gwwiki_buf;
        static std::string map_id_buf;
        static bool is_explorable = false;
        static bool is_outpost = false;
        static bool is_pvp = false;
        static std::string model_type;

        bool selected_item_hash_changed = prev_selected_item_hash != item_hash;
        if (selected_item_hash_changed) {
            curr_map_id_input_buf.clear();
            prev_map_id_input.clear();
            prev_added_map_ids.clear();
            curr_name_input_buf.clear();
            prev_name_input.clear();
            prev_added_names.clear();
            selected_model_types.clear();
            selected_map_ids.clear();
            selected_names.clear();
            name_buf.clear();
            gwwiki_buf.clear();
            map_id_buf.clear();
            is_explorable = false;
            is_outpost = false;
            is_pvp = false;
            model_type.clear();

            edit_mode = false;
            found_row_index = -1;

            if (item_hash_to_row_index.contains(item_hash)) {
                found_row_index = item_hash_to_row_index[item_hash];
            }
        }

        // If no matching row is found, create a new one but we don't add it to the csv until the user press 'save'
        static std::vector<std::string> row_backup;
        std::vector<std::string> row(9);
        if (found_row_index == -1 && item_hash != -1) {
            row[0] = selected_item_hash_hex;
            row[8] = type_strings[selected_file_type];
            row_backup = row;
        }

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
            selected_map_ids.clear();
            selected_names.clear();

            // Split the model_type string by semicolons and convert each part to the enum value
            for (auto token : std::views::split(model_type, multival_separator)) {
                std::string_view token_sv(&*token.begin(), std::ranges::distance(token));
                ModelTypes type = StringToModelType(std::string(token_sv)); // Convert token to string and then to ModelTypes enum
                if (type != ModelTypes::unknown) { // Check for a valid type
                    selected_model_types.insert(type);
                }
            }

            for (auto map_id_token : std::views::split(map_id_buf, multival_separator)) {
                auto map_id = std::stoi(std::string(map_id_token.begin(), map_id_token.end()));
                selected_map_ids.insert(map_id);
            }

            for (auto name_token : std::views::split(name_buf, multival_separator)) {
                selected_names.emplace(std::string(name_token.begin(), name_token.end()));
            }
        }

        // Display the row's data
        ImGui::Text("File ID: %s", row[0].c_str());

        if (edit_mode) {
            edit_name(curr_name_input_buf, prev_added_names, prev_name_input, selected_names, name_buf);

            ImGui::InputText("URL", &gwwiki_buf);

            if (selected_file_type == FFNA_Type2) {
                static ModelTypes current_selection = ModelTypes::unknown;
                edit_model(selected_model_types, current_selection, model_type);
            }
            else if (selected_file_type == FFNA_Type3) {
                edit_map(curr_map_id_input_buf, prev_added_map_ids, prev_map_id_input, selected_map_ids, map_id_buf, is_explorable, is_outpost, is_pvp);
            }

            if (ImGui::Button("Save")) {
                save(edit_mode, found_row_index, csv_data, row, name_buf, gwwiki_buf, model_type, map_id_buf, is_explorable, is_outpost, is_pvp, selected_item_hash_hex, csv_changed);
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                edit_mode = false;
                row = row_backup;
            }
        }
        else {
            if (row[1] != "") {
                ImGui::Text("Names: ");
                ImGui::SameLine();
                display_semicolon_separated_string(row[1]);
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
                if (map_ids_string != "") {
                    ImGui::Text("Map ids: ");
                    ImGui::SameLine();

                    display_semicolon_separated_string(map_ids_string);
                }
                else {
                    ImGui::Text("Map ids: N/A");
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

        prev_selected_item_hash = item_hash;
    }

    if (!duplicate_hashes_in_csv.empty()) {
        ImGui::Separator();
        ImGui::Text("Duplicate entries:");
        for (const auto& hash : duplicate_hashes_in_csv) {
            std::string duplicate_row_text = std::format("0x{:08x}", hash);
            ImGui::Text(duplicate_row_text.c_str());
        }
    }

    ImGui::End();

    return csv_changed;
}
