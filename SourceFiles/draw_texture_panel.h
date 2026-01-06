#pragma once
#include "MapRenderer.h"
#include "FFNA_ModelFile_Other.h"

void draw_texture_panel(MapRenderer* map_renderer);

void TexPanelExportDDS(const std::optional<TextureData>& texture_data, std::wstring& savePath, const CompressionFormat compression_format);

// Storage for inline texture GPU resources (inventory icons, created when an "other" model is loaded)
struct InlineTextureDisplay
{
    int texture_id = -1;
    uint16_t width = 0;
    uint16_t height = 0;
    std::string format;
    int index = 0;
};

// Storage for model texture references (textures used for the 3D model, from texture_filenames_chunk)
struct ModelTextureDisplay
{
    int texture_id = -1;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t file_hash = 0;  // The decoded filename hash (e.g., 0x2139E)
    int index = 0;
};

extern std::vector<InlineTextureDisplay> inline_texture_displays;
extern std::vector<ModelTextureDisplay> model_texture_displays;
