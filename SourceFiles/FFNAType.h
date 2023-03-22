#pragma once
struct GeneralChunk
{
    uint32_t chunk_id;
    uint32_t chunk_size;
    std::vector<uint8_t> chunk_data;

    GeneralChunk() = default;
    GeneralChunk(int offset, const unsigned char* data)
    {
        std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
        std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
        chunk_data.resize(chunk_size);
        std::memcpy(chunk_data.data(), &data[offset + 8], chunk_size);
    }
};

enum FFNAType : unsigned char
{
    Type0,
    Type1,
    Model,
    Map,
    Type4,
    Type5,
    Type6,
    Type7
};
