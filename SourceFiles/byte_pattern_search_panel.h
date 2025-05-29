#pragma once
#include <unordered_set>
#include <memory>
#include <map>
#include <DATManager.h>

void draw_byte_pattern_search_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers,
	int& dat_manager_to_show,
	std::unordered_set<uint32_t>& dat_compare_filter_result_out,
	bool& filter_result_changed_out);