#pragma once
#include <DirectXMath.h>
#include "DirectionalLight.h"
#include "RenderConstants.h"

// Per-frame constant buffer struct
struct PerFrameCB
{
    DirectionalLight directionalLight; // Sun
    float time_elapsed;
    float fog_color_rgb[3];
    float fog_start;
    float fog_end;
    float fog_start_y; // The height at which fog starts.
    float fog_end_y; // The height at which fog ends.
};
