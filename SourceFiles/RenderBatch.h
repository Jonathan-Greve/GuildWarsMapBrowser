#pragma once
#include "RenderCommand.h"

class RenderBatch
{
public:
    void AddCommand(const RenderCommand& command)
    {
        int mesh_id = command.meshInstance->GetMeshID();
        m_commands[mesh_id] = command;
        m_sortedCommands[mesh_id] = command;
        m_needsSorting = true;
    }

    void RemoveCommand(const int mesh_id)
    {
        m_commands.erase(mesh_id);
        m_sortedCommands.erase(mesh_id);
        m_needsSorting = true;
    }

    void SortCommands()
    {
        if (! m_needsSorting)
        {
            return;
        }

        std::vector<RenderCommand> sortedCommandsVector;
        sortedCommandsVector.reserve(m_commands.size());

        for (auto& cmd : m_commands)
        {
            sortedCommandsVector.push_back(cmd.second);
        }

        std::sort(sortedCommandsVector.begin(), sortedCommandsVector.end(),
                  [](const RenderCommand& a, const RenderCommand& b)
                  {
                      if (a.primitiveTopology == b.primitiveTopology)
                      {
                          return a.pixelShaderType < b.pixelShaderType;
                      }
                      return a.primitiveTopology < b.primitiveTopology;
                  });

        m_sortedCommands.clear();
        for (auto& cmd : sortedCommandsVector)
        {
            int mesh_id = cmd.meshInstance->GetMeshID();
            m_sortedCommands[mesh_id] = cmd;
        }

        m_needsSorting = false;
    }

    void SetShouldRender(int mesh_id, bool should_render)
    {
        auto it = m_commands.find(mesh_id);
        if (it != m_commands.end())
        {
            it->second.should_render = should_render;
            m_sortedCommands[mesh_id].should_render = should_render;
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

        std::vector<RenderCommand> result;
        result.reserve(m_sortedCommands.size());

        for (const auto& cmd : m_sortedCommands)
        {
            result.push_back(cmd.second);
        }

        return result;
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
    std::unordered_map<int, RenderCommand> m_sortedCommands;
    bool m_needsSorting = false;
};
