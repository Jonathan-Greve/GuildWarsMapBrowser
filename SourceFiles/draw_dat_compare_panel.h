#pragma once
#include <DATManager.h>

void add_dat_manager(const std::wstring& filepath, const std::wstring& existing_dat_filepath, std::map<int, std::unique_ptr<DATManager>>& dat_managers);

void draw_dat_compare_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show);

