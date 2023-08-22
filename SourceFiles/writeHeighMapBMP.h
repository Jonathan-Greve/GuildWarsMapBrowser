#pragma once
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <vector>

bool write_heightmap_png(const std::vector<std::vector<float>>& heightmap, const char* filename);
bool write_heightmap_tiff(const std::vector<std::vector<float>>& heightmap, const char* filename);
bool write_terrain_ints_tiff(const std::vector<std::vector<uint32_t>>& terrain_indices, const char* filename);
