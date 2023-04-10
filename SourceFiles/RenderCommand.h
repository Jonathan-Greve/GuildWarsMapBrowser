#pragma once
#include "MeshInstance.h"
#include "PixelShader.h"
#include "BlendStateManager.h"

struct RenderCommand
{
    std::shared_ptr<MeshInstance> meshInstance;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;
    PixelShaderType pixelShaderType;
    bool should_cull;
    BlendState blend_state;
    bool should_render = true;
};
