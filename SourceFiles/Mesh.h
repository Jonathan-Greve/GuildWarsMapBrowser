#pragma once
#include "Vertex.h"

struct Mesh
{
    std::vector<GWVertex> vertices; // A vector of vertices
    std::vector<uint32_t> indices; // A vector of indices
    int num_textures;
};
