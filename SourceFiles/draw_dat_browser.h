#pragma once
#include "DATManager.h"
#include "MapRenderer.h"

using FileData = std::variant<FFNA_ModelFile /*, other types */>;

struct SelectedDatTexture
{
	DatTexture dat_texture;
	int texture_id = -1;
	int file_id = -1; // Also called hash in many places
};

enum DatBrowserItemColumnID
{
	DatBrowserItemColumnID_id,
	DatBrowserItemColumnID_hash,
	DatBrowserItemColumnID_filename,
	DatBrowserItemColumnID_name,
	DatBrowserItemColumnID_type,
	DatBrowserItemColumnID_size,
	DatBrowserItemColumnID_decompressed_size,
	DatBrowserItemColumnID_map_id,
	DatBrowserItemColumnID_is_pvp,
	DatBrowserItemColumnID_murmurhash3,
	DatBrowserItemColumnID_chunk_ids
};

struct DatBrowserItem
{
	uint32_t id;
	uint32_t hash;
	FileType type;
	uint32_t size;
	uint32_t decompressed_size;
	uint16_t file_id_0;
	uint16_t file_id_1;

	std::vector<uint32_t> map_ids;
	std::vector<std::string> names;
	std::vector<int> is_pvp;

	uint32_t murmurhash3;

	std::vector<uint32_t> chunk_ids;  // Chunk IDs found in FFNA files

	static const ImGuiTableSortSpecs* s_current_sort_specs;

	static int IMGUI_CDECL CompareWithSortSpecs(const void* lhs, const void* rhs);
};

struct CustomFileInfoEntry
{
	uint32_t hash;
	std::vector<std::string> names;
	std::vector<uint32_t> map_ids;
	bool is_pvp;
};

bool parse_file(DATManager* dat_manager, int index, MapRenderer* map_renderer,
	std::unordered_map<int, std::vector<int>>& hash_index);

void draw_data_browser(DATManager* dat_manager, MapRenderer* map_renderer, bool dat_manager_changed, const std::unordered_set<uint32_t>& dat_compare_filter_result, const bool dat_compare_filter_result_changed,
	std::vector<std::vector<std::string>>& csv_data, bool custom_file_info_changed);

void ExportDDS2(DATManager* dat_manager, DatBrowserItem& item, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format);

void ExportDDS(DATManager* dat_manager, int mft_file_index, int file_id, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format);

