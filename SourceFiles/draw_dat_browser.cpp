#include "pch.h"
#include "draw_dat_browser.h"
#include "GuiGlobalConstants.h"
#include "maps_constant_data.h"
#include <commdlg.h>

inline extern FileType selected_file_type = FileType::NONE;
inline extern FFNA_ModelFile selected_ffna_model_file{};
inline extern FFNA_MapFile selected_ffna_map_file{};
inline extern SelectedDatTexture selected_dat_texture{};

inline extern std::vector<FileData> selected_map_files{};

inline bool is_parsing_data = false;
inline bool items_to_parse = 0;
inline bool items_parsed = 0;

const char* type_strings[26] = {
  " ",        "AMAT",     "Amp",      "ATEXDXT1", "ATEXDXT2",     "ATEXDXT3",   "ATEXDXT4",
  "ATEXDXT5", "ATEXDXTN", "ATEXDXTA", "ATEXDXTL", "ATTXDXT1",     "ATTXDXT3",   "ATTXDXT5",
  "ATTXDXTN", "ATTXDXTA", "ATTXDXTL", "DDS",      "FFNA - Model", "FFNA - Map", "FFNA - Unknown",
  "MFTBase",  "NOT_READ", "Sound",    "Text",     "Unknown"};

const ImGuiTableSortSpecs* DatBrowserItem::s_current_sort_specs = NULL;

void apply_filter(const std::vector<int>& new_filter, std::unordered_set<int>& intersection)
{
    if (intersection.empty())
    {
        intersection.insert(new_filter.begin(), new_filter.end());
    }
    else
    {
        std::unordered_set<int> new_intersection;
        for (int id : new_filter)
        {
            if (intersection.count(id))
            {
                new_intersection.insert(id);
            }
        }
        intersection = std::move(new_intersection);
    }
}

void parse_file(DATManager& dat_manager, int index, MapRenderer* map_renderer,
                std::unordered_map<int, std::vector<int>>& hash_index, std::vector<DatBrowserItem>& items)
{
    const auto MFT = dat_manager.get_MFT();
    if (index >= MFT.size())
        return;

    const auto* entry = &MFT[index];
    if (! entry)
        return;

    selected_file_type = static_cast<FileType>(entry->type);
    std::unique_ptr<Terrain> terrain;

    switch (entry->type)
    {
    case ATEXDXT1:
    case ATEXDXT2:
    case ATEXDXT3:
    case ATEXDXT4:
    case ATEXDXT5:
    case ATEXDXTN:
    //case ATEXDXTA: Cannot parse this
    case ATEXDXTL:
    case ATTXDXT1:
    case ATTXDXT3:
    case ATTXDXT5:
    case ATTXDXTN:
    //case ATTXDXTA: Cannot parse this
    case ATTXDXTL:
    {
        selected_dat_texture.dat_texture = dat_manager.parse_ffna_texture_file(index);
        if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
        {
            map_renderer->GetTextureManager()->CreateTextureFromRGBA(
              selected_dat_texture.dat_texture.width, selected_dat_texture.dat_texture.height,
              selected_dat_texture.dat_texture.rgba_data.data(), &selected_dat_texture.texture_id,
              entry->Hash);
        }
    }
    break;
    case DDS:
    {
        const std::vector<uint8_t> ddsData = dat_manager.parse_dds_file(index);
        size_t ddsDataSize = ddsData.size();
        HRESULT hr = map_renderer->GetTextureManager()->CreateTextureFromDDSInMemory(
          ddsData.data(), ddsDataSize, &selected_dat_texture.texture_id,
          &selected_dat_texture.dat_texture.width, &selected_dat_texture.dat_texture.height,
          selected_dat_texture.dat_texture.rgba_data, entry->Hash); // Pass the RGBA vector
        if (FAILED(hr))
        {
            // Handle the error
        }
    }
    break;
        break;
    case FFNA_Type2:
        selected_ffna_model_file = dat_manager.parse_ffna_model_file(index);
        if (selected_ffna_model_file.parsed_correctly)
        {
            map_renderer->UnsetTerrain();

            std::vector<Mesh> prop_meshes;
            float overallMinX = FLT_MAX, overallMinY = FLT_MAX, overallMinZ = FLT_MAX;
            float overallMaxX = FLT_MIN, overallMaxY = FLT_MIN, overallMaxZ = FLT_MIN;

            auto& models = selected_ffna_model_file.geometry_chunk.models;
            for (int i = 0; i < models.size(); i++)
            {
                Mesh prop_mesh = selected_ffna_model_file.GetMesh(i);

                overallMinX = std::min(overallMinX, models[i].minX);
                overallMinY = std::min(overallMinY, models[i].minY);
                overallMinZ = std::min(overallMinZ, models[i].minZ);

                overallMaxX = std::max(overallMaxX, models[i].maxX);
                overallMaxY = std::max(overallMaxY, models[i].maxY);
                overallMaxZ = std::max(overallMaxZ, models[i].maxZ);

                if ((prop_mesh.indices.size() % 3) == 0)
                {
                    prop_meshes.push_back(prop_mesh);
                }
            }

            // Load textures
            std::vector<int> texture_ids;
            std::vector<DatTexture> model_dat_textures;
            for (int j = 0; j < selected_ffna_model_file.texture_filenames_chunk.texture_filenames.size();
                 j++)
            {
                auto texture_filename = selected_ffna_model_file.texture_filenames_chunk.texture_filenames[j];
                auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);

                int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(decoded_filename);
                if (texture_id >= 0)
                {
                    texture_ids.push_back(texture_id);
                    continue;
                }

                auto mft_entry_it = hash_index.find(decoded_filename);
                if (mft_entry_it != hash_index.end())
                {
                    auto dat_texture = dat_manager.parse_ffna_texture_file(mft_entry_it->second.at(0));
                    model_dat_textures.push_back(dat_texture);

                    // Create texture
                    auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                      dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(), &texture_id,
                      decoded_filename);
                    texture_ids.push_back(texture_id);
                }
            }

            selected_dat_texture.dat_texture =
              map_renderer->GetTextureManager()->BuildTextureAtlas(model_dat_textures, -1, -1);

            if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
            {
                map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                  selected_dat_texture.dat_texture.width, selected_dat_texture.dat_texture.height,
                  selected_dat_texture.dat_texture.rgba_data.data(), &selected_dat_texture.texture_id,
                  entry->Hash);
            }

            // The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
            // So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
            std::vector<std::vector<int>> per_mesh_tex_ids(prop_meshes.size());
            for (int i = 0; i < prop_meshes.size(); i++)
            {
                std::vector<uint8_t> mesh_tex_indices;
                for (int j = 0; j < prop_meshes[i].tex_indices.size(); j++)
                {
                    int tex_index = prop_meshes[i].tex_indices[j];
                    per_mesh_tex_ids[i].push_back(texture_ids[tex_index]);

                    mesh_tex_indices.push_back(j);
                }

                prop_meshes[i].tex_indices = mesh_tex_indices;
            }

            // Create the PerObjectCB for each submodel
            std::vector<PerObjectCB> per_object_cbs;
            per_object_cbs.resize(prop_meshes.size());
            for (int i = 0; i < per_object_cbs.size(); i++)
            {
                float modelWidth = overallMaxX - overallMinX;
                float modelHeight = overallMaxY - overallMinY;
                float modelDepth = overallMaxZ - overallMinZ;

                float maxDimension = std::max({modelWidth, modelHeight, modelDepth});

                float boundingBoxSize = 3000.0f;
                float scale = boundingBoxSize / maxDimension;

                float centerX = overallMinX + modelWidth * 0.5f;
                float centerY = overallMinY + modelHeight * 0.5f;
                float centerZ = overallMinZ + modelDepth * 0.5f;

                DirectX::XMMATRIX scaling_matrix = DirectX::XMMatrixScaling(scale, scale, scale);
                DirectX::XMMATRIX translation_matrix =
                  DirectX::XMMatrixTranslation(-centerX * scale, -centerY * scale, -centerZ * scale);
                DirectX::XMMATRIX world_matrix = scaling_matrix * translation_matrix;

                DirectX::XMStoreFloat4x4(&per_object_cbs[i].world, world_matrix);

                auto& prop_mesh = prop_meshes[i];
                if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
                    prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
                {
                    return; // Failed, maybe throw here on handle error.
                }

                per_object_cbs[i].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();

                for (int j = 0; j < prop_mesh.uv_coord_indices.size(); j++)
                {
                    int index0 = j / 4;
                    int index1 = j % 4;

                    per_object_cbs[i].uv_indices[index0][index1] = (uint32_t)prop_mesh.uv_coord_indices[j];
                    per_object_cbs[i].texture_indices[index0][index1] = (uint32_t)prop_mesh.tex_indices[j];
                    per_object_cbs[i].blend_flags[index0][index1] = (uint32_t)prop_mesh.blend_flags[j];
                }
            }

            auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, index);
            for (int i = 0; i < mesh_ids.size(); i++)
            {
                int mesh_id = mesh_ids[i];
                auto& mesh_texture_ids = per_mesh_tex_ids[i];

                map_renderer->GetMeshManager()->SetTexturesForMesh(
                  mesh_id, map_renderer->GetTextureManager()->GetTextures(mesh_texture_ids), 0);
            }
        }

        break;
    case FFNA_Type3:
        selected_map_files.clear();
        selected_ffna_map_file = dat_manager.parse_ffna_map_file(index);

        if (selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() > 0 &&
            selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() ==
              selected_ffna_map_file.terrain_chunk.terrain_x_dims *
                selected_ffna_map_file.terrain_chunk.terrain_y_dims)
        {
            auto& terrain_texture_filenames = selected_ffna_map_file.terrain_texture_filenames.array;
            std::vector<DatTexture> terrain_dat_textures;
            for (int i = 0; i < terrain_texture_filenames.size(); i++)
            {
                auto decoded_filename =
                  decode_filename(selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id0,
                                  selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id1);
                auto mft_entry_it = hash_index.find(decoded_filename);
                if (mft_entry_it != hash_index.end())
                {
                    const DatTexture dat_texture =
                      dat_manager.parse_ffna_texture_file(mft_entry_it->second.at(0));
                    terrain_dat_textures.push_back(dat_texture);
                }
            }

            selected_dat_texture.dat_texture =
              map_renderer->GetTextureManager()->BuildTextureAtlas(terrain_dat_textures, 8, 8);

            if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
            {
                map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                  selected_dat_texture.dat_texture.width, selected_dat_texture.dat_texture.height,
                  selected_dat_texture.dat_texture.rgba_data.data(), &selected_dat_texture.texture_id,
                  entry->Hash);
            }

            auto& terrain_texture_indices =
              selected_ffna_map_file.terrain_chunk.terrain_texture_indices_maybe;

            auto& terrain_texture_blend_weights =
              selected_ffna_map_file.terrain_chunk.terrain_texture_blend_weights_maybe;

            // Create terrain
            terrain = std::make_unique<Terrain>(selected_ffna_map_file.terrain_chunk.terrain_x_dims,
                                                selected_ffna_map_file.terrain_chunk.terrain_y_dims,
                                                selected_ffna_map_file.terrain_chunk.terrain_heightmap,
                                                terrain_texture_indices, terrain_texture_blend_weights,
                                                selected_ffna_map_file.map_info_chunk.map_bounds);
            map_renderer->SetTerrain(std::move(terrain), selected_dat_texture.texture_id);
        }

        // Load models
        for (int i = 0; i < selected_ffna_map_file.prop_filenames_chunk.array.size(); i++)
        {
            auto decoded_filename =
              decode_filename(selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id0,
                              selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id1);
            auto mft_entry_it = hash_index.find(decoded_filename);
            if (mft_entry_it != hash_index.end())
            {
                auto type = items[mft_entry_it->second.at(0)].type;
                if (type == FFNA_Type2)
                {
                    selected_map_files.emplace_back(
                      dat_manager.parse_ffna_model_file(mft_entry_it->second.at(0)));
                }
            }
        }

        // Load models
        for (int i = 0; i < selected_ffna_map_file.more_filnames_chunk.array.size(); i++)
        {
            auto decoded_filename =
              decode_filename(selected_ffna_map_file.more_filnames_chunk.array[i].filename.id0,
                              selected_ffna_map_file.more_filnames_chunk.array[i].filename.id1);
            auto mft_entry_it = hash_index.find(decoded_filename);
            if (mft_entry_it != hash_index.end())
            {
                auto type = items[mft_entry_it->second.at(0)].type;
                if (type == FFNA_Type2)
                {
                    auto map_model = dat_manager.parse_ffna_model_file(mft_entry_it->second.at(0));
                    selected_map_files.emplace_back(map_model);
                }
            }
        }

        for (int i = 0; i < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size(); i++)
        {
            PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[i];

            if (prop_info.filename_index < selected_map_files.size())
            {
                if (auto ffna_model_file_ptr =
                      std::get_if<FFNA_ModelFile>(&selected_map_files[prop_info.filename_index]))
                {
                    if (ffna_model_file_ptr->parsed_correctly)
                    {
                        // Load geometry
                        std::vector<Mesh> prop_meshes;
                        for (int j = 0; j < ffna_model_file_ptr->geometry_chunk.models.size(); j++)
                        {
                            Mesh prop_mesh = ffna_model_file_ptr->GetMesh(j);
                            if ((prop_mesh.indices.size() % 3) == 0)
                            {
                                prop_meshes.push_back(prop_mesh);
                            }
                        }

                        // Load textures
                        std::vector<int> texture_ids;
                        for (int j = 0;
                             j < ffna_model_file_ptr->texture_filenames_chunk.texture_filenames.size(); j++)
                        {
                            auto texture_filename =
                              ffna_model_file_ptr->texture_filenames_chunk.texture_filenames[j];
                            auto decoded_filename =
                              decode_filename(texture_filename.id0, texture_filename.id1);

                            int texture_id =
                              map_renderer->GetTextureManager()->GetTextureIdByHash(decoded_filename);
                            if (texture_id >= 0)
                            {
                                texture_ids.push_back(texture_id);
                                continue;
                            }

                            auto mft_entry_it = hash_index.find(decoded_filename);
                            if (mft_entry_it != hash_index.end())
                            {
                                // Get texture from .dat
                                auto dat_texture =
                                  dat_manager.parse_ffna_texture_file(mft_entry_it->second.at(0));

                                // Create texture
                                auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                                  dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
                                  &texture_id, decoded_filename);
                                texture_ids.push_back(texture_id);
                            }
                        }

                        // The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
                        // So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
                        std::vector<std::vector<int>> per_mesh_tex_ids(prop_meshes.size());
                        for (int i = 0; i < prop_meshes.size(); i++)
                        {
                            std::vector<uint8_t> mesh_tex_indices;
                            for (int j = 0; j < prop_meshes[i].tex_indices.size(); j++)
                            {
                                int tex_index = prop_meshes[i].tex_indices[j];
                                per_mesh_tex_ids[i].push_back(texture_ids[tex_index]);

                                mesh_tex_indices.push_back(j);
                            }

                            prop_meshes[i].tex_indices = mesh_tex_indices;
                        }

                        std::vector<PerObjectCB> per_object_cbs;
                        per_object_cbs.resize(prop_meshes.size());
                        for (int j = 0; j < per_object_cbs.size(); j++)
                        {
                            DirectX::XMFLOAT3 translation(prop_info.x, prop_info.y, prop_info.z);
                            float sin_angle = prop_info.sin_angle;
                            float cos_angle = prop_info.cos_angle;
                            float rotation_angle = std::atan2(
                              sin_angle,
                              cos_angle); // Calculate the rotation angle from the sine and cosine values

                            float scale = prop_info.scaling_factor;

                            DirectX::XMMATRIX scaling_matrix = DirectX::XMMatrixScaling(scale, scale, scale);
                            DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixRotationY(rotation_angle);
                            DirectX::XMMATRIX translation_matrix =
                              DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&translation));

                            DirectX::XMMATRIX transform_matrix = DirectX::XMMatrixMultiply(
                              scaling_matrix, DirectX::XMMatrixMultiply(rotation_matrix, translation_matrix));

                            DirectX::XMStoreFloat4x4(&per_object_cbs[j].world, transform_matrix);

                            auto& prop_mesh = prop_meshes[j];
                            if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
                                prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
                            {
                                return; // Failed, maybe throw here on handle error.
                            }

                            per_object_cbs[j].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();

                            for (int k = 0; k < prop_mesh.uv_coord_indices.size(); k++)
                            {
                                int index0 = k / 4;
                                int index1 = k % 4;

                                per_object_cbs[j].uv_indices[index0][index1] =
                                  (uint32_t)prop_mesh.uv_coord_indices[k];
                                per_object_cbs[j].texture_indices[index0][index1] =
                                  (uint32_t)prop_mesh.tex_indices[k];
                                per_object_cbs[j].blend_flags[index0][index1] =
                                  (uint32_t)prop_mesh.blend_flags[k];
                            }
                        }

                        auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, i);
                        for (int i = 0; i < mesh_ids.size(); i++)
                        {
                            int mesh_id = mesh_ids[i];
                            auto& mesh_texture_ids = per_mesh_tex_ids[i];

                            map_renderer->GetMeshManager()->SetTexturesForMesh(
                              mesh_id, map_renderer->GetTextureManager()->GetTextures(mesh_texture_ids), 0);
                        }
                    }
                }
            }
        }

        break;
    default:
        break;
    }
}

std::string truncate_text_with_ellipsis(const std::string& text, float maxWidth);
int custom_stoi(const std::string& input);
std::string to_lower(const std::string& input);
std::wstring OpenFileDialog(std::wstring filename = L"");

void draw_data_browser(DATManager& dat_manager, MapRenderer* map_renderer)
{
    static std::vector<DatBrowserItem> items;
    static std::vector<DatBrowserItem> filtered_items;

    static std::unordered_map<int, std::vector<int>> id_index;
    static std::unordered_map<int, std::vector<int>> hash_index;
    static std::unordered_map<FileType, std::vector<int>> type_index;

    static std::unordered_map<int, std::vector<int>> map_id_index;
    static std::unordered_map<std::string, std::vector<int>> name_index;
    static std::unordered_map<bool, std::vector<int>> pvp_index;

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
    if (items.size() == 0)
    {
        const auto& entries = dat_manager.get_MFT();
        for (int i = 0; i < entries.size(); i++)
        {
            const auto& entry = entries[i];
            DatBrowserItem new_item{
              i, entry.Hash, (FileType)entry.type, entry.Size, entry.uncompressedSize, {}, {}, {}};
            if (entry.Hash != 0 && entry.type == FFNA_Type3)
            {
                auto it = constant_maps_info.find(entry.Hash);
                if (it != constant_maps_info.end())
                {
                    for (const auto& map : it->second)
                    {
                        new_item.map_ids.push_back(map.map_id);
                        new_item.names.push_back(map.map_name);
                        new_item.is_pvp.push_back(map.is_pvp);
                    }
                }
            }

            items.push_back(new_item);
        }
        filtered_items = items;
    }

    if (items.size() != 0 && id_index.empty())
    {
        for (int i = 0; i < items.size(); i++)
        {
            const auto& item = items[i];
            id_index[item.id].push_back(i);
            hash_index[item.hash].push_back(i);
            type_index[item.type].push_back(i);
            for (const auto map_id : item.map_ids)
            {
                map_id_index[map_id].push_back(i);
            }
            for (const auto& name : item.names)
            {
                if (name != "" && name != "-")
                    name_index[name].push_back(i);
            }
            for (const auto is_pvp : item.is_pvp)
            {
                pvp_index[is_pvp].push_back(i);
            }
        }
    }

    // Set after filtering is complete.
    static std::string curr_id_filter = "";
    static std::string curr_hash_filter = "";
    static FileType curr_type_filter = NONE;
    static std::string curr_map_id_filter = "";
    static std::string curr_name_filter = "";
    static int curr_pvp_filter = -1;

    // The values set by the user in the GUI
    static std::string id_filter_text;
    static std::string hash_filter_text;
    static FileType type_filter_value = NONE;
    static std::string map_id_filter_text;
    static std::string name_filter_text;
    static int pvp_filter_value = -1; // -1 means no filter, 0 means false, 1 means true

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

    if (curr_map_id_filter != map_id_filter_text)
    {
        curr_map_id_filter = map_id_filter_text;
        filter_update_required = true;
    }

    if (curr_name_filter != name_filter_text)
    {
        curr_name_filter = name_filter_text;
        filter_update_required = true;
    }

    if (curr_pvp_filter != pvp_filter_value)
    {
        curr_pvp_filter = pvp_filter_value;
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

        if (! map_id_filter_text.empty())
        {
            int map_id_filter_value = custom_stoi(map_id_filter_text);
            if (map_id_filter_value >= 0 && map_id_index.count(map_id_filter_value))
            {
                apply_filter(map_id_index[map_id_filter_value], intersection);
            }
        }

        if (! name_filter_text.empty())
        {
            std::vector<int> matching_indices;
            std::string name_filter_text_lower = to_lower(name_filter_text);

            for (const auto& name_entry : name_index)
            {
                std::string name_entry_lower = to_lower(name_entry.first);
                if (name_entry_lower.find(name_filter_text_lower) != std::string::npos)
                {
                    matching_indices.insert(matching_indices.end(), name_entry.second.begin(),
                                            name_entry.second.end());
                }
            }
            if (! matching_indices.empty())
            {
                apply_filter(matching_indices, intersection);
            }
        }

        if (pvp_filter_value != -1)
        {
            bool pvp_filter_bool = pvp_filter_value == 1;
            apply_filter(pvp_index[pvp_filter_bool], intersection);
        }

        if (id_filter_text.empty() && hash_filter_text.empty() && type_filter_value == NONE &&
            map_id_filter_text.empty() && name_filter_text.empty() && pvp_filter_value == -1)
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
        curr_map_id_filter = map_id_filter_text;
        curr_name_filter = name_filter_text;
        curr_pvp_filter = pvp_filter_value;
    }

    // Filter table
    // Render the filter inputs and the table
    ImGui::Columns(5);
    ImGui::Text("Id:");
    ImGui::SameLine();
    ImGui::InputText("##IdFilter", &id_filter_text);
    ImGui::NextColumn();

    ImGui::Text("Hash:");
    ImGui::SameLine();
    ImGui::InputText("##HashFilter", &hash_filter_text);
    ImGui::NextColumn();

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::InputText("##NameFilter", &name_filter_text);
    ImGui::NextColumn();

    ImGui::Text("Map ID:");
    ImGui::SameLine();
    ImGui::InputText("##MapID", &map_id_filter_text);
    ImGui::NextColumn();

    ImGui::Text("Type:");
    ImGui::SameLine();
    ImGui::Combo("##EnumFilter", reinterpret_cast<int*>(&type_filter_value), type_strings, 25);
    ImGui::Columns(1);

    ImGui::Separator();

    ImGui::Text("Filtered items: %d", filtered_items.size());
    ImGui::SameLine();
    ImGui::Text("Total items: %d", items.size());

    // Options
    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("data browser", 8, flags))
    {
        // Declare columns
        // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
        // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
        // Demonstrate using a mixture of flags among available sort-related flags:
        // - ImGuiTableColumnFlags_DefaultSort
        // - ImGuiTableColumnFlags_NoSort / ImGuiTableColumnFlags_NoSortAscending / ImGuiTableColumnFlags_NoSortDescending
        // - ImGuiTableColumnFlags_PreferSortAscending / ImGuiTableColumnFlags_PreferSortDescending
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, DatBrowserItemColumnID_id);
        ImGui::TableSetupColumn("Name", 0, 0.0f, DatBrowserItemColumnID_name);
        ImGui::TableSetupColumn("Hash", 0, 0.0f, DatBrowserItemColumnID_hash);
        ImGui::TableSetupColumn("Type", 0, 0.0f, DatBrowserItemColumnID_type);
        ImGui::TableSetupColumn("Size", 0, 0.0f, DatBrowserItemColumnID_size);
        ImGui::TableSetupColumn("Decompressed size", 0, 0.0f, DatBrowserItemColumnID_decompressed_size);
        ImGui::TableSetupColumn("Map id", 0, 0.0f, DatBrowserItemColumnID_map_id);
        ImGui::TableSetupColumn("PvP", 0, 0.0f, DatBrowserItemColumnID_is_pvp);
        ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
            if (sorts_specs->SpecsDirty)
            {
                DatBrowserItem::s_current_sort_specs =
                  sorts_specs; // Store in variable accessible by the sort function.
                if (filtered_items.size() > 1)
                    qsort(&filtered_items[0], (size_t)filtered_items.size(), sizeof(filtered_items[0]),
                          DatBrowserItem::CompareWithSortSpecs);
                DatBrowserItem::s_current_sort_specs = NULL;
                sorts_specs->SpecsDirty = false;
            }

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(filtered_items.size());

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
                        parse_file(dat_manager, item.id, map_renderer, hash_index, items);
                    }
                }

                //Add context menu on right clicking item in table
                if (ImGui::BeginPopupContextItem("ItemContextMenu"))
                {
                    if (ImGui::MenuItem("Save decompressed data to file"))
                    {
                        std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", item.hash));
                        if (! savePath.empty())
                        {
                            dat_manager.save_raw_decompressed_data_to_file(item.id, savePath);
                        }
                    }

                    ImGui::EndPopup();
                }

                ImGui::TableNextColumn();
                if (item.type == FFNA_Type3)
                {
                    std::string name;
                    for (int i = 0; i < item.names.size(); i++)
                    {
                        name += item.names[i];
                        if (i < item.names.size() - 1)
                        {
                            name += " | ";
                        }
                    }

                    // Check if the text would be clipped
                    ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
                    float availableWidth = ImGui::GetContentRegionAvail().x;

                    if (textSize.x > availableWidth)
                    {
                        // Truncate the text to fit the available width
                        std::string truncatedName = truncate_text_with_ellipsis(name, availableWidth);

                        // Display the truncated text
                        ImGui::TextUnformatted(truncatedName.c_str());

                        // Check if the mouse is hovering over the text
                        if (ImGui::IsItemHovered())
                        {
                            // Show a tooltip with the full list of names
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(name.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    else
                    {
                        // Display the full text without truncation
                        ImGui::TextUnformatted(name.c_str());
                    }
                }
                else
                {
                    ImGui::Text("-");
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
                ImGui::TableNextColumn();
                if (item.type == FFNA_Type3)
                {
                    std::string map_ids_text;
                    for (int i = 0; i < item.map_ids.size(); i++)
                    {
                        map_ids_text += std::format("{}", item.map_ids[i]);
                        if (i < item.map_ids.size() - 1)
                        {
                            map_ids_text += ",";
                        }
                    }

                    // Check if the text would be clipped
                    ImVec2 textSize = ImGui::CalcTextSize(map_ids_text.c_str());
                    float availableWidth = ImGui::GetContentRegionAvail().x;

                    if (textSize.x > availableWidth)
                    {
                        // Truncate the text to fit the available width
                        std::string truncatedMapIdsText =
                          truncate_text_with_ellipsis(map_ids_text, availableWidth);

                        // Display the truncated text
                        ImGui::TextUnformatted(truncatedMapIdsText.c_str());

                        // Check if the mouse is hovering over the text
                        if (ImGui::IsItemHovered())
                        {
                            // Show a tooltip with the full list of map_ids
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(map_ids_text.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    else
                    {
                        // Display the full text without truncation
                        ImGui::TextUnformatted(map_ids_text.c_str());
                    }
                }
                else
                {
                    ImGui::Text("-");
                }
                ImGui::TableNextColumn();

                // Display the checkboxes only if there is enough room for all of them.
                // If there is not enough room, display a single checkbox if all checkboxes share the same value (either all true or all false), or display '...' otherwise.
                if (item.type == FFNA_Type3 && item.is_pvp.size() > 0)
                {
                    ImVec2 checkboxSize = ImGui::CalcTextSize("[ ]");
                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float requiredWidth = checkboxSize.x * item.is_pvp.size() +
                      (item.is_pvp.size() - 1) * ImGui::GetStyle().ItemSpacing.x;

                    bool allTrue =
                      std::all_of(item.is_pvp.begin(), item.is_pvp.end(), [](int v) { return v >= 1; });
                    bool allFalse =
                      std::all_of(item.is_pvp.begin(), item.is_pvp.end(), [](int v) { return v < 1; });

                    if (allTrue || allFalse)
                    {
                        ImGui::BeginDisabled(); // Disable the checkbox to make it non-editable
                        ImGui::Checkbox("##IsPvp",
                                        (bool*)&allTrue); // Show a single checkbox with the respective value
                        ImGui::EndDisabled();
                    }
                    else if (requiredWidth > availableWidth)
                    {
                        ImGui::TextUnformatted("..."); // Not enough space and mixed values, show '...'
                    }
                    else
                    {
                        for (int i = 0; i < item.is_pvp.size(); i++)
                        {
                            ImGui::PushID(i);
                            ImGui::BeginDisabled(); // Disable the checkbox to make it non-editable
                            ImGui::Checkbox(("##IsPvp" + std::to_string(i)).c_str(), (bool*)&item.is_pvp[i]);
                            ImGui::EndDisabled();
                            ImGui::PopID();

                            if (i < item.is_pvp.size() - 1)
                            {
                                ImGui::SameLine();
                            }
                        }
                    }
                }

                else
                {
                    ImGui::Text("-");
                }
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
        case DatBrowserItemColumnID_map_id: // Modified map_id case
            for (size_t i = 0; i < std::min(a->map_ids.size(), b->map_ids.size()); ++i)
            {
                delta = (a->map_ids[i] - b->map_ids[i]);
                if (delta != 0)
                    break;
            }
            if (delta == 0)
                delta = (a->map_ids.size() - b->map_ids.size());
            break;

        case DatBrowserItemColumnID_name: // Modified name case
            for (size_t i = 0; i < std::min(a->names.size(), b->names.size()); ++i)
            {
                delta = a->names[i].compare(b->names[i]);
                if (delta != 0)
                    break;
            }
            if (delta == 0)
                delta = (a->names.size() - b->names.size());
            break;

        case DatBrowserItemColumnID_is_pvp: // Modified is_pvp case
            for (size_t i = 0; i < std::min(a->is_pvp.size(), b->is_pvp.size()); ++i)
            {
                delta = (a->is_pvp[i] - b->is_pvp[i]);
                if (delta != 0)
                    break;
            }
            if (delta == 0)
                delta = (a->is_pvp.size() - b->is_pvp.size());
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

    return (a->id - b->id);
}

std::string truncate_text_with_ellipsis(const std::string& text, float maxWidth)
{
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    if (textSize.x <= maxWidth)
    {
        return text;
    }

    std::string truncatedText;
    float ellipsisWidth = ImGui::CalcTextSize("...").x;

    for (size_t i = 0; i < text.size(); ++i)
    {
        textSize = ImGui::CalcTextSize((truncatedText + text[i] + "...").c_str());
        if (textSize.x <= maxWidth)
        {
            truncatedText += text[i];
        }
        else
        {
            break;
        }
    }

    truncatedText += "...";
    return truncatedText;
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

std::string to_lower(const std::string& input)
{
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

inline std::wstring OpenFileDialog(std::wstring filename)
{
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH];
    wcsncpy(fileName, filename.c_str(), MAX_PATH);
    fileName[MAX_PATH - 1] = L'\0'; // Ensure null termination
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"GWRAW Files (*.gwraw)\0*.gwraw\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"gwraw";

    if (GetSaveFileName(&ofn))
    {
        std::wstring wFileName(fileName);
        return wFileName;
    }

    return L"";
}
