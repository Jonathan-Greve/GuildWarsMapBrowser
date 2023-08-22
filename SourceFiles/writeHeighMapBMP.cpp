#include "pch.h"
#include "writeHeighMapBMP.h"
#include "stb_image_write.h""
#include "tinytiff/tinytiffwriter.h"

bool write_heightmap_png(const std::vector<std::vector<float>>& heightmap, const char* filename)
{
	int width = heightmap[0].size();
	int height = heightmap.size();
	std::vector<unsigned char> pixels(width * height);

	// Find min and max values in heightmap
	float min = FLT_MAX;
	float max = FLT_MIN;
	for (const auto& row : heightmap)
	{
		for (float value : row)
		{
			if (value < min) min = value;
			if (value > max) max = value;
		}
	}

	// Scale float values to 8-bit
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			float value = (heightmap[y][x] - min) / (max - min) * 255;
			pixels[(height - y - 1) * width + x] = static_cast<unsigned char>(value); // Flipping the y-axis
		}
	}

	// Write PNG
	if (stbi_write_png(filename, width, height, 1, pixels.data(), width) == 0)
	{
		return false;
	}

	return true;
}

bool write_heightmap_tiff(const std::vector<std::vector<float>>& heightmap, const char* filename) {
	int width = heightmap[0].size();
	int height = heightmap.size();

	// Create a TIFF file with 32-bit depth, 1 sample per pixel (grayscale), and float format
	TinyTIFFWriterFile* tif = TinyTIFFWriter_open(filename, 32, TinyTIFFWriter_Float, 1, width, height,
		TinyTIFFWriter_Greyscale);
	if (!tif) {
		return false; // Error opening TIFF file
	}

	float min = FLT_MAX;
	float max = FLT_MIN;
	for (const auto& row : heightmap) {
		for (float value : row) {
			if (value < min) min = value;
			if (value > max) max = value;
		}
	}

	std::vector<float> scaledData(width * height);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			scaledData[(height - y - 1) * width + x] = (heightmap[y][x] - min) / (max - min); // Scale the value to [0, 1] and flip y-axis
		}
	}

	// Write the entire image to TIFF
	TinyTIFFWriter_writeImage(tif, scaledData.data());

	// Close the TIFF file
	TinyTIFFWriter_close(tif);

	return true;
}


