#pragma once
#include "FFNAType.h"
#include "AMAT_file.h"
#include <array>
#include <stdint.h>
#include "DXMathHelpers.h"
#include "Mesh.h"
#include "Vertex.h"

constexpr uint32_t GR_FVF_POSITION = 1; // 3 floats
constexpr uint32_t GR_FVF_GROUP = 2; // uint32?
constexpr uint32_t GR_FVF_NORMAL = 4; // 3 floats
constexpr uint32_t GR_FVF_DIFFUSE = 8;
constexpr uint32_t GR_FVF_TANGENT_AND_BITANGENT = 0x30;

inline uint32_t get_fvf(uint32_t dat_fvf)
{
	return (dat_fvf & 0xff0) << 4 | dat_fvf >> 8 & 0x30 | dat_fvf & 0xf;
}

inline uint32_t get_vertex_size_from_fvf(uint32_t fvf)
{
	constexpr uint32_t fvf_array_0[22] = { 0x0, 0x8,  0x8,  0x10, 0x8,        0x10,      0x10, 0x18,
										  0x8, 0x10, 0x10, 0x18, 0x10,       0x18,      0x18, 0x20,
										  0x0, 0x0,  0x0,  0x1,  0xFFFFFFFF, 0xFFFFFFFF };
	constexpr uint32_t fvf_array_1[8] = { 0x0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24 };
	constexpr uint32_t fvf_array_2[16] = { 0x0, 0xC,  0x4, 0x10, 0xC,  0x18, 0x10, 0x1C,
										  0x4, 0x10, 0x8, 0x14, 0x10, 0x1C, 0x14, 0x20 };

	return fvf_array_0[fvf >> 0xc & 0xf] + fvf_array_0[fvf >> 8 & 0xf] + fvf_array_1[fvf >> 4 & 7] +
		fvf_array_2[fvf & 0xf];
}

inline uint32_t get_some_size(const unsigned char* data, uint32_t sub_1_0x52, uint32_t data_size_bytes,
	bool& parsed_correctly)
{
	uint32_t iVar3 = 0;
	uint32_t iVar5 = 0;
	uint32_t iVar6 = 0;
	uint32_t iVar7 = 0;
	uint32_t local_c = 0;

	if (1 < sub_1_0x52)
	{
		int32_t iVar4 = ((sub_1_0x52 - 2) >> 1) + 1;

		local_c = iVar4 * 2;

		const unsigned char* piVar1 = data + 0x2C;

		while (true)
		{
			if (piVar1 + 0xC * 4 - data >= data_size_bytes)
			{
				parsed_correctly = false;
				return 0;
			}

			uint32_t v3 = *reinterpret_cast<const uint32_t*>(piVar1 + 0xC * 4);
			iVar3 += v3;

			if (piVar1 - 4 - data >= data_size_bytes)
			{
				parsed_correctly = false;
				return 0;
			}

			uint32_t v5 = *reinterpret_cast<const uint32_t*>(piVar1 - 4);
			iVar5 += v5;

			if (piVar1 - data >= data_size_bytes)
			{
				parsed_correctly = false;
				return 0;
			}

			uint32_t v6 = *reinterpret_cast<const uint32_t*>(piVar1);
			iVar6 += v6;

			if (piVar1 + 0xB * 4 - data >= data_size_bytes)
			{
				parsed_correctly = false;
				return 0;
			}

			uint32_t v7 = *reinterpret_cast<const uint32_t*>(piVar1 + 0xB * 4);
			iVar7 += v7;

			piVar1 += 0x60;

			iVar4 -= 1;
			if (iVar4 <= 0)
			{
				break;
			}
		}
	}

	uint32_t iVar4 = 0;
	uint32_t local_18 = 0;
	if (local_c < sub_1_0x52)
	{
		if (data + 0x28 + local_c * 6 * 8 - data >= data_size_bytes)
		{
			parsed_correctly = false;
			return 0;
		}

		local_18 = *reinterpret_cast<const uint32_t*>(data + 0x28 + local_c * 6 * 8);

		if (data + 0x2C + local_c * 6 * 8 - data >= data_size_bytes)
		{
			parsed_correctly = false;
			return 0;
		}

		iVar4 = *reinterpret_cast<const uint32_t*>(data + 0x2C + local_c * 6 * 8);
	}

	local_18 += iVar7 + iVar5;
	iVar4 += iVar3 + iVar6;

	uint32_t size = iVar4 * 0x10 + local_18 * 0x18;

	return size;
}

struct ModelVertex
{
	bool has_position;
	bool has_group;
	bool has_normal;
	bool has_diffuse;
	bool has_specular;
	bool has_tex_coord[8];
	bool has_tangent;
	bool has_bitangent;

	float x, y, z;
	uint32_t group;
	float normal_x, normal_y, normal_z;
	float diffuse[4]; // Note sure if this is 3 or 4 floats
	float specular[4]; // Note sure if this is 3 or 4 floats
	float tangent_x, tangent_y, tangent_z;
	float bitangent_x, bitangent_y, bitangent_z;
	std::vector<float> unknown; // GW specific;
	float tex_coord[8][2];

	int num_texcoords = 0;
	int num_unknown = 0;

	ModelVertex() = default;
	ModelVertex(DWORD FVF, bool& parsed_correctly, int vertex_size)
	{
		const auto actual_FVF = FVF_to_ActualFVF(FVF);
		if (actual_FVF == 0)
		{
			parsed_correctly = false;
			return;
		}
		has_position = (actual_FVF & D3DFVF_POSITION_MASK) != 0;
		has_group = FVF & GR_FVF_GROUP; // GW specific?, so use GW FVF
		has_normal = actual_FVF & D3DFVF_NORMAL; // Has normal map (texture)?
		has_diffuse = actual_FVF & D3DFVF_DIFFUSE;
		has_specular = actual_FVF & D3DFVF_SPECULAR;
		has_tangent = FVF & GR_FVF_TANGENT_AND_BITANGENT; // No tangent flag in D3DFVF, so use GW_FVF
		has_bitangent = has_tangent; // No bitangent flag I think. the tangent and bitangent always come together? so if the flag is set they are both set in the vertex format.

		num_texcoords = (actual_FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
		for (int i = 0; i < 8; ++i)
		{
			has_tex_coord[i] = i < num_texcoords;
		}

		num_unknown = (vertex_size / 4) - (has_position) * 3 - (has_group) * 1 - (has_normal) * 3 -
			(has_diffuse) * 3 - (has_specular) * 3 - (has_tangent) * 3 - (has_bitangent) * 3 - num_texcoords * 2;
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
	uint8_t max_UV_index; // Num pixel shaders?
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
	uint8_t f0x31[3];
	uint32_t f0x34;
	uint32_t f0x38;
	uint32_t f0x3C;
	uint32_t f0x40;
	uint32_t num_models;
	uint32_t f0x48;
	uint16_t collision_count;
	uint8_t f0x4E[2];
	uint16_t num_some_struct2;
	uint16_t f0x52;

	Chunk1_sub1() = default;

	Chunk1_sub1(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }
};

struct Chunk1_sub1_other
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
	uint8_t max_UV_index;
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

	Chunk1_sub1_other() = default;

	Chunk1_sub1_other(const unsigned char* data) { std::memcpy(this, data, sizeof(*this)); }
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

struct Sub1F0x52Struct
{
	std::vector<uint8_t> data0x52;
	std::vector<uint8_t> data0x52_2;

	Sub1F0x52Struct() = default;

	Sub1F0x52Struct(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& parsed_correctly, Chunk1_sub1& sub_1)
	{
		if ((sub_1.f0x8 & 8) >> 3 == 1)
		{
			if (sub_1.f0x52 != 0)
			{
				uint32_t size1 =
					get_some_size(data + curr_offset, sub_1.f0x52, data_size_bytes, parsed_correctly);
				if (!parsed_correctly)
				{
					return;
				}

				data0x52.resize(sub_1.f0x52 * 0x30);

				if (curr_offset + data0x52.size() <= data_size_bytes)
				{
					std::memcpy(data0x52.data(), &data[curr_offset], data0x52.size());
				}
				else
				{
					parsed_correctly = false;
					return;
				}
				curr_offset += data0x52.size();

				if (curr_offset + size1 >= data_size_bytes)
				{
					parsed_correctly = false;
					return;
				}

				data0x52_2.resize(size1);
				if (curr_offset + data0x52_2.size() <= data_size_bytes)
				{
					std::memcpy(data0x52_2.data(), &data[curr_offset], data0x52_2.size());
				}
				else
				{
					parsed_correctly = false;
					return;
				}
				curr_offset += data0x52_2.size();
			}
		}
	}
};

struct GeometryModelOther
{
	uint32_t unknown_0x04;
	uint32_t submodel_index;
	uint32_t num_indices;
	uint32_t num_vertices;
	uint32_t unknown_0x14;
	uint32_t unknown_0x18;
	uint32_t dat_fvf;
	uint32_t unknown_0x20;
	std::vector<uint16_t> indices;
	std::vector<ModelVertex> vertices;
	std::vector<uint8_t> extra_data; // Has size: (u0 + u1 + u2 * 3) * 4

	// Extra properties that I've added. Not from model file
	uint32_t total_num_indices;
	float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::min();
	float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::min();
	float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::min();
	float sumX = 0, sumY = 0, sumZ = 0, avgX = 0, avgY = 0, avgZ = 0;

	GeometryModelOther() = default;
	GeometryModelOther(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& parsed_correctly, int chunk_size)
	{
		if (curr_offset + 0x20 >= data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}

		std::memcpy(&unknown_0x04, &data[curr_offset], sizeof(unknown_0x04));
		curr_offset += sizeof(unknown_0x04);

		std::memcpy(&submodel_index, &data[curr_offset], sizeof(submodel_index));
		curr_offset += sizeof(submodel_index);

		std::memcpy(&num_indices, &data[curr_offset], sizeof(num_indices));
		curr_offset += sizeof(num_indices);

		std::memcpy(&num_vertices, &data[curr_offset], sizeof(num_vertices));
		curr_offset += sizeof(num_vertices);

		std::memcpy(&unknown_0x14, &data[curr_offset], sizeof(unknown_0x14));
		curr_offset += sizeof(unknown_0x14);

		std::memcpy(&unknown_0x18, &data[curr_offset], sizeof(unknown_0x18));
		curr_offset += sizeof(unknown_0x18);

		std::memcpy(&dat_fvf, &data[curr_offset], sizeof(dat_fvf));
		curr_offset += sizeof(dat_fvf);

		std::memcpy(&unknown_0x20, &data[curr_offset], sizeof(unknown_0x20));
		curr_offset += sizeof(unknown_0x20);

		uint32_t vertex_size = 12;

		total_num_indices = num_indices;

		if (curr_offset + total_num_indices * 2 < data_size_bytes && total_num_indices < 1000000)
		{
			indices.resize(total_num_indices);
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
				ModelVertex vertex;
				vertex.has_position = true;

				if (vertex.has_position)
				{
					std::memcpy(&vertex.x, &data[curr_offset], sizeof(vertex.x));
					curr_offset += sizeof(vertex.x);

					std::memcpy(&vertex.z, &data[curr_offset], sizeof(vertex.z));
					curr_offset += sizeof(vertex.z);

					std::memcpy(&vertex.y, &data[curr_offset], sizeof(vertex.y));
					vertex.y = -vertex.y;
					curr_offset += sizeof(vertex.y);
				}

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
		uint32_t extra_data_size = 0;
		if (curr_offset + extra_data_size < data_size_bytes)
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

		total_num_indices = num_indices0 + (num_indices0 != num_indices1) * num_indices1 +
			(num_indices1 != num_indices2) * num_indices2;

		if (curr_offset + total_num_indices * 2 < data_size_bytes && total_num_indices < 1000000)
		{
			indices.resize(total_num_indices);
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
				ModelVertex vertex(get_fvf(dat_fvf), parsed_correctly, vertex_size);
				if (vertex.num_texcoords < 0)
				{
					parsed_correctly = false;
					return;
				}
				if (vertex.has_position)
				{
					std::memcpy(&vertex.x, &data[curr_offset], sizeof(vertex.x));
					curr_offset += sizeof(vertex.x);

					std::memcpy(&vertex.z, &data[curr_offset], sizeof(vertex.z));
					curr_offset += sizeof(vertex.z);

					std::memcpy(&vertex.y, &data[curr_offset], sizeof(vertex.y));
					vertex.y = -vertex.y;
					curr_offset += sizeof(vertex.y);
				}

				if (vertex.has_group)
				{
					std::memcpy(&vertex.group, &data[curr_offset], sizeof(vertex.group));
					curr_offset += sizeof(vertex.group);
				}

				if (vertex.has_normal)
				{
					std::memcpy(&vertex.normal_x, &data[curr_offset], sizeof(vertex.normal_x));
					curr_offset += sizeof(vertex.normal_x);

					std::memcpy(&vertex.normal_z, &data[curr_offset], sizeof(vertex.normal_z));
					curr_offset += sizeof(vertex.normal_z);

					std::memcpy(&vertex.normal_y, &data[curr_offset], sizeof(vertex.normal_y));
					vertex.normal_y = -vertex.normal_y;
					curr_offset += sizeof(vertex.normal_y);
				}

				// Assuming that tangent and bitangent information is stored as 3 floats each
				if (vertex.has_tangent)
				{
					std::memcpy(&vertex.tangent_x, &data[curr_offset], sizeof(vertex.tangent_x));
					curr_offset += sizeof(vertex.tangent_x);

					std::memcpy(&vertex.tangent_z, &data[curr_offset], sizeof(vertex.tangent_z));
					curr_offset += sizeof(vertex.tangent_z);

					std::memcpy(&vertex.tangent_y, &data[curr_offset], sizeof(vertex.tangent_y));
					vertex.tangent_y = -vertex.tangent_y;
					curr_offset += sizeof(vertex.tangent_y);
				}

				if (vertex.has_bitangent)
				{
					std::memcpy(&vertex.bitangent_x, &data[curr_offset], sizeof(vertex.bitangent_x));
					curr_offset += sizeof(vertex.bitangent_x);

					std::memcpy(&vertex.bitangent_z, &data[curr_offset], sizeof(vertex.bitangent_z));
					curr_offset += sizeof(vertex.bitangent_z);

					std::memcpy(&vertex.bitangent_y, &data[curr_offset], sizeof(vertex.bitangent_y));
					vertex.bitangent_y = -vertex.bitangent_y;
					curr_offset += sizeof(vertex.bitangent_y);
				}

				if (vertex.has_diffuse)
				{
					std::memcpy(&vertex.diffuse, &data[curr_offset], sizeof(vertex.diffuse));
					curr_offset += sizeof(vertex.diffuse);
				}

				if (vertex.has_specular)
				{
					std::memcpy(&vertex.specular, &data[curr_offset], sizeof(vertex.specular));
					curr_offset += sizeof(vertex.specular);
				}

				if (vertex.num_unknown > 0)
				{
					vertex.unknown.resize(vertex.num_unknown);
					std::memcpy(vertex.unknown.data(), &data[curr_offset],
						sizeof(float) * vertex.num_unknown);
					curr_offset += sizeof(float) * vertex.num_unknown;
				}

				for (int j = 0; j < 8; ++j)
				{
					if (vertex.has_tex_coord[j] && curr_offset + sizeof(float) < data_size_bytes)
					{
						std::memcpy(&vertex.tex_coord[j][0], &data[curr_offset], sizeof(float));
						curr_offset += sizeof(float);

						std::memcpy(&vertex.tex_coord[j][1], &data[curr_offset], sizeof(float));
						curr_offset += sizeof(float);
					}
				}

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
	std::vector<ModelVertex> vertices;

	InteractiveModelMaybe() = default;
	InteractiveModelMaybe(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& parsed_correctly)
	{
		// Read num_indices and num_vertices
		if (curr_offset + sizeof(num_indices) + sizeof(num_vertices) > data_size_bytes)
		{
			parsed_correctly = false;
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
		}

		// Read vertices
		if (curr_offset + num_vertices * sizeof(ModelVertex) <= data_size_bytes)
		{
			vertices.resize(num_vertices);
			std::memcpy(vertices.data(), &data[curr_offset], num_vertices * sizeof(ModelVertex));
			curr_offset += num_vertices * sizeof(ModelVertex);
		}
		else
		{
			parsed_correctly = false;
			return;
		}
	}
};

#pragma pack(push, 1) // Set packing alignment to 1 byte
struct UnknownTexStruct0
{
	uint8_t using_no_cull; // 0 if cull enabled, 1 if no culling enabled.
	uint8_t f0x1;
	uint32_t f0x2;
	uint8_t pixel_shader_id;
	uint8_t f0x7;
};
#pragma pack(pop) // Restore the original packing alignment

#pragma pack(push, 1) // Set packing alignment to 1 byte
struct UnknownTexStruct1
{
	uint16_t some_flags0;
	uint16_t some_flags1;
	uint8_t f0x4;
	uint8_t f0x5;
	uint8_t num_textures_to_use;
	uint8_t f0x7;
	uint8_t f0x8;
};
#pragma pack(pop) // Restore the original packing alignment

struct TextureAndVertexShader
{
	std::vector<UnknownTexStruct0> uts0;
	std::vector<uint16_t> flags0;
	std::vector<uint8_t> tex_array;
	std::vector<uint8_t> zeros;
	std::vector<uint8_t> blend_state;
	std::vector<uint8_t> texture_index_UV_mapping_maybe;
	std::vector<uint8_t> unknown;

	TextureAndVertexShader() = default;
	TextureAndVertexShader(size_t max_UV_index, size_t num1, bool f0x20, uint32_t& curr_offset,
		const unsigned char* data, uint32_t data_size_bytes, bool& parsed_correctly)
	{
		if (max_UV_index > 100 || num1 > 100)
		{
			parsed_correctly = false;
			return;
		}
		// Resize vectors
		uts0.resize(max_UV_index);
		flags0.resize(num1);
		tex_array.resize(num1);
		zeros.resize(num1 * 4);
		blend_state.resize(num1);
		texture_index_UV_mapping_maybe.resize(num1);

		// Read uts0
		if (curr_offset + sizeof(UnknownTexStruct0) * max_UV_index > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		std::memcpy(uts0.data(), &data[curr_offset], sizeof(UnknownTexStruct0) * max_UV_index);
		curr_offset += sizeof(UnknownTexStruct0) * max_UV_index;

		if (curr_offset + sizeof(uint16_t) * num1 > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		// Read flags0
		std::memcpy(flags0.data(), &data[curr_offset], sizeof(uint16_t) * num1);
		curr_offset += sizeof(uint16_t) * num1;

		// Read tex_array
		if (curr_offset + sizeof(uint8_t) * num1 > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		std::memcpy(tex_array.data(), &data[curr_offset], sizeof(uint8_t) * num1);
		curr_offset += sizeof(uint8_t) * num1;

		// Read zeros
		if (curr_offset + sizeof(uint8_t) * num1 * 4 > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		std::memcpy(zeros.data(), &data[curr_offset], sizeof(uint8_t) * num1 * 4);
		curr_offset += sizeof(uint8_t) * num1 * 4;

		// Read blend_state
		if (curr_offset + sizeof(uint8_t) * num1 > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		std::memcpy(blend_state.data(), &data[curr_offset], sizeof(uint8_t) * num1);
		curr_offset += sizeof(uint8_t) * num1;

		// Read texture_index_UV_mapping_maybe
		if (curr_offset + sizeof(uint8_t) * num1 > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}
		std::memcpy(texture_index_UV_mapping_maybe.data(), &data[curr_offset], sizeof(uint8_t) * num1);
		curr_offset += sizeof(uint8_t) * num1;

		if (-(f0x20 != 0))
		{
			if (curr_offset + sizeof(uint8_t) * num1 > data_size_bytes)
			{
				parsed_correctly = false;
				return;
			}

			unknown.resize(num1);
			std::memcpy(unknown.data(), &data[curr_offset], sizeof(uint8_t) * num1);
			curr_offset += sizeof(uint8_t) * num1;
		}
	}
};

// SomeStruct2
struct SomeStruct2
{
	std::array<uint8_t, 8> unknown;
	uint32_t f0x8;
	uint32_t f0xC;
	uint32_t f0x10;
	uint32_t f0x14;
	std::vector<uint8_t> some_data; // dynamically sized

	SomeStruct2() = default;
	SomeStruct2(uint32_t& curr_offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& parsed_correctly)
	{
		if (curr_offset + sizeof(unknown) + sizeof(f0x8) + sizeof(f0xC) + sizeof(f0x10) + sizeof(f0x14) >
			data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}

		std::memcpy(unknown.data(), &data[curr_offset], unknown.size());
		curr_offset += unknown.size();

		std::memcpy(&f0x8, &data[curr_offset], sizeof(f0x8));
		curr_offset += sizeof(f0x8);

		std::memcpy(&f0xC, &data[curr_offset], sizeof(f0xC));
		curr_offset += sizeof(f0xC);

		std::memcpy(&f0x10, &data[curr_offset], sizeof(f0x10));
		curr_offset += sizeof(f0x10);

		std::memcpy(&f0x14, &data[curr_offset], sizeof(f0x14));
		curr_offset += sizeof(f0x14);

		uint32_t data_size = f0x8 * 7 + (f0xC + f0x10 + f0x14) * 8;
		if (curr_offset + data_size > data_size_bytes)
		{
			parsed_correctly = false;
			return;
		}

		some_data.resize(data_size);
		std::memcpy(some_data.data(), &data[curr_offset], data_size);
		curr_offset += data_size;
	}
};

struct GeometryChunkOther {
	uint32_t chunk_id;
	uint32_t chunk_size;
	Chunk1_sub1_other sub_1;

	TextureAndVertexShader tex_and_vertex_shader_struct;
	std::vector<UnknownTexStruct1> uts1;
	std::vector<uint8_t> unknown2;
	std::vector<uint8_t> unknown3;
	std::vector<uint8_t> unknown_data_0;
	std::vector<uint8_t> unknown_data_1;
	std::vector<std::string> strings;
	std::vector<ComplexStruct> complex_structs;
	std::vector<uint16_t> unknown_tex_stuff0;
	std::vector<uint8_t> unknown_tex_stuff1;
	uint32_t unknown4;
	uint32_t unknown5;

	uint32_t num_models;
	std::vector<GeometryModelOther> models;
	std::vector<uint8_t> chunk_data;

	uint32_t compute_str_len_plus_one(const unsigned char* data, uint32_t address)
	{
		uint32_t counter = 0;
		bool found = false;
		while (!found)
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

	GeometryChunkOther() = default;

	GeometryChunkOther(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& parsed_correctly)
	{
		std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
		std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));

		uint32_t curr_offset = offset + 8;
		sub_1 = Chunk1_sub1_other(&data[curr_offset]);
		curr_offset += sizeof(sub_1);

		const bool prev_parsed_correctly = parsed_correctly;
		const int prev_offset = curr_offset;
		tex_and_vertex_shader_struct =
			TextureAndVertexShader(sub_1.max_UV_index, sub_1.some_num1, sub_1.f0x20, curr_offset, data,
				data_size_bytes, parsed_correctly);
		if (prev_parsed_correctly != parsed_correctly)
		{
			// We want to render the models even if we can't apply textures.
			parsed_correctly == true;
			curr_offset = prev_offset + sub_1.max_UV_index * 3 + sub_1.some_num1 * 9 +
				(-(sub_1.f0x20 != 0) & sub_1.some_num1);
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
						uts1.resize(sub_1.f0x19);
						std::memcpy(uts1.data(), &data[curr_offset],
							sizeof(UnknownTexStruct1) * sub_1.f0x19);
						curr_offset += sizeof(UnknownTexStruct1) * sub_1.f0x19;

						unknown_tex_stuff0.resize(sub_1.f0x1d);
						std::memcpy(unknown_tex_stuff0.data(), &data[curr_offset],
							sizeof(uint16_t) * sub_1.f0x1d);
						curr_offset += sizeof(uint16_t) * sub_1.f0x1d;

						unknown_tex_stuff1.resize(sub_1.f0x1d);
						std::memcpy(unknown_tex_stuff1.data(), &data[curr_offset],
							sizeof(uint8_t) * sub_1.f0x1d);
						curr_offset += sizeof(uint8_t) * sub_1.f0x1d;

						uint32_t unknown_data0_size = _Src - sizeof(UnknownTexStruct1) * uts1.size() -
							sizeof(uint16_t) * unknown_tex_stuff0.size() -
							sizeof(uint8_t) * unknown_tex_stuff1.size();
						unknown_data_0.resize(unknown_data0_size);
						std::memcpy(unknown_data_0.data(), &data[curr_offset], unknown_data0_size);
						curr_offset += unknown_data0_size;

						strings.resize(sub_1.f0x1a);
						for (uint32_t i = 0; i < sub_1.f0x1a; ++i)
						{
							uint32_t str_len = compute_str_len_plus_one(data, curr_offset);
							strings[i] =
								std::string(reinterpret_cast<const char*>(&data[curr_offset]), str_len - 1);
							curr_offset += str_len;
						}

						unknown_data_1.resize(sub_1.f0x1e * 2 * 4);
						std::memcpy(unknown_data_1.data(), &data[curr_offset], sub_1.f0x1e * 2 * 4);
						curr_offset += sub_1.f0x1e * 2 * 4;
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


		std::memcpy(&num_models, &data[curr_offset], sizeof(num_models));
		curr_offset += sizeof(num_models);
		// Only load 1 model for now
		num_models = 1;
		for (int i = 0; i < num_models; i++)
		{
			auto new_model =
				GeometryModelOther(curr_offset, data, data_size_bytes, parsed_correctly, chunk_size);
			models.push_back(new_model);
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

struct GeometryChunk
{
	uint32_t chunk_id;
	uint32_t chunk_size;
	Chunk1_sub1 sub_1;
	std::vector<uint8_t> unknown0;
	std::vector<SomeStruct2> some_struct_2s;

	TextureAndVertexShader tex_and_vertex_shader_struct;
	std::vector<uint8_t> unknown2;
	std::vector<uint8_t> unknown3;
	std::vector<uint8_t> unknown_data_0;
	std::vector<uint8_t> unknown_data_1;
	std::vector<std::string> strings;

	std::vector<UnknownTexStruct1> uts1;
	std::vector<uint16_t> unknown_tex_stuff0;
	std::vector<uint8_t> unknown_tex_stuff1;

	Sub1F0x52Struct sub1_f0x52_struct;

	uint32_t unknown4;
	uint32_t unknown5;
	std::vector<ComplexStruct> complex_structs;
	std::vector<GeometryModel> models;
	std::vector<uint8_t> unknown_data;
	std::vector<uint8_t> chunk_data;

	uint32_t compute_str_len_plus_one(const unsigned char* data, uint32_t address)
	{
		uint32_t counter = 0;
		bool found = false;
		while (!found)
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

		uint32_t unknown0_size = sub_1.num_some_struct0 * 0x1C;
		if (offset + unknown0_size > offset + chunk_size + 8)
		{
			parsed_correctly = false;
			return;
		}

		unknown0.resize(unknown0_size);
		std::memcpy(unknown0.data(), &data[curr_offset], unknown0_size);
		curr_offset += unknown0_size;

		for (int i = 0; i < sub_1.num_some_struct2; i++)
		{
			SomeStruct2 new_some_struct2 = SomeStruct2(curr_offset, data, data_size_bytes, parsed_correctly);
			some_struct_2s.push_back(new_some_struct2);

			if (!parsed_correctly) // Check if parsing was successful
			{
				break;
			}
		}

		if (sub_1.num_models > 0)
		{
			const bool prev_parsed_correctly = parsed_correctly;
			const int prev_offset = curr_offset;
			tex_and_vertex_shader_struct =
				TextureAndVertexShader(sub_1.max_UV_index, sub_1.some_num1, sub_1.f0x20, curr_offset, data,
					data_size_bytes, parsed_correctly);
			if (prev_parsed_correctly != parsed_correctly)
			{
				// We want to render the models even if we can't apply textures.
				parsed_correctly == true;
				curr_offset = prev_offset + sub_1.max_UV_index * 3 + sub_1.some_num1 * 9 +
					(-(sub_1.f0x20 != 0) & sub_1.some_num1);
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
							uts1.resize(sub_1.f0x19);
							std::memcpy(uts1.data(), &data[curr_offset],
								sizeof(UnknownTexStruct1) * sub_1.f0x19);
							curr_offset += sizeof(UnknownTexStruct1) * sub_1.f0x19;

							unknown_tex_stuff0.resize(sub_1.f0x1d);
							std::memcpy(unknown_tex_stuff0.data(), &data[curr_offset],
								sizeof(uint16_t) * sub_1.f0x1d);
							curr_offset += sizeof(uint16_t) * sub_1.f0x1d;

							unknown_tex_stuff1.resize(sub_1.f0x1d);
							std::memcpy(unknown_tex_stuff1.data(), &data[curr_offset],
								sizeof(uint8_t) * sub_1.f0x1d);
							curr_offset += sizeof(uint8_t) * sub_1.f0x1d;

							uint32_t unknown_data0_size = _Src - sizeof(UnknownTexStruct1) * uts1.size() -
								sizeof(uint16_t) * unknown_tex_stuff0.size() -
								sizeof(uint8_t) * unknown_tex_stuff1.size();
							unknown_data_0.resize(unknown_data0_size);
							std::memcpy(unknown_data_0.data(), &data[curr_offset], unknown_data0_size);
							curr_offset += unknown_data0_size;

							strings.resize(sub_1.f0x1a);
							for (uint32_t i = 0; i < sub_1.f0x1a; ++i)
							{
								uint32_t str_len = compute_str_len_plus_one(data, curr_offset);
								strings[i] =
									std::string(reinterpret_cast<const char*>(&data[curr_offset]), str_len - 1);
								curr_offset += str_len;
							}

							unknown_data_1.resize(sub_1.f0x1e * 2 * 4);
							std::memcpy(unknown_data_1.data(), &data[curr_offset], sub_1.f0x1e * 2 * 4);
							curr_offset += sub_1.f0x1e * 2 * 4;
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

			sub1_f0x52_struct = Sub1F0x52Struct(curr_offset, data, data_size_bytes, parsed_correctly, sub_1);
			if (!parsed_correctly)
			{
				return;
			}

			int num_models = sub_1.num_models;
			for (int i = 0; i < num_models; i++)
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

		uint32_t unknown_data_size = sub_1.f0x34;
		if (curr_offset + unknown_data_size > offset + chunk_size + 8)
		{
			parsed_correctly = false;
			return;
		}

		unknown_data.resize(unknown_data_size);
		std::memcpy(unknown_data.data(), &data[curr_offset], unknown_data_size);
		curr_offset += unknown_data_size;

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

struct TextureFileName
{
	uint16_t id0;
	uint16_t id1;
	uint16_t unknown;

	TextureFileName() = default;

	TextureFileName(uint32_t offset, const unsigned char* data)
	{
		std::memcpy(&id0, &data[offset], sizeof(id0));
		std::memcpy(&id1, &data[offset + 2], sizeof(id1));
		std::memcpy(&unknown, &data[offset + 4], sizeof(unknown));
	}
};

struct TextureFileNamesChunk
{
	uint32_t chunk_id;
	uint32_t chunk_size;
	uint32_t num_texture_filenames;
	uint32_t actual_num_texture_filenames;
	std::vector<TextureFileName> texture_filenames;
	std::vector<uint8_t> chunk_data;

	TextureFileNamesChunk() = default;

	TextureFileNamesChunk(uint32_t offset, const unsigned char* data, uint32_t data_size_bytes,
		bool& textures_parsed_correctly)
	{
		std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
		std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));
		std::memcpy(&num_texture_filenames, &data[offset + 8], sizeof(num_texture_filenames));

		actual_num_texture_filenames = std::min(num_texture_filenames, (chunk_size - 4) / sizeof(TextureFileName));

		uint32_t curr_offset = offset + 12;
		texture_filenames.resize(actual_num_texture_filenames);

		for (uint32_t i = 0; i < actual_num_texture_filenames; ++i)
		{
			texture_filenames[i] = TextureFileName(curr_offset, data);
			curr_offset += sizeof(TextureFileName);
		}

		size_t remaining_bytes = chunk_size - 4 - (sizeof(TextureFileName) * actual_num_texture_filenames);
		if (curr_offset + remaining_bytes <= offset + chunk_size + 8 && remaining_bytes < chunk_size)
		{
			chunk_data.resize(remaining_bytes);
			std::memcpy(chunk_data.data(), &data[curr_offset], remaining_bytes);
		}
		else
		{
			textures_parsed_correctly = false;
			return;
		}
	}
};

constexpr uint32_t CHUNK_ID_GEOMETRY = 0x00000FA0;
constexpr uint32_t CHUNK_ID_GEOMETRY_OTHER = 0x00000BB8;
constexpr uint32_t CHUNK_ID_TEXTURE_ATEXDXT3_PLAIN = 0x00000FA3;
constexpr uint32_t CHUNK_ID_TEXTURE_FILENAMES = 0x00000FA5;
constexpr uint32_t CHUNK_ID_AMAT_FILENAMES = 0x00000FAD;

struct FFNA_ModelFile
{
	char ffna_signature[4];
	FFNAType ffna_type;
	GeometryChunk geometry_chunk;
	GeometryChunkOther geometry_chunk_other;
	DatTexture atexdxt3_plain_texture;
	TextureFileNamesChunk texture_filenames_chunk;
	TextureFileNamesChunk AMAT_filenames_chunk;

	bool parsed_correctly = true;
	bool textures_parsed_correctly = true;
	bool AMATs_parsed_correctly = true;

	std::unordered_map<uint32_t, int> riff_chunks;

	std::unordered_set<int> seen_model_ids;

	FFNA_ModelFile() = default;
	FFNA_ModelFile(int offset, std::span<unsigned char>& data)
	{
		uint32_t current_offset = offset;

		std::memcpy(ffna_signature, &data[offset], sizeof(ffna_signature));
		current_offset += 4;
		std::memcpy(&ffna_type, &data[offset], sizeof(ffna_type));
		current_offset += 1;

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

		it = riff_chunks.find(CHUNK_ID_GEOMETRY_OTHER);
		if (it != riff_chunks.end())
		{
			int offset = it->second;
			geometry_chunk_other = GeometryChunkOther(offset, data.data(), data.size_bytes(), parsed_correctly);
		}

		// Check if the CHUNK_ID_TEXTURE_ATEXDXT3_PLAIN is in the riff_chunks map
		it = riff_chunks.find(CHUNK_ID_TEXTURE_ATEXDXT3_PLAIN);
		if (it != riff_chunks.end())
		{
			int offset = it->second;

			uint32_t chunk_id;
			uint32_t chunk_size;
			std::memcpy(&chunk_id, &data[offset], sizeof(chunk_id));
			std::memcpy(&chunk_size, &data[offset + 4], sizeof(chunk_size));


			std::vector<uint8_t> texture_data;
			texture_data.resize(chunk_size);
			std::memcpy(texture_data.data(), &data[it->second + 8], chunk_size);

			atexdxt3_plain_texture = ProcessImageFile(texture_data.data(), texture_data.size());
		}

		// Check if the CHUNK_ID_TEXTURE_FILENAMES is in the riff_chunks map
		it = riff_chunks.find(CHUNK_ID_TEXTURE_FILENAMES);
		if (it != riff_chunks.end())
		{
			int offset = it->second;
			texture_filenames_chunk =
				TextureFileNamesChunk(offset, data.data(), data.size_bytes(), textures_parsed_correctly);
		}

		// Check if the CHUNK_ID_AMAT_FILENAMES is in the riff_chunks map
		it = riff_chunks.find(CHUNK_ID_AMAT_FILENAMES);
		if (it != riff_chunks.end())
		{
			int offset = it->second;
			AMAT_filenames_chunk =
				TextureFileNamesChunk(offset, data.data(), data.size_bytes(), AMATs_parsed_correctly);
		}
	}

	Mesh GetMesh(int model_index, AMAT_file& amat_file)
	{
		std::vector<GWVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<uint32_t> indices1;
		std::vector<uint32_t> indices2;

		auto sub_model = geometry_chunk.models[model_index];

		int sub_model_index = sub_model.unknown;
		if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
		{
			sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
		}

		bool parsed_texture_with_UTS = textures_parsed_correctly &&
			sizeof(geometry_chunk.tex_and_vertex_shader_struct) > 0 &&
			geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0 &&
			geometry_chunk.tex_and_vertex_shader_struct.tex_array.size() > 0 &&
			geometry_chunk.tex_and_vertex_shader_struct.texture_index_UV_mapping_maybe.size() > 0;
		int max_num_tex_coords = 0;
		bool should_cull = false;
		BlendState blend_state = BlendState::Opaque;

		int num_uv_coords_start_index = 0;
		if (parsed_texture_with_UTS)
		{
			for (int i = 0; i < sub_model_index; i++)
			{
				auto uts = geometry_chunk.tex_and_vertex_shader_struct.uts0[i];
				num_uv_coords_start_index += uts.f0x7;
			}
		}


		for (int i = 0; i < sub_model.vertices.size(); i++)
		{
			ModelVertex model_vertex = sub_model.vertices[i];
			GWVertex vertex;
			if (!model_vertex.has_position)
				return Mesh();

			if (max_num_tex_coords < model_vertex.num_texcoords)
			{
				max_num_tex_coords = model_vertex.num_texcoords;
			}

			vertex.position = XMFLOAT3(model_vertex.x, model_vertex.y, model_vertex.z);

			if (model_vertex.has_normal) {
				vertex.normal = XMFLOAT3(model_vertex.normal_x, model_vertex.normal_y, model_vertex.normal_z);
			}

			if (model_vertex.has_tangent) {
				vertex.tangent = XMFLOAT3(model_vertex.tangent_x, model_vertex.tangent_y, model_vertex.tangent_z);
			}

			if (model_vertex.has_bitangent) {
				vertex.bitangent = XMFLOAT3(model_vertex.bitangent_x, model_vertex.bitangent_y, model_vertex.bitangent_z);
			}

			if (model_vertex.has_tex_coord[0])
			{
				vertex.tex_coord0 = XMFLOAT2(model_vertex.tex_coord[0][0], model_vertex.tex_coord[0][1]);
			}
			if (model_vertex.has_tex_coord[1])
			{
				vertex.tex_coord1 = XMFLOAT2(model_vertex.tex_coord[1][0], model_vertex.tex_coord[1][1]);
			}
			if (model_vertex.has_tex_coord[2])
			{
				vertex.tex_coord2 = XMFLOAT2(model_vertex.tex_coord[2][0], model_vertex.tex_coord[2][1]);
			}
			if (model_vertex.has_tex_coord[3])
			{
				vertex.tex_coord3 = XMFLOAT2(model_vertex.tex_coord[3][0], model_vertex.tex_coord[3][1]);
			}
			if (model_vertex.has_tex_coord[4])
			{
				vertex.tex_coord4 = XMFLOAT2(model_vertex.tex_coord[4][0], model_vertex.tex_coord[4][1]);
			}
			if (model_vertex.has_tex_coord[5])
			{
				vertex.tex_coord5 = XMFLOAT2(model_vertex.tex_coord[5][0], model_vertex.tex_coord[5][1]);
			}
			if (model_vertex.has_tex_coord[6])
			{
				vertex.tex_coord6 = XMFLOAT2(model_vertex.tex_coord[6][0], model_vertex.tex_coord[6][1]);
			}
			if (model_vertex.has_tex_coord[7])
			{
				vertex.tex_coord7 = XMFLOAT2(model_vertex.tex_coord[7][0], model_vertex.tex_coord[7][1]);
			}

			vertices.push_back(vertex);
		}

		// Highest quality LOD indices
		for (int i = 0; i < sub_model.num_indices0; i += 3)
		{
			int index0 = sub_model.indices[i];
			int index1 = sub_model.indices[i + 1];
			int index2 = sub_model.indices[i + 2];

			if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
			{
				return Mesh();
			}

			indices.push_back(index0);
			indices.push_back(index1);
			indices.push_back(index2);
		}

		// Medium quality LOD indices
		bool has_med_lod = false;
		if (sub_model.num_indices0 != sub_model.num_indices1) {
			has_med_lod = true;
			for (int i = sub_model.num_indices0; i < sub_model.num_indices0 + sub_model.num_indices1; i += 3)
			{
				int index0 = sub_model.indices[i];
				int index1 = sub_model.indices[i + 1];
				int index2 = sub_model.indices[i + 2];

				if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
				{
					return Mesh();
				}

				indices1.push_back(index0);
				indices1.push_back(index1);
				indices1.push_back(index2);
			}
		}
		else {
			indices1 = indices;
		}

		// Low quality LOD indices
		if (sub_model.num_indices0 != sub_model.num_indices2 && sub_model.num_indices1 != sub_model.num_indices2) {
			int indices_offset = has_med_lod ? sub_model.num_indices0 + sub_model.num_indices1 : sub_model.num_indices0;
			for (int i = indices_offset; i < indices_offset + sub_model.num_indices2; i += 3)
			{
				int index0 = sub_model.indices[i];
				int index1 = sub_model.indices[i + 1];
				int index2 = sub_model.indices[i + 2];

				if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
				{
					return Mesh();
				}

				indices2.push_back(index0);
				indices2.push_back(index1);
				indices2.push_back(index2);
			}
		}
		else {
			indices2 = indices1;
		}

		std::vector<uint8_t> uv_coords_indices;
		std::vector<uint8_t> tex_indices;
		std::vector<uint8_t> blend_flags;
		std::vector<uint16_t> texture_types;

		// Old model format (mostly Prophecies and Factions)
		if (parsed_texture_with_UTS && textures_parsed_correctly && geometry_chunk.unknown_tex_stuff1.size() == 0)
		{
			auto uts = geometry_chunk.tex_and_vertex_shader_struct.uts0[sub_model_index];
			int num_uv_coords_to_use = uts.f0x7;
			should_cull = uts.using_no_cull == 0;

			std::vector<int> indices_to_skip;

			for (int i = num_uv_coords_start_index; i < num_uv_coords_start_index + num_uv_coords_to_use; i++)
			{
				bool textures_swapped = false;

				uint8_t uv_set_index = geometry_chunk.tex_and_vertex_shader_struct.tex_array[i];

				if (uv_set_index == 255)
				{
					indices_to_skip.push_back(i);
					continue;
				}

				if (uv_set_index == 253 && geometry_chunk.tex_and_vertex_shader_struct.tex_array.size() > i + 1 && i + 1 < num_uv_coords_start_index + num_uv_coords_to_use)
				{
					const auto flag0 = geometry_chunk.tex_and_vertex_shader_struct.flags0[i];
					const auto next_flag0 = geometry_chunk.tex_and_vertex_shader_struct.flags0[i + 1];
					if (flag0 != next_flag0) {
						uint8_t next_uv_set_index = geometry_chunk.tex_and_vertex_shader_struct.tex_array[i + 1];
						uv_coords_indices.push_back(next_uv_set_index);

						textures_swapped = true;
					}
				}

				if (max_num_tex_coords < uv_set_index && max_num_tex_coords > 0)
				{
					uv_set_index = 0;
				}

				uv_coords_indices.push_back(uv_set_index);

				uint8_t texture_index = geometry_chunk.tex_and_vertex_shader_struct.texture_index_UV_mapping_maybe[i];
				if (texture_filenames_chunk.actual_num_texture_filenames < texture_index &&
					texture_filenames_chunk.actual_num_texture_filenames > 0)
				{
					texture_index = texture_filenames_chunk.actual_num_texture_filenames - 1;
				}

				// Swap this and the next texture
				if (textures_swapped && geometry_chunk.tex_and_vertex_shader_struct.texture_index_UV_mapping_maybe.size() > i + 1 && i + 1 < num_uv_coords_start_index + num_uv_coords_to_use)
				{
					uint8_t next_texture_index = geometry_chunk.tex_and_vertex_shader_struct.texture_index_UV_mapping_maybe[i + 1];
					if (texture_filenames_chunk.actual_num_texture_filenames > next_texture_index)
					{
						tex_indices.push_back(next_texture_index);
						// Skip the next texture since we inserted it here
					}
				}

				if (textures_swapped)
				{
					i++;
				}

				tex_indices.push_back(texture_index);
			}


			for (int i = num_uv_coords_start_index; i < num_uv_coords_start_index + num_uv_coords_to_use; i++)
			{
				uint8_t blend_flag = geometry_chunk.tex_and_vertex_shader_struct.blend_state[i];

				//if (textures_swapped && geometry_chunk.tex_and_vertex_shader_struct.blend_state.size() > i + 1)
				//{
					//uint8_t next_blend_flag = geometry_chunk.tex_and_vertex_shader_struct.blend_state[i+1];
	 //               blend_flags.push_back(next_blend_flag);
	 //               i++;
				//}

				if (std::find(indices_to_skip.begin(), indices_to_skip.end(), i) != indices_to_skip.end())
				{
					continue;
				}

				if (uts.f0x7 == 2 && blend_flag == 6 && blend_flags.size() > 0 && blend_flags[blend_flags.size() - 1] == 8)
				{
					if (tex_indices.size() > 0)
					{
						const auto tex_index0 = tex_indices[blend_flags.size()];
						const auto tex_index1 = tex_indices[blend_flags.size() - 1];
						if (tex_index0 < texture_filenames_chunk.actual_num_texture_filenames && tex_index1 < texture_filenames_chunk.actual_num_texture_filenames) {
							const auto fname0 = texture_filenames_chunk.texture_filenames[tex_index0];
							const auto fname1 = texture_filenames_chunk.texture_filenames[tex_index1];

							if (decode_filename(fname0.id0, fname0.id1) == decode_filename(fname1.id0, fname1.id1)) {
								// Don't use inverse alpha
								blend_flag |= 0x10;
							}
						}
					}
				}

				blend_flags.push_back(blend_flag);
			}

			for (int i = num_uv_coords_start_index; i < num_uv_coords_start_index + num_uv_coords_to_use; i++)
			{
				uint16_t texture_type = geometry_chunk.tex_and_vertex_shader_struct.flags0[i];

				//if (textures_swapped && geometry_chunk.tex_and_vertex_shader_struct.flags0.size() > i + 1)
				//{
					//uint16_t next_texture_type = geometry_chunk.tex_and_vertex_shader_struct.flags0[i+1];
	 //               texture_types.push_back(next_texture_type);
	 //               i++;
				//}
				if (std::find(indices_to_skip.begin(), indices_to_skip.end(), i) != indices_to_skip.end())
				{
					continue;
				}


				texture_types.push_back(texture_type);
			}


			// Blend state (Wrong not how the game does it, just for testing)
			for (int i = num_uv_coords_start_index; i < num_uv_coords_start_index + num_uv_coords_to_use; i++)
			{
				// Note this is just an approximation. All models use alpha blend in the pixel shader but this is used for optimization.
				// For example models with blend_state set to AlphaBlend is rendered after water and those with opaque before water.
				// Other than that it has no uses so far. Was once used for sorting meshes but that has been disabled for now.
				if (geometry_chunk.tex_and_vertex_shader_struct.blend_state[i] == 8)
				{
					blend_state = BlendState::AlphaBlend;
					break;
				}
			}
		}

		// Modern model format (Primarily Nightfall and EoTN)
		if (textures_parsed_correctly && geometry_chunk.unknown_tex_stuff1.size() > 0)
		{
			tex_indices.clear();
			uv_coords_indices.clear();
			blend_flags.clear();
			texture_types.clear();

			int tex_index_start = 0;
			for (int i = 0; i < sub_model_index; i++)
			{
				auto uts = geometry_chunk.uts1[i % geometry_chunk.uts1.size()];
				tex_index_start += uts.num_textures_to_use;
			}

			const auto uts1 = geometry_chunk.uts1[model_index % geometry_chunk.uts1.size()];
			uint8_t blend_flag = 0;
			for (int i = 0; i < uts1.num_textures_to_use; i++)
			{
				uint8_t texture_index = geometry_chunk.unknown_tex_stuff1[(tex_index_start + i) % geometry_chunk.unknown_tex_stuff1.size()];

				if (AMATs_parsed_correctly && amat_file.GRSN_chunk.chunk_data.size() > 6 * 4 && amat_file.GRSN_chunk.chunk_data[6 * 4] != 0)
				{
					// Tell the pixel shader to use alpha for this texture. (it sets alpha to 1 otherwise).
					blend_flag = 8;
					blend_state = BlendState::AlphaBlend;
				}

				if (texture_index > texture_filenames_chunk.actual_num_texture_filenames)
				{
					texture_index = texture_index & 0x0F;
				}

				if (texture_filenames_chunk.actual_num_texture_filenames < texture_index &&
					texture_filenames_chunk.actual_num_texture_filenames > 0)
				{
					break;
				}

				tex_indices.push_back(texture_index);
				uv_coords_indices.push_back(i % sub_model.vertices[0].num_texcoords);
			}

			for (int i = 0; i < tex_indices.size(); i++)
			{
				blend_flags.push_back(blend_flag);
				texture_types.push_back(0xFFFF); // not defined in the "new" model files
			}

			const auto tex_infos = amat_file.DX9S_chunk.sub_chunk_0.tex_infos;

			if (tex_infos.size() == tex_indices.size())
			{

				// Order the texture and uv indices correctly based on the info from the AMAT file.
				std::unordered_map<int, int> actual_index_map;
				std::vector<uint8_t> actual_tex_indices(tex_indices.size());
				std::vector<uint8_t> actual_uv_coords_indices(uv_coords_indices.size());
				for (int i = 0; i < tex_infos.size(); i++)
				{
					actual_tex_indices[i] = tex_indices[tex_infos[i].tex_index % tex_indices.size()];
					actual_uv_coords_indices[i] = i % sub_model.vertices[0].num_texcoords; //uv_coords_indices[tex_infos[i].tex_index];
				}

				tex_indices = actual_tex_indices;
				uv_coords_indices = actual_uv_coords_indices;
			}
		}

		return Mesh(vertices, indices, indices1, indices2, uv_coords_indices, tex_indices, blend_flags, texture_types, should_cull, blend_state,
			tex_indices.size());
	}

	Mesh GetMeshOther(int model_index) {
		std::vector<GWVertex> vertices;
		std::vector<uint32_t> indices;

		if (model_index < 0 || model_index >= geometry_chunk_other.models.size())
		{
			return Mesh();
		}

		auto sub_model = geometry_chunk_other.models[model_index];

		for (int i = 0; i < sub_model.vertices.size(); i++)
		{
			ModelVertex model_vertex = sub_model.vertices[i];
			GWVertex vertex;
			if (!model_vertex.has_position)
				return Mesh();

			vertex.position = XMFLOAT3(model_vertex.x, model_vertex.y, model_vertex.z);
			vertices.push_back(vertex);
		}

		// Highest quality LOD indices
		for (int i = 0; i < sub_model.num_indices; i += 3)
		{
			int index0 = sub_model.indices[i];
			int index1 = sub_model.indices[i + 1];
			int index2 = sub_model.indices[i + 2];

			if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
			{
				return Mesh();
			}

			indices.push_back(index0);
			indices.push_back(index1);
			indices.push_back(index2);
		}

		return Mesh(vertices, indices, {}, {}, {}, {}, {}, {}, false, BlendState::Opaque, 0);
	}
};
