#pragma once
#include "FFNAType.h"
#include <array>
#include <stdint.h>

inline int decode_filename(int id0, int id1) { return (id0 - 0xff00ff) + (id1 * 0xff00); }

inline uint32_t get_fvf(uint32_t dat_fvf)
{
    return (dat_fvf & 0xff0) << 4 | dat_fvf >> 8 & 0x30 | dat_fvf & 0xf;
}

inline uint32_t get_vertex_size_from_fvf(uint32_t fvf)
{
    // Just the arrays storing the size each FVF flag adds to the vertex size.
    // There might be some DX9 function I could call to get the size from the FVF.
    // But I'm not familiar with DX9 so I'll just do it like the GW source.
    constexpr uint32_t fvf_array_0[22] = {0x0, 0x8,  0x8,  0x10, 0x8,        0x10,      0x10, 0x18,
                                          0x8, 0x10, 0x10, 0x18, 0x10,       0x18,      0x18, 0x20,
                                          0x0, 0x0,  0x0,  0x1,  0xFFFFFFFF, 0xFFFFFFFF};
    constexpr uint32_t fvf_array_1[8] = {0x0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24};
    constexpr uint32_t fvf_array_2[16] = {0x0, 0xC,  0x4, 0x10, 0xC,  0x18, 0x10, 0x1C,
                                          0x4, 0x10, 0x8, 0x14, 0x10, 0x1C, 0x14, 0x20};

    return fvf_array_0[fvf >> 0xc & 0xf] + fvf_array_0[fvf >> 8 & 0xf] + fvf_array_1[fvf >> 4 & 7] +
      fvf_array_2[fvf & 0xf];
}

struct ModelVertex
{
    float x;
    float y;
    float z;
    std::vector<float> dunno;

    ModelVertex() = default;
    ModelVertex(size_t dunno_size)
        : dunno(dunno_size)
    {
    }
};

struct Chunk1_sub1
{
    uint32_t some_type_maybe;
    uint32_t f0x4;
    uint32_t f0x8;
    uint32_t f0xC;
    uint32_t f0x10;
    uint8_t f0x14;
    uint8_t f0x15;
    uint8_t f0x16;
    uint8_t f0x17;
    uint32_t some_num0;
    uint32_t some_num1;
    uint32_t f0x20;
    uint8_t f0x24[8];
    uint32_t f0x2C;
    uint8_t num_some_struct;
    uint8_t f0x31[7];
    uint32_t f0x38;
    uint32_t f0x3C;
    uint32_t f0x40;
    uint32_t num_models_maybe;
    uint32_t f0x48;
    uint16_t f0x4C;
    uint8_t f0x4E[2];
    uint16_t f0x50;
    uint16_t num_some_struct2;

    Chunk1_sub1() = default;

    Chunk1_sub1(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }
};

struct GeometryChunk
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    Chunk1_sub1 sub_1;
    std::vector<uint8_t> unknown;
    std::vector<uint8_t> unknown2;
    std::vector<uint8_t> unknown3;
    uint32_t some_array_size;
    std::vector<uint8_t> some_array;
    uint32_t num_indices;
    uint32_t num_indices_cpy;
    uint32_t num_indices_cpy2;
    uint32_t num_vertices;
    uint32_t dat_fvf;
    std::vector<uint8_t> unknown1;
    std::vector<uint16_t> indices;
    std::vector<ModelVertex> vertices;
    std::vector<uint8_t> chunk_data;

    GeometryChunk() = default;

    GeometryChunk(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));

        int curr_offset = offset + 8;
        sub_1 = Chunk1_sub1(&data[curr_offset]);
        curr_offset += sizeof(sub_1);
        if (sub_1.num_models_maybe > 0)
        {

            uint32_t unknown_size =
              sub_1.some_num0 * 8 + sub_1.some_num1 * 9 + (-(sub_1.f0x20 != 0) & sub_1.some_num1);
            unknown.resize(unknown_size);
            std::memcpy(unknown.data(), &data[curr_offset], unknown_size);
            curr_offset += unknown_size;

            if (sub_1.num_some_struct2 > 0)
            {
                // unknown2
                uint32_t unknown2_size = sub_1.num_some_struct2 * 0x30;
                unknown2.resize(unknown2_size);
                std::memcpy(unknown2.data(), &data[curr_offset], unknown2_size);
                curr_offset += unknown2_size;

                // unknown3
                uint32_t unknown3_size = unknown2[0x28] * 0x18 + unknown2[0x2C] * 0x10;
                unknown3.resize(unknown3_size);
                std::memcpy(unknown3.data(), &data[curr_offset], unknown3_size);
                curr_offset += unknown3_size;
            }

            std::memcpy(&some_array_size, &data[curr_offset], sizeof(some_array_size));
            curr_offset += sizeof(some_array_size);

            if (some_array_size > 18)
            {
                some_array.resize(some_array_size + 8);
                std::memcpy(some_array.data(), &data[curr_offset], some_array_size + 8);
                curr_offset += some_array.size();
            }

            std::memcpy(&num_indices, &data[curr_offset], sizeof(num_indices));
            curr_offset += sizeof(num_indices);

            std::memcpy(&num_indices_cpy, &data[curr_offset], sizeof(num_indices_cpy));
            curr_offset += sizeof(num_indices_cpy);

            std::memcpy(&num_indices_cpy2, &data[curr_offset], sizeof(num_indices_cpy2));
            curr_offset += sizeof(num_indices_cpy2);

            std::memcpy(&num_vertices, &data[curr_offset], sizeof(num_vertices));
            curr_offset += sizeof(num_vertices);

            std::memcpy(&dat_fvf, &data[curr_offset], sizeof(dat_fvf));
            curr_offset += sizeof(dat_fvf);

            uint32_t unknown1_size = 12;
            unknown1.resize(unknown1_size);
            std::memcpy(unknown1.data(), &data[curr_offset], unknown1_size);
            curr_offset += unknown1_size;

            uint32_t vertex_size = get_vertex_size_from_fvf(get_fvf(dat_fvf));
            size_t dunno_size = (vertex_size - 3 * sizeof(float)) / sizeof(float);

            indices.resize(num_indices);
            std::memcpy(indices.data(), &data[curr_offset], num_indices * sizeof(uint16_t));
            curr_offset += num_indices * sizeof(uint16_t);

            vertices.resize(num_vertices);
            for (uint32_t i = 0; i < num_vertices; ++i)
            {
                ModelVertex vertex(dunno_size);
                std::memcpy(&vertex.x, &data[curr_offset], sizeof(vertex.x));
                curr_offset += sizeof(vertex.x);

                std::memcpy(&vertex.y, &data[curr_offset], sizeof(vertex.y));
                curr_offset += sizeof(vertex.y);

                std::memcpy(&vertex.z, &data[curr_offset], sizeof(vertex.z));
                curr_offset += sizeof(vertex.z);

                std::memcpy(vertex.dunno.data(), &data[curr_offset], dunno_size * sizeof(float));
                curr_offset += dunno_size * sizeof(float);

                vertices[i] = vertex;
            }
        }
        // Copy remaining chunk_data after reading all other fields
        if (curr_offset < chunk_size + 8)
        {
            size_t remaining_bytes = chunk_size + 5 + 8 - curr_offset;
            chunk_data.resize(remaining_bytes);
            std::memcpy(chunk_data.data(), &data[curr_offset], remaining_bytes);
        }
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
