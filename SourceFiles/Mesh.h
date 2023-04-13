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
    std::vector<uint8_t>
      blend_flags; // one per tex_index. 0 is opaque set alpha to 1. 8 is alphablend, use alpha as normal. 6/7 are reverse alphablend set alpha to 1-alpha. 3 also seem seem to some reversed alpha stuff. Flags seem to be from 0 through 8.

    bool should_cull = true;
    BlendState blend_state = BlendState::Opaque;

    int num_textures;
};
