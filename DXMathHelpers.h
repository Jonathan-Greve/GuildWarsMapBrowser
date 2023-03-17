#pragma once
using namespace DirectX;

static inline XMFLOAT3 compute_normal(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
{
    XMFLOAT3 ab, ac, normal;
    XMVECTOR vec_a, vec_b, vec_c;

    vec_a = XMLoadFloat3(&a);
    vec_b = XMLoadFloat3(&b);
    vec_c = XMLoadFloat3(&c);

    ab = XMFLOAT3(b.x - a.x, b.y - a.y, b.z - a.z);
    ac = XMFLOAT3(c.x - a.x, c.y - a.y, c.z - a.z);

    XMVECTOR vec_ab = XMLoadFloat3(&ab);
    XMVECTOR vec_ac = XMLoadFloat3(&ac);
    XMVECTOR vec_normal = XMVector3Cross(vec_ab, vec_ac);

    XMStoreFloat3(&normal, vec_normal);
    return normal;
}
