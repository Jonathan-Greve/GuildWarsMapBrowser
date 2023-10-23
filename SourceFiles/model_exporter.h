#pragma once
#include <vector>

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
	std::vector<std::vector<gwmb_vec2f>> uv_maps;

	// The index of the texture to use for each UV map. Must have the same dimension as the outer vector of uv_maps.
	std::vector<int> texture_indices;
};

// In GW a model is usually divided into smaller parts. 
// For example a bridge might have the main bridge submodel and then a fence submodel.
class gwmb_submodel {
	std::vector<gwmb_vertex> vertices;

	// Faces are in counter-clockwise order.
	// Each consecutive 3 indices represents a face so: indices.size() % 3 == 0 and indices.size() > 0.
	std::vector<int> indices;

	std::vector<gwmb_texture> textures;
};

// GW Map Browser model contains the info required for the export
class gwmb_model
{
	// Some basic file info. Might not be needed in the exporter but might be interesting to store for debugging purposes.
	int file_hash;
	int filename_id0;
	int filename_id1;
	int dat_decompressed_size;

	std::vector<gwmb_submodel> submodels;
	std::vector<int> submodels_draw_order; // Lower values are drawn before bigger values. This is local to the individual model.
};

// Step 1) Convert data into the gwmb_model format.
// Step 2) Write the data to a .gwmb file. A custom data format to be used when importing into other programs like Blender.
class model_exporter {
public:
	bool export_model(std::string save_path) {
		// Build model
		gwmb_model model_to_export;
		generate_gwmb_model(model_to_export);

		return false;
	}

private:
	bool generate_gwmb_model(gwmb_model& model_out){
		return false;
	}
};
