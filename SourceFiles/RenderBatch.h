#pragma once
#include "RenderCommand.h"

class RenderBatch
{
public:
    void AddCommand(const RenderCommand& command) { m_commands.push_back(command); }
    void RemoveCommand(const int mesh_id)
    {
        m_commands.erase(std::remove_if(m_commands.begin(), m_commands.end(),
                                        [mesh_id](const RenderCommand& cmd)
                                        { return cmd.meshInstance->GetMeshID() == mesh_id; }),
                         m_commands.end());
    }

    void SortCommands()
    {
        std::sort(m_commands.begin(), m_commands.end(),
                  [](const RenderCommand& a, const RenderCommand& b)
                  {
                      if (a.primitiveTopology == b.primitiveTopology)
                      {
                          return a.pixelShaderType < b.pixelShaderType;
                      }
                      return a.primitiveTopology < b.primitiveTopology;
                  });
    }

    void Clear() { m_commands.clear(); }

    std::vector<RenderCommand>& GetCommands() { return m_commands; }

private:
    std::vector<RenderCommand> m_commands;
};
