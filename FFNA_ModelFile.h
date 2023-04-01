#pragma once
#include "FFNAType.h"
#include <array>
#include <stdint.h>
#include "DXMathHelpers.h"
#include "Mesh.h"
#include "Vertex.h"

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
    uint8_t some_num0; // Num pixel shaders?
    uint8_t f0x19;
    uint8_t f0x1a;
    uint8_t f0x1b;
    uint8_t some_num1;
    uint8_t f0x1d;
    uint8_t f0x1e;
    uint8_t f0x1f;
    uint32_t f0x20;
    uint8_t f0x24[8];
    uint32_t f0x2C;
    uint8_t num_some_struct0;
    uint8_t f0x31[7];
    uint32_t f0x38;
    uint32_t f0x3C;
    uint32_t f0x40;
    uint32_t num_models;
    uint32_t f0x48;
    uint16_t f0x4C;
    uint8_t f0x4E[2];
    uint16_t f0x50;
    uint16_t num_some_struct2;

    Chunk1_sub1() = default;

    Chunk1_sub1(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }
};

struct ComplexStruct
{
    uint32_t u0x0;
    uint32_t u0x4;
    uint32_t u0x8;
    uint32_t u0xC;
    uint16_t u0x10;
    uint8_t u0x12;
    uint8_t u0x13;
    uint16_t u0x14;
    uint32_t u0x16;
    uint32_t u0x1A;
    uint32_t u0x1E;
    uint16_t u0x22;
    uint16_t u0x24;
    uint16_t u0x26;
    uint16_t u0x28;
    uint32_t u0x2A;

    std::vector<uint8_t> struct_data;

    ComplexStruct() = default;
    ComplexStruct(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
                  bool& parsed_correctly, Chunk1_sub1& sub_1)
    {
        if (curr_offset + 0x2E >= data_size_bytes)
        {
            parsed_correctly = false;
            return;
            return;
        }

        u0x0 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x4 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x8 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0xC = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x10 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x12 = *reinterpret_cast<const uint8_t*>(data + curr_offset);
        curr_offset += sizeof(uint8_t);

        u0x13 = *reinterpret_cast<const uint8_t*>(data + curr_offset);
        curr_offset += sizeof(uint8_t);

        u0x14 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x16 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x1A = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x1E = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        u0x22 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x24 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x26 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x28 = *reinterpret_cast<const uint16_t*>(data + curr_offset);
        curr_offset += sizeof(uint16_t);

        u0x2A = *reinterpret_cast<const uint32_t*>(data + curr_offset);
        curr_offset += sizeof(uint32_t);

        uint32_t uVar2 = u0x14;
        uint32_t iVar3 = 0;
        if ((u0xC & 2) == 0)
        {
            iVar3 = uVar2 - u0x28;
        }
        uint32_t uVar4 = uVar2;
        if ((u0xC & 0x40) == 0)
        {
            uVar4 = u0x1A;
        }

        uint32_t res0 = (u0x26 + u0x24) * 2;
        res0 += u0x22;
        res0 += u0x4;
        res0 += uVar4;
        res0 += u0x0;

        uint32_t res1 = (iVar3 + u0x1E * 2) * 9;
        uint32_t res2 = res1 + res0 * 2 + u0x2A + u0x16;
        uint32_t res3 = (u0x13 * 8 + 0xC) * uVar2;

        uint32_t size = res3 + res2 * 2;

        if (curr_offset + size < data_size_bytes && (curr_offset + size) > size)
        {
            struct_data.resize(size);
            std::memcpy(struct_data.data(), &data[curr_offset], struct_data.size());
            curr_offset += size;
        }
        else
        {
            parsed_correctly = false;
            return;
        }
    }
};

struct GeometryModel
{
    uint32_t unknown;
    uint32_t num_indices0;
    uint32_t num_indices1;
    uint32_t num_indices2;
    uint32_t num_vertices;
    uint32_t dat_fvf;
    uint32_t u0;
    uint32_t u1;
    uint32_t u2;
    std::vector<uint16_t> indices;
    std::vector<ModelVertex> vertices;
    std::vector<uint8_t> extra_data; // Has size: (u0 + u1 + u2 * 3) * 4

    // Extra properties that I've added. Not from model file
    uint32_t total_num_indices;
    float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::min();
    float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::min();
    float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::min();
    float sumX = 0, sumY = 0, sumZ = 0, avgX = 0, avgY = 0, avgZ = 0;

    GeometryModel() = default;
    GeometryModel(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
                  bool& parsed_correctly, int chunk_size)
    {
        if (curr_offset + 0x24 >= data_size_bytes)
        {
            parsed_correctly = false;
            return;
            return;
        }

        std::memcpy(&unknown, &data[curr_offset], sizeof(unknown));
        curr_offset += sizeof(unknown);

        std::memcpy(&num_indices0, &data[curr_offset], sizeof(num_indices0));
        curr_offset += sizeof(num_indices0);

        std::memcpy(&num_indices1, &data[curr_offset], sizeof(num_indices1));
        curr_offset += sizeof(num_indices1);

        std::memcpy(&num_indices2, &data[curr_offset], sizeof(num_indices2));
        curr_offset += sizeof(num_indices2);

        std::memcpy(&num_vertices, &data[curr_offset], sizeof(num_vertices));
        curr_offset += sizeof(num_vertices);

        std::memcpy(&dat_fvf, &data[curr_offset], sizeof(dat_fvf));
        curr_offset += sizeof(dat_fvf);

        std::memcpy(&u0, &data[curr_offset], sizeof(u0));
        curr_offset += sizeof(u0);

        std::memcpy(&u1, &data[curr_offset], sizeof(u1));
        curr_offset += sizeof(u1);

        std::memcpy(&u2, &data[curr_offset], sizeof(u2));
        curr_offset += sizeof(u2);

        uint32_t vertex_size = get_vertex_size_from_fvf(get_fvf(dat_fvf));
        size_t dunno_size = vertex_size > 12 ? (vertex_size - 3 * sizeof(float)) / sizeof(float) : 0;

        total_num_indices = num_indices0 + (num_indices0 != num_indices1) * num_indices1 +
          (num_indices1 != num_indices2) * num_indices2;

        if (curr_offset + total_num_indices * 2 < data_size_bytes && total_num_indices < 1000000)
        {
            indices.resize(total_num_indices * 2);
            std::memcpy(indices.data(), &data[curr_offset], total_num_indices * 2);
            curr_offset += total_num_indices * 2;
        }
        else
        {
            parsed_correctly = false;
            return;
        }

        if (vertex_size >= 8 && curr_offset + num_vertices * vertex_size < data_size_bytes &&
            num_vertices < 2000000)
        {

            vertices.resize(num_vertices);
            for (uint32_t i = 0; i < num_vertices; ++i)
            {
                ModelVertex vertex(dunno_size);
                std::memcpy(&vertex.x, &data[curr_offset], sizeof(vertex.x));
                curr_offset += sizeof(vertex.x);

                std::memcpy(&vertex.z, &data[curr_offset], sizeof(vertex.z));
                curr_offset += sizeof(vertex.z);

                std::memcpy(&vertex.y, &data[curr_offset], sizeof(vertex.y));
                vertex.y = -vertex.y;
                curr_offset += sizeof(vertex.y);

                std::memcpy(vertex.dunno.data(), &data[curr_offset], dunno_size * sizeof(float));
                curr_offset += dunno_size * sizeof(float);

                vertices[i] = vertex;

                // Update min, max, and sum for each coordinate
                minX = std::min(minX, vertex.x);
                maxX = std::max(maxX, vertex.x);
                minY = std::min(minY, vertex.y);
                maxY = std::max(maxY, vertex.y);
                minZ = std::min(minZ, vertex.z);
                maxZ = std::max(maxZ, vertex.z);
                sumX += vertex.x;
                sumY += vertex.y;
                sumZ += vertex.z;
            }

            // Calculate the averages
            avgX = sumX / num_vertices;
            avgY = sumY / num_vertices;
            avgZ = sumZ / num_vertices;
        }
        else
        {
            parsed_correctly = false;
            return;
        }
        uint32_t extra_data_size = (u0 + u1 + u2 * 3) * 4;
        if (curr_offset + extra_data_size < data_size_bytes && u0 < 10000 && u1 < 10000 && u2 < 10000)
        {
            extra_data.resize(extra_data_size);
            std::memcpy(extra_data.data(), &data[curr_offset], extra_data_size);
            curr_offset += extra_data.size();
        }
        else
        {
            parsed_correctly = false;
            return;
        }
    }
};

struct InteractiveModelMaybe
{
    uint32_t num_indices;
    uint32_t num_vertices;
    std::vector<uint16_t> indices;
    std::vector<Vertex> vertices;

    InteractiveModelMaybe() = default;
    InteractiveModelMaybe(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
                          bool& parsed_correctly)
    {
        // Read num_indices and num_vertices
        if (curr_offset + sizeof(num_indices) + sizeof(num_vertices) > data_size_bytes)
        {
            parsed_correctly = false;
            return;
            return;
        }

        std::memcpy(&num_indices, &data[curr_offset], sizeof(num_indices));
        curr_offset += sizeof(num_indices);

        std::memcpy(&num_vertices, &data[curr_offset], sizeof(num_vertices));
        curr_offset += sizeof(num_vertices);

        // Read indices
        if (curr_offset + num_indices * sizeof(uint16_t) <= data_size_bytes)
        {
            indices.resize(num_indices);
            std::memcpy(indices.data(), &data[curr_offset], num_indices * sizeof(uint16_t));
            curr_offset += num_indices * sizeof(uint16_t);
        }
        else
        {
            parsed_correctly = false;
            return;
            return;
        }

        // Read vertices
        if (curr_offset + num_vertices * sizeof(ModelVertex(0)) <= data_size_bytes)
        {
            vertices.resize(num_vertices);
            std::memcpy(vertices.data(), &data[curr_offset], num_vertices * sizeof(ModelVertex(0)));
            curr_offset += num_vertices * sizeof(ModelVertex(0));
        }
        else
        {
            parsed_correctly = false;
            return;
            return;
        }
    }
};

struct GeometryChunk
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    Chunk1_sub1 sub_1;
    std::vector<uint8_t> unknown;
    std::vector<uint8_t> unknown2;
    std::vector<uint8_t> unknown3;
    std::vector<uint8_t> unknown_data_0;
    std::vector<uint8_t> unknown_data_1;
    std::vector<std::string> strings;

    uint32_t unknown4;
    uint32_t unknown5;
    std::vector<ComplexStruct> complex_structs;
    std::vector<GeometryModel> models;
    std::vector<uint8_t> chunk_data;

    uint32_t compute_str_len_plus_one(const unsigned char* data, uint32_t address)
    {
        uint32_t counter = 0;
        bool found = false;
        while (! found)
        {
            uint8_t curr_char = data[address + counter];
            if (curr_char == 0)
            {
                found = true;
            }
            counter += 1;
        }
        return counter;
    }

    GeometryChunk() = default;

    GeometryChunk(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
                  bool& parsed_correctly)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));

        uint32_t curr_offset = offset + 8;
        sub_1 = Chunk1_sub1(&data[curr_offset]);
        curr_offset += sizeof(sub_1);
        if (sub_1.num_models > 0)
        {

            uint32_t unknown_size =
              sub_1.some_num0 * 8 + sub_1.some_num1 * 9 + (-(sub_1.f0x20 != 0) & sub_1.some_num1);
            if (unknown_size < chunk_size)
            {
                unknown.resize(unknown_size);
                std::memcpy(unknown.data(), &data[curr_offset], unknown_size);
                curr_offset += unknown_size;
            }

            if (sub_1.f0x19 > 0)
            {
                uint32_t puVar15 = sub_1.f0x19 * 9;
                if (curr_offset + puVar15 < data_size_bytes)
                {
                    uint32_t puVar16 = sub_1.f0x1d * ((sub_1.f0x20 != 0) + 3) + puVar15;
                    if (curr_offset + puVar16 < data_size_bytes)
                    {
                        uint32_t _Src = puVar16 + sub_1.f0x1a * 8;
                        if (curr_offset + _Src < data_size_bytes)
                        {
                            unknown_data_0.resize(_Src);
                            std::memcpy(unknown_data_0.data(), &data[curr_offset], _Src);
                            curr_offset += _Src;

                            strings.resize(sub_1.f0x1a);
                            for (uint32_t i = 0; i < sub_1.f0x1a; ++i)
                            {
                                uint32_t str_len = compute_str_len_plus_one(data, curr_offset);
                                strings[i] =
                                  std::string(reinterpret_cast<const char*>(&data[curr_offset]), str_len - 1);
                                curr_offset += str_len;
                            }

                            unknown_data_1.resize(sub_1.f0x1e * 2);
                            std::memcpy(unknown_data_1.data(), &data[curr_offset], sub_1.f0x1e * 2);
                            curr_offset += sub_1.f0x1e * 2;
                        }
                        else
                        {
                            unknown_data_0.resize(puVar16);
                            std::memcpy(unknown_data_0.data(), &data[curr_offset], puVar16);
                            curr_offset += puVar16;
                            parsed_correctly = false;
                            return;
                        }
                    }
                    else
                    {
                        unknown_data_0.resize(puVar15);
                        std::memcpy(unknown_data_0.data(), &data[curr_offset], puVar15);
                        curr_offset += puVar15;
                        parsed_correctly = false;
                        return;
                    }
                }
                else
                {
                    parsed_correctly = false;
                    return;
                }
            }

            if ((sub_1.f0x8 & 0x20))
            {
                if (curr_offset + 8 > data_size_bytes)
                {
                    parsed_correctly = false;
                    return;
                }
                else
                {
                    unknown4 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
                    curr_offset += sizeof(uint32_t);

                    unknown5 = *reinterpret_cast<const uint32_t*>(data + curr_offset);
                    curr_offset += sizeof(uint32_t);

                    if (curr_offset + unknown5 * sizeof(ComplexStruct) > data_size_bytes)
                    {
                        parsed_correctly = false;
                        return;
                    }
                    for (int i = 0; i < unknown5; i++)
                    {

                        complex_structs.push_back(
                          ComplexStruct(curr_offset, data, data_size_bytes, parsed_correctly, sub_1));
                    }
                }
            }

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

            for (int i = 0; i < sub_1.num_models; i++)
            {
                auto new_model =
                  GeometryModel(curr_offset, data, data_size_bytes, parsed_correctly, chunk_size);
                models.push_back(new_model);
            }
        }
        else
        {
            parsed_correctly = false;
            return;
        }

        // Copy remaining chunk_data after reading all other fields
        size_t remaining_bytes = chunk_size + 5 + 8 - curr_offset;
        if (curr_offset + remaining_bytes <= offset + chunk_size + 8 && remaining_bytes < chunk_size)
        {
            chunk_data.resize(remaining_bytes);
            std::memcpy(chunk_data.data(), &data[curr_offset], remaining_bytes);
        }
        else
        {
            parsed_correctly = false;
            return;
        }
    }
};

constexpr uint32_t CHUNK_ID_GEOMETRY = 0x00000FA0;

struct FFNA_ModelFile
{
    char ffna_signature[4];
    FFNAType ffna_type;
    GeometryChunk geometry_chunk;

    bool parsed_correctly = true;

    std::unordered_map<uint32_t, int> riff_chunks;

    FFNA_ModelFile() = default;
    FFNA_ModelFile(int offset, std::span<unsigned char>& data)
    {
        uint32_t current_offset = offset;

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
            geometry_chunk = GeometryChunk(offset, data.data(), data.size_bytes(), parsed_correctly);
        }
    }

    Mesh GetMesh(int model_index)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto sub_model = geometry_chunk.models[model_index];
        //if (sub_model.unknown != model_index)
        //    return Mesh({}, {});

        for (int i = 0; i < sub_model.vertices.size(); i++)
        {
            ModelVertex model_vertex = sub_model.vertices[i];
            Vertex vertex;

            vertex.position = XMFLOAT3(model_vertex.x, model_vertex.y, model_vertex.z);
            vertex.tex_coord = XMFLOAT2(model_vertex.dunno[0], model_vertex.dunno[1]);
            vertices.push_back(vertex);
        }

        for (int i = 0; i < sub_model.indices.size(); i += 3)
        {
            int index0 = sub_model.indices[i];
            int index1 = sub_model.indices[i + 1];
            int index2 = sub_model.indices[i + 2];

            if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
            {
                return Mesh({}, {});
            }

            //if (index0 == index1 || index0 == index2 || index1 == index2)
            //    continue;

            XMFLOAT3 normal =
              compute_normal(vertices[index0].position, vertices[index1].position, vertices[index2].position);

            vertices[index0].normal = normal;
            vertices[index1].normal = normal;
            vertices[index2].normal = normal;

            indices.push_back(index0);
            indices.push_back(index1);
            indices.push_back(index2);
        }

        return Mesh(vertices, indices);
    }
};
