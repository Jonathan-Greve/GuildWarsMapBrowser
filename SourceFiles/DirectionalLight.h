#pragma once

using namespace DirectX;

struct DirectionalLight
{
    DirectionalLight() { ZeroMemory(this, sizeof(this)); }

    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
    XMFLOAT4 specular;
    XMFLOAT3 direction;

    float pad;
};
