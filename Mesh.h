#pragma once
#include "Vertex.h"

struct Mesh
{
    std::vector<Vertex> vertices; // A vector of vertices
    std::vector<uint32_t> indices; // A vector of indices
};
