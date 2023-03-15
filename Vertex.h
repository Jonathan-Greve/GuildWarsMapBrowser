#pragma once

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position; // The position of the vertex
    XMFLOAT3 normal; // The normal of the vertex
    XMFLOAT2 tex_coord; // The texture coordinate of the vertex
};
