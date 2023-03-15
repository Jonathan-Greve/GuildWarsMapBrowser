#pragma once
#include <DirectXMath.h>
#include "DirectionalLight.h"
#include "RenderConstants.h"

// Per-frame constant buffer struct
struct PerFrameCB
{
    DirectionalLight directionalLights[MAX_LIGHTS];
    int numActiveLights;
    float pad[3]; // Padding to ensure 16-byte alignment
    // Add any other per-frame data here
};
