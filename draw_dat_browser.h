#pragma once
#include "DATManager.h"

enum DatBrowserItemColumnID
{
    DatBrowserItemColumnID_id,
    DatBrowserItemColumnID_hash,
    DatBrowserItemColumnID_type,
    DatBrowserItemColumnID_size,
    DatBrowserItemColumnID_decompressed_size,
};

struct DatBrowserItem
{
    uint32_t id;
    uint32_t hash;
    uint32_t type;
    uint32_t size;
    uint32_t decompressed_size;

    static const ImGuiTableSortSpecs* s_current_sort_specs;

    static int IMGUI_CDECL CompareWithSortSpecs(const void* lhs, const void* rhs);
};

void parse_file(DATManager& dat_manager, int index);

void draw_data_browser(DATManager& dat_manager);
