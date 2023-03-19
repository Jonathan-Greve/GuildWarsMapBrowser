#pragma once
#include "RenderCommand.h"

class RenderBatch
{
public:
    void AddCommand(const RenderCommand& command) { m_commands.push_back(command); }

    void SortCommands()
    {
        std::sort(m_commands.begin(), m_commands.end(),
                  [](const RenderCommand& a, const RenderCommand& b)
                  { return a.primitiveTopology < b.primitiveTopology; });
    }

    void Clear() { m_commands.clear(); }

    const std::vector<RenderCommand>& GetCommands() const { return m_commands; }

private:
    std::vector<RenderCommand> m_commands;
};
