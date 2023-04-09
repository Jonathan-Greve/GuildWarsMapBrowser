#pragma once
#include "Vertex.h"

struct Mesh
{
    std::vector<GWVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint8_t> uv_coord_indices; // The number of uv_coords used to draw the mesh.
    std::vector<uint8_t> tex_indices; // The indices of the texture files used by the mesh.

    int num_textures;
};
