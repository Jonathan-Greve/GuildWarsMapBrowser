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

inline XMFLOAT3 RGBAtoHSL(const XMFLOAT4& rgba) {
    float r = rgba.x, g = rgba.y, b = rgba.z;
    float maxVal = std::max({ r, g, b });
    float minVal = std::min({ r, g, b });
    float delta = maxVal - minVal;

    XMFLOAT3 hsl; // Initialize HSL

    // Lightness
    hsl.z = (maxVal + minVal) / 2;

    if (delta == 0) { // Gray color, no saturation, and hue is undefined
        hsl.x = 0; // Hue
        hsl.y = 0; // Saturation
    }
    else {
        // Saturation
        hsl.y = hsl.z > 0.5 ? delta / (2.0f - maxVal - minVal) : delta / (maxVal + minVal);

        // Hue
        if (maxVal == r) {
            hsl.x = (g - b) / delta + (g < b ? 6.0f : 0.0f);
        }
        else if (maxVal == g) {
            hsl.x = (b - r) / delta + 2.0f;
        }
        else { // maxVal == b
            hsl.x = (r - g) / delta + 4.0f;
        }

        hsl.x /= 6.0f; // Normalize hue to be between 0 and 1
    }

    return hsl;
}

inline XMFLOAT4 HSLtoRGBA(const XMFLOAT3& hsl) {
    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 0.5f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
        };

    float h = hsl.x, s = hsl.y, l = hsl.z;
    XMFLOAT4 rgba;

    if (s == 0.0f) {
        rgba.x = rgba.y = rgba.z = l; // achromatic
    }
    else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        rgba.x = hue2rgb(p, q, h + 1.0f / 3.0f);
        rgba.y = hue2rgb(p, q, h);
        rgba.z = hue2rgb(p, q, h - 1.0f / 3.0f);
    }

    rgba.w = 1.0f; // Alpha is always set to 1

    return rgba;
}

