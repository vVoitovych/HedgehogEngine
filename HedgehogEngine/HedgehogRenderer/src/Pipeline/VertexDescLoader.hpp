#pragma once

#include "RHI/api/RHITypes.hpp"

#include <string>
#include <vector>

namespace Renderer
{

struct VertexFileDesc
{
    std::vector<RHI::VertexBinding>   m_Bindings;
    std::vector<RHI::VertexAttribute> m_Attributes;
};

class VertexDescLoader
{
public:
    // Load and parse a .vdes file.
    // assetRelativePath must begin with '/' and be relative to the repo root,
    // e.g. "/HedgehogEngine/HedgehogRenderer/Assets/VertexDescriptions/PositionOnly.vdes".
    static VertexFileDesc Load(const std::string& assetRelativePath);

private:
    static RHI::Format          ParseFormat(const std::string& s);
    static RHI::VertexInputRate ParseInputRate(const std::string& s);
};

} // namespace Renderer
