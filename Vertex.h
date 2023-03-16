#pragma once

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position; // The position of the vertex
    XMFLOAT3 normal; // The normal of the vertex
    XMFLOAT2 tex_coord; // The texture coordinate of the vertex
};

// Define the input layout
inline extern D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, tex_coord), D3D11_INPUT_PER_VERTEX_DATA, 0},
};
