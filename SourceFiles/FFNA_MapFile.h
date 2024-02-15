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
    uint16_t size;
    uint16_t unknown;
    std::vector<uint8_t> data;

    Chunk5Element() = default;

    Chunk5Element(int& offset, const unsigned char* data)
    {
        std::memcpy(&tag, &data[offset], sizeof(tag));
        offset += sizeof(tag);

        std::memcpy(&size, &data[offset], sizeof(size));
        offset += sizeof(size);

        std::memcpy(&unknown, &data[offset], sizeof(unknown));
        offset += sizeof(unknown);

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
    uint32_t num_vertices;
    std::vector<Vertex2> vertices;
    std::array<uint8_t, 28> some_data;
    uint8_t zero;

    Chunk5Element2() = default;
    Chunk5Element2(int& offset, const unsigned char* data)
    {
        std::memcpy(&num_vertices, &data[offset], sizeof(num_vertices));
        offset += sizeof(num_vertices);

        vertices.reserve(num_vertices);
        for (uint32_t i = 0; i < num_vertices; ++i)
        {
            vertices.emplace_back(offset, data);
            offset += sizeof(Vertex2);
        }

        std::memcpy(some_data.data(), &data[offset], sizeof(some_data));
        offset += sizeof(some_data);

        std::memcpy(&zero, &data[offset], sizeof(zero));
        offset += sizeof(zero);
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
    std::vector<uint8_t> terrain_shadow_map;
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

        terrain_shadow_map.resize(num_terrain_tiles1);
        std::memcpy(terrain_shadow_map.data(), &data[offset],
                    terrain_shadow_map.size());
        offset += terrain_shadow_map.size();

        std::memcpy(&tag7, &data[offset], sizeof(tag7));
        offset += sizeof(tag7);

        std::memcpy(&some_size7, &data[offset], sizeof(some_size7));
        offset += sizeof(some_size7);

        some_data7.resize(some_size7);
        std::memcpy(some_data7.data(), &data[offset], some_data7.size());
        offset += some_data7.size();

        int chunk_data_size = chunk_size - 75 - terrain_heightmap.size() * sizeof(float) -
          terrain_texture_indices_maybe.size() - some_data3.size() - some_data4.size() - some_data5.size() -
          terrain_shadow_map.size() - some_data7.size();
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

struct EnvironmentInfoFilenamesChunk {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint16_t unknown0;
    uint16_t unknown1;
    uint8_t unknown2;
    std::vector<Chunk4DataElement> filenames;
    std::vector<uint8_t> chunk_data;

    EnvironmentInfoFilenamesChunk() = default;
    EnvironmentInfoFilenamesChunk(int& offset, const unsigned char* data)
    {
        // Copy the chunk ID
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        // Copy the chunk size
        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        // Copy the unknown0
        std::memcpy(&unknown0, &data[offset], sizeof(unknown0));
        offset += sizeof(unknown0);

        // Copy the unknown1
        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        // Copy the unknown2
        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        // Initialize the filenames
        int chunk_data_remaining = chunk_size - 5;
        while (chunk_data_remaining >= sizeof(Chunk4DataElement)) {
            filenames.emplace_back(Chunk4DataElement(offset, data));
            chunk_data_remaining -= sizeof(Chunk4DataElement);
        }

        if (chunk_data_remaining > 0) {
            chunk_data.resize(chunk_data_remaining);
            std::memcpy(chunk_data.data(), &data[offset], chunk_data_remaining);
        }
    }
};

struct ShoreSubChunk {
    float float0;
    float float1;
    float float2;
    float float3;
    uint32_t vertices_count;
    std::vector<Vertex2> vertices;
    uint32_t integer0;

    ShoreSubChunk() = default;
    ShoreSubChunk(int& offset, const unsigned char* data) {
        std::memcpy(&float0, &data[offset], sizeof(float0));
        offset += sizeof(float0);

        std::memcpy(&float1, &data[offset], sizeof(float1));
        offset += sizeof(float1);

        std::memcpy(&float2, &data[offset], sizeof(float2));
        offset += sizeof(float2);

        std::memcpy(&float3, &data[offset], sizeof(float3));
        offset += sizeof(float3);

        std::memcpy(&vertices_count, &data[offset], sizeof(vertices_count));
        offset += sizeof(vertices_count);

        vertices.resize(vertices_count);
        std::memcpy(vertices.data(), &data[offset], vertices_count * sizeof(Vertex2));
        offset += vertices_count * sizeof(Vertex2);

        std::memcpy(&integer0, &data[offset], sizeof(integer0));
        offset += sizeof(integer0);
    }
};

struct ShoreChunk {
    uint32_t chunk_id;
    uint32_t chunk_size;
    char name[4];
    uint32_t unknown1;
    uint32_t subchunk_count;
    std::vector<ShoreSubChunk> subchunks;
    uint32_t unknown9;
    uint8_t zero;

    ShoreChunk() = default;
    ShoreChunk(int& offset, const unsigned char* data) {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(name, &data[offset], sizeof(name));
        offset += sizeof(name);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&subchunk_count, &data[offset], sizeof(subchunk_count));
        offset += sizeof(subchunk_count);

        subchunks.reserve(subchunk_count);
        for (uint32_t i = 0; i < subchunk_count; ++i) {
            subchunks.emplace_back(ShoreSubChunk(offset, data));
        }

        std::memcpy(&unknown9, &data[offset], sizeof(unknown9));
        offset += sizeof(unknown9);

        std::memcpy(&zero, &data[offset], sizeof(zero));
        offset += sizeof(zero);
    }
};

struct EnvSubChunk0 {
    uint8_t unknown0;
    uint8_t unknown1;
    int32_t unknown2;
    uint8_t unknown3;
    uint8_t unknown4;
    uint16_t unknown5;

    EnvSubChunk0() = default;
    EnvSubChunk0(const unsigned char* data, int& offset) {
        std::memcpy(&unknown0, &data[offset], sizeof(unknown0));
        offset += sizeof(unknown0);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        std::memcpy(&unknown3, &data[offset], sizeof(unknown3));
        offset += sizeof(unknown3);

        std::memcpy(&unknown4, &data[offset], sizeof(unknown4));
        offset += sizeof(unknown4);

        std::memcpy(&unknown5, &data[offset], sizeof(unknown5));
        offset += sizeof(unknown5);
    }
};

struct EnvSubChunk1 {
    uint8_t sky_brightness_maybe;
    uint8_t sky_saturaion_maybe;
    uint8_t some_color_scale;
    uint16_t unknown2;
    uint8_t unknown4;

    EnvSubChunk1() = default;
    EnvSubChunk1(const unsigned char* data, int& offset) {
        std::memcpy(&sky_brightness_maybe, &data[offset], sizeof(sky_brightness_maybe));
        offset += sizeof(sky_brightness_maybe);

        std::memcpy(&sky_saturaion_maybe, &data[offset], sizeof(sky_saturaion_maybe));
        offset += sizeof(sky_saturaion_maybe);

        std::memcpy(&some_color_scale, &data[offset], sizeof(some_color_scale));
        offset += sizeof(some_color_scale);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        std::memcpy(&unknown4, &data[offset], sizeof(unknown4));
        offset += sizeof(unknown4);
    }
};

struct EnvSubChunk2 {
    uint8_t fog_blue;
    uint8_t fog_green;
    uint8_t fog_red;
    uint32_t fog_distance_start;
    uint32_t fog_distance_end;
    int32_t fog_z_start_maybe;
    int32_t fog_z_end_maybe;

    EnvSubChunk2() = default;
    EnvSubChunk2(const unsigned char* data, int& offset) {
        std::memcpy(&fog_blue, &data[offset], sizeof(fog_blue));
        offset += sizeof(fog_blue);

        std::memcpy(&fog_green, &data[offset], sizeof(fog_green));
        offset += sizeof(fog_green);

        std::memcpy(&fog_red, &data[offset], sizeof(fog_red));
        offset += sizeof(fog_red);

        std::memcpy(&fog_distance_start, &data[offset], sizeof(fog_distance_start));
        offset += sizeof(fog_distance_start);

        std::memcpy(&fog_distance_end, &data[offset], sizeof(fog_distance_end));
        offset += sizeof(fog_distance_end);

        std::memcpy(&fog_z_start_maybe, &data[offset], sizeof(fog_z_start_maybe));
        offset += sizeof(fog_z_start_maybe);

        std::memcpy(&fog_z_end_maybe, &data[offset], sizeof(fog_z_end_maybe));
        offset += sizeof(fog_z_end_maybe);
    }
};

struct EnvSubChunk3 {
    uint8_t ambient_blue;
    uint8_t ambient_green;
    uint8_t ambient_red;
    uint8_t ambient_intensity;
    uint8_t sun_blue;
    uint8_t sun_green;
    uint8_t sun_red;
    uint8_t sun_intensity;

    EnvSubChunk3() = default;
    EnvSubChunk3(const unsigned char* data, int& offset) {
        std::memcpy(&ambient_blue, &data[offset], sizeof(ambient_blue));
        offset += sizeof(ambient_blue);

        std::memcpy(&ambient_green, &data[offset], sizeof(ambient_green));
        offset += sizeof(ambient_green);

        std::memcpy(&ambient_red, &data[offset], sizeof(ambient_red));
        offset += sizeof(ambient_red);

        std::memcpy(&ambient_intensity, &data[offset], sizeof(ambient_intensity));
        offset += sizeof(ambient_intensity);

        std::memcpy(&sun_blue, &data[offset], sizeof(sun_blue));
        offset += sizeof(sun_blue);

        std::memcpy(&sun_green, &data[offset], sizeof(sun_green));
        offset += sizeof(sun_green);

        std::memcpy(&sun_red, &data[offset], sizeof(sun_red));
        offset += sizeof(sun_red);

        std::memcpy(&sun_intensity, &data[offset], sizeof(sun_intensity));
        offset += sizeof(sun_intensity);
    }
};

struct EnvSubChunk4 {
    uint8_t data[2];

    EnvSubChunk4() = default;
    EnvSubChunk4(const unsigned char* data, int& offset) {
        std::memcpy(this->data, &data[offset], sizeof(this->data));
        offset += sizeof(this->data);
    }
};

struct EnvSubChunk5 {
    uint8_t cloud_cylinder_radius_scale_maybe;
    uint16_t sky_background_texture_index;
    uint16_t sky_clouds_texture_index0;
    uint16_t sky_clouds_texture_index1;
    uint16_t sky_sun_texture_index;
    uint8_t unknown0;
    int16_t unknown1;
    int16_t unknown2;
    uint8_t unknown3;

    EnvSubChunk5() = default;
    EnvSubChunk5(const unsigned char* data, int& offset) {
        std::memcpy(&cloud_cylinder_radius_scale_maybe, &data[offset], sizeof(cloud_cylinder_radius_scale_maybe));
        offset += sizeof(cloud_cylinder_radius_scale_maybe);

        std::memcpy(&sky_background_texture_index, &data[offset], sizeof(sky_background_texture_index));
        offset += sizeof(sky_background_texture_index);

        std::memcpy(&sky_clouds_texture_index0, &data[offset], sizeof(sky_clouds_texture_index0));
        offset += sizeof(sky_clouds_texture_index0);

        std::memcpy(&sky_clouds_texture_index1, &data[offset], sizeof(sky_clouds_texture_index1));
        offset += sizeof(sky_clouds_texture_index1);

        std::memcpy(&sky_sun_texture_index, &data[offset], sizeof(sky_sun_texture_index));
        offset += sizeof(sky_sun_texture_index);

        std::memcpy(&unknown0, &data[offset], sizeof(unknown0));
        offset += sizeof(unknown0);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        std::memcpy(&unknown3, &data[offset], sizeof(unknown3));
        offset += sizeof(unknown3);
    }
};

struct EnvSubChunk5_other {
    uint8_t cloud_cylinder_radius_scale_maybe;
    uint16_t sky_background_texture_index;
    uint16_t sky_clouds_texture_index0;
    uint16_t sky_clouds_texture_index1;
    uint16_t sky_sun_texture_index;
    uint8_t unknown[7];

    EnvSubChunk5_other() = default;
    EnvSubChunk5_other(const unsigned char* data, int& offset) {
        std::memcpy(&cloud_cylinder_radius_scale_maybe, &data[offset], sizeof(cloud_cylinder_radius_scale_maybe));
        offset += sizeof(cloud_cylinder_radius_scale_maybe);

        std::memcpy(&sky_background_texture_index, &data[offset], sizeof(sky_background_texture_index));
        offset += sizeof(sky_background_texture_index);

        std::memcpy(&sky_clouds_texture_index0, &data[offset], sizeof(sky_clouds_texture_index0));
        offset += sizeof(sky_clouds_texture_index0);

        std::memcpy(&sky_clouds_texture_index1, &data[offset], sizeof(sky_clouds_texture_index1));
        offset += sizeof(sky_clouds_texture_index1);

        std::memcpy(&sky_sun_texture_index, &data[offset], sizeof(sky_sun_texture_index));
        offset += sizeof(sky_sun_texture_index);

        std::memcpy(unknown, &data[offset], sizeof(unknown));
        offset += sizeof(unknown);
    }
};

struct EnvSubChunk6 {
    uint8_t unknown[9];
    float unknown1;
    float water_distortion_tex_scale;
    float water_distortion_scale;
    float water_distortion_tex_speed;
    float water_color_tex_scale;
    float water_color_tex_speed;
    float water_color_and_alpha_related;
    float water0;
    float water1;
    uint8_t blue_color0;
    uint8_t green_color0;
    uint8_t red_color0;
    uint8_t alpha0;
    uint8_t blue_color1;
    uint8_t green_color1;
    uint8_t red_color1;
    uint8_t alpha1;
    uint16_t water_color_texture_index;
    uint16_t water_distortion_texture_index;

    EnvSubChunk6() = default;

    EnvSubChunk6(const unsigned char* data, int& offset) {
        std::memcpy(unknown, &data[offset], sizeof(unknown));
        offset += sizeof(unknown);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&water_distortion_tex_scale, &data[offset], sizeof(water_distortion_tex_scale));
        offset += sizeof(water_distortion_tex_scale);

        std::memcpy(&water_distortion_scale, &data[offset], sizeof(water_distortion_scale));
        offset += sizeof(water_distortion_scale);

        std::memcpy(&water_distortion_tex_speed, &data[offset], sizeof(water_distortion_tex_speed));
        offset += sizeof(water_distortion_tex_speed);

        std::memcpy(&water_color_tex_scale, &data[offset], sizeof(water_color_tex_scale));
        offset += sizeof(water_color_tex_scale);

        std::memcpy(&water_color_tex_speed, &data[offset], sizeof(water_color_tex_speed));
        offset += sizeof(water_color_tex_speed);

        std::memcpy(&water_color_and_alpha_related, &data[offset], sizeof(water_color_and_alpha_related));
        offset += sizeof(water_color_and_alpha_related);

        std::memcpy(&water0, &data[offset], sizeof(water0));
        offset += sizeof(water0);

        std::memcpy(&water1, &data[offset], sizeof(water1));
        offset += sizeof(water1);

        std::memcpy(&blue_color0, &data[offset], sizeof(blue_color0));
        offset += sizeof(blue_color0);

        std::memcpy(&green_color0, &data[offset], sizeof(green_color0));
        offset += sizeof(green_color0);

        std::memcpy(&red_color0, &data[offset], sizeof(red_color0));
        offset += sizeof(red_color0);

        std::memcpy(&alpha0, &data[offset], sizeof(alpha0));
        offset += sizeof(alpha0);

        std::memcpy(&blue_color1, &data[offset], sizeof(blue_color1));
        offset += sizeof(blue_color1);

        std::memcpy(&green_color1, &data[offset], sizeof(green_color1));
        offset += sizeof(green_color1);

        std::memcpy(&red_color1, &data[offset], sizeof(red_color1));
        offset += sizeof(red_color1);

        std::memcpy(&alpha1, &data[offset], sizeof(alpha1));
        offset += sizeof(alpha1);

        std::memcpy(&water_color_texture_index, &data[offset], sizeof(water_color_texture_index));
        offset += sizeof(water_color_texture_index);

        std::memcpy(&water_distortion_texture_index, &data[offset], sizeof(water_distortion_texture_index));
        offset += sizeof(water_distortion_texture_index);
    }
};

struct EnvSubChunk7 {
    uint8_t wind_dir0;
    uint8_t wind_dir1;
    uint8_t wind_speed0;
    uint8_t wind_speed1;

    EnvSubChunk7() = default;
    EnvSubChunk7(const unsigned char* data, int& offset) {
        std::memcpy(&wind_dir0, &data[offset], sizeof(wind_dir0));
        offset += sizeof(wind_dir0);

        std::memcpy(&wind_dir1, &data[offset], sizeof(wind_dir1));
        offset += sizeof(wind_dir1);

        std::memcpy(&wind_speed0, &data[offset], sizeof(wind_speed0));
        offset += sizeof(wind_speed0);

        std::memcpy(&wind_speed1, &data[offset], sizeof(wind_speed1));
        offset += sizeof(wind_speed1);
    }
};

struct EnvSubChunk8 {
    uint8_t unknown1[0x9];
    float unknown2;
    float unknown3;
    uint8_t data[15];

    EnvSubChunk8() = default;
    EnvSubChunk8(const unsigned char* data, int& offset) {
        std::memcpy(unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        std::memcpy(&unknown3, &data[offset], sizeof(unknown3));
        offset += sizeof(unknown3);

        std::memcpy(this->data, &data[offset], sizeof(this->data));
        offset += sizeof(this->data);
    }
};

struct EnvironmentInfoChunk {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magic_num_0x92991030;
    uint16_t always_0x10_maybe;
    uint16_t unknown;
    uint8_t data_type;
    std::vector<EnvSubChunk0> env_sub_chunk0;
    uint8_t data_type1;
    std::vector<EnvSubChunk1> env_sub_chunk1;
    uint8_t data_type2;
    std::vector<EnvSubChunk2> env_sub_chunk2;
    uint8_t data_type3;
    std::vector<EnvSubChunk3> env_sub_chunk3;
    uint8_t data_type4;
    std::vector<EnvSubChunk4> env_sub_chunk4;
    uint8_t data_type5;
    std::vector<EnvSubChunk5> env_sub_chunk5; // Used if unknown <= 0
    std::vector<EnvSubChunk5_other> env_sub_chunk5_other; // Used if unknown > 0
    uint8_t data_type6;
    std::vector<EnvSubChunk6> env_sub_chunk6;
    uint8_t data_type7;
    std::vector<EnvSubChunk7> env_sub_chunk7;
    uint8_t unknown0;
    uint16_t unknown1[8];
    uint8_t unknown2;
    uint8_t data_type8;
    std::vector<EnvSubChunk8> env_sub_chunk8;
    uint16_t unknown_3; // always 0x00 0x0B?
    std::vector<uint8_t> structs9; // 5 bytes per struct, size is num_structs9
    uint8_t end_byte_0xFF;

    EnvironmentInfoChunk() = default;
    EnvironmentInfoChunk(int offset, unsigned char* data) {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(&magic_num_0x92991030, &data[offset], sizeof(magic_num_0x92991030));
        offset += sizeof(magic_num_0x92991030);

        std::memcpy(&always_0x10_maybe, &data[offset], sizeof(always_0x10_maybe));
        offset += sizeof(always_0x10_maybe);

        std::memcpy(&unknown, &data[offset], sizeof(unknown));
        offset += sizeof(unknown);

        // Parse EnvSubChunk0
        std::memcpy(&data_type, &data[offset], sizeof(data_type));
        offset += sizeof(data_type);
        uint16_t num_structs0;
        std::memcpy(&num_structs0, &data[offset], sizeof(num_structs0));
        offset += sizeof(num_structs0);
        env_sub_chunk0.resize(num_structs0);
        for (auto& sub_chunk : env_sub_chunk0) {
            sub_chunk = EnvSubChunk0(data, offset);
        }

        // Parse EnvSubChunk1
        std::memcpy(&data_type1, &data[offset], sizeof(data_type1));
        offset += sizeof(data_type1);
        uint16_t num_structs1;
        std::memcpy(&num_structs1, &data[offset], sizeof(num_structs1));
        offset += sizeof(num_structs1);
        env_sub_chunk1.resize(num_structs1);
        for (auto& sub_chunk : env_sub_chunk1) {
            sub_chunk = EnvSubChunk1(data, offset);
        }

        // Parse EnvSubChunk2
        std::memcpy(&data_type2, &data[offset], sizeof(data_type2));
        offset += sizeof(data_type2);
        uint16_t num_structs2;
        std::memcpy(&num_structs2, &data[offset], sizeof(num_structs2));
        offset += sizeof(num_structs2);
        env_sub_chunk2.resize(num_structs2);
        for (auto& sub_chunk : env_sub_chunk2) {
            sub_chunk = EnvSubChunk2(data, offset);
        }

        // Parse EnvSubChunk3
        std::memcpy(&data_type3, &data[offset], sizeof(data_type3));
        offset += sizeof(data_type3);
        uint16_t num_structs3;
        std::memcpy(&num_structs3, &data[offset], sizeof(num_structs3));
        offset += sizeof(num_structs3);
        env_sub_chunk3.resize(num_structs3);
        for (auto& sub_chunk : env_sub_chunk3) {
            sub_chunk = EnvSubChunk3(data, offset);
        }

        // Parse EnvSubChunk4
        std::memcpy(&data_type4, &data[offset], sizeof(data_type4));
        offset += sizeof(data_type4);
        uint16_t num_structs4;
        std::memcpy(&num_structs4, &data[offset], sizeof(num_structs4));
        offset += sizeof(num_structs4);
        env_sub_chunk4.resize(num_structs4);
        for (auto& sub_chunk : env_sub_chunk4) {
            sub_chunk = EnvSubChunk4(data, offset);
        }

        // Conditionally parse EnvSubChunk5 or EnvSubChunk5_other based on 'unknown'
        if (unknown > 0) {
            std::memcpy(&data_type5, &data[offset], sizeof(data_type5));
            offset += sizeof(data_type5);
            uint16_t num_structs5_other;
            std::memcpy(&num_structs5_other, &data[offset], sizeof(num_structs5_other));
            offset += sizeof(num_structs5_other);
            env_sub_chunk5_other.resize(num_structs5_other);
            for (auto& sub_chunk : env_sub_chunk5_other) {
                sub_chunk = EnvSubChunk5_other(data, offset);
            }
        }
        else {
            std::memcpy(&data_type5, &data[offset], sizeof(data_type5));
            offset += sizeof(data_type5);
            uint16_t num_structs5;
            std::memcpy(&num_structs5, &data[offset], sizeof(num_structs5));
            offset += sizeof(num_structs5);
            env_sub_chunk5.resize(num_structs5);
            for (auto& sub_chunk : env_sub_chunk5) {
                sub_chunk = EnvSubChunk5(data, offset);
            }
        }

        // Parse EnvSubChunk6
        std::memcpy(&data_type6, &data[offset], sizeof(data_type6));
        offset += sizeof(data_type6);
        uint16_t num_structs6;
        std::memcpy(&num_structs6, &data[offset], sizeof(num_structs6));
        offset += sizeof(num_structs6);
        env_sub_chunk6.resize(num_structs6);
        for (auto& sub_chunk : env_sub_chunk6) {
            sub_chunk = EnvSubChunk6(data, offset);
        }

        // Parse EnvSubChunk7
        std::memcpy(&data_type7, &data[offset], sizeof(data_type7));
        offset += sizeof(data_type7);
        uint16_t num_structs7;
        std::memcpy(&num_structs7, &data[offset], sizeof(num_structs7));
        offset += sizeof(num_structs7);
        env_sub_chunk7.resize(num_structs7);
        for (auto& sub_chunk : env_sub_chunk7) {
            sub_chunk = EnvSubChunk7(data, offset);
        }

        // Parse EnvSubChunk8
        std::memcpy(&data_type8, &data[offset], sizeof(data_type8));
        offset += sizeof(data_type8);
        uint8_t num_structs8;
        std::memcpy(&num_structs8, &data[offset], sizeof(num_structs8));
        offset += sizeof(num_structs8);
        env_sub_chunk8.resize(num_structs8);
        for (auto& sub_chunk : env_sub_chunk8) {
            sub_chunk = EnvSubChunk8(data, offset);
        }

        std::memcpy(&end_byte_0xFF, &data[offset], sizeof(end_byte_0xFF));
        offset += sizeof(end_byte_0xFF);
    }
};


struct BigChunkSub0 {
    uint8_t unknown[20];
    Vertex2 vertices[3];

    BigChunkSub0() = default;
    BigChunkSub0(const unsigned char* data) {
        std::memcpy(unknown, data, sizeof(unknown));
        std::memcpy(vertices, data + sizeof(unknown), sizeof(vertices));
    }
};

struct BigChunk {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t magicnum_BE_0x4c70feee;
    uint32_t magicnum_0xC;
    uint32_t unknown1;
    uint8_t tag;
    uint32_t next_subdata_size0;
    uint16_t vertex_count;
    std::vector<Vertex2> vertices0;
    uint8_t tag0;
    uint32_t some_size;
    uint8_t unknown2;
    uint32_t unknown3;
    uint32_t unknown4;
    uint32_t vertex_count1;
    uint32_t unknown5;
    uint32_t unknown6;
    uint32_t unknown7;
    uint32_t unknown8;
    uint32_t unknown9;
    uint32_t unknown10;
    uint32_t unknown11;
    uint32_t unknown12;
    uint8_t unknown13;
    std::vector<Vertex2> vertices1;
    uint8_t unknown14;
    uint32_t vertices_data_size;
    std::vector<Vertex2> vertices2;
    uint8_t unknown15;
    uint32_t bcs0_size;
    std::vector<BigChunkSub0> bcs0;
    std::vector<uint8_t> some_data;
    uint8_t tag1;
    uint32_t some_size1;
    std::vector<uint8_t> some_data1;
    uint8_t tag2;
    uint32_t some_size2;
    std::vector<uint8_t> some_data2;
    uint8_t tag3;
    uint32_t some_size3;
    std::vector<uint8_t> some_data3;
    uint32_t unknown33;
    uint8_t zero;

    BigChunk() = default;
    BigChunk(int& offset, const unsigned char* data) {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        offset += sizeof(chunk_id);

        std::memcpy(&chunk_size, &data[offset], sizeof(chunk_size));
        offset += sizeof(chunk_size);

        std::memcpy(&magicnum_BE_0x4c70feee, &data[offset], sizeof(magicnum_BE_0x4c70feee));
        offset += sizeof(magicnum_BE_0x4c70feee);

        std::memcpy(&magicnum_0xC, &data[offset], sizeof(magicnum_0xC));
        offset += sizeof(magicnum_0xC);

        std::memcpy(&unknown1, &data[offset], sizeof(unknown1));
        offset += sizeof(unknown1);

        std::memcpy(&tag, &data[offset], sizeof(tag));
        offset += sizeof(tag);

        std::memcpy(&next_subdata_size0, &data[offset], sizeof(next_subdata_size0));
        offset += sizeof(next_subdata_size0);

        std::memcpy(&vertex_count, &data[offset], sizeof(vertex_count));
        offset += sizeof(vertex_count);

        vertices0.resize(vertex_count);
        std::memcpy(vertices0.data(), &data[offset], vertex_count * sizeof(Vertex2));
        offset += vertex_count * sizeof(Vertex2);

        std::memcpy(&tag0, &data[offset], sizeof(tag0));
        offset += sizeof(tag0);

        std::memcpy(&some_size, &data[offset], sizeof(some_size));
        offset += sizeof(some_size);

        std::memcpy(&unknown2, &data[offset], sizeof(unknown2));
        offset += sizeof(unknown2);

        std::memcpy(&unknown3, &data[offset], sizeof(unknown3));
        offset += sizeof(unknown3);

        std::memcpy(&unknown4, &data[offset], sizeof(unknown4));
        offset += sizeof(unknown4);

        std::memcpy(&vertex_count1, &data[offset], sizeof(vertex_count1));
        offset += sizeof(vertex_count1);

        std::memcpy(&unknown5, &data[offset], sizeof(unknown5));
        offset += sizeof(unknown5);

        std::memcpy(&unknown6, &data[offset], sizeof(unknown6));
        offset += sizeof(unknown6);

        std::memcpy(&unknown7, &data[offset], sizeof(unknown7));
        offset += sizeof(unknown7);

        std::memcpy(&unknown8, &data[offset], sizeof(unknown8));
        offset += sizeof(unknown8);

        std::memcpy(&unknown9, &data[offset], sizeof(unknown9));
        offset += sizeof(unknown9);

        std::memcpy(&unknown10, &data[offset], sizeof(unknown10));
        offset += sizeof(unknown10);

        std::memcpy(&unknown11, &data[offset], sizeof(unknown11));
        offset += sizeof(unknown11);

        std::memcpy(&unknown12, &data[offset], sizeof(unknown12));
        offset += sizeof(unknown12);

        std::memcpy(&unknown13, &data[offset], sizeof(unknown13));
        offset += sizeof(unknown13);

        vertices1.resize(vertex_count1);
        std::memcpy(vertices1.data(), &data[offset], vertex_count1 * sizeof(Vertex2));
        offset += vertex_count1 * sizeof(Vertex2);

        std::memcpy(&unknown14, &data[offset], sizeof(unknown14));
        offset += sizeof(unknown14);

        std::memcpy(&vertices_data_size, &data[offset], sizeof(vertices_data_size));
        offset += sizeof(vertices_data_size);

        size_t vertices2_count = vertices_data_size / (2 * sizeof(float));
        vertices2.resize(vertices2_count);
        std::memcpy(vertices2.data(), &data[offset], vertices2_count * sizeof(Vertex2));
        offset += vertices2_count * sizeof(Vertex2);

        std::memcpy(&unknown15, &data[offset], sizeof(unknown15));
        offset += sizeof(unknown15);

        std::memcpy(&bcs0_size, &data[offset], sizeof(bcs0_size));
        offset += sizeof(bcs0_size);

        size_t bcs0_count = bcs0_size / sizeof(BigChunkSub0);
        bcs0.resize(bcs0_count);
        for (size_t i = 0; i < bcs0_count; ++i) {
            bcs0[i] = BigChunkSub0(&data[offset]);
            offset += sizeof(BigChunkSub0);
        }

        /*int some_data_size = some_size - 1 - 4 * 11 - 1 - sizeof(vertices1) - 1 - 4 - sizeof(vertices2) - 1 - 4 - sizeof(bcs0);
        some_data.resize(some_data_size);
        std::memcpy(some_data.data(), &data[offset], some_data_size);
        offset += some_data_size;

        std::memcpy(&tag1, &data[offset], sizeof(tag1));
        offset += sizeof(tag1);

        std::memcpy(&some_size1, &data[offset], sizeof(some_size1));
        offset += sizeof(some_size1);

        some_data1.resize(some_size1);
        std::memcpy(some_data1.data(), &data[offset], some_size1);
        offset += some_size1;

        std::memcpy(&tag2, &data[offset], sizeof(tag2));
        offset += sizeof(tag2);

        std::memcpy(&some_size2, &data[offset], sizeof(some_size2));
        offset += sizeof(some_size2);

        some_data2.resize(some_size2);
        std::memcpy(some_data2.data(), &data[offset], some_size2);
        offset += some_size2;

        std::memcpy(&tag3, &data[offset], sizeof(tag3));
        offset += sizeof(tag3);

        std::memcpy(&some_size3, &data[offset], sizeof(some_size3));
        offset += sizeof(some_size3);

        some_data3.resize(some_size3);
        std::memcpy(some_data3.data(), &data[offset], some_size3);
        offset += some_size3;

        std::memcpy(&unknown33, &data[offset], sizeof(unknown33));
        offset += sizeof(unknown33);

        std::memcpy(&zero, &data[offset], sizeof(zero));
        offset += sizeof(zero);*/
    }
};

constexpr uint32_t CHUNK_ID_20000000 = 0x20000000;
constexpr uint32_t CHUNK_ID_TERRAIN = 0x20000002;
constexpr uint32_t CHUNK_ID_ZONES_STUFF = 0x20000003;
constexpr uint32_t CHUNK_ID_PROPS_INFO = 0x20000004;
constexpr uint32_t CHUNK_ID_PATH_INFO = 0x20000008;
constexpr uint32_t CHUNK_ID_ENVIRONMENT_INFO = 0x20000009;
constexpr uint32_t CHUNK_ID_MAP_INFO = 0x2000000C;
constexpr uint32_t CHUNK_ID_SHOR = 0x20000010;
constexpr uint32_t CHUNK_ID_TERRAIN_FILENAMES = 0x21000002;
constexpr uint32_t CHUNK_ID_PROPS_FILENAMES0 = 0x21000003;
constexpr uint32_t CHUNK_ID_PROPS_FILENAMES = 0x21000004;
constexpr uint32_t CHUNK_ID_ENVIRONMENT_INFO_FILENAMES = 0x21000009;

struct FFNA_MapFile
{
    char ffna_signature[4];
    FFNAType ffna_type;
    Chunk1 chunk1;
    Chunk2 map_info_chunk;
    Chunk3 props_info_chunk;
    Chunk4 prop_filenames_chunk;
    Chunk5 chunk5; // The actual chunk 5 doesn't work for all files. Haven't figured out the format yet.
    Chunk4 more_filnames_chunk; // same structure as chunk 4
    Chunk7 chunk7;
    Chunk8 terrain_chunk;
    Chunk4 terrain_texture_filenames; // same structure as chunk 4
    EnvironmentInfoChunk environment_info_chunk;
    EnvironmentInfoFilenamesChunk environment_info_filenames_chunk;
    ShoreChunk shore_chunk;
    BigChunk big_chunk;

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

        //Check if the CHUNK_ID_ENVIRONMENT_INFO_FILENAMES is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_ENVIRONMENT_INFO);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            environment_info_chunk = EnvironmentInfoChunk(offset, data.data());
        }

        // Check if the CHUNK_ID_ENVIRONMENT_INFO_FILENAMES is in the riff_chunks map
        it = riff_chunks.find(CHUNK_ID_ENVIRONMENT_INFO_FILENAMES);
        if (it != riff_chunks.end())
        {
            int offset = it->second;
            environment_info_filenames_chunk = EnvironmentInfoFilenamesChunk(offset, data.data());
        }

        // Check if the CHUNK_ID_SHOR is in the riff_chunks map
        //it = riff_chunks.find(CHUNK_ID_SHOR);
        //if (it != riff_chunks.end())
        //{
        //    int offset = it->second;
        //    shore_chunk = ShoreChunk(offset, data.data());
        //}

        //// Check if the CHUNK_ID_PATH_INFO is in the riff_chunks map
        //it = riff_chunks.find(CHUNK_ID_PATH_INFO);
        //if (it != riff_chunks.end())
        //{
        //    int offset = it->second;
        //    big_chunk = BigChunk(offset, data.data());
        //}

        //// Check if the CHUNK_ID_ZONES_STUFF is in the riff_chunks map
        //it = riff_chunks.find(CHUNK_ID_ZONES_STUFF);
        //if (it != riff_chunks.end())
        //{
        //    int offset = it->second;
        //    chunk5 = Chunk5(offset, data.data());
        //}
    }
};
