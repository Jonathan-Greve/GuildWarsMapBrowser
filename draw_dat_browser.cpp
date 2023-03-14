#include "pch.h"
#include "draw_dat_browser.h"

const ImGuiTableSortSpecs* DatBrowserItem::s_current_sort_specs = NULL;

void draw_data_browser(std::vector<MFTEntry>& entries)
{
    // Create item list
    static ImVector<DatBrowserItem> items;
    if (items.Size == 0)
    {
        items.resize(entries.size(), DatBrowserItem());
        for (int i = 0; i < entries.size(); i++)
        {
            const auto& entry = entries[i];
            DatBrowserItem new_item{i, entry.Hash, entry.type, entry.Size};
            items[i] = new_item;
        }
    }

    // Options
    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("data browser", 4, flags))
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
        ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
            if (sorts_specs->SpecsDirty)
            {
                DatBrowserItem::s_current_sort_specs =
                  sorts_specs; // Store in variable accessible by the sort function.
                if (items.Size > 1)
                    qsort(&items[0], (size_t)items.Size, sizeof(items[0]),
                          DatBrowserItem::CompareWithSortSpecs);
                DatBrowserItem::s_current_sort_specs = NULL;
                sorts_specs->SpecsDirty = false;
            }

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(items.Size);
        while (clipper.Step())
            for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
            {
                // Display a data item
                DatBrowserItem* item = &items[row_n];
                ImGui::PushID(item->id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%04d", item->id);
                ImGui::TableNextColumn();
                const auto file_hash_text = std::format("0x{:X} ({})", item->hash, item->hash);
                ImGui::Text(file_hash_text.c_str());
                ImGui::TableNextColumn();
                ImGui::Text(typeToString(item->type).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%04d", item->size);
                ImGui::PopID();
            }
        ImGui::EndTable();
    }
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
