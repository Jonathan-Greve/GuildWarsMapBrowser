#pragma once
#include "RenderCommand.h"
#include "DXMathHelpers.h"

class RenderBatch
{
public:
	void AddCommand(const RenderCommand& command)
	{
		int mesh_id = command.meshInstance->GetMeshID();
		m_commands[mesh_id] = command;
		m_needsSorting = true;
	}

	void RemoveCommand(const int mesh_id)
	{
		m_commands.erase(mesh_id);
		m_needsSorting = true;
	}

	void SortCommands(XMFLOAT3& camera_pos)
	{
		if (!m_needsSorting) { return; }

		m_sortedCommands.clear();
		m_sortedCommands.reserve(m_commands.size());

		for (const auto& entry : m_commands) { m_sortedCommands.push_back(entry.second); }

		auto comparator = [&camera_pos](const RenderCommand& a, const RenderCommand& b) -> bool
		{
			const auto a_local_pos = a.meshInstance->GetMesh().center;
			const auto b_local_pos = b.meshInstance->GetMesh().center;

			const auto a_world_pos = GetPositionFromMatrix(a.meshInstance->GetPerObjectData().world);
			const auto b_world_pos = GetPositionFromMatrix(b.meshInstance->GetPerObjectData().world);

			const auto a_pos = XMFLOAT3(a_local_pos.x + a_world_pos.x, a_local_pos.y + a_world_pos.y, a_local_pos.z + a_world_pos.z);
			const auto b_pos = XMFLOAT3(b_local_pos.x + b_world_pos.x, b_local_pos.y + b_world_pos.y, b_local_pos.z + b_world_pos.z);

			const auto a_distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&a_pos),
				                                                     XMLoadFloat3(&camera_pos))));
			const auto b_distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&b_pos),
				                                                     XMLoadFloat3(&camera_pos))));

			if (a.blend_state != BlendState::AlphaBlend && b.blend_state != BlendState::AlphaBlend)
			{
				return a_distance < b_distance;
				return false;
			}

			if (a.blend_state != BlendState::AlphaBlend)
			{
				return false;
			}

			if (b.blend_state != BlendState::AlphaBlend)
			{
				return true;
			}

			if (a.blend_state == BlendState::AlphaBlend && b.blend_state == BlendState::AlphaBlend)
			{
				return a_distance < b_distance;
			}

			throw "test";
		};

		std::sort(m_sortedCommands.rbegin(), m_sortedCommands.rend(), comparator);
	}

	void SetShouldRender(int mesh_id, bool should_render)
	{
		auto it = m_commands.find(mesh_id);
		if (it != m_commands.end())
		{
			it->second.should_render = should_render;
			m_needsSorting = true;
		}
	}

	void Clear()
	{
		m_commands.clear();
		m_sortedCommands.clear();
	}

	std::vector<RenderCommand> GetCommands() { return m_sortedCommands; }

	const RenderCommand* const GetCommand(int mesh_id)
	{
		auto it = m_commands.find(mesh_id);
		if (it != m_commands.end()) { return &it->second; }
		return nullptr;
	}

	bool SetPixelShader(int mesh_id, PixelShaderType pixel_shader_type)
	{
		auto it = m_commands.find(mesh_id);
		if (it != m_commands.end())
		{
			it->second.pixelShaderType = pixel_shader_type;
			m_needsSorting = true;

			return true;
		}

		return false;
	}

private:
	std::unordered_map<int, RenderCommand> m_commands;
	std::vector<RenderCommand> m_sortedCommands;
	bool m_needsSorting = false;
};
