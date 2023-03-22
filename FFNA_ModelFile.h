#pragma once
#include "FFNAType.h"
#include <array>
#include <stdint.h>

struct GeometryChunk
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    std::vector<uint8_t> chunk_data;

    GeometryChunk() = default;
    GeometryChunk(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        chunk_data.resize(chunk_size);
        std::memcpy(chunk_data.data(), &data[offset + 8], chunk_size);
    }
};

constexpr uint32_t CHUNK_ID_GEOMETRY = 0x00000FA0;

struct FFNA_ModelFile
{
    char ffna_signature[4];
    FFNAType ffna_type;
    GeometryChunk geometry_chunk;

    std::unordered_map<uint32_t, int> riff_chunks;

    FFNA_ModelFile() = default;
    FFNA_ModelFile(int offset, std::span<unsigned char>& data)
    {
        int current_offset = offset;

        std::memcpy(ffna_signature, &data[offset], sizeof(ffna_signature));
        std::memcpy(&ffna_type, &data[offset], sizeof(ffna_type));
        current_offset += 5;

        // Read all chunks
        while (current_offset < data.size())
        {
            // Create a GeneralChunk instance using the current offset and data pointer
            GeneralChunk chunk(current_offset, data.data());

            // Add the GeneralChunk instance to the riff_chunks map using its chunk_id as the key
            riff_chunks.emplace(chunk.chunk_id, current_offset);

            // Move to the next chunk by updating the current_offset
            current_offset += 8 + chunk.chunk_size;
        }

        // Check if the CHUNK_ID_TERRAIN is in the riff_chunks map
        auto it = riff_chunks.find(CHUNK_ID_GEOMETRY);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            geometry_chunk = GeometryChunk(offset, data.data());
        }
    }
};
