#include "pch.h"
#include "draw_dat_compare_panel.h"
#include <filesystem>
#include <draw_dat_load_progress_bar.h>
#include <comparer_dsl.h>
#include <show_how_to_use_dat_comparer_guide.h>
#include <GuiGlobalConstants.h>

std::vector<std::wstring> file_paths;
std::map<std::wstring, int> filepath_to_alias; // Map to track filepath and its alias
std::vector<std::unordered_map<uint32_t, DatCompareFileInfo>> fileid_to_compare_file_infos;
std::unordered_set<uint32_t> filter_eval_result;
std::set<uint32_t> all_dats_file_ids; // all the file_ids in fileid_to_compare_file_infos

static bool show_how_to_use_guide = false;

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
            data->Buf[i] = std::tolower(data->Buf[i]);
        }

        // Place any additional code here to do something whenever the InputText changes
    }
    return 0;
}

void draw_dat_compare_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, std::unordered_set<uint32_t>& dat_compare_filter_result_out, bool& filter_result_changed_out)
{
    static ComparerDSL comparer_dsl;

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

    if (GuiGlobalConstants::is_compare_panel_open && ImGui::Begin("Compare DAT files", &GuiGlobalConstants::is_compare_panel_open)) {

        if (!is_analyzing) {
            // File selection button
            if (ImGui::Button("Select File")) {
                std::string initial_filepath = ".";

                const auto filepath_existing = load_last_filepath("dat_browser_last_filepath.txt");
                if (filepath_existing.has_value()) {
                    initial_filepath = filepath_existing.value().parent_path().string();
                }

                if (!std::filesystem::exists(initial_filepath) || !std::filesystem::is_directory(initial_filepath)) {
                    const auto filepath_curr_dir = get_executable_directory();
                    if (filepath_curr_dir.has_value()) {
                        initial_filepath = filepath_curr_dir.value().string();
                    }
                }

                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".dat", initial_filepath + "\\.");
            }

            // Display File Dialog
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                    std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

                    save_last_filepath(filePathName, "dat_browser_last_filepath.txt");

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
                            fileid_to_compare_file_infos.clear();

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

            ImGui::Text("Filter Expression");
            ImGui::SameLine();
            if (ImGui::InputText("##filter_expression", &filter_expression)) {
                filter_expr_error = "";
                filter_last_success_parsed_expr = filter_expression;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("To learn how to use the tool press the checkbox at the bottom (The guide opens in a separate ImGui window).");
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
                        std::unordered_map<int, DatCompareFileInfo> file_infos;
                        for (int i = 0; i < dat_managers.size(); i++) {
                            const auto it = fileid_to_compare_file_infos[i].find(file_id);
                            if (it != fileid_to_compare_file_infos[i].end()) {
                                file_infos.insert_or_assign(i, it->second);
                            }
                        }

                        try
                        {
                            const auto should_include_file = comparer_dsl.parse(filter_expression, file_infos);

                            if (should_include_file)
                                filter_eval_result.emplace(file_id);
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

        if (filter_eval_result.size() > 0) {
            if (ImGui::Button("Clear filter")) {
                filter_result_changed_out = true;
                dat_compare_filter_result_out.clear();
                filter_eval_result.clear();
            }
        }

        const auto log_messages = comparer_dsl.get_log_messages();
        if (log_messages.size()) {
            std::set<std::string> errors;
            ImGui::Text("Parsing errors:");
            for (const auto& msg : log_messages) {
                errors.insert(msg);
            }

            for (const auto& error : errors) {
                ImGui::Text(error.c_str());
            }
        }

        ImGui::Separator();
        ImGui::Checkbox("Show How to Use Guide", &show_how_to_use_guide);

    }
    ImGui::End();

    show_how_to_use_dat_comparer_guide(&show_how_to_use_guide);

    // Create maps for each dat file mapping filehash(file_id) to each entrys murmurhash.
    // We don't include files where the file_id is the same (i.e. many files have the file_id 0) which I don't know how to compare between multiples dats
    if (!is_analyzing && fileid_to_compare_file_infos.size() < dat_managers.size()) {
        all_dats_file_ids.clear();
        fileid_to_compare_file_infos.clear();
        for (int i = 0; i < dat_managers.size(); i++) {
            std::set<uint32_t> seen_file_ids;
            std::unordered_map<uint32_t, DatCompareFileInfo> new_map;
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
                    int filename_id_0 = 0;
                    int filename_id_1 = 0;

                    // Update filename_id_0 and filename_id_1 with the proper values.
                    encode_filehash(mft[j].Hash, filename_id_0, filename_id_1);

                    DatCompareFileInfo file_info{ mft[j].murmurhash3, mft[j].uncompressedSize, filename_id_0, filename_id_1 };

                    new_map.emplace(file_id, file_info);
                    seen_file_ids.emplace(file_id);
                    all_dats_file_ids.emplace(file_id);
                }
            }

            fileid_to_compare_file_infos.emplace_back(new_map);
        }
    }
}