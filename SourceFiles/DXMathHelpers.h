#pragma once
using namespace DirectX;

struct Vertex3 {
    float x, y, z;
};

static inline XMFLOAT3 compute_normal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
    XMVECTOR vEdge1 = XMVectorSubtract(XMLoadFloat3(&v1), XMLoadFloat3(&v0));
    XMVECTOR vEdge2 = XMVectorSubtract(XMLoadFloat3(&v2), XMLoadFloat3(&v0));
    XMVECTOR vNormal = XMVector3Cross(vEdge1, vEdge2);
    XMFLOAT3 normal;
    XMStoreFloat3(&normal, XMVector3Normalize(vNormal));
    return normal;
}

static inline DirectX::XMFLOAT3 GetPositionFromMatrix(const DirectX::XMFLOAT4X4& worldMatrix) {
    return DirectX::XMFLOAT3(worldMatrix._41, worldMatrix._42, worldMatrix._43);
}

// Function to add two XMFLOAT3 vectors
inline XMFLOAT3 AddXMFLOAT3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

// Function to calculate the length of an XMFLOAT3 vector
inline float LengthXMFLOAT3(const XMFLOAT3& v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Function to normalize an XMFLOAT3 vector
inline XMFLOAT3 NormalizeXMFLOAT3(const XMFLOAT3& v) {
    float len = LengthXMFLOAT3(v);
    return XMFLOAT3(v.x / len, v.y / len, v.z / len);
}

inline XMFLOAT3 Vertex3ToXMFLOAT3(const Vertex3& vertex) {
    return XMFLOAT3(vertex.x, vertex.y, vertex.z);
}