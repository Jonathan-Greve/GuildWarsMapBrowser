#pragma once
using namespace DirectX;

static inline XMFLOAT3 compute_normal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
    XMVECTOR vEdge1 = XMVectorSubtract(XMLoadFloat3(&v1), XMLoadFloat3(&v0));
    XMVECTOR vEdge2 = XMVectorSubtract(XMLoadFloat3(&v2), XMLoadFloat3(&v0));
    XMVECTOR vNormal = XMVector3Cross(vEdge1, vEdge2);
    XMFLOAT3 normal;
    XMStoreFloat3(&normal, XMVector3Normalize(vNormal));
    return normal;
}
