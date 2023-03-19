#pragma once
#include <DirectXMath.h>
#include "DirectionalLight.h"
#include "RenderConstants.h"

// Per-frame constant buffer struct
struct PerFrameCB
{
    DirectionalLight directionalLight; // Sun
};
