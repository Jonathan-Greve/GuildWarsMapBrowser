#pragma once
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <vector>

bool write_heightmap_png(const std::vector<std::vector<float>>& heightmap, const char* filename);
bool write_heightmap_tiff(const std::vector<std::vector<float>>& heightmap, const char* filename);

