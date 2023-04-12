#pragma once
#include "Vertex.h"
#include "BlendStateManager.h"

constexpr int MAX_NUM_TEX_INDICES = 8;

struct Mesh
{
    std::vector<GWVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint8_t> uv_coord_indices; // The number of uv_coords used to draw the mesh.
    std::vector<uint8_t> tex_indices; // The indices of the texture files used by the mesh.

    bool should_cull = true;
    BlendState blend_state = BlendState::Opaque;

    int num_textures;
};
