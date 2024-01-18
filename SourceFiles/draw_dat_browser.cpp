#include "pch.h"
#include "draw_dat_browser.h"

#include <codecvt>

#include "GuiGlobalConstants.h"
#include "maps_constant_data.h"
#include <numeric>

#include <model_exporter.h>

#include "map_exporter.h"
#include "writeHeighMapBMP.h"
#include "writeOBJ.h"

// BASS
extern LPFNBASSSTREAMCREATEFILE lpfnBassStreamCreateFile;
extern LPFNBASSCHANNELBYTES2SECONDS lpfnBassChannelBytes2Seconds;
extern LPFNBASSCHANNELGETLENGTH lpfnBassChannelGetLength;
extern LPFNBASSSTREAMGETFILEPOSITION lpfnBassStreamGetFilePosition;
extern LPFNBASSCHANNELGETINFO lpfnBassChannelGetInfo;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop;
extern LPFNBASSSTREAMFREE lpfnBassStreamFree;
extern LPFNBASSCHANNELFLAGS lpfnBassChannelFlags;
extern LPFNBASSCHANNELSETATTRIBUTE lpfnBassChannelSetAttribute;

// BASS_FX
extern LPFNBASSFXTMPOCREATE lpfnBassFxTempoCreate;

extern bool repeat_audio;
extern float playback_speed;
extern float volume_level;

extern std::string selected_text_file_str = "";

inline int selected_map_file_index = -1;

inline extern uint32_t selected_item_hash = -1;
inline extern uint32_t selected_item_murmurhash3 = -1;
inline int last_focused_item_index = -1;

inline extern FileType selected_file_type = NONE;
inline extern FFNA_ModelFile selected_ffna_model_file{};
inline extern FFNA_MapFile selected_ffna_map_file{};
inline extern SelectedDatTexture selected_dat_texture{};
inline extern std::vector<uint8_t> selected_raw_data{};

inline extern std::unordered_map<uint32_t, uint32_t> object_id_to_prop_index{};
inline extern std::unordered_map<uint32_t, uint32_t> object_id_to_submodel_index{};
inline extern std::unordered_map<uint32_t, uint32_t> prop_index_to_selected_map_files_index{};

inline extern HSTREAM selected_audio_stream_handle{};
inline extern std::string audio_info = "";

inline extern std::vector<FileData> selected_map_files{};

inline bool is_parsing_data = false;
inline bool items_to_parse = false;
inline bool items_parsed = false;

inline std::unordered_map<int, TextureType> model_texture_types;

std::unique_ptr<Terrain> terrain;
std::vector<Mesh> prop_meshes;

const ImGuiTableSortSpecs* DatBrowserItem::s_current_sort_specs = nullptr;

void apply_filter(const std::vector<int>& new_filter, std::unordered_set<int>& intersection)
{
    if (intersection.empty()) { intersection.insert(new_filter.begin(), new_filter.end()); }
    else
    {
        std::unordered_set<int> new_intersection;
        for (int id : new_filter) { if (intersection.contains(id)) { new_intersection.insert(id); } }
        intersection = std::move(new_intersection);
    }
}

bool parse_file(DATManager* dat_manager, int index, MapRenderer* map_renderer,
    std::unordered_map<int, std::vector<int>>& hash_index)
{
    bool success = false;

    const auto MFT = dat_manager->get_MFT();
    if (index >= MFT.size())
        return false;

    const auto* entry = &MFT[index];
    if (!entry)
        return false;

    selected_file_type = static_cast<FileType>(entry->type);

    if (selected_audio_stream_handle != 0)
    {
        lpfnBassChannelStop(selected_audio_stream_handle);
        lpfnBassStreamFree(selected_audio_stream_handle);
    }

    unsigned char* raw_data = dat_manager->read_file(index);
    selected_raw_data = std::vector<uint8_t>(raw_data, raw_data + entry->uncompressedSize);
    delete raw_data;

    if (entry->type != FFNA_Type3)
    {
        // If we select a map with index 123. Then select a model we would not be able to go back to map 123 unless we did this.
        selected_map_file_index = -1;
    }

    switch (entry->type)
    {
    case TEXT:
    {
        std::string text_str(reinterpret_cast<char*>(selected_raw_data.data()));
        selected_text_file_str = text_str;
        success = true;
    }
    break;
    case SOUND:
    case AMP:
    {
        if (selected_raw_data.size() > 0)
        {
            // create the original stream
            HSTREAM orig_stream = lpfnBassStreamCreateFile(TRUE, // mem
                selected_raw_data.data(), // file
                0, // offset
                entry->uncompressedSize, // length
                BASS_STREAM_PRESCAN | BASS_STREAM_DECODE); // flags

            // create the tempo stream from the original stream
            selected_audio_stream_handle = lpfnBassFxTempoCreate(orig_stream, BASS_FX_FREESOURCE);

            float time = lpfnBassChannelBytes2Seconds(
                selected_audio_stream_handle,
                lpfnBassChannelGetLength(selected_audio_stream_handle,
                    BASS_POS_BYTE)); // playback duration
            DWORD len =
                lpfnBassStreamGetFilePosition(selected_audio_stream_handle, BASS_FILEPOS_END); // file length
            DWORD bitrate = static_cast<DWORD>(len / (125 * time) + 0.5); // bitrate (Kbps)

            BASS_CHANNELINFO info;
            lpfnBassChannelGetInfo(selected_audio_stream_handle, &info);

            audio_info = "Bitrate: " + std::to_string(bitrate) +
                "\nFrequency: " + std::to_string(info.freq / 1000) + " kHz" +
                "\nChannels: " + (info.chans == 1 ? "mono" : "Stereo") +
                "\nFormat: " + (info.ctype == BASS_CTYPE_STREAM_MP3 ? "mp3" : "unknown");

            if (repeat_audio)
            {
                // Turn on looping
                lpfnBassChannelFlags(selected_audio_stream_handle, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
            }

            // Adjust the tempo
            lpfnBassChannelSetAttribute(selected_audio_stream_handle, BASS_ATTRIB_TEMPO,
                (playback_speed - 1.0f) * 100.0f);

            // Set audio volume level
            lpfnBassChannelSetAttribute(selected_audio_stream_handle, BASS_ATTRIB_VOL, volume_level);

            lpfnBassChannelPlay(selected_audio_stream_handle, TRUE);

            success = true;
        }
    }

    break;
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
        selected_dat_texture.dat_texture = dat_manager->parse_ffna_texture_file(index);
        selected_dat_texture.file_id = entry->Hash;
        if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
        {
            map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                selected_dat_texture.dat_texture.width,
                selected_dat_texture.dat_texture.height,
                selected_dat_texture.dat_texture.rgba_data.
                data(), &selected_dat_texture.texture_id,
                entry->Hash);

            success = true;
        }
    }
    break;
    case DDS:
    {
        selected_dat_texture.file_id = entry->Hash;
        const std::vector<uint8_t> ddsData = dat_manager->parse_dds_file(index);
        size_t ddsDataSize = ddsData.size();
        HRESULT hr = map_renderer->GetTextureManager()->CreateTextureFromDDSInMemory(
            ddsData.data(), ddsDataSize, &selected_dat_texture.texture_id,
            &selected_dat_texture.dat_texture.width, &selected_dat_texture.dat_texture.height,
            selected_dat_texture.dat_texture.rgba_data, entry->Hash); // Pass the RGBA vector
        if (FAILED(hr))
        {
            // Handle the error
        }
        else {
            success = true;
        }
    }
    break;
    break;
    case FFNA_Type2:
        // Clear up some GPU memory (especially important for GPUs with little VRAM)
        map_renderer->GetTextureManager()->Clear();
        map_renderer->ClearProps();

        selected_ffna_model_file = dat_manager->parse_ffna_model_file(index);
        if (selected_ffna_model_file.parsed_correctly)
        {
            map_renderer->UnsetTerrain();
            prop_meshes.clear();

            float overallMinX = FLT_MAX, overallMinY = FLT_MAX, overallMinZ = FLT_MAX;
            float overallMaxX = FLT_MIN, overallMaxY = FLT_MIN, overallMaxZ = FLT_MIN;

            const auto& geometry_chunk = selected_ffna_model_file.geometry_chunk;
            auto& models = selected_ffna_model_file.geometry_chunk.models;

            std::vector<int> sort_orders;
            for (int i = 0; i < models.size(); i++)
            {
                AMAT_file amat_file;
                if (selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames.size() > 0) {
                    int sub_model_index = models[i].unknown;
                    if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
                    {
                        sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
                    }
                    const auto uts1 = geometry_chunk.uts1[sub_model_index % geometry_chunk.uts1.size()];

                    const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames.size();
                    const auto amat_filename = selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames[amat_file_index];

                    const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


                    auto mft_entry_it = hash_index.find(decoded_filename);
                    if (mft_entry_it != hash_index.end())
                    {
                        auto file_index = mft_entry_it->second.at(0);
                        amat_file = dat_manager->parse_amat_file(file_index);
                    }
                }
                Mesh prop_mesh = selected_ffna_model_file.GetMesh(i, amat_file);
                prop_mesh.center = {
                    (models[i].maxX - models[i].minX) / 2.0f, (models[i].maxY - models[i].minY) / 2.0f,
                    (models[i].maxZ - models[i].minZ) / 2.0f
                };

                overallMinX = std::min(overallMinX, models[i].minX);
                overallMinY = std::min(overallMinY, models[i].minY);
                overallMinZ = std::min(overallMinZ, models[i].minZ);

                overallMaxX = std::max(overallMaxX, models[i].maxX);
                overallMaxY = std::max(overallMaxY, models[i].maxY);
                overallMaxZ = std::max(overallMaxZ, models[i].maxZ);

                uint32_t sort_order = amat_file.GRMT_chunk.sort_order;
                sort_orders.push_back(sort_order);

                if ((prop_mesh.indices.size() % 3) == 0) { prop_meshes.push_back(prop_mesh); }
            }

            std::vector<size_t> indices(sort_orders.size());
            std::iota(indices.begin(), indices.end(), 0);  // Fill with 0, 1, 2, ...

            std::sort(indices.begin(), indices.end(),
                [&sort_orders](size_t i1, size_t i2) { return sort_orders[i1] < sort_orders[i2]; });

            // Create new vectors with sorted elements
            std::vector<Mesh> sorted_prop_meshes(prop_meshes.size());
            std::vector<int> sorted_sort_orders(sort_orders.size());

            for (size_t i = 0; i < indices.size(); ++i) {
                sorted_prop_meshes[i] = prop_meshes[indices[i]];
                sorted_sort_orders[i] = sort_orders[indices[i]];
            }

            // If you want, you can now swap the sorted vectors with the original ones
            prop_meshes.swap(sorted_prop_meshes);
            sort_orders.swap(sorted_sort_orders);

            // Load textures
            std::vector<int> texture_ids;
            std::vector<DatTexture> model_dat_textures;
            std::vector<std::vector<int>> per_mesh_tex_ids(prop_meshes.size());
            if (selected_ffna_model_file.textures_parsed_correctly)
            {
                for (int j = 0; j < selected_ffna_model_file.texture_filenames_chunk.texture_filenames.size();
                    j++)
                {
                    auto texture_filename =
                        selected_ffna_model_file.texture_filenames_chunk.texture_filenames[j];
                    auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);

                    int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(decoded_filename);

                    auto mft_entry_it = hash_index.find(decoded_filename);
                    if (mft_entry_it != hash_index.end())
                    {
                        auto file_index = mft_entry_it->second.at(0);
                        const auto* entry = &MFT[file_index];

                        if (!entry)
                            return false;

                        DatTexture dat_texture;
                        if (entry->type == DDS)
                        {
                            const auto ddsData = dat_manager->parse_dds_file(file_index);
                            size_t ddsDataSize = ddsData.size();
                            const auto hr = map_renderer->GetTextureManager()->
                                CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
                                    &dat_texture.height, dat_texture.rgba_data, entry->Hash);
                            model_texture_types.insert({ texture_id, DDSt });
                            dat_texture.texture_type = DDSt;
                        }
                        else
                        {
                            dat_texture = dat_manager->parse_ffna_texture_file(file_index);
                            // Create texture if it wasn't cached.
                            if (texture_id < 0)
                            {
                                auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(dat_texture.width,
                                    dat_texture.height, dat_texture.rgba_data.data(), &texture_id,
                                    decoded_filename);
                            }

                            model_texture_types.insert({ texture_id, dat_texture.texture_type });
                        }

                        model_dat_textures.push_back(dat_texture);

                        assert(texture_id >= 0);
                        if (texture_id >= 0) { texture_ids.push_back(texture_id); }
                    }
                }

                selected_dat_texture.dat_texture =
                    map_renderer->GetTextureManager()->BuildTextureAtlas(model_dat_textures, -1, -1);

                if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
                {
                    map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                        selected_dat_texture.dat_texture.width,
                        selected_dat_texture.dat_texture.height,
                        selected_dat_texture.dat_texture.rgba_data.
                        data(), &selected_dat_texture.texture_id,
                        entry->Hash);

                }

                // The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
                // So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
                for (int i = 0; i < prop_meshes.size(); i++)
                {
                    std::vector<uint8_t> mesh_tex_indices;
                    for (int j = 0; j < prop_meshes[i].tex_indices.size(); j++)
                    {
                        int tex_index = prop_meshes[i].tex_indices[j];
                        if (tex_index < texture_ids.size())
                        {
                            per_mesh_tex_ids[i].push_back(texture_ids[tex_index]);

                            mesh_tex_indices.push_back(j);
                        }
                    }

                    prop_meshes[i].tex_indices = mesh_tex_indices;
                }
            }

            // Create the PerObjectCB for each submodel
            std::vector<PerObjectCB> per_object_cbs;
            per_object_cbs.resize(prop_meshes.size());
            for (int i = 0; i < per_object_cbs.size(); i++)
            {
                float modelWidth = overallMaxX - overallMinX;
                float modelHeight = overallMaxY - overallMinY;
                float modelDepth = overallMaxZ - overallMinZ;

                float maxDimension = std::max({ modelWidth, modelHeight, modelDepth });

                float boundingBoxSize = 3000.0f;
                float scale = boundingBoxSize / maxDimension;

                float centerX = overallMinX + modelWidth * 0.5f;
                float centerY = overallMinY + modelHeight * 0.5f;
                float centerZ = overallMinZ + modelDepth * 0.5f;

                XMMATRIX scaling_matrix = XMMatrixScaling(scale, scale, scale);
                XMMATRIX translation_matrix =
                    XMMatrixTranslation(-centerX * scale, -centerY * scale, -centerZ * scale);
                XMMATRIX world_matrix = scaling_matrix * translation_matrix;

                XMStoreFloat4x4(&per_object_cbs[i].world, world_matrix);

                auto& prop_mesh = prop_meshes[i];
                if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
                    prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
                {
                    selected_ffna_model_file.textures_parsed_correctly = false;
                    continue;
                }

                if (selected_ffna_model_file.textures_parsed_correctly)
                {
                    per_object_cbs[i].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();
                    for (int j = 0; j < prop_mesh.uv_coord_indices.size(); j++)
                    {
                        int index0 = j / 4;
                        int index1 = j % 4;

                        per_object_cbs[i].uv_indices[index0][index1] =
                            static_cast<uint32_t>(prop_mesh.uv_coord_indices[j]);
                        per_object_cbs[i].texture_indices[index0][index1] =
                            static_cast<uint32_t>(prop_mesh.tex_indices[j]);
                        per_object_cbs[i].blend_flags[index0][index1] = static_cast<uint32_t>(prop_mesh.blend_flags[j]);
                        per_object_cbs[i].texture_types[index0][index1] = static_cast<uint32_t>(model_texture_types.at(per_mesh_tex_ids[i][j]));
                    }
                }
            }

            auto pixel_shader_type = PixelShaderType::OldModel;
            if (selected_ffna_model_file.geometry_chunk.unknown_tex_stuff1.size() > 0)
            {
                pixel_shader_type = PixelShaderType::NewModel;
            }

            auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, index, pixel_shader_type);
            if (selected_ffna_model_file.textures_parsed_correctly)
            {
                for (int i = 0; i < mesh_ids.size(); i++)
                {
                    int mesh_id = mesh_ids[i];
                    auto& mesh_texture_ids = per_mesh_tex_ids[i];

                    map_renderer->GetMeshManager()->SetTexturesForMesh(
                        mesh_id,
                        map_renderer->GetTextureManager()->
                        GetTextures(mesh_texture_ids), 3);
                }
            }
            success = true;
        }

        break;
    case FFNA_Type3:
    {
        if (selected_map_file_index == index)
        {
            break;
        }

        

        selected_map_file_index = index;

        object_id_to_prop_index.clear();
        object_id_to_submodel_index.clear();
        selected_map_files.clear();
        selected_ffna_map_file = dat_manager->parse_ffna_map_file(index);

        if (selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() > 0 &&
            selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() ==
            selected_ffna_map_file.terrain_chunk.terrain_x_dims *
            selected_ffna_map_file.terrain_chunk.terrain_y_dims)
        {
            // Clear up some GPU memory (especially important for GPUs with little VRAM)
            map_renderer->GetTextureManager()->Clear();
            map_renderer->ClearProps();

            auto& terrain_texture_filenames = selected_ffna_map_file.terrain_texture_filenames.array;
            std::vector<DatTexture> terrain_dat_textures;
            for (int i = 0; i < terrain_texture_filenames.size(); i++)
            {
                auto decoded_filename =
                    decode_filename(selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id0,
                        selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id1);

                // Jade Quarry, Island of Jade and The Antechamber . Each on them uses a normal map as their first texture.
                if (decoded_filename == 0x25e09 || decoded_filename == 0x00028615 || decoded_filename == 0x46db6)
                {
                    continue;
                }

                auto mft_entry_it = hash_index.find(decoded_filename);
                if (mft_entry_it != hash_index.end())
                {
                    const DatTexture dat_texture =
                        dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));
                    if (dat_texture.width > 0 && dat_texture.height > 0) {
                        terrain_dat_textures.push_back(dat_texture);
                    }
                }
            }

            // For displaying the texture atlas. Not used in rendering.
            if (terrain_dat_textures.size() > 0)
            {
                selected_dat_texture.dat_texture =
                    map_renderer->GetTextureManager()->BuildTextureAtlas(terrain_dat_textures, -1, -1);

                if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
                {
                    map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                        selected_dat_texture.dat_texture.width,
                        selected_dat_texture.dat_texture.height,
                        selected_dat_texture.dat_texture.rgba_data.
                        data(), &selected_dat_texture.texture_id,
                        entry->Hash);
                }
            }
            else
            {
                selected_dat_texture.texture_id = -1;
            }

            std::vector<void*> raw_data_ptrs;
            for (int i = 0; i < terrain_dat_textures.size(); i++)
            {
                raw_data_ptrs.push_back(terrain_dat_textures[i].rgba_data.data());
            }

            const auto terrain_texture_id = map_renderer->GetTextureManager()->AddTextureArray(raw_data_ptrs,
                terrain_dat_textures[0].width, terrain_dat_textures[0].height, DXGI_FORMAT_B8G8R8A8_UNORM,
                entry->Hash, true);

            auto& terrain_texture_indices =
                selected_ffna_map_file.terrain_chunk.terrain_texture_indices_maybe;

            auto& terrain_shadow_map =
                selected_ffna_map_file.terrain_chunk.terrain_shadow_map;

            // Create terrain
            terrain = std::make_unique<Terrain>(selected_ffna_map_file.terrain_chunk.terrain_x_dims,
                selected_ffna_map_file.terrain_chunk.terrain_y_dims,
                selected_ffna_map_file.terrain_chunk.terrain_heightmap,
                terrain_texture_indices, terrain_shadow_map,
                selected_ffna_map_file.map_info_chunk.map_bounds);
            map_renderer->SetTerrain(terrain.get(), terrain_texture_id);

            success = true;
        }

        // Load models
        std::vector<int> selected_map_file{};
        for (int i = 0; i < selected_ffna_map_file.prop_filenames_chunk.array.size(); i++)
        {
            auto decoded_filename =
                decode_filename(selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id0,
                    selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id1);
            auto mft_entry_it = hash_index.find(decoded_filename);
            if (mft_entry_it != hash_index.end())
            {
                auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
                if (type == FFNA_Type2)
                {
                    selected_map_files.emplace_back(
                        dat_manager->parse_ffna_model_file(mft_entry_it->second.at(0)));
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
                auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
                if (type == FFNA_Type2)
                {
                    auto map_model = dat_manager->parse_ffna_model_file(mft_entry_it->second.at(0));
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

                    std::vector<std::vector<int>> per_mesh_tex_ids;
                    // Load geometry
                    const auto& geometry_chunk = ffna_model_file_ptr->geometry_chunk;
                    prop_meshes.clear();

                    std::vector<int> sort_orders;
                    for (int j = 0; j < geometry_chunk.models.size(); j++)
                    {
                        const auto& models = geometry_chunk.models;

                        // Get AMAT file if any
                        AMAT_file amat_file;
                        if (ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames.size() > 0) {
                            int sub_model_index = models[j].unknown;
                            if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
                            {
                                sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
                            }
                            const auto uts1 = geometry_chunk.uts1[sub_model_index % geometry_chunk.uts1.size()];

                            const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames.size();
                            const auto amat_filename = ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames[amat_file_index];

                            const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


                            auto mft_entry_it = hash_index.find(decoded_filename);
                            if (mft_entry_it != hash_index.end())
                            {
                                auto file_index = mft_entry_it->second.at(0);
                                amat_file = dat_manager->parse_amat_file(file_index);
                            }
                        }

                        Mesh prop_mesh = ffna_model_file_ptr->GetMesh(j, amat_file);
                        prop_mesh.center = {
                            (models[j].maxX - models[j].minX) / 2.0f, (models[j].maxY - models[j].minY) / 2.0f,
                            (models[j].maxZ - models[j].minZ) / 2.0f
                        };

                        uint32_t sort_order = amat_file.GRMT_chunk.sort_order;
                        sort_orders.push_back(sort_order);

                        if ((prop_mesh.indices.size() % 3) == 0) { prop_meshes.push_back(prop_mesh); }
                    }

                    std::vector<size_t> indices(sort_orders.size());
                    std::iota(indices.begin(), indices.end(), 0);

                    std::sort(indices.begin(), indices.end(),
                        [&sort_orders](size_t i1, size_t i2) { return sort_orders[i1] < sort_orders[i2]; });

                    // Create new vectors with sorted elements
                    std::vector<Mesh> sorted_prop_meshes(prop_meshes.size());
                    std::vector<int> sorted_sort_orders(sort_orders.size());

                    for (size_t i = 0; i < indices.size(); ++i) {
                        sorted_prop_meshes[i] = prop_meshes[indices[i]];
                        sorted_sort_orders[i] = sort_orders[indices[i]];
                    }

                    prop_meshes.swap(sorted_prop_meshes);
                    sort_orders.swap(sorted_sort_orders);

                    if (ffna_model_file_ptr->parsed_correctly)
                    {
                        // Load textures
                        std::vector<int> texture_ids;
                        if (ffna_model_file_ptr->textures_parsed_correctly)
                        {
                            for (int j = 0;
                                j < ffna_model_file_ptr->texture_filenames_chunk.texture_filenames.size();
                                j++)
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
                                        dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));

                                    // Create texture
                                    auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
                                        dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
                                        &texture_id, decoded_filename);

                                    model_texture_types.insert({ texture_id, dat_texture.texture_type });

                                    texture_ids.push_back(texture_id);
                                }
                            }

                            // The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
                            // So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
                            per_mesh_tex_ids.resize(prop_meshes.size());
                            for (int k = 0; k < prop_meshes.size(); k++)
                            {
                                std::vector<uint8_t> mesh_tex_indices;
                                for (int j = 0; j < prop_meshes[k].tex_indices.size(); j++)
                                {
                                    int tex_index = prop_meshes[k].tex_indices[j];
                                    if (tex_index < texture_ids.size())
                                    {
                                        per_mesh_tex_ids[k].push_back(texture_ids[tex_index]);

                                        mesh_tex_indices.push_back(j);
                                    }
                                }

                                prop_meshes[k].tex_indices = mesh_tex_indices;
                            }
                        }

                        std::vector<PerObjectCB> per_object_cbs;
                        per_object_cbs.resize(prop_meshes.size());
                        for (int j = 0; j < per_object_cbs.size(); j++)
                        {
                            XMFLOAT3 translation(prop_info.x, prop_info.y, prop_info.z);

                            XMFLOAT3 vec1{ prop_info.f4, -prop_info.f6, prop_info.f5 };
                            XMFLOAT3 vec2{ prop_info.sin_angle, -prop_info.f9, prop_info.cos_angle };

                            // Load vectors into XMVECTORs
                            XMVECTOR v2 = XMLoadFloat3(&vec1);
                            XMVECTOR v3 = XMLoadFloat3(&vec2);

                            // Compute the third orthogonal vector with cross product
                            // Note: This is for left-handed coordinate systems
                            XMVECTOR v1 = XMVector3Cross(v3, v2);

                            // Ensure all vectors are normalized
                            v1 = XMVector3Normalize(v1);
                            v2 = XMVector3Normalize(v2);
                            v3 = XMVector3Normalize(v3);

                            // Fill the rotation matrix
                            auto rotation_matrix = XMMATRIX(
                                -v1.m128_f32[0], -v1.m128_f32[1], v1.m128_f32[2], 0.0f,
                                v2.m128_f32[0], v2.m128_f32[1], v2.m128_f32[2], 0.0f,
                                -v3.m128_f32[0], -v3.m128_f32[1], v3.m128_f32[2], 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f
                            );

                            // Create the scaling and translation matrices
                            const auto scale = prop_info.scaling_factor;
                            XMMATRIX scaling_matrix = XMMatrixScaling(scale, scale, scale);
                            XMMATRIX translation_matrix = XMMatrixTranslationFromVector(XMLoadFloat3(&translation));

                            // Compute the final transformation matrix
                            XMMATRIX transform_matrix = scaling_matrix * XMMatrixTranspose(rotation_matrix) *
                                translation_matrix;

                            // Store the transform matrix into the constant buffer
                            XMStoreFloat4x4(&per_object_cbs[j].world, transform_matrix);

                            auto& prop_mesh = prop_meshes[j];
                            if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
                                prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
                            {
                                ffna_model_file_ptr->textures_parsed_correctly = false;
                                continue;
                            }

                            if (ffna_model_file_ptr->textures_parsed_correctly)
                                per_object_cbs[j].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();

                            for (int k = 0; k < prop_mesh.uv_coord_indices.size(); k++)
                            {
                                int index0 = k / 4;
                                int index1 = k % 4;

                                per_object_cbs[j].uv_indices[index0][index1] =
                                    static_cast<uint32_t>(prop_mesh.uv_coord_indices[k]);
                                per_object_cbs[j].texture_indices[index0][index1] =
                                    static_cast<uint32_t>(prop_mesh.tex_indices[k]);
                                per_object_cbs[j].blend_flags[index0][index1] =
                                    static_cast<uint32_t>(prop_mesh.blend_flags[k]);
                                per_object_cbs[j].texture_types[index0][index1] =
                                    static_cast<uint32_t>(model_texture_types[per_mesh_tex_ids[j][k]]) | (prop_mesh.texture_types[k] << 8);
                            }
                        }

                        auto pixel_shader_type = PixelShaderType::OldModel;
                        if (ffna_model_file_ptr->geometry_chunk.unknown_tex_stuff1.size() > 0)
                        {
                            pixel_shader_type = PixelShaderType::NewModel;
                        }

                        auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, i, pixel_shader_type);
                        if (ffna_model_file_ptr->textures_parsed_correctly)
                        {
                            for (int l = 0; l < mesh_ids.size(); l++)
                            {
                                int mesh_id = mesh_ids[l];
                                auto& mesh_texture_ids = per_mesh_tex_ids[l];

                                map_renderer->GetMeshManager()->SetTexturesForMesh(
                                    mesh_id, map_renderer->GetTextureManager()->GetTextures(mesh_texture_ids),
                                    3);
                            }
                        }

                        // Add prop index to map for later picking
                        for (int l = 0; l < prop_meshes.size(); l++)
                        {
                            int object_id = per_object_cbs[l].object_id;
                            int prop_index = i;

                            object_id_to_prop_index.insert({ object_id, prop_index });
                            object_id_to_submodel_index.emplace(object_id, l);
                        }
                    }
                }
            }
        }

        //for (int i = 0; i < selected_ffna_map_file.props_info_chunk.some_vertex_data.vertices.size(); i++) {
        //    const auto vertex = selected_ffna_map_file.props_info_chunk.some_vertex_data.vertices[i];
        //    map_renderer->AddBox(vertex.x, -vertex.z, vertex.y, 50);
        //}

        //for (int i = 0; i < selected_ffna_map_file.props_info_chunk.some_data1.array.size(); i++) {
        //    const auto vertex = selected_ffna_map_file.props_info_chunk.some_data1.array[i];
        //    map_renderer->AddBox(vertex.x, 2000, vertex.y, 50);
        //}

        //for (int i = 0; i < selected_ffna_map_file.chunk5.some_array.size(); i++) {
        //    auto color1 = CheckerboardTexture::GetColorForIndex(i);
        //    auto color2 = CheckerboardTexture::GetColorForIndex(i + 1);

        //    const auto& vertices = selected_ffna_map_file.chunk5.some_array[i].vertices;
        //    for (int j = 0; j < vertices.size(); j++) {
        //        const auto& vertex = vertices[j];
        //        map_renderer->AddBox(vertex.x, 2000, vertex.y, 200, color1, color2);
        //    }
        //}
    }

    break;
    default:
        break;
    }

    return success;
}

std::string truncate_text_with_ellipsis(const std::string& text, float maxWidth);
int custom_stoi(const std::string& input);
std::string to_lower(const std::string& input);

void draw_data_browser(DATManager* dat_manager, MapRenderer* map_renderer, const bool dat_manager_changed, const std::unordered_set<uint32_t>& dat_compare_filter_result, const bool dat_compare_filter_result_changed,
    std::vector<std::vector<std::string>>& csv_data, bool custom_file_info_changed)
{
    static std::vector<DatBrowserItem> items;
    static std::vector<DatBrowserItem> filtered_items;

    static std::unordered_map<int, std::vector<int>> id_index;
    static std::unordered_map<int, std::vector<int>> hash_index;
    static std::unordered_map<int, std::vector<int>> file_id_0_index;
    static std::unordered_map<int, std::vector<int>> file_id_1_index;
    static std::unordered_map<FileType, std::vector<int>> type_index;

    static std::unordered_map<int, std::vector<int>> map_id_index;
    static std::unordered_map<std::string, std::vector<int>> name_index;
    static std::unordered_map<bool, std::vector<int>> pvp_index;

    static std::unordered_map<int, CustomFileInfoEntry> custom_file_info_map;

    if (custom_file_info_changed) {
        for (int i = 1; i < csv_data.size(); i++) {
            const auto& row = csv_data[i];
            CustomFileInfoEntry new_entry;
            if (row[0][0] == '0' && std::tolower(row[0][1]) == 'x') {
                new_entry.hash = std::stoul(row[0], 0, 16);
            }
            else {
                new_entry.hash = std::stoul(row[0]);
            }

            for (auto name_token : std::views::split(row[1], '|')) {
                if (!name_token.empty()) {
                    std::string name(&*name_token.begin(), std::ranges::distance(name_token));
                    new_entry.names.push_back(name);
                }
            }

            for (auto map_id_token : std::views::split(row[3], '|')) {
                if (!map_id_token.empty()) {
                    std::string map_id(&*map_id_token.begin(), std::ranges::distance(map_id_token));
                    new_entry.map_ids.push_back(std::stoi(map_id));
                }
            }

            new_entry.is_pvp = row[6] == "yes";

            custom_file_info_map.insert_or_assign(new_entry.hash, new_entry);
        }
    }

    if (dat_manager_changed || custom_file_info_changed) {
        items.clear();
        filtered_items.clear();
        id_index.clear();
        hash_index.clear();
        file_id_0_index.clear();
        file_id_1_index.clear();
        type_index.clear();
        map_id_index.clear();
        name_index.clear();
        pvp_index.clear();
    }

    if (!GuiGlobalConstants::is_dat_browser_resizeable)
    {
        auto dat_browser_window_size =
            ImVec2(ImGui::GetIO().DisplaySize.x -
                (GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2) -
                (GuiGlobalConstants::right_panel_width + GuiGlobalConstants::panel_padding * 2),
                300);
        ImGui::SetNextWindowSize(dat_browser_window_size);
    }

    if (!GuiGlobalConstants::is_dat_browser_movable) {
        auto dat_browser_window_pos =
            ImVec2(GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2,
                GuiGlobalConstants::panel_padding);
        ImGui::SetNextWindowPos(dat_browser_window_pos);
    }


    if (GuiGlobalConstants::is_dat_browser_open) {
        if (ImGui::Begin("Browse .dat file contents", &GuiGlobalConstants::is_dat_browser_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
            // Create item list
            if (items.size() == 0)
            {
                const auto& entries = dat_manager->get_MFT();
                for (int i = 0; i < entries.size(); i++)
                {
                    const auto& entry = entries[i];
                    int filename_id_0 = 0;
                    int filename_id_1 = 0;

                    // Update filename_id_0 and filename_id_1 with the proper values.
                    encode_filehash(entry.Hash, filename_id_0, filename_id_1);

                    DatBrowserItem new_item{
                        i, entry.Hash, static_cast<FileType>(entry.type), entry.Size, entry.uncompressedSize, filename_id_0, filename_id_1, {}, {}, {}, entry.murmurhash3
                    };
                    auto custom_file_info_it = custom_file_info_map.find(entry.Hash);
                    if (custom_file_info_it == custom_file_info_map.end()) {
                        // Files with file_id == 0 uses murmurhash3 instead when saved to custom file
                        custom_file_info_it = custom_file_info_map.find(entry.murmurhash3);
                    }

                    if (entry.type == FFNA_Type3)
                    {
                        if (custom_file_info_it != custom_file_info_map.end()) {
                            new_item.names = custom_file_info_it->second.names;
                            new_item.map_ids = custom_file_info_it->second.map_ids;
                            new_item.is_pvp = { custom_file_info_it->second.is_pvp };
                        }
                        else {
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
                    }
                    else {
                        if (custom_file_info_it != custom_file_info_map.end()) {
                            new_item.names = custom_file_info_it->second.names;
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
                    file_id_0_index[item.file_id_0].push_back(i);
                    file_id_1_index[item.file_id_1].push_back(i);

                    for (const auto map_id : item.map_ids) { map_id_index[map_id].push_back(i); }
                    for (const auto& name : item.names)
                    {
                        if (name != "" && name != "-")
                            name_index[name].push_back(i);
                    }
                    for (const auto is_pvp : item.is_pvp) { pvp_index[is_pvp].push_back(i); }
                }
            }

            // Set after filtering is complete.
            static std::string curr_id_filter = "";
            static std::string curr_hash_filter = "";
            static FileType curr_type_filter = NONE;
            static std::string curr_map_id_filter = "";
            static std::string curr_name_filter = "";
            static int curr_pvp_filter = -1;
            static std::string curr_filename_filter = "";

            // The values set by the user in the GUI
            static std::string id_filter_text;
            static std::string hash_filter_text;
            static FileType type_filter_value = NONE;
            static std::string map_id_filter_text;
            static std::string name_filter_text;
            static int pvp_filter_value = -1; // -1 means no filter, 0 means false, 1 means true
            static std::string filename_filter_text;

            static bool filter_update_required = true;

            if (dat_manager_changed || custom_file_info_changed) {
                filter_update_required = true;
            }

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

            if (curr_filename_filter != filename_filter_text)
            {
                curr_filename_filter = filename_filter_text;
                filter_update_required = true;
            }

            if (dat_compare_filter_result_changed) {
                filter_update_required = true;
            }

            // Only re-run the filter when the user changed filter params in the GUI.
            bool filter_updated = filter_update_required;
            if (filter_update_required)
            {

                filter_update_required = false;

                filtered_items.clear();

                std::unordered_set<int> intersection;

                if (!id_filter_text.empty())
                {
                    int id_filter_value = custom_stoi(id_filter_text);
                    if (id_index.contains(id_filter_value))
                    {
                        intersection.insert(id_index[id_filter_value].begin(), id_index[id_filter_value].end());
                    }
                }

                if (!hash_filter_text.empty())
                {
                    int hash_filter_value = custom_stoi(hash_filter_text);
                    if (hash_index.contains(hash_filter_value))
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
                                if (intersection.contains(id)) { new_intersection.insert(id); }
                            }
                            intersection = std::move(new_intersection);
                        }
                    }
                }

                if (!filename_filter_text.empty())
                {
                    int filename_filter_value = custom_stoi(filename_filter_text);
                    uint16_t id0 = filename_filter_value & 0xFFFF;
                    uint16_t id1 = (filename_filter_value >> 16) & 0xFFFF;

                    bool is_full_filename_hash = id0 > 0xFF && id1 > 0xFF;

                    if (!is_full_filename_hash) {
                        if (file_id_0_index.contains(id0))
                        {
                            apply_filter(file_id_0_index[id0], intersection);
                        }

                        if (file_id_1_index.contains(id1))
                        {
                            apply_filter(file_id_1_index[id1], intersection);
                        }
                    }
                    else {
                        if (file_id_0_index.contains(id0) && file_id_1_index.contains(id1))
                        {
                            apply_filter(file_id_0_index[id0], intersection);
                            apply_filter(file_id_1_index[id1], intersection);
                        }
                        else if (file_id_0_index.contains(id1) && file_id_1_index.contains(id0)) {
                            apply_filter(file_id_0_index[id1], intersection);
                            apply_filter(file_id_1_index[id0], intersection);
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
                            if (intersection.contains(id)) { new_intersection.insert(id); }
                        }
                        intersection = std::move(new_intersection);
                    }
                }

                if (!map_id_filter_text.empty())
                {
                    int map_id_filter_value = custom_stoi(map_id_filter_text);
                    if (map_id_index.contains(map_id_filter_value))
                    {
                        apply_filter(map_id_index[map_id_filter_value], intersection);
                    }
                }

                if (!name_filter_text.empty())
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
                    if (!matching_indices.empty()) { apply_filter(matching_indices, intersection); }
                }

                if (pvp_filter_value != -1)
                {
                    bool pvp_filter_bool = pvp_filter_value == 1;
                    apply_filter(pvp_index[pvp_filter_bool], intersection);
                }

                if (id_filter_text.empty() && hash_filter_text.empty() && type_filter_value == NONE &&
                    map_id_filter_text.empty() && name_filter_text.empty() && pvp_filter_value == -1 && filename_filter_text.empty()) {
                    for (const auto& item : items) {
                        if (dat_compare_filter_result.contains(item.hash) || dat_compare_filter_result.empty())
                        {
                            filtered_items.push_back(item);
                        }
                    }
                }
                else {
                    for (const auto& id : intersection)
                    {
                        const auto& item = items[id];
                        if (dat_compare_filter_result.contains(item.hash) || dat_compare_filter_result.empty()) {
                            filtered_items.push_back(item);
                        }
                    }
                }

                // Set them equal so that the filter won't run again until the filter changes.
                curr_id_filter = id_filter_text;
                curr_hash_filter = hash_filter_text;
                curr_type_filter = type_filter_value;
                curr_map_id_filter = map_id_filter_text;
                curr_name_filter = name_filter_text;
                curr_pvp_filter = pvp_filter_value;
                curr_filename_filter = filename_filter_text;
            }

            // Filter table
            // Render the filter inputs and the table
            ImGui::Columns(6);
            ImGui::Text("Id:");
            ImGui::SameLine();
            ImGui::InputText("##IdFilter", &id_filter_text);
            ImGui::NextColumn();

            ImGui::Text("File ID:");
            ImGui::SameLine();
            ImGui::InputText("##HashFilter", &hash_filter_text);
            ImGui::NextColumn();

            ImGui::Text("Filename");
            ImGui::SameLine();
            ImGui::InputText("##FilenameFilter", &filename_filter_text);
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

            if (ImGui::BeginTable("data browser", 10, flags))
            {
                // Declare columns
                // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
                // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
                // Demonstrate using a mixture of flags among available sort-related flags:
                // - ImGuiTableColumnFlags_DefaultSort
                // - ImGuiTableColumnFlags_NoSort / ImGuiTableColumnFlags_NoSortAscending / ImGuiTableColumnFlags_NoSortDescending
                // - ImGuiTableColumnFlags_PreferSortAscending / ImGuiTableColumnFlags_PreferSortDescending
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, DatBrowserItemColumnID_id);
                ImGui::TableSetupColumn("File ID", 0, 0.0f, DatBrowserItemColumnID_hash);
                ImGui::TableSetupColumn("Filename", 0, 0.0f, DatBrowserItemColumnID_filename);
                ImGui::TableSetupColumn("Name", 0, 0.0f, DatBrowserItemColumnID_name);
                ImGui::TableSetupColumn("Type", 0, 0.0f, DatBrowserItemColumnID_type);
                ImGui::TableSetupColumn("Size", 0, 0.0f, DatBrowserItemColumnID_size);
                ImGui::TableSetupColumn("Decompressed size", 0, 0.0f, DatBrowserItemColumnID_decompressed_size);
                ImGui::TableSetupColumn("Map id", 0, 0.0f, DatBrowserItemColumnID_map_id);
                ImGui::TableSetupColumn("PvP", 0, 0.0f, DatBrowserItemColumnID_is_pvp);
                ImGui::TableSetupColumn("murmur3", 0, 0.0f, DatBrowserItemColumnID_murmurhash3);
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

                ImGui::TableHeadersRow();

                // Sort our data if sort specs have been changed!
                ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs();
                if (sorts_specs)
                    if (dat_manager_changed || custom_file_info_changed || filter_updated) {
                        sorts_specs->SpecsDirty = true;
                    }

                if (sorts_specs->SpecsDirty)
                {
                    DatBrowserItem::s_current_sort_specs =
                        sorts_specs; // Store in variable accessible by the sort function.
                    if (filtered_items.size() > 1)
                        qsort(&filtered_items[0], filtered_items.size(), sizeof(filtered_items[0]),
                            DatBrowserItem::CompareWithSortSpecs);
                    DatBrowserItem::s_current_sort_specs = nullptr;
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

                        // Check if this is the row to highlight
                        if (item.hash > 0 && item.hash == selected_item_hash) {
                            // Set the background color for this row
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
                        }

                        if (ImGui::Selectable(label.c_str(), item_is_selected, selectable_flags))
                        {
                            if (ImGui::GetIO().KeyCtrl) {}
                            else
                            {
                                parse_file(dat_manager, item.id, map_renderer, hash_index);
                                selected_item_id = item.id;
                                selected_item_hash = item.hash;
                                selected_item_murmurhash3 = item.murmurhash3;
                            }
                        }
                        // If the item is focused (highlighted by navigation), select it immediately
                        if (ImGui::IsItemFocused() && selected_item_id != item.id) {
                            if (selected_item_id != item.id) {
                                parse_file(dat_manager, item.id, map_renderer, hash_index);
                                selected_item_id = item.id;
                                selected_item_hash = item.hash;
                                selected_item_murmurhash3 = item.murmurhash3;
                            }

                            // Check the direction of focus movement and adjust the scroll position
                            if (last_focused_item_index != -1) {
                                float row_height = ImGui::GetTextLineHeightWithSpacing();

                                if (row_n < last_focused_item_index) {
                                    // Focus moved up, scroll up
                                    ImGui::SetScrollY(ImGui::GetScrollY() - row_height);
                                }
                                else if (row_n > last_focused_item_index) {
                                    // Focus moved down, scroll down
                                    ImGui::SetScrollY(ImGui::GetScrollY() + row_height);
                                }
                            }

                            last_focused_item_index = row_n; // Update the last focused item index
                        }

                        if (dat_manager_changed || custom_file_info_changed) {
                            // Find the index of the item with item.hash == selected_item_hash or item.murmurhash3 == selected_item_hash
                            int item_index = -1;
                            for (int i = 0; i < filtered_items.size(); ++i) {
                                if (filtered_items[i].hash == selected_item_hash) {
                                    item_index = i;
                                    break;
                                }
                            }

                            if (item_index == -1) {
                                for (int i = 0; i < filtered_items.size(); ++i) {
                                    if (filtered_items[i].murmurhash3 == selected_item_hash) { // note multiple files can share the same murmurhash3
                                        item_index = i;
                                        break;
                                    }
                                }
                            }

                            // If the item was found
                            if (item_index != -1) {
                                // Calculate the height of a single row (this is an approximation)
                                float row_height = ImGui::GetTextLineHeightWithSpacing();

                                // Calculate the number of visible rows in the table
                                float visible_rows = ImGui::GetWindowHeight() / row_height;

                                // Calculate the position to scroll to
                                // We want the selected row to be at the top of the table, so we scroll to its position minus half the visible rows
                                float scroll_pos = (item_index - visible_rows / 2) * row_height;

                                // Clamp the scroll position to the valid range
                                scroll_pos = std::max(0.0f, std::min(scroll_pos, ImGui::GetScrollMaxY()));

                                // Scroll to the item
                                ImGui::SetScrollY(scroll_pos);
                            }
                        }

                        //Add context menu on right clicking item in table
                        if (ImGui::BeginPopupContextItem("ItemContextMenu"))
                        {
                            if (ImGui::MenuItem("Save decompressed data to file"))
                            {
                                std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", item.hash), L"gwraw");
                                if (!savePath.empty()) { dat_manager->save_raw_decompressed_data_to_file(item.id, savePath); }
                            }

                            if (item.type == SOUND || item.type == AMP)
                            {
                                if (ImGui::MenuItem("Save to mp3"))
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", item.hash), L"mp3");
                                    if (!savePath.empty()) { dat_manager->save_raw_decompressed_data_to_file(item.id, savePath); }
                                }
                            }
                            else if (item.type == FFNA_Type2)
                            {
                                if (ImGui::MenuItem("Export model as JSON"))
                                {
                                    std::wstring saveDir = OpenDirectoryDialog();
                                    if (!saveDir.empty())
                                    {
                                        // Use std::format to create the filename
                                        std::wstring filename = std::format(L"model_0x{:X}_gwmb.json", item.hash);


                                        // Export the model to the chosen path
                                        model_exporter::export_model(saveDir, filename, item.id, dat_manager, hash_index, map_renderer->GetTextureManager());
                                    }
                                }
                                if (ImGui::MenuItem("Export Mesh"))
                                {
                                    std::wstring savePath =
                                        OpenFileDialog(std::format(L"model_mesh_0x{:X}", item.hash), L"obj");
                                    if (!savePath.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        const auto obj_file_str = write_obj_str(prop_meshes);

                                        std::ofstream outFile(savePath);
                                        if (outFile.is_open())
                                        {
                                            outFile << obj_file_str;
                                            outFile.close();
                                        }
                                        else
                                        {
                                            // Error handling
                                        }
                                    }
                                }

                                if (ImGui::MenuItem("Export Submeshes Individually"))
                                {
                                    std::wstring saveDir = OpenDirectoryDialog();
                                    if (!saveDir.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        for (size_t prop_mesh_index = 0; prop_mesh_index < prop_meshes.size();
                                            ++prop_mesh_index)
                                        {
                                            const auto& prop_mesh = prop_meshes[prop_mesh_index];
                                            const auto obj_file_str = write_obj_str(&prop_mesh);

                                            // Generate unique file name
                                            std::wstring filename = std::format(L"model_mesh_0x{:X}_{}.obj", item.hash, prop_mesh_index);

                                            // Append the filename to the saveDir
                                            std::wstring savePath = saveDir + L"\\" + filename;

                                            std::ofstream outFile(savePath);
                                            if (outFile.is_open())
                                            {
                                                outFile << obj_file_str;
                                                outFile.close();
                                            }
                                            else
                                            {
                                                // Error handling
                                            }
                                        }
                                    }
                                }

                                if (ImGui::MenuItem("Export model textures (.png)"))
                                {
                                    std::wstring saveDir = OpenDirectoryDialog();
                                    if (!saveDir.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);

                                        for (int tex_index = 0; tex_index < selected_ffna_model_file.texture_filenames_chunk.
                                            texture_filenames.
                                            size(); tex_index++)
                                        {
                                            const auto& texture_filename = selected_ffna_model_file.texture_filenames_chunk.
                                                texture_filenames[tex_index];

                                            auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);
                                            int texture_id = map_renderer->GetTextureManager()->
                                                GetTextureIdByHash(decoded_filename);

                                            std::wstring filename = std::format(L"model_0x{:X}_tex_index{}_texture_0x{:X}.png",
                                                item.hash, tex_index, decoded_filename);

                                            // Append the filename to the saveDir
                                            std::wstring savePath = saveDir + L"\\" + filename;

                                            ID3D11ShaderResourceView* texture =
                                                map_renderer->GetTextureManager()->GetTexture(texture_id);
                                            if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
                                            {
                                                // Success
                                            }
                                            else
                                            {
                                                // Error handling }
                                            }
                                        }
                                    }
                                }
                                if (ImGui::MenuItem("Export model textures (.dds) BC1"))
                                {
                                    const auto compression_format = CompressionFormat::BC1;
                                    ExportDDS(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                                if (ImGui::MenuItem("Export model textures (.dds) BC3"))
                                {
                                    const auto compression_format = CompressionFormat::BC3;
                                    ExportDDS(dat_manager, item, map_renderer, hash_index, compression_format);
                                }

                                if(ImGui::MenuItem("Export model textures (.dds) BC5"))
                                {
                                    const auto compression_format = CompressionFormat::BC5;
                                    ExportDDS(dat_manager, item, map_renderer, hash_index, compression_format);
                                }

                                if (ImGui::MenuItem("Export model textures (.dds) no compression"))
                                {
                                    const auto compression_format = CompressionFormat::None;
                                    ExportDDS(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                            }
                            else if (item.type == FFNA_Type3)
                            {
                                if (ImGui::MenuItem("Export full map"))
                                {
                                    std::wstring savePath = OpenDirectoryDialog();
                                    if (!savePath.empty())
                                    {
                                        // Create a new directory name using the item's hash
                                        std::wstring newDirName = L"gwmb_map_" + std::to_wstring(item.hash);

                                        // Append the new directory name to the existing path
                                        std::filesystem::path newDirPath = std::filesystem::path(savePath) / newDirName;

                                        // Check if the directory exists and if not, create it
                                        if (!exists(newDirPath))
                                        {
                                            create_directory(newDirPath);
                                        }

                                        map_exporter::export_map(newDirPath, item.hash, item.id, dat_manager, hash_index, map_renderer->GetTextureManager());
                                    }
                                }
                                else if (ImGui::MenuItem("Export Terrain Mesh as .obj"))
                                {
                                    std::wstring savePath =
                                        OpenFileDialog(std::format(L"height_map_0x{:X}", item.hash), L"obj");
                                    if (!savePath.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        const auto& terrain_mesh = terrain.get()->get_mesh();
                                        const auto obj_file_str = write_obj_str(terrain_mesh);

                                        std::string savePathStr(savePath.begin(), savePath.end());

                                        std::ofstream outFile(savePathStr);
                                        if (outFile.is_open())
                                        {
                                            outFile << obj_file_str;
                                            outFile.close();
                                        }
                                        else
                                        {
                                            // Error handling
                                        }
                                    }
                                }
                                else if (ImGui::MenuItem("Export heightmap as .tiff"))
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_height_map_0x{:X}", item.hash),
                                        L"tiff");
                                    if (!savePath.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        const auto& terrain_mesh = terrain.get()->get_heightmap_grid();
                                        // Assuming the accessor function is available.

                                        // Convert the savePath to a string
                                        std::string save_path_str(savePath.begin(), savePath.end());

                                        // Write the BMP file
                                        if (!write_heightmap_tiff(terrain_mesh, save_path_str.c_str()))
                                        {
                                            // Error handling
                                        }
                                    }
                                }
                                else if (ImGui::MenuItem("Export terrain texture indices as .tiff"))
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
                                        L"tiff");
                                    if (!savePath.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        const auto& terrain_texture_indices = terrain.get()->get_texture_index_grid();
                                        // Assuming the accessor function is available.

                                        // Convert the savePath to a string
                                        std::string save_path_str(savePath.begin(), savePath.end());

                                        // Write the BMP file
                                        if (!write_terrain_ints_tiff(terrain_texture_indices, save_path_str.c_str()))
                                        {
                                            // Error handling
                                        }
                                    }
                                }
                                else if (ImGui::MenuItem("Export terrain shadow map as .tiff"))
                                {
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_shadow_map_0x{:X}", item.hash),
                                        L"tiff");
                                    if (!savePath.empty())
                                    {
                                        parse_file(dat_manager, item.id, map_renderer, hash_index);
                                        const auto& terrain_unknown = terrain.get()->get_terrain_shadow_map_grid();
                                        // Assuming the accessor function is available.

                                        // Convert the savePath to a string
                                        std::string save_path_str(savePath.begin(), savePath.end());

                                        // Write the BMP file
                                        if (!write_terrain_ints_tiff(terrain_unknown, save_path_str.c_str()))
                                        {
                                            // Error handling
                                        }
                                    }
                                }
                            }
                            else if (item.type == DDS) {
                                if (ImGui::MenuItem("Export texture as DDS")) {
                                    parse_file(dat_manager, item.id, map_renderer, hash_index);
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
                                        L"dds");
                                    if (!savePath.empty())
                                    {
                                         dat_manager->save_raw_decompressed_data_to_file(item.id, savePath);
                                    }
                                }
                                else if (ImGui::MenuItem("Export texture as png")) {
                                    parse_file(dat_manager, item.id, map_renderer, hash_index);
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
                                        L"png");
                                    if (!savePath.empty())
                                    {
                                        int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(item.hash);

                                        std::wstring filename = std::format(L"texture_0x{:X}.png", item.hash);

                                        ID3D11ShaderResourceView* texture =
                                            map_renderer->GetTextureManager()->GetTexture(texture_id);
                                        if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
                                        {
                                            // Success
                                        }
                                        else
                                        {
                                            // Error handling }
                                        }
                                    }
                                }
                            }
                            else if (item.type >= ATEXDXT1 && item.type <= ATTXDXTL &&
                                item.type != ATEXDXTA && item.type != ATTXDXTA)
                            {
                                if (ImGui::MenuItem("Export texture as DDS (BC1)")) {
                                    const auto compression_format = CompressionFormat::BC1;
                                    ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                                else if (ImGui::MenuItem("Export texture as DDS (BC3)")) {
                                    const auto compression_format = CompressionFormat::BC3;
                                    ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                                else if (ImGui::MenuItem("Export texture as DDS (BC5)")) {
                                    const auto compression_format = CompressionFormat::BC5;
                                    ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                                else if (ImGui::MenuItem("Export texture as DDS (No compression)")) {
                                    const auto compression_format = CompressionFormat::None;
                                    ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
                                }
                                else if (ImGui::MenuItem("Export texture as png")) {

                                    parse_file(dat_manager, item.id, map_renderer, hash_index);
                                    std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
                                        L"png");
                                    if (!savePath.empty())
                                    {
                                        int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(item.hash);

                                        std::wstring filename = std::format(L"texture_0x{:X}.png", item.hash);

                                        ID3D11ShaderResourceView* texture =
                                            map_renderer->GetTextureManager()->GetTexture(texture_id);
                                        if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
                                        {
                                            // Success
                                        }
                                        else
                                        {
                                            // Error handling }
                                        }
                                    }
                                }
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::TableNextColumn();

                        const auto file_hash_text = std::format("0x{:X} ({})", item.hash, item.hash);
                        ImGui::Text(file_hash_text.c_str());
                        ImGui::TableNextColumn();

                        const auto filename_text = std::format("0x{:X} 0x{:X}", item.file_id_0, item.file_id_1);
                        ImGui::Text(filename_text.c_str());
                        ImGui::TableNextColumn();


                        if (!item.names.empty())
                        {
                            std::string name;
                            for (int i = 0; i < item.names.size(); i++)
                            {
                                name += item.names[i];
                                if (i < item.names.size() - 1) { name += " | "; }
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
                        else { ImGui::Text("-"); }

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
                                if (i < item.map_ids.size() - 1) { map_ids_text += ","; }
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
                        else { ImGui::Text("-"); }
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
                                    &allTrue); // Show a single checkbox with the respective value
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

                                    if (i < item.is_pvp.size() - 1) { ImGui::SameLine(); }
                                }
                            }
                        }
                        else { ImGui::Text("-"); }

                        ImGui::TableNextColumn();
                        ImGui::Text("%u", item.murmurhash3);

                        ImGui::PopID();
                    }
                    ImGui::EndTable();
            }
        }

        ImGui::End();
    }
}

void ExportDDS2(DATManager* dat_manager, DatBrowserItem& item, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format)
{
    parse_file(dat_manager, item.id, map_renderer, hash_index);
    std::wstring savePath = OpenFileDialog(std::format(L"texture_0x{:X}", item.hash),
        L"dds");
    if (!savePath.empty())
    {
        const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(item.hash);
        if (texture_data.has_value()) {
            std::wstring filename = std::format(L"texture_0x{:X}.dds", item.hash);

            if (SaveTextureToDDS(texture_data.value(), savePath, compression_format))
            {
                // Success
            }
            else
            {
                // Error handling }
            }
        }
    }
}

void ExportDDS(DATManager* dat_manager, DatBrowserItem& item, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format)
{
    std::wstring saveDir = OpenDirectoryDialog();
    if (!saveDir.empty())
    {
        parse_file(dat_manager, item.id, map_renderer, hash_index);

        for (int tex_index = 0; tex_index < selected_ffna_model_file.texture_filenames_chunk.
            texture_filenames.
            size(); tex_index++)
        {
            const auto& texture_filename = selected_ffna_model_file.texture_filenames_chunk.
                texture_filenames[tex_index];

            auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);
            const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(decoded_filename);

            if (texture_data.has_value()) {

                std::wstring filename = std::format(L"model_0x{:X}_tex_index{}_texture_0x{:X}.dds",
                    item.hash, tex_index, decoded_filename);

                // Append the filename to the saveDir
                std::wstring savePath = saveDir + L"\\" + filename;

                if (SaveTextureToDDS(texture_data.value(), savePath, compression_format))
                {
                    // Success
                }
                else
                {
                    // Error handling }
                }
            }
        }
    }
}

inline int IMGUI_CDECL DatBrowserItem::CompareWithSortSpecs(const void* lhs, const void* rhs)
{
    auto a = static_cast<const DatBrowserItem*>(lhs);
    auto b = static_cast<const DatBrowserItem*>(rhs);
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
        case DatBrowserItemColumnID_murmurhash3:
            delta = (a->murmurhash3 - b->murmurhash3);
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
        case DatBrowserItemColumnID_filename:
            delta = (a->file_id_0 + (a->file_id_1 << 16)) - (b->file_id_0 + (b->file_id_1 << 16));
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
    if (textSize.x <= maxWidth) { return text; }

    std::string truncatedText;
    float ellipsisWidth = ImGui::CalcTextSize("...").x;

    for (size_t i = 0; i < text.size(); ++i)
    {
        textSize = ImGui::CalcTextSize((truncatedText + text[i] + "...").c_str());
        if (textSize.x <= maxWidth) { truncatedText += text[i]; }
        else { break; }
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
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') { ++str; }

    // check for sign
    if (*str == '-')
    {
        negative = true;
        ++str;
    }
    else if (*str == '+') { ++str; }

    // check for hex prefix
    if (std::strlen(str) >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;

        // read hex digits
        while (*str != '\0')
        {
            if (*str >= '0' && *str <= '9') { value = value * 16 + (*str - '0'); }
            else if (*str >= 'a' && *str <= 'f') { value = value * 16 + (*str - 'a' + 10); }
            else if (*str >= 'A' && *str <= 'F') { value = value * 16 + (*str - 'A' + 10); }
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
            if (*str >= '0' && *str <= '9') { value = value * 10 + (*str - '0'); }
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
