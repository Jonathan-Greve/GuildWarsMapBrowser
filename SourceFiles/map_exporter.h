#pragma once
#include "model_exporter.h"
#include "Terrain.h"

struct gwmb_map_vertex
{
    // Position in local space
    // Left handed coordinate system.
    // x left/right.
    // y up/down.
    // z forward/back (in/out of screen).
    gwmb_vec3f pos;

    // Vertex normal vector
    // Most models have this.
    gwmb_vec3f normal;

    // UV coord
    gwmb_vec2f uv_coord;
};

struct gwmb_terrain
{
    std::vector<gwmb_map_vertex> vertices;
    std::vector<int> indices;
    std::vector<gwmb_texture> textures; // A terrain has up to 64 textures.
};

struct gwmb_map_models
{
    gwmb_model model;
    gwmb_vec3f world_pos; // To translate model into its world position.
    gwmb_vec3f model_up; // The models up vector. Used for rotation.
    gwmb_vec3f model_look; // THe models look vector. Used for rotation.
    float scale; // The scaling factor of the model when placed in the world.
};

struct gwmb_map
{
    gwmb_terrain terrain;
    std::vector<gwmb_map_models> models;
};

namespace nlohmann {
    template<>
    struct adl_serializer<gwmb_map_vertex> {
        static void to_json(json& j, const gwmb_map_vertex& mv) {
            j = json{
                {"pos", mv.pos},
                {"normal", mv.normal},
                {"uv_coord", mv.uv_coord}
            };
        }

        static void from_json(const json& j, gwmb_map_vertex& mv) {
            j.at("pos").get_to(mv.pos);
            j.at("normal").get_to(mv.normal);
            j.at("uv_coord").get_to(mv.uv_coord);
        }
    };

    template<>
    struct adl_serializer<gwmb_terrain> {
        static void to_json(json& j, const gwmb_terrain& t) {
            j = json{
                {"vertices", t.vertices},
                {"indices", t.indices},
                {"textures", t.textures}
            };
        }
        static void from_json(const json& j, gwmb_terrain& t) {
            j.at("vertices").get_to(t.vertices);
            j.at("indices").get_to(t.indices);
            j.at("textures").get_to(t.textures);
        }
    };

    template<>
    struct adl_serializer<gwmb_map_models> {
        static void to_json(json& j, const gwmb_map_models& mm) {
            j = json{
                {"model", mm.model},
                {"world_pos", mm.world_pos},
                {"model_up", mm.model_up},
                {"model_look", mm.model_look},
                {"scale", mm.scale}
            };
        }
        static void from_json(const json& j, gwmb_map_models& mm) {
            j.at("model").get_to(mm.model);
            j.at("world_pos").get_to(mm.world_pos);
            j.at("model_up").get_to(mm.model_up);
            j.at("model_look").get_to(mm.model_look);
            j.at("scale").get_to(mm.scale);
        }
    };

    template<>
        struct adl_serializer<gwmb_map> {
        static void to_json(json& j, const gwmb_map& m) {
            j = json{
                {"terrain", m.terrain},
                {"models", m.models}
            };
        }

        static void from_json(const json& j, gwmb_map& m) {
            j.at("terrain").get_to(m.terrain);
            j.at("models").get_to(m.models);
        }
    };
}

class map_exporter
{
public:
    static bool export_map(const std::string& save_path, const int map_mft_index, DATManager& dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager, const bool json_pretty_print = false) {
        // Build model
        gwmb_map map;
        const bool success = generate_gwmb_map(map, map_mft_index, dat_manager, hash_index, texture_manager);
        if (!success)
            return false;

        const nlohmann::json j = map;

        std::ofstream file(save_path);
        if (!file) {
            return false; // Failed to open the file
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

private:
    static bool generate_gwmb_map(gwmb_map& map, int map_mft_index, DATManager& dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager) {
        auto map_file = dat_manager.parse_ffna_map_file(map_mft_index);

        if (map_file.terrain_chunk.terrain_heightmap.size() > 0 &&
            map_file.terrain_chunk.terrain_heightmap.size() ==
            map_file.terrain_chunk.terrain_x_dims *
            map_file.terrain_chunk.terrain_y_dims)
        {
            // First add terrain textures
            gwmb_terrain new_terrain;
            const auto& terrain_texture_filenames = map_file.terrain_texture_filenames.array;
            std::vector<DatTexture> terrain_dat_textures;
            for (int i = 0; i < terrain_texture_filenames.size(); i++)
            {
                auto decoded_filename =
                    decode_filename(map_file.terrain_texture_filenames.array[i].filename.id0,
                        map_file.terrain_texture_filenames.array[i].filename.id1);

                // Jade Quarry, Island of Jade. Each on them uses a normal map as their first texture.
                if (decoded_filename == 0x25e09 || decoded_filename == 0x00028615)
                {
                    continue;
                }

                auto mft_entry_it = hash_index.find(decoded_filename);
                if (mft_entry_it != hash_index.end())
                {
                    const DatTexture dat_texture =
                        dat_manager.parse_ffna_texture_file(mft_entry_it->second.at(0));
                    if (dat_texture.width > 0 && dat_texture.height > 0) {
                        gwmb_texture gwmb_texture_i;
                        gwmb_texture_i.file_hash = decoded_filename;
                        gwmb_texture_i.height = dat_texture.height;
                        gwmb_texture_i.width = dat_texture.width;
                        gwmb_texture_i.texture_type = dat_texture.texture_type;
                        gwmb_texture_i.rgba_pixels.resize(dat_texture.rgba_data.size());
                        for (int j = 0; j < dat_texture.rgba_data.size(); j++) {
                            // Switch r and b (the current pixels are in bgra so we save it as rgba instead)
                            gwmb_texture_i.rgba_pixels[j] = { dat_texture.rgba_data[j].b / 255.f, dat_texture.rgba_data[j].g / 255.f, dat_texture.rgba_data[j].r / 255.f, dat_texture.rgba_data[j].a / 255.f };
                        }

                        new_terrain.textures.push_back(gwmb_texture_i);
                    }
                }
            }

            // Now add the terrain mesh
            auto& terrain_texture_indices =
                map_file.terrain_chunk.terrain_texture_indices_maybe;

            auto& terrain_shadow_map =
                map_file.terrain_chunk.terrain_shadow_map;

            // Create terrain
            const auto terrain = std::make_unique<Terrain>(map_file.terrain_chunk.terrain_x_dims,
                map_file.terrain_chunk.terrain_y_dims,
                map_file.terrain_chunk.terrain_heightmap,
                terrain_texture_indices, terrain_shadow_map,
                map_file.map_info_chunk.map_bounds);

            const auto terrain_mesh = terrain->get_mesh();
            new_terrain.vertices.resize(terrain_mesh->vertices.size());
            for (int i = 0; i < terrain_mesh->vertices.size(); i++) {
                const auto vertex = terrain_mesh->vertices[i];

                gwmb_map_vertex new_gwmb_map_vertex;
                new_gwmb_map_vertex.normal = { vertex.normal.x, vertex.normal.y, vertex.normal.z };
                new_gwmb_map_vertex.pos = { vertex.position.x, vertex.position.y, vertex.position.z };

                new_gwmb_map_vertex.uv_coord = { vertex.tex_coord0.x , vertex.tex_coord0.y};

                new_terrain.vertices[i] = new_gwmb_map_vertex;
            }

            new_terrain.indices.resize(terrain->get_mesh()->indices.size());
            for (int i = 0; i< terrain_mesh->indices.size(); i++) {
                const auto index = terrain_mesh->indices[i];
                new_terrain.indices[i] = index;
            }

            // Now add all the models to the map

            map.terrain = new_terrain;
        }
        else
        {
            return false;
        }

        return true;
    }
};

