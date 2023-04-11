#pragma once
#include "RenderCommand.h"

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

    void SortCommands()
    {
        if (! m_needsSorting)
        {
            return;
        }

        m_sortedCommands.clear();
        m_sortedCommands.reserve(m_commands.size());

        for (auto& cmd : m_commands)
        {
            m_sortedCommands.push_back(cmd.second);
        }

        std::sort(m_sortedCommands.begin(), m_sortedCommands.end(),
                  [](const RenderCommand& a, const RenderCommand& b)
                  { return a.blend_state < b.blend_state; });

        m_needsSorting = false;
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

    std::vector<RenderCommand> GetCommands()
    {
        if (m_needsSorting)
        {
            SortCommands();
        }
        return m_sortedCommands;
    }

    const RenderCommand* const GetCommand(int mesh_id)
    {
        auto it = m_commands.find(mesh_id);
        if (it != m_commands.end())
        {
            return &it->second;
        }
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
