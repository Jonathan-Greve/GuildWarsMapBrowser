#pragma once
#include <vector>
#include <DATManager.h>
#include <AMAT_file.h>
#include <FFNA_ModelFile.h>
#include <TextureManager.h>
#include <PixelShader.h>
#include <json.hpp>

struct gwmb_vec2f
{
    float x, y;
};

struct gwmb_vec3f
{
    float x, y, z;
};

struct gwmb_vec4f
{
    float x, y, z, w;
};

struct gwmb_texture
{
    int file_hash;
    int width;
    int height;
    TextureType texture_type;
};

// gwmb_vertex
struct gwmb_vertex
{
    bool has_normal;
    bool has_tangent;
    bool has_bitangent;
    int num_tex_coords;

    // Position in local space
    // Left handed coordinate system.
    // x left/right.
    // y up/down.
    // z forward/back (in/out of screen).
    gwmb_vec3f pos;

    // Vertex normal vector
    // Most models have this.
    gwmb_vec3f normal;

    // Vertex tangent vector
    // Mainly "new models" (used in EotN) has tangent and bitangent vectors.
    gwmb_vec3f tangent;

    // Vertex bitangent vector
    gwmb_vec3f bitangent;

    // UV maps
    std::vector<gwmb_vec2f> texture_uv_coords;
};

// In GW a model is usually divided into smaller parts. 
// For example a bridge might have the main bridge submodel and then a fence submodel.
struct gwmb_submodel {
    std::vector<gwmb_vertex> vertices;

    // Faces are in counter-clockwise order.
    // Each consecutive 3 indices represents a face so: indices.size() % 3 == 0 and indices.size() > 0.
    // These indices are used for the "High" (best quality) LOD.
    std::vector<int> indices;

    // Indices for medium and low quality LODs.
    std::vector<int> indices_med;
    std::vector<int> indices_low;

    // Flags telling us if the model has medium or low LOD indices.
    bool has_med_lod;
    bool has_low_lod;

    // The index of the texture to use for each UV map. The vector has length: num_texcoords
    std::vector<int> texture_indices;

    // Tells us which of the texture_uv_coords to use. So the value of any element in texture_uv_map_index is < texture_uv_coords.size()
    std::vector<int> texture_uv_map_index;

    // Used for deciding how to combine textures in "old" models.
    std::vector<int> texture_blend_flags;

    PixelShaderType pixel_shader_type;
};

// GW Map Browser model contains the info required for the export
struct gwmb_model
{
    std::vector<gwmb_texture> textures; // Multiple submodels can use the same textures (see their texture_indices which maps into this vector)
    std::vector<gwmb_submodel> submodels; // A model consists of 1 or more submodels.
    std::vector<int> submodels_draw_order; // Lower values are drawn before bigger values. This is local to the individual model.
};

namespace nlohmann {

    template<>
    struct adl_serializer<gwmb_vec2f> {
        static void to_json(json& j, const gwmb_vec2f& v) {
            j = json{ {"x", v.x}, {"y", v.y} };
        }
        static void from_json(const json& j, gwmb_vec2f& v) {
            j.at("x").get_to(v.x);
            j.at("y").get_to(v.y);
        }
    };

    template<>
    struct adl_serializer<gwmb_vec3f> {
        static void to_json(json& j, const gwmb_vec3f& v) {
            j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
        }
        static void from_json(const json& j, gwmb_vec3f& v) {
            j.at("x").get_to(v.x);
            j.at("y").get_to(v.y);
            j.at("z").get_to(v.z);
        }
    };

    template<>
    struct adl_serializer<gwmb_vec4f> {
        static void to_json(json& j, const gwmb_vec4f& v) {
            j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
        }
        static void from_json(const json& j, gwmb_vec4f& v) {
            j.at("x").get_to(v.x);
            j.at("y").get_to(v.y);
            j.at("z").get_to(v.z);
            j.at("w").get_to(v.w);
        }
    };

    template<>
    struct adl_serializer<gwmb_texture> {
        static void to_json(json& j, const gwmb_texture& t) {
            j = json{
                {"file_hash", t.file_hash},
                {"width", t.width},
                {"height", t.height},
                {"texture_type", t.texture_type},
            };
        }
        static void from_json(const json& j, gwmb_texture& t) {
            j.at("file_hash").get_to(t.file_hash);
            j.at("width").get_to(t.width);
            j.at("height").get_to(t.height);
            j.at("texture_type").get_to(t.texture_type);
        }
    };

    template<>
    struct adl_serializer<gwmb_vertex> {
        static void to_json(json& j, const gwmb_vertex& v) {
            j = json{
                {"has_normal", v.has_normal},
                {"has_tangent", v.has_tangent},
                {"has_bitangent", v.has_bitangent},
                {"num_tex_coords", v.num_tex_coords},
                {"pos", v.pos},
                {"normal", v.normal},
                {"texture_uv_coords", v.texture_uv_coords}
            };

            if (v.has_tangent) {
                j["tangent"] = v.tangent;
            }

            if (v.has_bitangent) {
                j["bitangent"] = v.bitangent;
            }
        }
        static void from_json(const json& j, gwmb_vertex& v) {
            j.at("has_normal").get_to(v.has_normal);
            j.at("has_tangent").get_to(v.has_tangent);
            j.at("has_bitangent").get_to(v.has_bitangent);
            j.at("num_tex_coords").get_to(v.num_tex_coords);
            j.at("pos").get_to(v.pos);
            j.at("normal").get_to(v.normal);
            j.at("texture_uv_coords").get_to(v.texture_uv_coords);

            if (j.contains("has_tangent")) {
                j.at("tangent").get_to(v.tangent);
            }
            else {
                v.has_tangent = false;
            }

            if (j.contains("has_bitangent")) {
                j.at("bitangent").get_to(v.bitangent);
            }
            else {
                v.has_bitangent = false;
            }
        }
    };

    template<>
    struct adl_serializer<gwmb_submodel> {
        static void to_json(json& j, const gwmb_submodel& s) {
            j = json{
                {"vertices", s.vertices},
                {"indices", s.indices},
                {"indices_med", s.indices_med},
                {"indices_low", s.indices_low},
                {"has_med_lod", s.has_med_lod},
                {"has_low_lod", s.has_low_lod},
                {"texture_indices", s.texture_indices},
                {"texture_uv_map_index", s.texture_uv_map_index},
                {"texture_blend_flags", s.texture_blend_flags},
                {"pixel_shader_type", s.pixel_shader_type}
            };
        }

        static void from_json(const json& j, gwmb_submodel& s) {
            j.at("vertices").get_to(s.vertices);
            j.at("indices").get_to(s.indices);
            j.at("indices_med").get_to(s.indices_med);
            j.at("indices_low").get_to(s.indices_low);
            j.at("has_med_lod").get_to(s.has_med_lod);
            j.at("has_low_lod").get_to(s.has_low_lod);
            j.at("texture_indices").get_to(s.texture_indices);
            j.at("texture_uv_map_index").get_to(s.texture_uv_map_index);
            j.at("texture_blend_flags").get_to(s.texture_blend_flags);
            j.at("pixel_shader_type").get_to(s.pixel_shader_type);
        }
    };


    template<>
    struct adl_serializer<gwmb_model> {
        static void to_json(json& j, const gwmb_model& m) {
            j = json{
                {"textures", m.textures},
                {"submodels", m.submodels},
                {"submodels_draw_order", m.submodels_draw_order}
            };
        }
        static void from_json(const json& j, gwmb_model& m) {
            j.at("textures").get_to(m.textures);
            j.at("submodels").get_to(m.submodels);
            j.at("submodels_draw_order").get_to(m.submodels_draw_order);
        }
    };
}


// Step 1) Convert data into the gwmb_model format.
// Step 2) Write the data to a .gwmb file. A custom data format to be used when importing into other programs like Blender.
class model_exporter {
public:
    static bool export_model(const std::wstring& save_dir, const std::wstring& filename, const int model_mft_index, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager, const bool json_pretty_print = false) {
        auto model_file = dat_manager->parse_ffna_model_file(model_mft_index);
        return export_model_to_file(save_dir, filename, &model_file, dat_manager, hash_index, texture_manager, json_pretty_print);
    }

    static bool export_model(const std::wstring& save_dir, const std::wstring& filename, FFNA_ModelFile* model_file, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager, const bool json_pretty_print = false) {
        return export_model_to_file(save_dir, filename, model_file, dat_manager, hash_index, texture_manager, json_pretty_print);
    }

private:
    static bool export_model_to_file(const std::wstring& save_dir, const std::wstring& filename, FFNA_ModelFile* model_file, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager, const bool json_pretty_print) {
        std::wstring saveFilePath = save_dir + L"\\" + filename;
        if (std::filesystem::exists(saveFilePath)) {
            return true; // Return immediately if the file already exists
        }

        gwmb_model model;
        const bool success = generate_gwmb_model(model, model_file, dat_manager, hash_index, texture_manager, save_dir);
        if (!success) {
            return false; // Failed to build the model
        }

        const nlohmann::json j = model;
        std::ofstream file(saveFilePath);
        if (!file) {
            return false; // Failed to open the file for writing
        }

        if (json_pretty_print) {
            file << j.dump(4);
        }
        else {
            file << j.dump();
        }
        file.close();

        return true; // Successfully exported the model
    }

    static bool generate_gwmb_model(gwmb_model& model_out, FFNA_ModelFile* model_file, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager, const std::wstring& save_dir) {

        if (!model_file->parsed_correctly)
            return false;
        if (!model_file->textures_parsed_correctly)
            return false;

        const auto& texture_filenames = model_file->texture_filenames_chunk.texture_filenames;

        // Build gwmb_textures
        for (int i = 0; i < texture_filenames.size(); i++) {
            const auto texture_filename = texture_filenames[i];
            auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);

            auto mft_entry_it = hash_index.find(decoded_filename);
            if (mft_entry_it != hash_index.end())
            {
                auto file_index = mft_entry_it->second.at(0);
                const auto* entry = &(dat_manager->get_MFT()[file_index]);

                if (!entry)
                    return false;

                int texture_id = -1;
                DatTexture dat_texture;
                if (entry->type == DDS)
                {
                    const auto ddsData = dat_manager->parse_dds_file(file_index);
                    size_t ddsDataSize = ddsData.size();
                    const auto hr = texture_manager->
                        CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
                            &dat_texture.height, dat_texture.rgba_data, entry->Hash);
                    dat_texture.texture_type = DDSt;
                }
                else
                {
                    dat_texture = dat_manager->parse_ffna_texture_file(file_index);
                    auto HR = texture_manager->CreateTextureFromRGBA(dat_texture.width,
                        dat_texture.height, dat_texture.rgba_data.data(), &texture_id,
                        decoded_filename);
                }

                gwmb_texture gwmb_texture_i;
                gwmb_texture_i.file_hash = decoded_filename;
                gwmb_texture_i.height = dat_texture.height;
                gwmb_texture_i.width = dat_texture.width;
                gwmb_texture_i.texture_type = dat_texture.texture_type;

                ID3D11ShaderResourceView* texture =
                    texture_manager->GetTexture(texture_id);

                std::wstring texture_save_path = save_dir + L"\\" + std::to_wstring(gwmb_texture_i.file_hash) + L".png";

                if (!SaveTextureToPng(texture, texture_save_path, texture_manager))
                {
                    throw "Unable to save texture to png while exporting model";
                }

                model_out.textures.push_back(gwmb_texture_i);
            }
        }

        const auto& geometry_chunk = model_file->geometry_chunk;

        // Loop over each submodel
        for (int i = 0; i < geometry_chunk.models.size(); i++) {
            const auto& submodel = geometry_chunk.models[i];

            // Populate gwmb_submodel
            gwmb_submodel gwmb_submodel_i;

            // Add vertices to gwmb_submodel
            gwmb_submodel_i.vertices.resize(submodel.vertices.size());
            for (int j = 0; j < submodel.vertices.size(); j++) {
                const auto vertex = submodel.vertices[j];

                gwmb_vertex new_gwmb_vertex;
                new_gwmb_vertex.bitangent = { vertex.bitangent_x, vertex.bitangent_y, vertex.bitangent_z };
                new_gwmb_vertex.has_bitangent = vertex.has_bitangent;
                new_gwmb_vertex.has_normal = vertex.has_normal;
                new_gwmb_vertex.has_tangent = vertex.has_tangent;
                new_gwmb_vertex.normal = { vertex.normal_x, vertex.normal_y, vertex.normal_z };
                new_gwmb_vertex.num_tex_coords = vertex.num_texcoords;
                new_gwmb_vertex.pos = { vertex.x, vertex.y, vertex.z };
                new_gwmb_vertex.tangent = { vertex.tangent_x, vertex.tangent_y, vertex.tangent_z };

                for (int k = 0; k < vertex.num_texcoords; k++) {
                    new_gwmb_vertex.texture_uv_coords.push_back({ vertex.tex_coord[k][0], vertex.tex_coord[k][1] });
                }

                gwmb_submodel_i.vertices[j] = new_gwmb_vertex;
            }

            // Add High LOD indices to gwmb_submodel
            gwmb_submodel_i.indices.resize(submodel.num_indices0);
            for (int j = 0; j < submodel.num_indices0; j++) {
                const auto index = submodel.indices[j];
                gwmb_submodel_i.indices[j] = index;
            }

            if (submodel.num_indices0 != submodel.num_indices1) {
                gwmb_submodel_i.has_med_lod = true;

                gwmb_submodel_i.indices_med.resize(submodel.num_indices1);
                int index_offset = submodel.num_indices0;
                for (int j = 0; j < submodel.num_indices1; j++) {
                    const auto index = submodel.indices[index_offset+j];
                    gwmb_submodel_i.indices_med[j] = index;
                }
            }
            else {
                gwmb_submodel_i.has_med_lod = false;
            }

            if (submodel.num_indices0 != submodel.num_indices2 && submodel.num_indices1 != submodel.num_indices2) {
                gwmb_submodel_i.has_low_lod = true;

                int index_offset = gwmb_submodel_i.has_med_lod ? submodel.num_indices0 + submodel.num_indices1 : submodel.num_indices0;
                gwmb_submodel_i.indices_low.resize(submodel.num_indices2);
                for (int j = 0; j < submodel.num_indices2; j++) {
                    const auto index = submodel.indices[index_offset + j];
                    gwmb_submodel_i.indices_low[j] = index;
                }
            }
            else {
                gwmb_submodel_i.has_low_lod = false;
            }

            AMAT_file amat_file;
            if (model_file->AMAT_filenames_chunk.texture_filenames.size() > 0) {
                int sub_model_index = geometry_chunk.models[i].unknown;
                if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
                {
                    sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
                }
                const auto uts1 = geometry_chunk.uts1[sub_model_index % geometry_chunk.uts1.size()];

                const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % model_file->AMAT_filenames_chunk.texture_filenames.size();
                const auto amat_filename = model_file->AMAT_filenames_chunk.texture_filenames[amat_file_index];

                const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


                auto mft_entry_it = hash_index.find(decoded_filename);
                if (mft_entry_it != hash_index.end())
                {
                    auto file_index = mft_entry_it->second.at(0);
                    amat_file = dat_manager->parse_amat_file(file_index);
                }
            }

            Mesh prop_mesh = model_file->GetMesh(i, amat_file);

            gwmb_submodel_i.texture_indices.resize(prop_mesh.tex_indices.size());
            for (int j = 0; j < prop_mesh.tex_indices.size(); j++) {
                gwmb_submodel_i.texture_indices[j] = prop_mesh.tex_indices[j];
            }
            gwmb_submodel_i.texture_uv_map_index.resize(prop_mesh.uv_coord_indices.size());
            for (int j = 0; j < prop_mesh.uv_coord_indices.size(); j++) {
                gwmb_submodel_i.texture_uv_map_index[j] = prop_mesh.uv_coord_indices[j];
            }
            gwmb_submodel_i.texture_blend_flags.resize(prop_mesh.blend_flags.size());
            for (int j = 0; j < prop_mesh.blend_flags.size(); j++) {
                gwmb_submodel_i.texture_blend_flags[j] = prop_mesh.blend_flags[j];
            }

            int draw_order = 0;

            auto pixel_shader_type = PixelShaderType::OldModel;
            if (model_file->geometry_chunk.unknown_tex_stuff1.size() > 0)
            {
                pixel_shader_type = PixelShaderType::NewModel;
                draw_order = amat_file.GRMT_chunk.sort_order;
            }

            gwmb_submodel_i.pixel_shader_type = pixel_shader_type;

            model_out.submodels.push_back(gwmb_submodel_i);
            model_out.submodels_draw_order.push_back(draw_order);
        }

        return true;
    }
};
