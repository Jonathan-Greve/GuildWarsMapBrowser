#include "pch.h"
#include "draw_dat_compare_panel.h"
#include <filesystem>
#include <draw_dat_load_progress_bar.h>
#include <dat_comparer_lexer.h>
#include <dat_comparer_parser.h>

std::vector<std::wstring> file_paths;
std::map<std::wstring, int> filepath_to_alias; // Map to track filepath and its alias
std::vector<std::unordered_map<uint32_t, uint32_t>> dat_fileid_to_mm3hash;
std::unordered_set<uint32_t> filter_eval_result;
std::set<uint32_t> all_dats_file_ids; // all the file_ids in dat_fileid_to_mm3hash

Lexer lexer{ "" };
std::unique_ptr<Parser> parser;

void add_dat_manager(const std::wstring& filepath, std::map<int, std::unique_ptr<DATManager>>& dat_managers)
{
    if (dat_managers.find(filepath_to_alias[filepath]) == dat_managers.end()) {
        auto new_dat_manager = std::make_unique<DATManager>();
        if (new_dat_manager->Init(filepath)) {
            dat_managers[filepath_to_alias[filepath]] = std::move(new_dat_manager);
        }
    }
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        for (int i = 0; i < data->BufTextLen; i++) {
            data->Buf[i] = std::toupper(data->Buf[i]);
        }

        // Place any additional code here to do something whenever the InputText changes
    }
    return 0;
}

void draw_dat_compare_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, std::unordered_set<uint32_t>& dat_compare_filter_result_out, bool& filter_result_changed_out)
{
    static const std::wstring& existing_dat_filepath = dat_managers[0]->get_filepath();
    static bool isExistingFilePathAdded = false;

    if (!isExistingFilePathAdded) {
        file_paths.push_back(existing_dat_filepath);
        filepath_to_alias[existing_dat_filepath] = 0; // Alias 0 for existing DAT file
        isExistingFilePathAdded = true;
    }

    // Variables to store cumulative progress
    int total_additional_files_read = 0;
    int total_additional_files = 0;

    // Sum up the progress of all additional dat files
    for (const auto& filepath : file_paths) {
        if (filepath != existing_dat_filepath) { // Skip the existing dat file
            int alias = filepath_to_alias[filepath];
            if (dat_managers.find(alias) != dat_managers.end()) {
                // Assuming each DATManager has methods to get the number of files read and the total files
                total_additional_files_read += dat_managers[alias]->get_num_files_type_read();
                total_additional_files += dat_managers[alias]->get_num_files();
            }
        }
    }

    bool is_analyzing = total_additional_files_read < total_additional_files;

    // Show progress bar of loading additional dat files
    // Draw a single progress bar for the cumulative progress
    if (is_analyzing) {
        draw_dat_load_progress_bar(total_additional_files_read, total_additional_files);
    }

    ImGui::Begin("Compare DAT files");

    if (!is_analyzing) {
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
                    add_dat_manager(wstr, dat_managers);
                    is_analyzing = true;
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }

    // List selected files
    for (size_t i = 0; i < file_paths.size(); ++i) {
        int alias = filepath_to_alias[file_paths[i]];
        ImGui::Text("DAT%d: %ls", alias, file_paths[i].c_str());

        if (!is_analyzing) {
            if (alias != dat_manager_to_show) {
                ImGui::SameLine();
                std::string btn_label = "Remove ##" + std::to_string(i);
                if (ImGui::Button(btn_label.c_str())) {

                    int alias_to_remove = filepath_to_alias[file_paths[i]];

                    if (alias_to_remove < dat_manager_to_show) {
                        dat_manager_to_show--;
                    }

                    // Remove DATManager if it exists
                    if (dat_managers.find(alias_to_remove) != dat_managers.end()) {
                        dat_managers.erase(alias_to_remove);

                    // Erase file from vectors and map
                    filepath_to_alias.erase(file_paths[i]);
                    file_paths.erase(file_paths.begin() + i);
                    dat_fileid_to_mm3hash.clear();

                    // Reorder the aliases in the map
                    for (auto& pair : filepath_to_alias) {
                        if (pair.second > alias_to_remove) {
                            pair.second -= 1;
                        }
                    }

                    // Update dat_managers map to reflect new aliases
                    std::map<int, std::unique_ptr<DATManager>> updated_dat_managers;
                    for (auto& pair : dat_managers) {
                        if (pair.first > alias_to_remove) {
                            updated_dat_managers[pair.first - 1] = std::move(pair.second);
                        }
                        else if (pair.first < alias_to_remove) {
                            updated_dat_managers[pair.first] = std::move(pair.second);
                        }
                    }
                    dat_managers = std::move(updated_dat_managers);

                    --i; // Adjust loop counter as the list size has changed
                    }
                }

                ImGui::SameLine();
                std::string show_btn_label = "Show in DAT browser ##" + std::to_string(i);
                if (ImGui::Button(show_btn_label.c_str())) {
                    dat_manager_to_show = alias;
                }
            }
            else {
                ImGui::SameLine();
                ImGui::Text("Currently shown");
            }
        }
    }

    if (!is_analyzing) {
        ImGui::Separator();
        static std::string filter_expression = ""; // Buffer for input text

        static std::string filter_expr_error = "";
        static std::string filter_last_success_parsed_expr = "";
        static int num_eval_errors = 0;
        static std::shared_ptr<ASTNode> filter_last_success_parsed_AST;

        ImGui::Text("Filter Expression");
        ImGui::SameLine();
        if (ImGui::InputText("##filter_expression", &filter_expression, ImGuiInputTextFlags_CallbackAlways, TextEditCallback)) {
            try {
                lexer = Lexer(filter_expression);
                parser = std::make_unique<Parser>(lexer); // Create a new Parser instance
                filter_last_success_parsed_AST = parser->parse();

                filter_expr_error = "";
                filter_last_success_parsed_expr = filter_expression;
            }
            catch (const std::exception& e) {
                filter_expr_error = e.what();
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enter the comparison expression here. Examples include logical operations like AND, OR, XOR, NOT. For instance:\n- NOT (DAT0 AND DAT1) XOR DAT0\n- NOT (DAT0 AND (DAT1 OR DAT2))\n- (DAT0 OR DAT1) AND NOT (DAT2 XOR DAT3)");
        }

        ImGui::Text(std::format("Filter error: {}", filter_expr_error).c_str());
        if (filter_last_success_parsed_expr != "" && filter_expr_error.size() > 0) {
            ImGui::Text(std::format("Last successful parse: \"{}\"", filter_last_success_parsed_expr).c_str());
        }

        if (num_eval_errors > 0 || filter_eval_result.size() > 0) {
            ImGui::Text(std::format("Num eval errors: {}", num_eval_errors).c_str());
            ImGui::Text(std::format("Num unique files: {}", filter_eval_result.size()).c_str());
        }

        if (filter_expression.size() > 0 && filter_expression == filter_last_success_parsed_expr && filter_expr_error == "") {
            if (ImGui::Button("Start filtering")) {
                filter_eval_result.clear();
                num_eval_errors = 0;

                for (const auto file_id : all_dats_file_ids) {
                    std::map<int, uint32_t> DATs_hashes;
                    for (int i = 0; i < dat_managers.size(); i++) {
                        const auto it = dat_fileid_to_mm3hash[i].find(file_id);
                        if (it != dat_fileid_to_mm3hash[i].end()) {
                            DATs_hashes.insert_or_assign(i, it->second);
                        }
                    }

                    try
                    {
                        std::vector<uint32_t> exclude;
                        const auto eval_result = evaluate(filter_last_success_parsed_AST, DATs_hashes, exclude);
                        if (!eval_result.empty())
                            filter_eval_result.emplace(file_id);
                        //filter_eval_result.insert(eval_result.begin(), eval_result.end());
                    }
                    catch (const std::exception&)
                    {
                        num_eval_errors += 1;
                    }
                }

                filter_result_changed_out = true;
                dat_compare_filter_result_out = filter_eval_result;
            }
        }
    }

    ImGui::End();

    // Create maps for each dat file mapping filehash(file_id) to each entrys murmurhash.
    // We don't include files where the file_id is the same (i.e. many files have the file_id 0) which I don't know how to compare between multiples dats
    if (!is_analyzing && dat_fileid_to_mm3hash.size() < dat_managers.size()) {
        all_dats_file_ids.clear();
        dat_fileid_to_mm3hash.clear();
        for (int i = 0; i < dat_managers.size(); i++) {
            std::set<uint32_t> seen_file_ids;
            std::unordered_map<uint32_t, uint32_t> new_map;
            const auto& mft = dat_managers[i]->get_MFT();
            for (int j = 0; j < mft.size(); j++) {
                const auto file_id = mft[j].Hash;
                const auto seen_it = seen_file_ids.find(file_id);
                if (seen_it != seen_file_ids.end()) {
                    // remove the entry from the new_map if it is already there. Otherwise do nothing
                    // Basically we just don't want any entries in the map that has a file_id that is shared.
                    const auto new_map_it = new_map.find(file_id);
                    if (new_map_it != new_map.end()) {
                        new_map.erase(file_id);
                        all_dats_file_ids.erase(file_id);
                    }
                }
                else {
                    new_map.emplace(file_id, mft[j].murmurhash3);
                    seen_file_ids.emplace(file_id);
                    all_dats_file_ids.emplace(file_id);
                }
            }

            dat_fileid_to_mm3hash.emplace_back(new_map);
        }
    }
}