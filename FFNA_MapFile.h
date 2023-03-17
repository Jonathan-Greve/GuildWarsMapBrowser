#pragma once
#include "FFNAType.h"
#include <stdint.h>

struct MapBounds
{
    float map_min_x;
    float map_min_y;
    float map_max_x;
    float map_max_y;

    MapBounds() = default;
    MapBounds(int offset, const unsigned char* data)
    {
        std::memcpy(&map_min_x, &data[offset], sizeof(map_min_x));
        std::memcpy(&map_min_y, &data[offset + 4], sizeof(map_min_y));
        std::memcpy(&map_max_x, &data[offset + 8], sizeof(map_max_x));
        std::memcpy(&map_max_y, &data[offset + 12], sizeof(map_max_y));
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

struct PropInfo
{
    uint16_t f1;
    float x;
    float y;
    float z;
    float f4;
    float f5;
    float f6;
    float cos_angle; // cos of angle in radians
    float sin_angle; // sin of angle in radians
    float f9;
    float f10;
    float f11;
    uint8_t f12;
    uint8_t num_some_structs;
    std::vector<uint8_t> some_structs; // has size: num_some_structs * 8

    PropInfo() = default;
    PropInfo(int offset, const unsigned char* data)
    {
        std::memcpy(&f1, &data[offset], sizeof(f1));
        std::memcpy(&x, &data[offset + 2], sizeof(x));
        std::memcpy(&y, &data[offset + 6], sizeof(y));
        std::memcpy(&z, &data[offset + 10], sizeof(z));
        std::memcpy(&f4, &data[offset + 14], sizeof(f4));
        std::memcpy(&f5, &data[offset + 18], sizeof(f5));
        std::memcpy(&f6, &data[offset + 22], sizeof(f6));
        std::memcpy(&cos_angle, &data[offset + 26], sizeof(cos_angle));
        std::memcpy(&sin_angle, &data[offset + 30], sizeof(sin_angle));
        std::memcpy(&f9, &data[offset + 34], sizeof(f9));
        std::memcpy(&f10, &data[offset + 38], sizeof(f10));
        std::memcpy(&f11, &data[offset + 42], sizeof(f11));
        std::memcpy(&f12, &data[offset + 46], sizeof(f12));
        std::memcpy(&num_some_structs, &data[offset + 47], sizeof(num_some_structs));

        some_structs.resize(num_some_structs * 8);
        std::memcpy(some_structs.data(), &data[offset + 48], some_structs.size());
    }
};

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
            offset += sizeof(new_prop_info) - sizeof(new_prop_info.some_structs) +
              new_prop_info.some_structs.size() - 4;
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
        std::memcpy(array.data(), &data[offset + 7], array_size_in_bytes);
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
        offset += some_vertex_data.array_size_in_bytes;

        some_data0 = SomeData(offset, data);
        offset += some_data0.array_size_in_bytes;

        some_data1 = SomeData1(offset, data);
        offset += some_data1.array_size_in_bytes;

        some_data2 = SomeData2(offset, data);
        offset += some_data2.array_size_in_bytes;

        chunk_data.resize(chunk_size - offset);
        std::memcpy(chunk_data.data(), &data[offset], chunk_data.size());
    }
};
//
//struct Chunk4DataHeader
//{
//    uint32_t signature;
//    uint8_t version;
//};
//
//// To get file hash from name do:
//// (id0 - 0xff00ff) + (id2 * 0xff00);
//struct FileName
//{
//    uint16_t id0;
//    uint16_t id1;
//};
//
//struct Chunk4DataElement
//{
//    uint16_t f1;
//    FileName filename;
//};
//
//struct Chunk4
//{
//    uint32_t chunk_id;
//    uint32_t chunk_size;
//    Chunk4DataHeader data_header;
//    FileName file_name;
//    //Chunk4DataElement array[228];
//    Chunk4DataElement array[(chunk_size - sizeof(data_header) - sizeof(file_name)) / 6];
//    uint8_t chunk_data[chunk_size - sizeof(data_header) - sizeof(file_name) - sizeof(array)];
//};
//
//struct Chunk5Element
//{
//    uint8_t tag;
//    uint32_t size;
//    uint8_t data[size];
//};
//
//struct Chunk5Element1
//{
//    uint8_t tag;
//    uint32_t size;
//    uint32_t num_zones;
//};
//
//struct Chunk5Element2
//{
//    uint8_t unknown[20];
//    uint8_t unknown1;
//    uint16_t some_size;
//    uint16_t unknown2;
//    if (some_size <= 0)
//    {
//        uint32_t unknown3;
//        uint32_t count2;
//        Vertex2 vertices[count2];
//    }
//    else
//    {
//        uint8_t some_data[some_size];
//    }
//};
//
//struct Chunk5
//{
//    uint32_t chunk_id;
//    uint32_t chunk_size;
//    // Data start
//    uint32_t magic_num; // 0x59220329
//    uint32_t magic_num1; // 0xA0
//    Chunk5Element element_0;
//    Chunk5Element element_1;
//    Chunk5Element1 element_2;
//    Chunk5Element element_3;
//    if (element_2.num_zones > 0)
//    {
//        uint32_t unknown0;
//        uint32_t unknown1;
//        float unknown2[8];
//        Chunk5Element2 some_array[element_2.num_zones];
//        uint8_t chunk_data[chunk_size - 8 - sizeof(element_0) - sizeof(element_1) - sizeof(element_2) -
//                           sizeof(element_3) - 8 - sizeof(unknown2) - sizeof(some_array)];
//    }
//    else
//    {
//        uint8_t chunk_data[chunk_size - 8 - sizeof(element_0) - sizeof(element_1) - sizeof(element_2) -
//                           sizeof(element_3)];
//    }
//};
//
//struct Chunk8
//{
//    uint32_t chunk_id;
//    uint32_t chunk_size;
//    uint32_t magic_num;
//    uint32_t magic_num1;
//    uint8_t tag;
//    uint32_t some_size;
//    uint32_t terrain_x_dims; // Not sure what dims refer to. Maybe the terrain is divided into grid tiles?
//    uint32_t terrain_y_dims;
//    float some_float;
//    float some_small_float; // checked that it is less than 1.570796 and >=0
//    uint16_t some_size1;
//    float some_float1;
//    float some_float2;
//    uint8_t tag1;
//    uint32_t terrain_height_size_bytes;
//    float terrain_height_vertices[terrain_x_dims * terrain_y_dims]; // same as [terrain_height_size_bytes/4]
//    uint8_t tag2;
//    uint32_t num_terrain_tiles; // num terrain height vertices
//    uint8_t something_for_each_tile[num_terrain_tiles];
//    uint8_t tag3;
//    uint32_t some_size2;
//    uint8_t some_size3;
//    uint8_t some_data3[some_size3];
//    uint8_t tag4;
//    uint32_t some_size4;
//    uint8_t some_data4[some_size4];
//    uint8_t tag5;
//    uint32_t some_size5;
//    uint8_t some_data5[some_size5];
//    uint8_t tag6;
//    uint32_t num_terrain_tiles1;
//    uint8_t something_for_each_tile1[num_terrain_tiles1];
//    uint8_t tag7;
//    uint32_t some_size7;
//    uint8_t some_data7[some_size7];
//    uint8_t chunk_data[chunk_size - 75 - sizeof(terrain_height_vertices) - sizeof(something_for_each_tile) -
//                       sizeof(some_data3) - sizeof(some_data4) - sizeof(some_data5) -
//                       sizeof(something_for_each_tile1) - sizeof(some_data7)];
//};
//
//struct Chunk7
//{
//    uint32_t chunk_id;
//    uint32_t chunk_size;
//    uint8_t chunk_data[chunk_size];
//};

struct FFNA_MapFile
{
    char ffna_signature[4];
    FFNAType ffna_type;
    Chunk1 chunk1;
    Chunk2 chunk2;
    Chunk3 chunk3;

    FFNA_MapFile() = default;
    FFNA_MapFile(int offset, std::span<unsigned char>& data)
    {
        std::memcpy(ffna_signature, &data[offset], sizeof(ffna_signature));
        offset += sizeof(ffna_signature);
        std::memcpy(&ffna_type, &data[offset], sizeof(ffna_type));
        offset += sizeof(ffna_type);
        chunk1 = Chunk1(offset, data.data());
        offset +=
          8 + chunk1.chunk_size; // + 8 because the chunk size doesn't count the id and chunksize fields.
        chunk2 = Chunk2(offset, data.data());
        offset +=
          8 + chunk2.chunk_size; // + 8 because the chunk size doesn't count the id and chunksize fields.
        chunk3 = Chunk3(offset, data.data());
    }
};
