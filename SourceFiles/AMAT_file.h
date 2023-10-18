#pragma once

#include "FFNAType.h"

struct GRMT
{
	uint32_t signature;
	uint32_t chunk_size;
	uint8_t tex_array_range;
	uint8_t num_textures;
	uint8_t tex_transform_range;
	uint8_t sort_order;
	uint16_t texs_bits;
	uint16_t unknown2;
	uint32_t unknown3;
	uint32_t unknown4;
	uint32_t unknown5;
	uint32_t unknown6;
	std::vector<uint8_t> chunk_data;

	GRMT() = default;

	GRMT(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes, bool& parsed_correctly)
	{
		int initial_offset = curr_offset;

		std::memcpy(&signature, &data[curr_offset], sizeof(signature));
		curr_offset += sizeof(signature);

		std::memcpy(&chunk_size, &data[curr_offset], sizeof(chunk_size));
		curr_offset += sizeof(chunk_size);

		std::memcpy(&tex_array_range, &data[curr_offset], sizeof(tex_array_range));
		curr_offset += sizeof(tex_array_range);

		std::memcpy(&num_textures, &data[curr_offset], sizeof(num_textures));
		curr_offset += sizeof(num_textures);

		std::memcpy(&tex_transform_range, &data[curr_offset], sizeof(tex_transform_range));
		curr_offset += sizeof(tex_transform_range);

		std::memcpy(&sort_order, &data[curr_offset], sizeof(sort_order));
		curr_offset += sizeof(sort_order);

		std::memcpy(&texs_bits, &data[curr_offset], sizeof(texs_bits));
		curr_offset += sizeof(texs_bits);

		std::memcpy(&unknown2, &data[curr_offset], sizeof(unknown2));
		curr_offset += sizeof(unknown2);

		std::memcpy(&unknown3, &data[curr_offset], sizeof(unknown3));
		curr_offset += sizeof(unknown3);

		std::memcpy(&unknown4, &data[curr_offset], sizeof(unknown4));
		curr_offset += sizeof(unknown4);

		std::memcpy(&unknown5, &data[curr_offset], sizeof(unknown5));
		curr_offset += sizeof(unknown5);

		std::memcpy(&unknown6, &data[curr_offset], sizeof(unknown6));
		curr_offset += sizeof(unknown6);

		uint32_t remaining_bytes = chunk_size - (curr_offset - (initial_offset + 8));

		if (remaining_bytes > 0)
		{
			chunk_data.resize(remaining_bytes);
			std::memcpy(chunk_data.data(), &data[curr_offset], remaining_bytes);
			curr_offset += remaining_bytes;
		}
	}
};

struct TextureInfo
{
	uint32_t tex_index;
	uint32_t data[6];
};

struct DX9S_0
{
	uint32_t num_vals;
	uint32_t f1;
	uint32_t f2;
	uint32_t size;
	uint32_t f4;
	uint32_t f5;
	std::vector<uint32_t> vals;
	std::vector<TextureInfo> tex_infos;
	std::vector<uint32_t> data;
	uint32_t u0;
	uint32_t size_of_next_shad_chunk_plus_4;

	DX9S_0() = default;

	DX9S_0(uint32_t& curr_offset, const unsigned char* data_buffer, int data_size_bytes, bool& parsed_correctly,
	       const GRMT& grmt_chunk)
	{
		std::memcpy(&num_vals, &data_buffer[curr_offset], sizeof(num_vals));
		curr_offset += sizeof(num_vals);

		std::memcpy(&f1, &data_buffer[curr_offset], sizeof(f1));
		curr_offset += sizeof(f1);

		std::memcpy(&f2, &data_buffer[curr_offset], sizeof(f2));
		curr_offset += sizeof(f2);

		std::memcpy(&size, &data_buffer[curr_offset], sizeof(size));
		curr_offset += sizeof(size);

		std::memcpy(&f4, &data_buffer[curr_offset], sizeof(f4));
		curr_offset += sizeof(f4);

		std::memcpy(&f5, &data_buffer[curr_offset], sizeof(f5));
		curr_offset += sizeof(f5);

		vals.resize(num_vals);
		std::memcpy(vals.data(), &data_buffer[curr_offset], num_vals * sizeof(uint32_t));
		curr_offset += num_vals * sizeof(uint32_t);

		int num_textures = grmt_chunk.num_textures;
		tex_infos.resize(num_textures);
		for (int i = 0; i < num_textures; i++)
		{
			std::memcpy(&tex_infos[i], &data_buffer[curr_offset], sizeof(TextureInfo));
			curr_offset += sizeof(TextureInfo);
		}


		size_t dataSize = (size - 4 - num_vals * 4 - num_textures * sizeof(TextureInfo) - 8) / 4;
		data.resize(dataSize);
		std::memcpy(data.data(), &data_buffer[curr_offset], dataSize * sizeof(uint32_t));
		curr_offset += dataSize * sizeof(uint32_t);

		std::memcpy(&u0, &data_buffer[curr_offset], sizeof(u0));
		curr_offset += sizeof(u0);

		std::memcpy(&size_of_next_shad_chunk_plus_4, &data_buffer[curr_offset], sizeof(size_of_next_shad_chunk_plus_4));
		curr_offset += sizeof(size_of_next_shad_chunk_plus_4);
	}
};

struct SHAD
{
	uint32_t signature;
	uint32_t chunk_size;
	std::vector<uint8_t> chunk_data;

	SHAD() = default;

	SHAD(uint32_t& curr_offset, const unsigned char* data_buffer, int data_size_bytes, bool& parsed_correctly)
	{
		std::memcpy(&signature, &data_buffer[curr_offset], sizeof(signature));
		curr_offset += sizeof(signature);

		std::memcpy(&chunk_size, &data_buffer[curr_offset], sizeof(chunk_size));
		curr_offset += sizeof(chunk_size);

		if (curr_offset + chunk_size >= data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}

		chunk_data.resize(chunk_size);
		std::memcpy(chunk_data.data(), &data_buffer[curr_offset], chunk_size);
		curr_offset += chunk_size;
	}
};

struct TECH
{
	char signature[4];
	uint32_t tech_size;
	uint32_t data0[5];
	std::string tech_type_signature; // Changed from std::vector<char> to std::string
	uint32_t u0;
	char pass_signature[4];
	uint32_t data_size;
	uint32_t u1;
	uint32_t u2;
	uint32_t u3;
	uint32_t some_size;
	std::vector<uint8_t> some_data;
	std::vector<uint32_t> tex_indices_array;

	TECH() = default;

	TECH(uint32_t& curr_offset, const unsigned char* data_buffer, int data_size_bytes, bool& parsed_correctly)
	{
		uint32_t initial_offset = curr_offset;
		std::memcpy(signature, &data_buffer[curr_offset], sizeof(signature));
		curr_offset += sizeof(signature);

		std::memcpy(&tech_size, &data_buffer[curr_offset], sizeof(tech_size));
		curr_offset += sizeof(tech_size);

		std::memcpy(data0, &data_buffer[curr_offset], sizeof(data0));
		curr_offset += sizeof(data0);

		// Since tech_type_signature size isn't given, I assume it's a null-terminated string
		tech_type_signature = std::string(reinterpret_cast<const char*>(&data_buffer[curr_offset]));
		curr_offset += tech_type_signature.size() + 1;

		std::memcpy(&u0, &data_buffer[curr_offset], sizeof(u0));
		curr_offset += sizeof(u0);

		std::memcpy(pass_signature, &data_buffer[curr_offset], sizeof(pass_signature));
		curr_offset += sizeof(pass_signature);

		std::memcpy(&data_size, &data_buffer[curr_offset], sizeof(data_size));
		curr_offset += sizeof(data_size);

		std::memcpy(&u1, &data_buffer[curr_offset], sizeof(u1));
		curr_offset += sizeof(u1);

		std::memcpy(&u2, &data_buffer[curr_offset], sizeof(u2));
		curr_offset += sizeof(u2);

		std::memcpy(&u3, &data_buffer[curr_offset], sizeof(u3));
		curr_offset += sizeof(u3);

		std::memcpy(&some_size, &data_buffer[curr_offset], sizeof(some_size));
		curr_offset += sizeof(some_size);

		uint32_t some_data_size = some_size - sizeof(some_size); // some_size includes itself so subtract sizeof(some_size)
		some_data.resize(some_data_size);

		std::memcpy(some_data.data(), &data_buffer[curr_offset], some_data_size);
		curr_offset += some_data_size;

		// Assuming tex_indices_array is filled until the end of TECH
		uint32_t remaining_bytes = tech_size - (curr_offset - (initial_offset + 8));
		uint32_t num_indices = remaining_bytes / sizeof(uint32_t);
		tex_indices_array.resize(num_indices);
		std::memcpy(tex_indices_array.data(), &data_buffer[curr_offset], num_indices * sizeof(uint32_t));
		curr_offset += num_indices * sizeof(uint32_t);
	}
};

struct DX9S
{
	uint32_t signature;
	uint32_t chunk_size;
	DX9S_0 sub_chunk_0;
	SHAD SHAD_chunk_0;
	SHAD SHAD_chunk_1;
	uint32_t data0[3];
	TECH tech_high;
	TECH tech_medium;
	TECH tech_low;
	std::vector<uint8_t> chunk_data;

	DX9S() = default;

	DX9S(uint32_t& curr_offset, const unsigned char* data_buffer, const int data_size_bytes, bool& parsed_correctly,
	     const GRMT& grmt_chunk)
	{
		uint32_t initial_offset = curr_offset;
		std::memcpy(&signature, &data_buffer[curr_offset], sizeof(signature));
		curr_offset += sizeof(signature);

		std::memcpy(&chunk_size, &data_buffer[curr_offset], sizeof(chunk_size));
		curr_offset += sizeof(chunk_size);

		sub_chunk_0 = DX9S_0(curr_offset, data_buffer, data_size_bytes, parsed_correctly, grmt_chunk);
		SHAD_chunk_0 = SHAD(curr_offset, data_buffer, data_size_bytes, parsed_correctly);
		if (!parsed_correctly) return;
		SHAD_chunk_1 = SHAD(curr_offset, data_buffer, data_size_bytes, parsed_correctly);
		if (!parsed_correctly) return;

		std::memcpy(data0, &data_buffer[curr_offset], sizeof(data0));
		curr_offset += sizeof(data0);

		if (curr_offset + 4 < data_size_bytes && *((uint32_t*)(&data_buffer[curr_offset])) == 0x48434554)
		{
			tech_high = TECH(curr_offset, data_buffer, data_size_bytes, parsed_correctly);
		}
		if (curr_offset + 4 < data_size_bytes && *((uint32_t*)(&data_buffer[curr_offset])) == 0x48434554)
		{
			tech_medium = TECH(curr_offset, data_buffer, data_size_bytes, parsed_correctly);
		}
		if (curr_offset + 4 < data_size_bytes && *((uint32_t*)(&data_buffer[curr_offset])) == 0x48434554)
		{
			tech_low = TECH(curr_offset, data_buffer, data_size_bytes, parsed_correctly);
		}

		uint32_t remaining_data = chunk_size - (curr_offset - (initial_offset+8));
		if (curr_offset + remaining_data > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		chunk_data.resize(remaining_data);
		std::memcpy(chunk_data.data(), &data_buffer[curr_offset], remaining_data);
		curr_offset += remaining_data;
	}
};

constexpr uint32_t CHUNK_ID_GRMT = 0x544D5247;
constexpr uint32_t CHUNK_ID_GRSN = 0x4E535247;
constexpr uint32_t CHUNK_ID_DX9S = 0x53395844;

struct AMAT_file
{
	char signature[4];
	uint32_t version;
	GRMT GRMT_chunk;
	GeneralChunk GRSN_chunk;
	DX9S DX9S_chunk;

	bool parsed_correctly = true;

	std::unordered_map<uint32_t, int> riff_chunks;

	AMAT_file() = default;

	AMAT_file(const unsigned char* data, uint32_t dataSize)
	{
		uint32_t current_offset = 0;

		std::memcpy(&signature, &data[current_offset], sizeof(signature));
		current_offset += 4;
		std::memcpy(&version, &data[current_offset], sizeof(version));
		current_offset += 4;

		// Read all chunks
		while (current_offset < dataSize)
		{
			// Create a GeneralChunk instance using the current offset and data pointer
			GeneralChunk chunk(current_offset, data);

			// Add the GeneralChunk instance to the riff_chunks map using its chunk_id as the key
			riff_chunks.emplace(chunk.chunk_id, current_offset);

			// Move to the next chunk by updating the current_offset
			current_offset += 8 + chunk.chunk_size;
		}

		// Check if the CHUNK_ID_TERRAIN is in the riff_chunks map
		auto it = riff_chunks.find(CHUNK_ID_GRMT);
		if (it != riff_chunks.end())
		{
			uint32_t offset = it->second;
			GRMT_chunk = GRMT(offset, data, dataSize, parsed_correctly);
		}
		else { parsed_correctly = false; }

		it = riff_chunks.find(CHUNK_ID_GRSN);
		if (it != riff_chunks.end())
		{
			int offset = it->second;
			GRSN_chunk = GeneralChunk(offset, data);
		}

		it = riff_chunks.find(CHUNK_ID_DX9S);
		if (it != riff_chunks.end() && parsed_correctly)
		{
			uint32_t offset = it->second;
			DX9S_chunk = DX9S(offset, data, dataSize, parsed_correctly, GRMT_chunk);
		}
	}
};
