#pragma once
#include "FFNAType.h"
#include <array>
#include <stdint.h>

struct MapBounds
{
    float map_min_x;
    float map_min_y;
    float map_min_z;
    float map_max_x;
    float map_max_y;
    float map_max_z;

    MapBounds() = default;
    MapBounds(int offset, const unsigned char* data)
    {
        std::memcpy(&map_min_x, &data[offset], sizeof(map_min_x));
        std::memcpy(&map_min_z, &data[offset + 4], sizeof(map_min_z));
        std::memcpy(&map_max_x, &data[offset + 8], sizeof(map_max_x));
        std::memcpy(&map_max_z, &data[offset + 12], sizeof(map_max_z));
    }
};

struct Chunk1
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    std::vector<uint8_t> chunk_data;

    Chunk1() = default;
    Chunk1(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        chunk_data.resize(chunk_size);
        std::memcpy(chunk_data.data(), &data[offset + 8], chunk_size);
    }
};

struct Chunk2
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magic_num;
    uint8_t always_2;
    MapBounds map_bounds;
    uint32_t f0;
    uint32_t f1;
    uint32_t f2;
    uint32_t f3;
    uint32_t f4;

    Chunk2() = default;
    Chunk2(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        std::memcpy(&magic_num, &data[offset + 8], sizeof(magic_num));
        std::memcpy(&always_2, &data[offset + 12], sizeof(always_2));
        map_bounds = MapBounds(offset + 13, data);
        std::memcpy(&f0, &data[offset + 29], sizeof(f0));
        std::memcpy(&f1, &data[offset + 33], sizeof(f1));
        std::memcpy(&f2, &data[offset + 37], sizeof(f2));
        std::memcpy(&f3, &data[offset + 41], sizeof(f3));
        std::memcpy(&f4, &data[offset + 45], sizeof(f4));
    }
};

#pragma pack(push, 1)
struct PropInfo
{
    uint16_t filename_index;
    float x;
    float y;
    float z;
    float f4;
    float f5;
    float f6;
    float sin_angle; // cos of angle in radians
    float cos_angle; // sin of angle in radians
    float f9;
    float scaling_factor;
    float f11;
    uint8_t f12;
    uint8_t num_some_structs;
    std::vector<uint8_t> some_structs; // has size: num_some_structs * 8

    PropInfo() = default;
    PropInfo(int offset, const unsigned char* data)
    {
        std::memcpy(&filename_index, &data[offset], sizeof(filename_index));
        std::memcpy(&x, &data[offset + 2], sizeof(x));
        std::memcpy(&z, &data[offset + 6], sizeof(y));
        std::memcpy(&y, &data[offset + 10], sizeof(z));
        y = -y;
        std::memcpy(&f4, &data[offset + 14], sizeof(f4));
        std::memcpy(&f5, &data[offset + 18], sizeof(f5));
        std::memcpy(&f6, &data[offset + 22], sizeof(f6));
        std::memcpy(&sin_angle, &data[offset + 26], sizeof(sin_angle));
        std::memcpy(&cos_angle, &data[offset + 30], sizeof(cos_angle));
        std::memcpy(&f9, &data[offset + 34], sizeof(f9));
        std::memcpy(&scaling_factor, &data[offset + 38], sizeof(scaling_factor));
        std::memcpy(&f11, &data[offset + 42], sizeof(f11));
        std::memcpy(&f12, &data[offset + 46], sizeof(f12));
        std::memcpy(&num_some_structs, &data[offset + 47], sizeof(num_some_structs));

        some_structs.resize(num_some_structs * 8);
        std::memcpy(some_structs.data(), &data[offset + 48], some_structs.size());
    }
};
#pragma pack(pop)

struct PropArray
{
    uint16_t num_props;
    std::vector<PropInfo> props_info;

    PropArray() = default;
    PropArray(int offset, const unsigned char* data)
    {
        std::memcpy(&num_props, &data[offset], sizeof(num_props));

        offset += sizeof(num_props);
        for (int prop_index = 0; prop_index < num_props; prop_index++)
        {
            auto new_prop_info = PropInfo(offset, data);
            props_info.push_back(new_prop_info);
            offset +=
              sizeof(new_prop_info) - sizeof(new_prop_info.some_structs) + new_prop_info.some_structs.size();
        }
    }
};

struct SomeVertex
{
    float x;
    float y;
    float z;
    uint32_t f6;
    uint32_t f7;
    uint32_t f8;

    SomeVertex() = default;
    SomeVertex(int offset, const unsigned char* data)
    {
        std::memcpy(&x, &data[offset], sizeof(x));
        std::memcpy(&y, &data[offset + 4], sizeof(y));
        std::memcpy(&z, &data[offset + 8], sizeof(z));
        std::memcpy(&f6, &data[offset + 12], sizeof(f6));
        std::memcpy(&f7, &data[offset + 16], sizeof(f7));
        std::memcpy(&f8, &data[offset + 20], sizeof(f8));
    }
};

struct Vertex2
{
    float x;
    float y;

    Vertex2() = default;
    Vertex2(int offset, const unsigned char* data)
    {
        std::memcpy(&x, &data[offset], sizeof(x));
        std::memcpy(&y, &data[offset + 4], sizeof(y));
    }
};

struct SomeVertexData
{
    uint8_t f0;
    uint32_t array_size_in_bytes;
    uint32_t num_elements;
    std::vector<SomeVertex> vertices;

    SomeVertexData() = default;
    SomeVertexData(int offset, const unsigned char* data)
    {
        std::memcpy(&f0, &data[offset], sizeof(f0));
        std::memcpy(&array_size_in_bytes, &data[offset + 1], sizeof(array_size_in_bytes));
        std::memcpy(&num_elements, &data[offset + 5], sizeof(num_elements));

        vertices.resize(num_elements);
        int vertex_offset = offset + 9;
        for (uint32_t i = 0; i < num_elements; i++)
        {
            vertices[i] = SomeVertex(vertex_offset, data);
            vertex_offset += sizeof(SomeVertex);
        }
    }
};

struct SomeData
{
    uint8_t f0;
    uint32_t array_size_in_bytes;
    uint32_t num_elements;
    std::vector<uint16_t> array;

    SomeData() = default;

    SomeData(int offset, const unsigned char* data)
    {
        std::memcpy(&f0, &data[offset], sizeof(f0));
        std::memcpy(&array_size_in_bytes, &data[offset + 1], sizeof(array_size_in_bytes));
        std::memcpy(&num_elements, &data[offset + 5], sizeof(num_elements));

        array.resize(num_elements);
        std::memcpy(array.data(), &data[offset + 9], array.size() * sizeof(uint16_t));
    }
};

struct SomeData1
{
    uint8_t f0;
    uint32_t array_size_in_bytes;
    uint32_t num_elements;
    std::vector<Vertex2> array;

    SomeData1() = default;

    SomeData1(int offset, const unsigned char* data)
    {
        std::memcpy(&f0, &data[offset], sizeof(f0));
        std::memcpy(&array_size_in_bytes, &data[offset + 1], sizeof(array_size_in_bytes));
        std::memcpy(&num_elements, &data[offset + 5], sizeof(num_elements));

        array.resize(num_elements);
        std::memcpy(array.data(), &data[offset + 9], sizeof(Vertex2) * num_elements);
    }
};

struct SomeData2Struct
{
    uint16_t f0;
    uint16_t prop_index;

    SomeData2Struct() = default;

    SomeData2Struct(int offset, const unsigned char* data)
    {
        std::memcpy(&f0, &data[offset], sizeof(f0));
        std::memcpy(&prop_index, &data[offset + 2], sizeof(prop_index));
    }
};

struct SomeData2
{
    uint8_t f0;
    uint32_t array_size_in_bytes;
    uint16_t num_elements;
    std::vector<SomeData2Struct> array;

    SomeData2() = default;

    SomeData2(int offset, const unsigned char* data)
    {
        std::memcpy(&f0, &data[offset], sizeof(f0));
        std::memcpy(&array_size_in_bytes, &data[offset + 1], sizeof(array_size_in_bytes));
        std::memcpy(&num_elements, &data[offset + 5], sizeof(num_elements));

        array.resize(num_elements);
        std::memcpy(array.data(), &data[offset + 7], array_size_in_bytes - sizeof(num_elements));
    }
};

struct Chunk3
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magic_number;
    uint16_t magic_number2;
    uint32_t prop_array_size_in_bytes;
    PropArray prop_array;
    SomeVertexData some_vertex_data;
    SomeData some_data0;
    SomeData1 some_data1;
    SomeData2 some_data2;
    std::vector<uint8_t> chunk_data;

    Chunk3() = default;

    Chunk3(int offset, const unsigned char* data)
    {
        int initial_offset = offset;

        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(&magic_number, &data[offset], sizeof(magic_number));
        offset += sizeof(magic_number);

        std::memcpy(&magic_number2, &data[offset], sizeof(magic_number2));
        offset += sizeof(magic_number2);

        std::memcpy(&prop_array_size_in_bytes, &data[offset], sizeof(prop_array_size_in_bytes));
        offset += sizeof(prop_array_size_in_bytes);

        prop_array = PropArray(offset, data);
        offset += prop_array_size_in_bytes;

        some_vertex_data = SomeVertexData(offset, data);
        offset += some_vertex_data.array_size_in_bytes + sizeof(some_vertex_data.f0) +
          sizeof(some_vertex_data.array_size_in_bytes);

        some_data0 = SomeData(offset, data);
        offset +=
          some_data0.array_size_in_bytes + sizeof(some_data0.f0) + sizeof(some_data0.array_size_in_bytes);

        some_data1 = SomeData1(offset, data);
        offset +=
          some_data1.array_size_in_bytes + sizeof(some_data1.f0) + sizeof(some_data1.array_size_in_bytes);

        some_data2 = SomeData2(offset, data);
        offset +=
          some_data2.array_size_in_bytes + sizeof(some_data2.f0) + sizeof(some_data2.array_size_in_bytes);

        int remaining_bytes = chunk_size + 8 - (offset - initial_offset);
        chunk_data.resize(remaining_bytes);
        std::memcpy(chunk_data.data(), &data[offset], chunk_data.size());
    }
};

#pragma pack(push, 1) // So that sizeof(Chunk4DataHeader) returns 5 instead of being aligned to 8.
struct Chunk4DataHeader
{
    uint32_t signature;
    uint8_t version;

    Chunk4DataHeader() = default;

    Chunk4DataHeader(int& offset, const unsigned char* data)
    {
        std::memcpy(&signature, &data[offset], sizeof(signature));
        offset += sizeof(signature);

        std::memcpy(&version, &data[offset], sizeof(version));
        offset += sizeof(version);
    }
};
#pragma pack(pop) // Restore the original alignment

struct FileName
{
    uint16_t id0;
    uint16_t id1;

    FileName() = default;

    FileName(int& offset, const unsigned char* data)
    {
        std::memcpy(&id0, &data[offset], sizeof(id0));
        offset += sizeof(id0);

        std::memcpy(&id1, &data[offset], sizeof(id1));
        offset += sizeof(id1);
    }
};

struct Chunk4DataElement
{
    FileName filename;
    uint16_t f1;

    Chunk4DataElement() = default;

    Chunk4DataElement(int& offset, const unsigned char* data)
    {
        filename = FileName(offset, data);

        std::memcpy(&f1, &data[offset], sizeof(f1));
        offset += sizeof(f1);
    }
};

struct Chunk4
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    Chunk4DataHeader data_header;
    std::vector<Chunk4DataElement> array;
    std::vector<uint8_t> chunk_data;

    Chunk4() = default;

    Chunk4(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        data_header = Chunk4DataHeader(offset, data);

        int array_size = (chunk_size - sizeof(data_header)) / 6;
        array.reserve(array_size);
        for (int i = 0; i < array_size; ++i)
        {
            array.emplace_back(offset, data);
        }

        int chunk_data_size = chunk_size - sizeof(data_header) - sizeof(Chunk4DataElement) * array_size;
        chunk_data.resize(chunk_data_size);
        std::memcpy(chunk_data.data(), &data[offset], chunk_data.size());
    }
};

struct Chunk5Element
{
    uint8_t tag;
    uint32_t size;
    std::vector<uint8_t> data;

    Chunk5Element() = default;

    Chunk5Element(int& offset, const unsigned char* data)
    {
        std::memcpy(&tag, &data[offset], sizeof(tag));
        offset += sizeof(tag);

        std::memcpy(&size, &data[offset], sizeof(size));
        offset += sizeof(size);

        this->data.resize(size);
        std::memcpy(this->data.data(), &data[offset], size);
        offset += size;
    }
};

struct Chunk5Element1
{
    uint8_t tag;
    uint32_t size;
    uint32_t num_zones;

    Chunk5Element1() = default;

    Chunk5Element1(int& offset, const unsigned char* data)
    {
        std::memcpy(&tag, &data[offset], sizeof(tag));
        offset += sizeof(tag);

        std::memcpy(&size, &data[offset], sizeof(size));
        offset += sizeof(size);

        std::memcpy(&num_zones, &data[offset], sizeof(num_zones));
        offset += sizeof(num_zones);
    }
};

struct Chunk5Element2
{
    uint8_t unknown[20];
    uint8_t unknown1;
    uint16_t some_size;
    uint16_t unknown2;
    std::vector<uint8_t> some_data;
    uint32_t unknown3;
    uint32_t count2;
    std::vector<Vertex2> vertices;

    Chunk5Element2() = default;

    Chunk5Element2(int& offset, const unsigned char* data)
    {
        std::memcpy(unknown, &data[offset], sizeof(unknown));
        offset += sizeof(unknown);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&some_size, &data[offset], sizeof(some_size));
        offset += sizeof(some_size);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        if (some_size <= 0)
        {
            std::memcpy(&unknown3, &data[offset], sizeof(unknown3));
            offset += sizeof(unknown3);

            std::memcpy(&count2, &data[offset], sizeof(count2));
            offset += sizeof(count2);

            vertices.reserve(count2);
            for (uint32_t i = 0; i < count2; ++i)
            {
                vertices.emplace_back(offset, data);
                offset += sizeof(Vertex2);
            }
        }
        else
        {
            some_data.resize(some_size);
            std::memcpy(some_data.data(), &data[offset], some_size);
            offset += some_size;
        }
    }
};

struct Chunk5
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magic_num;
    uint32_t magic_num1;
    Chunk5Element element_0;
    Chunk5Element element_1;
    Chunk5Element1 element_2;
    Chunk5Element element_3;
    uint32_t unknown0;
    uint32_t unknown1;
    std::array<float, 8> unknown2;
    std::vector<Chunk5Element2> some_array;
    std::vector<uint8_t> chunk_data;

    Chunk5() = default;

    Chunk5(int offset, const unsigned char* data)
    {
        int initial_offset = offset;
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(&magic_num, &data[offset], sizeof(magic_num));
        offset += sizeof(magic_num);

        std::memcpy(&magic_num1, &data[offset], sizeof(magic_num1));
        offset += sizeof(magic_num1);

        element_0 = Chunk5Element(offset, data);
        element_1 = Chunk5Element(offset, data);
        element_2 = Chunk5Element1(offset, data);
        element_3 = Chunk5Element(offset, data);

        if (element_2.num_zones > 0)
        {
            std::memcpy(&unknown0, &data[offset], sizeof(unknown0));
            offset += sizeof(unknown0);

            std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
            offset += sizeof(unknown1);

            for (size_t i = 0; i < unknown2.size(); ++i)
            {
                std::memcpy(&unknown2[i], &data[offset], sizeof(float));
                offset += sizeof(float);
            }

            some_array.reserve(element_2.num_zones);
            for (uint32_t i = 0; i < element_2.num_zones; ++i)
            {
                some_array.emplace_back(offset, data);
            }
        }

        int remaining_bytes = chunk_size + 8 - (offset - initial_offset);
        if (remaining_bytes > 0)
        {
            chunk_data.resize(remaining_bytes);
            std::memcpy(chunk_data.data(), &data[offset], chunk_data.size());
        }
    }
};

struct Chunk8
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magic_num;
    uint32_t magic_num1;
    uint8_t tag;
    uint32_t some_size;
    uint32_t terrain_x_dims;
    uint32_t terrain_y_dims;
    float some_float;
    float some_small_float;
    uint16_t some_size1;
    float some_float1;
    float some_float2;
    uint8_t tag1;
    uint32_t terrain_height_size_bytes;
    std::vector<float> terrain_heightmap;
    uint8_t tag2;
    uint32_t num_terrain_tiles;
    std::vector<uint8_t> terrain_texture_indices_maybe;
    uint8_t tag3;
    uint32_t some_size2;
    uint8_t some_size3;
    std::vector<uint8_t> some_data3;
    uint8_t tag4;
    uint32_t some_size4;
    std::vector<uint8_t> some_data4;
    uint8_t tag5;
    uint32_t some_size5;
    std::vector<uint8_t> some_data5;
    uint8_t tag6;
    uint32_t num_terrain_tiles1;
    std::vector<uint8_t> terrain_texture_blend_weights_maybe;
    uint8_t tag7;
    uint32_t some_size7;
    std::vector<uint8_t> some_data7;
    std::vector<uint8_t> chunk_data;

    Chunk8() = default;
    Chunk8(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(&magic_num, &data[offset], sizeof(magic_num));
        offset += sizeof(magic_num);

        std::memcpy(&magic_num1, &data[offset], sizeof(magic_num1));
        offset += sizeof(magic_num1);

        std::memcpy(&tag, &data[offset], sizeof(tag));
        offset += sizeof(tag);

        std::memcpy(&some_size, &data[offset], sizeof(some_size));
        offset += sizeof(some_size);

        std::memcpy(&terrain_x_dims, &data[offset], sizeof(terrain_x_dims));
        offset += sizeof(terrain_x_dims);

        std::memcpy(&terrain_y_dims, &data[offset], sizeof(terrain_y_dims));
        offset += sizeof(terrain_y_dims);

        std::memcpy(&some_float, &data[offset], sizeof(some_float));
        offset += sizeof(some_float);

        std::memcpy(&some_small_float, &data[offset], sizeof(some_small_float));
        offset += sizeof(some_small_float);

        std::memcpy(&some_size1, &data[offset], sizeof(some_size1));
        offset += sizeof(some_size1);

        std::memcpy(&some_float1, &data[offset], sizeof(some_float1));
        offset += sizeof(some_float1);

        std::memcpy(&some_float2, &data[offset], sizeof(some_float2));
        offset += sizeof(some_float2);

        std::memcpy(&tag1, &data[offset], sizeof(tag1));
        offset += sizeof(tag1);

        std::memcpy(&terrain_height_size_bytes, &data[offset], sizeof(terrain_height_size_bytes));
        offset += sizeof(terrain_height_size_bytes);

        terrain_heightmap.resize(terrain_x_dims * terrain_y_dims);
        std::memcpy(terrain_heightmap.data(), &data[offset], terrain_heightmap.size() * sizeof(float));
        offset += terrain_heightmap.size() * sizeof(float);

        std::memcpy(&tag2, &data[offset], sizeof(tag2));
        offset += sizeof(tag2);

        std::memcpy(&num_terrain_tiles, &data[offset], sizeof(num_terrain_tiles));
        offset += sizeof(num_terrain_tiles);

        terrain_texture_indices_maybe.resize(num_terrain_tiles);
        std::memcpy(terrain_texture_indices_maybe.data(), &data[offset],
                    terrain_texture_indices_maybe.size());
        offset += terrain_texture_indices_maybe.size();

        std::memcpy(&tag3, &data[offset], sizeof(tag3));
        offset += sizeof(tag3);

        std::memcpy(&some_size2, &data[offset], sizeof(some_size2));
        offset += sizeof(some_size2);

        std::memcpy(&some_size3, &data[offset], sizeof(some_size3));
        offset += sizeof(some_size3);

        some_data3.resize(some_size3);
        std::memcpy(some_data3.data(), &data[offset], some_data3.size());
        offset += some_data3.size();

        std::memcpy(&tag4, &data[offset], sizeof(tag4));
        offset += sizeof(tag4);

        std::memcpy(&some_size4, &data[offset], sizeof(some_size4));
        offset += sizeof(some_size4);

        some_data4.resize(some_size4);
        std::memcpy(some_data4.data(), &data[offset], some_data4.size());
        offset += some_data4.size();

        std::memcpy(&tag5, &data[offset], sizeof(tag5));
        offset += sizeof(tag5);

        std::memcpy(&some_size5, &data[offset], sizeof(some_size5));
        offset += sizeof(some_size5);

        some_data5.resize(some_size5);
        std::memcpy(some_data5.data(), &data[offset], some_data5.size());
        offset += some_data5.size();

        std::memcpy(&tag6, &data[offset], sizeof(tag6));
        offset += sizeof(tag6);

        std::memcpy(&num_terrain_tiles1, &data[offset], sizeof(num_terrain_tiles1));
        offset += sizeof(num_terrain_tiles1);

        terrain_texture_blend_weights_maybe.resize(num_terrain_tiles1);
        std::memcpy(terrain_texture_blend_weights_maybe.data(), &data[offset],
                    terrain_texture_blend_weights_maybe.size());
        offset += terrain_texture_blend_weights_maybe.size();

        std::memcpy(&tag7, &data[offset], sizeof(tag7));
        offset += sizeof(tag7);

        std::memcpy(&some_size7, &data[offset], sizeof(some_size7));
        offset += sizeof(some_size7);

        some_data7.resize(some_size7);
        std::memcpy(some_data7.data(), &data[offset], some_data7.size());
        offset += some_data7.size();

        int chunk_data_size = chunk_size - 75 - terrain_heightmap.size() * sizeof(float) -
          terrain_texture_indices_maybe.size() - some_data3.size() - some_data4.size() - some_data5.size() -
          terrain_texture_blend_weights_maybe.size() - some_data7.size();
        chunk_data.resize(chunk_data_size);
        std::memcpy(chunk_data.data(), &data[offset], chunk_data.size());
    }
};

struct Chunk7
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    std::vector<uint8_t> chunk_data;

    Chunk7() = default;
    Chunk7(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        chunk_data.resize(chunk_size);
        std::memcpy(chunk_data.data(), &data[offset + 8], chunk_size);
    }
};

constexpr uint32_t CHUNK_ID_20000000 = 0x20000000;
constexpr uint32_t CHUNK_ID_TERRAIN = 0x20000002;
constexpr uint32_t CHUNK_ID_PROPS_INFO = 0x20000004;
constexpr uint32_t CHUNK_ID_MAP_INFO = 0x2000000C;
constexpr uint32_t CHUNK_ID_TERRAIN_FILENAMES = 0x21000002;
constexpr uint32_t CHUNK_ID_PROPS_FILENAMES0 = 0x21000004;
constexpr uint32_t CHUNK_ID_PROPS_FILENAMES = 0x21000004;

struct FFNA_MapFile
{
    char ffna_signature[4];
    FFNAType ffna_type;
    Chunk1 chunk1;
    Chunk2 map_info_chunk;
    Chunk3 props_info_chunk;
    Chunk4 prop_filenames_chunk;
    Chunk1 chunk5; // The actual chunk 5 doesn't work for all files. Haven't figured out the format yet.
    Chunk4 more_filnames_chunk; // same structure as chunk 4
    Chunk7 chunk7;
    Chunk8 terrain_chunk;
    Chunk4 terrain_texture_filenames; // same structure as chunk 4

    std::unordered_map<uint32_t, int> riff_chunks;

    FFNA_MapFile() = default;
    FFNA_MapFile(int offset, std::span<unsigned char>& data)
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

        //Check if the CHUNK_ID_20000000 is in the riff_chunks map
        auto it = riff_chunks.find(CHUNK_ID_20000000);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            chunk1 = Chunk1(offset, data.data());
        }

        // Check if the CHUNK_ID_TERRAIN is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_TERRAIN);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            terrain_chunk = Chunk8(offset, data.data());
        }

        // Check if the CHUNK_ID_PROPS_INFO is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_PROPS_INFO);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            props_info_chunk = Chunk3(offset, data.data());
        }

        //Check if the CHUNK_ID_MAP_INFO is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_MAP_INFO);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            map_info_chunk = Chunk2(offset, data.data());
        }

        // Check if the CHUNK_ID_PROPS_FILENAMES is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_PROPS_FILENAMES0);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            more_filnames_chunk = Chunk4(offset, data.data());
        }

        // Check if the CHUNK_ID_PROPS_FILENAMES is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_PROPS_FILENAMES);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            prop_filenames_chunk = Chunk4(offset, data.data());
        }

        // Check if the CHUNK_ID_TERRAIN_FILENAMES is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_TERRAIN_FILENAMES);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            terrain_texture_filenames = Chunk4(offset, data.data());
        }
    }
};
