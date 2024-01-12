#pragma once
#include "MapRenderer.h"

void draw_texture_panel(MapRenderer* map_renderer);

void TexPanelExportDDS(const std::optional<TextureData>& texture_data, std::wstring& savePath, const CompressionFormat compression_format);
