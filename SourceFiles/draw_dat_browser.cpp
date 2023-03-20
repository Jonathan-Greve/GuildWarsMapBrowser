#include "pch.h"
#include "draw_dat_browser.h"
#include "GuiGlobalConstants.h"

const char* type_strings[26] = {
  " ",        "AMAT",     "Amp",      "ATEXDXT1", "ATEXDXT2",     "ATEXDXT3",   "ATEXDXT4",
  "ATEXDXT5", "ATEXDXTN", "ATEXDXTA", "ATEXDXTL", "ATTXDXT1",     "ATTXDXT3",   "ATTXDXT5",
  "ATTXDXTN", "ATTXDXTA", "ATTXDXTL", "DDS",      "FFNA - Model", "FFNA - Map", "FFNA - Unknown",
  "MFTBase",  "NOT_READ", "Sound",    "Text",     "Unknown"};

const ImGuiTableSortSpecs* DatBrowserItem::s_current_sort_specs = NULL;

void parse_file(DATManager& dat_manager, int index, MapRenderer* map_renderer)
{
    const auto MFT = dat_manager.get_MFT();
    if (index >= MFT.size())
        return;

    const auto* entry = &MFT[index];
    if (! entry)
        return;

    FFNA_MapFile ffna_map_file;
    std::unique_ptr<Terrain> terrain;

    switch (entry->type)
    {
    case FFNA_Type2:
        break;
    case FFNA_Type3:
        ffna_map_file = dat_manager.parse_ffna_map_file(index);
        if (ffna_map_file.terrain_chunk.terrain_heightmap.size() > 0 &&
            ffna_map_file.terrain_chunk.terrain_heightmap.size() ==
              ffna_map_file.terrain_chunk.terrain_x_dims * ffna_map_file.terrain_chunk.terrain_y_dims)
        {
            terrain = std::make_unique<Terrain>(
              ffna_map_file.terrain_chunk.terrain_x_dims, ffna_map_file.terrain_chunk.terrain_y_dims,
              ffna_map_file.terrain_chunk.terrain_heightmap, ffna_map_file.map_info_chunk.map_bounds);
            map_renderer->SetTerrain(std::move(terrain));
        }
        break;
    default:
        break;
    }
}

int custom_stoi(const std::string& input);

void draw_data_browser(DATManager& dat_manager, MapRenderer* map_renderer)
{
    static std::unordered_map<int, std::vector<int>> id_index;
    static std::unordered_map<int, std::vector<int>> hash_index;
    static std::unordered_map<FileType, std::vector<int>> type_index;

    ImVec2 dat_browser_window_size =
      ImVec2(ImGui::GetIO().DisplaySize.x -
               (GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2) -
               (GuiGlobalConstants::right_panel_width + GuiGlobalConstants::panel_padding * 2),
             300);
    ImVec2 dat_browser_window_pos =
      ImVec2(GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2,
             GuiGlobalConstants::panel_padding);
    ImGui::SetNextWindowPos(dat_browser_window_pos);
    ImGui::SetNextWindowSize(dat_browser_window_size);

    ImGui::Begin("Browse .dat file contents");
    // Create item list
    static ImVector<DatBrowserItem> items;
    static ImVector<DatBrowserItem> filtered_items;
    if (items.Size == 0)
    {
        const auto& entries = dat_manager.get_MFT();
        items.resize(entries.size(), DatBrowserItem());
        filtered_items.resize(entries.size(), DatBrowserItem());
        for (int i = 0; i < entries.size(); i++)
        {
            const auto& entry = entries[i];
            DatBrowserItem new_item{i, entry.Hash, (FileType)entry.type, entry.Size, entry.uncompressedSize};
            items[i] = new_item;
            filtered_items[i] = new_item;
        }
    }

    if (items.Size != 0 && id_index.empty())
    {
        for (int i = 0; i < items.Size; i++)
        {
            const auto& item = items[i];
            id_index[item.id].push_back(i);
            hash_index[item.hash].push_back(i);
            type_index[item.type].push_back(i);
        }
    }

    // Set after filtering is complete.
    static std::string curr_id_filter = "";
    static std::string curr_hash_filter = "";
    static FileType curr_type_filter = NONE;

    // The values set by the user in the GUI
    static std::string id_filter_text;
    static std::string hash_filter_text;
    static FileType type_filter_value = NONE;

    static bool filter_update_required = true;

    if (curr_id_filter != id_filter_text)
    {
        curr_id_filter = id_filter_text;
        filter_update_required = true;
    }

    if (curr_hash_filter != hash_filter_text)
    {
        curr_hash_filter = hash_filter_text;
        filter_update_required = true;
    }

    if (curr_type_filter != type_filter_value)
    {
        curr_type_filter = type_filter_value;
        filter_update_required = true;
    }

    // Only re-run the filter when the user changed filter params in the GUI.
    if (filter_update_required)
    {
        filter_update_required = false;

        filtered_items.clear();

        std::unordered_set<int> intersection;

        if (! id_filter_text.empty())
        {
            int id_filter_value = custom_stoi(id_filter_text);
            if (id_filter_value >= 0 && id_index.count(id_filter_value))
            {
                intersection.insert(id_index[id_filter_value].begin(), id_index[id_filter_value].end());
            }
        }

        if (! hash_filter_text.empty())
        {
            int hash_filter_value = custom_stoi(hash_filter_text);
            if (hash_filter_value >= 0 && hash_index.count(hash_filter_value))
            {
                if (id_filter_text.empty())
                {
                    intersection.insert(hash_index[hash_filter_value].begin(),
                                        hash_index[hash_filter_value].end());
                }
                else
                {
                    std::unordered_set<int> new_intersection;
                    for (int id : hash_index[hash_filter_value])
                    {
                        if (intersection.count(id))
                        {
                            new_intersection.insert(id);
                        }
                    }
                    intersection = std::move(new_intersection);
                }
            }
        }

        if (type_filter_value != NONE)
        {
            if (id_filter_text.empty() && hash_filter_text.empty())
            {
                intersection.insert(type_index[type_filter_value].begin(),
                                    type_index[type_filter_value].end());
            }
            else
            {
                std::unordered_set<int> new_intersection;
                for (int id : type_index[type_filter_value])
                {
                    if (intersection.count(id))
                    {
                        new_intersection.insert(id);
                    }
                }
                intersection = std::move(new_intersection);
            }
        }

        if (id_filter_text.empty() && hash_filter_text.empty() && type_filter_value == NONE)
        {
            filtered_items = items;
        }
        else
        {
            for (const auto& id : intersection)
            {
                filtered_items.push_back(items[id]);
            }
        }

        // Set them equal so that the filter won't run again until the filter changes.
        curr_id_filter = id_filter_text;
        curr_hash_filter = hash_filter_text;
        curr_type_filter = type_filter_value;
    }

    // Filter table
    // Render the filter inputs and the table
    ImGui::Columns(3);
    ImGui::Text("Id filter:");
    ImGui::SameLine();
    ImGui::InputText("##IdFilter", &id_filter_text, sizeof(id_filter_text));
    ImGui::NextColumn();

    ImGui::Text("Hash filter:");
    ImGui::SameLine();
    ImGui::InputText("##HashFilter", &hash_filter_text, sizeof(hash_filter_text));
    ImGui::NextColumn();

    ImGui::Text("Type filter:");
    ImGui::SameLine();
    ImGui::Combo("##EnumFilter", reinterpret_cast<int*>(&type_filter_value), type_strings,
                 25); // add the combo box
    ImGui::Columns(1);

    ImGui::Separator();

    ImGui::Text("Filtered items: %d", filtered_items.Size);
    ImGui::SameLine();
    ImGui::Text("Total items: %d", items.Size);

    // Options
    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("data browser", 5, flags))
    {
        // Declare columns
        // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
        // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
        // Demonstrate using a mixture of flags among available sort-related flags:
        // - ImGuiTableColumnFlags_DefaultSort
        // - ImGuiTableColumnFlags_NoSort / ImGuiTableColumnFlags_NoSortAscending / ImGuiTableColumnFlags_NoSortDescending
        // - ImGuiTableColumnFlags_PreferSortAscending / ImGuiTableColumnFlags_PreferSortDescending
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed,
                                0.0f, DatBrowserItemColumnID_id);
        ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 0.0f, DatBrowserItemColumnID_hash);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0.0f, DatBrowserItemColumnID_type);
        ImGui::TableSetupColumn(
          "Size", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthStretch, 0.0f,
          DatBrowserItemColumnID_size);
        ImGui::TableSetupColumn("Decompressed size",
                                ImGuiTableColumnFlags_PreferSortDescending |
                                  ImGuiTableColumnFlags_WidthStretch,
                                0.0f, DatBrowserItemColumnID_decompressed_size);
        ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
            if (sorts_specs->SpecsDirty)
            {
                DatBrowserItem::s_current_sort_specs =
                  sorts_specs; // Store in variable accessible by the sort function.
                if (filtered_items.Size > 1)
                    qsort(&filtered_items[0], (size_t)filtered_items.Size, sizeof(filtered_items[0]),
                          DatBrowserItem::CompareWithSortSpecs);
                DatBrowserItem::s_current_sort_specs = NULL;
                sorts_specs->SpecsDirty = false;
            }

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(filtered_items.Size);

        static int selected_item_id = -1;
        ImGuiSelectableFlags selectable_flags =
          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

        while (clipper.Step())
            for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
            {
                DatBrowserItem& item = filtered_items[row_n];

                const bool item_is_selected = selected_item_id == item.id;

                auto label = std::format("{}", item.id);

                // Display a data item
                ImGui::PushID(item.id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(label.c_str(), item_is_selected, selectable_flags))
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                    }
                    else
                    {
                        selected_item_id = item.id;
                        parse_file(dat_manager, item.id, map_renderer);
                    }
                }

                ImGui::TableNextColumn();
                const auto file_hash_text = std::format("0x{:X} ({})", item.hash, item.hash);
                ImGui::Text(file_hash_text.c_str());
                ImGui::TableNextColumn();
                ImGui::Text(typeToString(item.type).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%04d", item.size);
                ImGui::TableNextColumn();
                ImGui::Text("%04d", item.decompressed_size);
                ImGui::PopID();
            }
        ImGui::EndTable();
    }

    ImGui::End();
}

inline int IMGUI_CDECL DatBrowserItem::CompareWithSortSpecs(const void* lhs, const void* rhs)
{
    const DatBrowserItem* a = (const DatBrowserItem*)lhs;
    const DatBrowserItem* b = (const DatBrowserItem*)rhs;
    for (int n = 0; n < s_current_sort_specs->SpecsCount; n++)
    {
        // Here we identify columns using the ColumnUserID value that we ourselves passed to TableSetupColumn()
        // We could also choose to identify columns based on their index (sort_spec->ColumnIndex), which is simpler!
        const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[n];
        int delta = 0;
        switch (sort_spec->ColumnUserID)
        {
        case DatBrowserItemColumnID_id:
            delta = (a->id - b->id);
            break;
        case DatBrowserItemColumnID_hash:
            delta = (a->hash - b->hash);
            break;
        case DatBrowserItemColumnID_type:
            delta = (a->type - b->type);
            break;
        case DatBrowserItemColumnID_size:
            delta = (a->size - b->size);
            break;
        case DatBrowserItemColumnID_decompressed_size:
            delta = (a->decompressed_size - b->decompressed_size);
            break;
        default:
            IM_ASSERT(0);
            break;
        }
        if (delta > 0)
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
        if (delta < 0)
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
    }

    // qsort() is instable so always return a way to differenciate items.
    // Your own compare function may want to avoid fallback on implicit sort specs e.g. a Name compare if it wasn't already part of the sort specs.
    return (a->id - b->id);
}

int custom_stoi(const std::string& input)
{
    const char* str = input.c_str();
    int value = 0;
    bool negative = false;

    // skip leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')
    {
        ++str;
    }

    // check for sign
    if (*str == '-')
    {
        negative = true;
        ++str;
    }
    else if (*str == '+')
    {
        ++str;
    }

    // check for hex prefix
    if (std::strlen(str) >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;

        // read hex digits
        while (*str != '\0')
        {
            if (*str >= '0' && *str <= '9')
            {
                value = value * 16 + (*str - '0');
            }
            else if (*str >= 'a' && *str <= 'f')
            {
                value = value * 16 + (*str - 'a' + 10);
            }
            else if (*str >= 'A' && *str <= 'F')
            {
                value = value * 16 + (*str - 'A' + 10);
            }
            else
            {
                return -1; // invalid character
            }

            ++str;
        }
    }
    else
    {
        // read decimal digits
        while (*str != '\0')
        {
            if (*str >= '0' && *str <= '9')
            {
                value = value * 10 + (*str - '0');
            }
            else
            {
                return -1; // invalid character
            }

            ++str;
        }
    }

    return negative ? -value : value;
}
