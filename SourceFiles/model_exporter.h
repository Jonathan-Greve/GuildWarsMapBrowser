#pragma once
#include <vector>
#include <DATManager.h>
#include <AMAT_file.h>
#include <FFNA_ModelFile.h>
#include <TextureManager.h>
#include <PixelShader.h>

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
	// Stored per row (from top of image (row_0) to bottom of image (row_height-1))
	// Each width*gwmb_vec4f is a row and are stored in order: row_0, row_1, ..., row_height-1.
	std::vector<gwmb_vec4f> rgba_pixels;
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
	std::vector<int> indices;


	// The index of the texture to use for each UV map. The vector has length: num_texcoords
	std::vector<int> texture_indices;

	// Tells us which of the texture_uv_coords to use. So the value of any element in texture_uv_map_index is < texture_uv_coords.size()
	std::vector<int> texture_uv_map_index;

	PixelShaderType pixel_shader_type;
};

// GW Map Browser model contains the info required for the export
struct gwmb_model
{
	std::vector<gwmb_texture> textures; // Multiple submodels can use the same textures (see their texture_indices which maps into this vector)
	std::vector<gwmb_submodel> submodels; // A model consists of 1 or more submodels.
	std::vector<int> submodels_draw_order; // Lower values are drawn before bigger values. This is local to the individual model.
};

// Step 1) Convert data into the gwmb_model format.
// Step 2) Write the data to a .gwmb file. A custom data format to be used when importing into other programs like Blender.
class model_exporter {
public:
	static bool export_model(const std::string save_path, const int model_mft_index, DATManager& dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager) {
		// Build model
		gwmb_model model_to_export;
		bool success = generate_gwmb_model(model_to_export, model_mft_index, dat_manager, hash_index, texture_manager);
		if (!success)
			return false;

		std::vector<unsigned char> serialized_gwmb_model;
		model_to_export.serialize(serialized_gwmb_model);

		if (serialized_gwmb_model.size() <= 0)
			return false;

		// Write serialized data to file
		std::ofstream file(save_path, std::ios::binary);
		if (!file) {
			return false; // Failed to open the file
		}

		file.write(reinterpret_cast<const char*>(serialized_gwmb_model.data()), serialized_gwmb_model.size());
		file.close();

		return true; // Successfully exported the model
	}

private:
	static bool generate_gwmb_model(gwmb_model& model_out, int model_mft_index, DATManager& dat_manager, std::unordered_map<int, std::vector<int>>& hash_index, TextureManager* texture_manager) {
		auto model_file = dat_manager.parse_ffna_model_file(model_mft_index);

		if (!model_file.parsed_correctly)
			return false;
		if (!model_file.textures_parsed_correctly)
			return false;

		const auto& texture_filenames = model_file.texture_filenames_chunk.texture_filenames;

		// Build gwmb_textures
		for (int i = 0; i < texture_filenames.size(); i++) {
			const auto texture_filename = texture_filenames[i];
			auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);

			auto mft_entry_it = hash_index.find(decoded_filename);
			if (mft_entry_it != hash_index.end())
			{
				auto file_index = mft_entry_it->second.at(0);
				const auto* entry = &(dat_manager.get_MFT()[file_index]);

				if (!entry)
					return false;

				int texture_id = -1; // not used
				DatTexture dat_texture;
				if (entry->type == DDS)
				{
					const auto ddsData = dat_manager.parse_dds_file(file_index);
					size_t ddsDataSize = ddsData.size();
					const auto hr = texture_manager->
						CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
							&dat_texture.height, dat_texture.rgba_data, entry->Hash);
					dat_texture.texture_type = DDSt;
				}
				else
				{
					dat_texture = dat_manager.parse_ffna_texture_file(file_index);
					auto HR = texture_manager->CreateTextureFromRGBA(dat_texture.width,
						dat_texture.height, dat_texture.rgba_data.data(), &texture_id,
						decoded_filename);
				}

				gwmb_texture gwmb_texture_i;
				gwmb_texture_i.file_hash = decoded_filename;
				gwmb_texture_i.height = dat_texture.height;
				gwmb_texture_i.width = dat_texture.width;
				gwmb_texture_i.rgba_pixels.resize(dat_texture.rgba_data.size());
				for (int j = 0; j < dat_texture.rgba_data.size(); j++) {
					gwmb_texture_i.rgba_pixels[j] = { dat_texture.rgba_data[j].r / 255.f, dat_texture.rgba_data[j].g / 255.f, dat_texture.rgba_data[j].b / 255.f, dat_texture.rgba_data[j].a / 255.f };
				}

				model_out.textures.push_back(gwmb_texture_i);
			}
		}

		const auto& geometry_chunk = model_file.geometry_chunk;

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

			// Add indices to gwmb_submodel
			gwmb_submodel_i.indices.resize(submodel.indices.size());
			for (int j = 0; j < submodel.indices.size(); j++) {
				const auto index = submodel.indices[j];
				gwmb_submodel_i.indices.push_back(index);
			}

			AMAT_file amat_file;
			if (model_file.AMAT_filenames_chunk.texture_filenames.size() > 0) {
				int sub_model_index = geometry_chunk.models[i].unknown;
				if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
				{
					sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
				}
				const auto uts1 = geometry_chunk.uts1[sub_model_index % geometry_chunk.uts1.size()];

				const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % model_file.AMAT_filenames_chunk.texture_filenames.size();
				const auto amat_filename = model_file.AMAT_filenames_chunk.texture_filenames[amat_file_index];

				const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


				auto mft_entry_it = hash_index.find(decoded_filename);
				if (mft_entry_it != hash_index.end())
				{
					auto file_index = mft_entry_it->second.at(0);
					amat_file = dat_manager.parse_amat_file(file_index);
				}
			}

			Mesh prop_mesh = model_file.GetMesh(i, amat_file);

			gwmb_submodel_i.texture_indices.resize(prop_mesh.tex_indices.size());
			for (int j = 0; j < prop_mesh.tex_indices.size(); j++) {
				gwmb_submodel_i.texture_indices[j] = prop_mesh.tex_indices[j];
			}
			gwmb_submodel_i.texture_uv_map_index.resize(prop_mesh.uv_coord_indices.size());
			for (int j = 0; j < prop_mesh.tex_indices.size(); j++) {
				gwmb_submodel_i.texture_uv_map_index[j] = prop_mesh.uv_coord_indices[j];
			}

			int draw_order = 0;

			auto pixel_shader_type = PixelShaderType::OldModel;
			if (model_file.geometry_chunk.unknown_tex_stuff1.size() > 0)
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
